#ifndef RPC_FIFOBUFFER_H
#define RPC_FIFOBUFFER_H

#include <vector>
#include <queue>

namespace android {
// ---------------------------------------------------------------------------

typedef unsigned int u4;

typedef struct FifoBuffer {
  u4 pos_head;
  u4 pos_tail;
  std::queue<char*> buffers;
  std::vector<char*> freeBufs;
} FifoBuffer;

FifoBuffer* fifoCreate();

void fifoDestroy(FifoBuffer* fb);

bool fifoEmpty(FifoBuffer* fb);

u4 fifoSize(FifoBuffer* fb);

u4 fifoGetBufferSize(FifoBuffer* fb);

char* fifoGetBuffer(FifoBuffer* fb);

void fifoReadBuffer(FifoBuffer* fb, char* buf, u4 bytes);

void fifoPopBytes(FifoBuffer* fb, u4 bytes);

void fifoPushData(FifoBuffer* fb, char* buf, u4 bytes);

// ---------------------------------------------------------------------------
}; // namespace android

#endif
