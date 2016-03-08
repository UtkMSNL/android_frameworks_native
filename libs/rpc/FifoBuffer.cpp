
#include <string.h>
#include <stdlib.h>
#include <cassert>
#include <rpc/FifoBuffer.h>
#include <utils/Log.h>

namespace android {
// ---------------------------------------------------------------------------

#define BUFFERSIZE (1 << 14) // 16Kb

FifoBuffer* fifoCreate() {
    FifoBuffer* fb = new FifoBuffer();
    fb->pos_head = fb->pos_tail = 0;
    return fb;
}

void fifoDestroy(FifoBuffer* fb) {
    if(fb == NULL) {
        return;
    }
    while(!fb->buffers.empty()) {
        free(fb->buffers.front());
        fb->buffers.pop();
    }
    for(u4 i = 0; i < fb->freeBufs.size(); i++) {
        free(fb->freeBufs.at(i));
    }
    fb->freeBufs.clear();
}

bool fifoEmpty(FifoBuffer* fb) {
    if(fb == NULL) {
        return true;
    }
    return fb->buffers.empty();
}

u4 fifoSize(FifoBuffer* fb) {
    if(fb == NULL) {
        return 0;
    }
    if(fb->buffers.empty()) return 0;
    return BUFFERSIZE * (fb->buffers.size() - 1) +
         (fb->pos_tail ? fb->pos_tail : BUFFERSIZE) - fb->pos_head;
}

u4 fifoGetBufferSize(FifoBuffer* fb) {
    if(fb == NULL) {
        return 0;
    }
    u4 bsz = fb->buffers.size();
    if(bsz == 0) return 0;
    if(bsz == 1 && fb->pos_tail != 0) return fb->pos_tail - fb->pos_head;
    return BUFFERSIZE - fb->pos_head;
}

char* fifoGetBuffer(FifoBuffer* fb) {
    if(fb == NULL) {
        return NULL;
    }
    return fb->buffers.front() + fb->pos_head;
}

void fifoReadBuffer(FifoBuffer* fb, char* buf, u4 bytes) {
    if(fb == NULL) {
        return;
    }
    while(bytes > 0) {
        u4 rbytes = fifoGetBufferSize(fb);
        assert(rbytes != 0 && "tried to read too much from buffer");
        if(rbytes > bytes) rbytes = bytes;
        memcpy(buf, fifoGetBuffer(fb), rbytes);
        fifoPopBytes(fb, rbytes);
        bytes -= rbytes;
        buf += rbytes;
    }
}

void fifoPopBytes(FifoBuffer* fb, u4 bytes) {
    if(fb == NULL) {
        return;
    }
    if(fb->pos_head + bytes == BUFFERSIZE) {
        char* buf = fb->buffers.front();
        fb->buffers.pop();
        fb->freeBufs.push_back(buf);
        fb->pos_head = 0;
    } else {
        fb->pos_head += bytes;
        if(fb->pos_head == fb->pos_tail && fb->buffers.size() == 1) {
            char* buf = fb->buffers.front();
            fb->buffers.pop();
            fb->freeBufs.push_back(buf);
            fb->pos_head = fb->pos_tail = 0;
        }
    }
}

void fifoPushData(FifoBuffer* fb, char* buf, u4 bytes) {
    if(fb == NULL) {
        return;
    }
    while(bytes > 0) {
        u4 amt = fb->pos_tail + bytes < BUFFERSIZE ? bytes :
                                                 BUFFERSIZE - fb->pos_tail;
        if(fb->pos_tail == 0) {
            /* Need to push on a new buffer. */
            if(!fb->freeBufs.empty()) {
                fb->buffers.push(fb->freeBufs.back());
                fb->freeBufs.pop_back();
            } else {
                fb->buffers.push((char*) malloc(BUFFERSIZE));
            }
        }
        memcpy(fb->buffers.back() + fb->pos_tail, buf, amt);
        fb->pos_tail = (fb->pos_tail + amt) & (BUFFERSIZE - 1);
        bytes -= amt;
        buf += amt;
    }
}


// ---------------------------------------------------------------------------
}; // namespace android
