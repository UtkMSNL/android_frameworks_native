/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <fcntl.h>
#include <unistd.h>

#include <utils/Errors.h>

#include <binder/Parcel.h>

#include <gui/BitTube.h>
#include <gui/RpcBitTube.h>
#include <rpc/share_rpc.h>

#include <sys/time.h>
#include <android/sensor.h>

//#define DELAY_TEST 1

#define THROUGHPUT_STAT

namespace android {
// ----------------------------------------------------------------------------

#define TCP_NODELAY   1 /* Turn off Nagle's algorithm. */

#ifdef DELAY_TEST
static void* evalDelayLoop(void* arg) {
    int sockFd = *((int*) arg);
    while(1) {
        fd_set rdst; FD_ZERO(&rdst);
        int nfd = sockFd + 1;
        FD_SET(sockFd, &rdst);
        int res = select(nfd, &rdst, NULL, NULL, NULL);
        if (res < 0) {
            return NULL;
        }
        if(FD_ISSET(sockFd, &rdst)) {
            struct timeval sendTime;
            ssize_t len = read(sockFd, (char*) &sendTime, sizeof(sendTime));
            if(len <= 0) {
                return NULL;
            }
            struct timeval receiveTime;
            gettimeofday(&receiveTime, NULL);
            long timeUsed = (receiveTime.tv_sec - sendTime.tv_sec) * 1000000 + receiveTime.tv_usec - sendTime.tv_usec;
            
            ALOGE("rpc sensor service experiment sensor data delivery taking time: %ld", timeUsed);
        }
    }
}
#endif


#ifdef THROUGHPUT_STAT
static u4 totalBytes = 0;

static void* evalThroughputLoop(void* arg) {
    u4 lastTotalBytes = 0;
    while(1) {
        sleep(60);
        ALOGE("[rpc evaluation], data throughput in bytes for last minute, %d", totalBytes - lastTotalBytes);
        lastTotalBytes = totalBytes;
    }
    return NULL;
}
#endif

void initBitTubeServer() {
    union {
        struct sockaddr_in addrin;
        struct sockaddr addr;
    } addr;
    
    int port = RpcUtilInst.sensorChannelPort;
  
    int iter;
    int s = -1;
    for(iter = 0; ; iter = iter < 7 ? iter + 1 : 7) {
        s = socket(AF_INET, SOCK_STREAM, 0);
        if(s == -1) {
            perror("socket creation failed");
            return;
        }
        int one = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

        addr.addrin.sin_family = AF_INET;
        addr.addrin.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.addrin.sin_port = htons(port);
        if(bind(s, &addr.addr, sizeof(addr.addrin)) < 0) {
            perror("bind server address failed");
            return;
        }
        if(listen(s, 5) < 0) {
            perror("listen to port failed");
            return;
        }

#ifdef THROUGHPUT_STAT
                // for sensor data throughput test
                pthread_t tempThroughputThread;
                pthread_create(&tempThroughputThread, NULL, evalThroughputLoop, NULL);
#endif
        //ALOGI("Ready to accept connections on %d", addr.addrin.sin_port);
        while(1) {
            union {
            struct sockaddr_in addrin;
            struct sockaddr addr;
            } cli_addr;
            socklen_t cli_len = sizeof(cli_addr.addrin);
            int s_cli = accept(s, &cli_addr.addr, &cli_len);
            if(s_cli == -1) {
                perror("accept socket connection failed");
            } else {
                int nodelay = 1;
                setsockopt(s_cli, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
                if(4 != write(s_cli, &s_cli, sizeof(s_cli))) {
                    close(s_cli);
                    continue;
                }
                
#ifdef DELAY_TEST
                // for sensor data delivery delay test
                pthread_t tempThread;
                pthread_create(&tempThread, NULL, evalDelayLoop, &s_cli);
#endif
            }
        }
    }
}

// Socket buffer size.  The default is typically about 128KB, which is much larger than
// we really need.  So we make it smaller.
static const size_t DEFAULT_SOCKET_BUFFER_SIZE = 4 * 1024;


RpcBitTube::RpcBitTube()
    : BitTube(false, DEFAULT_SOCKET_BUFFER_SIZE)
{
    init(DEFAULT_SOCKET_BUFFER_SIZE, DEFAULT_SOCKET_BUFFER_SIZE);
}

RpcBitTube::RpcBitTube(int sendFd, int receiveFd, size_t bufsize)
    : BitTube(false, bufsize)
{
    mSendFd = sendFd;
    mReceiveFd = receiveFd;
    init(bufsize, bufsize);
}

RpcBitTube::RpcBitTube(const Parcel& data)
    : BitTube(false, DEFAULT_SOCKET_BUFFER_SIZE)
{
    mReceiveFd = dup(data.readFileDescriptor());
    if (mReceiveFd < 0) {
        mReceiveFd = -errno;
        ALOGE("BitTube(Parcel): can't dup filedescriptor (%s)",
                strerror(-mReceiveFd));
    }
    
    /*char magic = 0x55;
    ALOGE("rpc sensor service send magic from rpc bit tube start");
    for(int i = 0; i < 100; i++) {
        int len = ::send(mReceiveFd, (char*) &magic, sizeof(magic), MSG_WAITALL);
        ALOGE("rpc sensor service send with dup socket, %s", strerror(errno));
    }
    ALOGE("rpc sensor service send magic from rpc bit tube end, errno: %d", errno);*/
}

RpcBitTube::~RpcBitTube()
{
    if (mSendFd >= 0)
        close(mSendFd);

    if (mReceiveFd >= 0)
        close(mReceiveFd);
}

void RpcBitTube::init(size_t rcvbuf, size_t sndbuf) {
    size_t size = DEFAULT_SOCKET_BUFFER_SIZE;
    if(mSendFd != -1) {
        setsockopt(mSendFd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
        // sine we don't use the "return channel", we keep it small...
        setsockopt(mSendFd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
        fcntl(mSendFd, F_SETFL, O_NONBLOCK);
    }
    if(mReceiveFd != -1) {
        setsockopt(mReceiveFd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
        // sine we don't use the "return channel", we keep it small...
        setsockopt(mReceiveFd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
        fcntl(mReceiveFd, F_SETFL, O_NONBLOCK);
    }
    //fcntl(sockets[0], F_SETFL, O_NONBLOCK);
    //fcntl(sockets[1], F_SETFL, O_NONBLOCK);
}

status_t RpcBitTube::initCheck() const
{
    if (mReceiveFd < 0) {
        return status_t(mReceiveFd);
    }
    return NO_ERROR;
}

int RpcBitTube::getFd() const
{
    return mReceiveFd;
}

int RpcBitTube::getSendFd() const
{
    return mSendFd;
}

#ifdef DELAY_TEST
static int times = 0;
#endif

ssize_t RpcBitTube::write(void const* vaddr, size_t size)
{
    //ALOGE("rpc sensor service write data to rpc bit tube start");
    ssize_t err, len;
    // added by yli118 - to make the network bit tube work, we need to read the size of the data and wait until all the data is read
    size_t start = 0;

#ifdef DELAY_TEST
    bool isTimeSent = false;
    struct timeval sendTime;
    if (times++ % 100 == 99) {
        gettimeofday(&sendTime, NULL);
        size += sizeof(struct timeval);
        isTimeSent = true;
    }
#endif
    do {
        //len = ::send(mSendFd, (char*) &size + start, sizeof(size) - start, MSG_WAITALL);
        len = ::write(mSendFd, (char*) &size + start, sizeof(size) - start);
        //ALOGE("rpc sensor service write len to rpc bit tube, fd: %d, len: %d, errno: %d, err msg: %s", mSendFd, len, errno, strerror(errno));
        if(start == 0 && len < 0) {
            return -errno;
        }
        if(start == 0 && len == 0) {
            return 0;
        }
        if(len < 0 && (errno != EINTR && errno != EAGAIN)) {
            return -errno;
        }
        if(len > 0) {
            start += len;
        }
    } while(start < sizeof(size));
#ifdef DELAY_TEST
    if (isTimeSent) {
        size -= sizeof(struct timeval);
    }
#endif
    // added end
    // modified by yli118 - read all data
    start = 0;
    do {
        //len = ::send(mSendFd, (char*) vaddr + start, size - start, MSG_DONTWAIT);
        len = ::write(mSendFd, (char*) vaddr + start, size - start);
        ALOGE("rpc sensor service write data content to rpc bit tube, fd: %d, len: %d, size, %d, errno: %d, err msg: %s", mSendFd, len, size, errno, strerror(errno));
        //if(start == 0 && (len < 0 && errno != EINTR && errno != EAGAIN)) {
        //    return -errno;
        //}
        if(len < 0 && (errno != EINTR && errno != EAGAIN)) {
            return -errno;
        }
        if(len > 0) {
            start += len;
        }
        err = len < 0 ? errno : 0;
    } while (start < size);
    //ALOGE("rpc sensor service write data to rpc bit tube end");

#ifdef DELAY_TEST
    if (isTimeSent) {
        int clkstart = 0;
        size_t clksize = sizeof(struct timeval);
        do {
            //len = ::send(mSendFd, (char*) vaddr + start, size - start, MSG_DONTWAIT);
            len = ::write(mSendFd, (char*) &sendTime + clkstart, clksize - clkstart);
            if(len < 0 && (errno != EINTR && errno != EAGAIN)) {
                return -errno;
            }
            if(len > 0) {
                clkstart += len;
            }
            err = len < 0 ? errno : 0;
        } while (clkstart < clksize);
    }
#endif

    // modify end 
    //do {
    //    len = ::send(mSendFd, vaddr, size, MSG_DONTWAIT | MSG_NOSIGNAL);
        // cannot return less than size, since we're using SOCK_SEQPACKET
    //    err = len < 0 ? errno : 0;
    //} while (err == EINTR);

#ifdef THROUGHPUT_STAT
    totalBytes += size + sizeof(size);
#endif
    return err == 0 ? size : -err;
}

ssize_t RpcBitTube::read(void* vaddr, size_t size)
{
    //ALOGE("rpc sensor service read data from rpc bit tube start, id: %d", mReceiveFd);
    ssize_t err, len;
    // added by yli118 - to make the network bit tube work, we need to read the size of the data and wait until all the data is read
    size_t dataSize, start = 0;
    do {
        //len = ::recv(mReceiveFd, (char*) &dataSize + start, sizeof(dataSize) - start, MSG_WAITALL);
        len = ::read(mReceiveFd, (char*) &dataSize + start, sizeof(dataSize) - start);
        //ALOGE("rpc sensor service reading len from rpc bit tube, fd: %d, len: %d, size: %d, errno: %d, err msg: %s", mReceiveFd, len, dataSize, errno, strerror(errno));
        if(start == 0 && len < 0) {
            return -errno;
        }
        if(start == 0 && len == 0) {
            return 0;
        }
        if(len < 0 && (errno != EINTR && errno != EAGAIN)) {
            return -errno;
        }
        if(len > 0) {
            start += len;
        }
    } while(start < sizeof(dataSize));
    //ALOGE("rpc sensor service finish reading len from rpc bit tube end");
    // added end
    // modified by yli118 - read all data

#ifdef DELAY_TEST
    // check if the server has sent the timestamp when the sensor data is sent
    bool isTimeSent = false;
    if (dataSize % sizeof(ASensorEvent) != 0) {
        isTimeSent = true;
        dataSize -= sizeof(struct timeval);
    }
#endif
    start = 0;
    do {
        //len = ::recv(mReceiveFd, (char*) vaddr + start, dataSize - start, MSG_WAITALL);
        len = ::read(mReceiveFd, (char*) vaddr + start, dataSize - start);
        //ALOGE("rpc sensor service reading data content from rpc bit tube, fd: %d, len: %d, dataSize: %d, errno: %d, err msg: %s", mReceiveFd, len, dataSize, errno, strerror(errno));
        //if(start == 0 && (len < 0 && errno != EINTR && errno != EAGAIN)) {
        //    return -errno;
        //}
        if(len < 0 && (errno != EINTR && errno != EAGAIN)) {
            return -errno;
        }
        if(len > 0) {
            start += len;
        }
        err = len < 0 ? errno : 0;
    } while (start < dataSize);
    //ALOGE("rpc sensor service read data from rpc bit tube end, err is: %d", err);
    // modify end
    //do {
    //    len = ::recv(mReceiveFd, vaddr, size, MSG_DONTWAIT);
    //    err = len < 0 ? errno : 0;
    //} while (err == EINTR);
    
        
#ifdef DELAY_TEST
    // check if the server has sent the timestamp when the sensor data is sent
    if (isTimeSent) {
        struct timeval sendTime;
        size_t clksize = sizeof(struct timeval);
        int clkstart = 0;
        do {
            //len = ::recv(mReceiveFd, (char*) vaddr + start, dataSize - start, MSG_WAITALL);
            len = ::read(mReceiveFd, (char*) &sendTime + clkstart, clksize - clkstart);
            if(len < 0 && (errno != EINTR && errno != EAGAIN)) {
                return -errno;
            }
            if(len > 0) {
                clkstart += len;
            }
            err = len < 0 ? errno : 0;
        } while (clkstart < clksize); 
        
        //ALOGE("rpc sensor service clock_t sent is: %ld.%ld, data Size is: %d, size is: %d", sendTime.tv_sec, sendTime.tv_usec, dataSize, sizeof(ASensorEvent));
        // send back the time when the data is send
        clkstart = 0;
        do {
            //len = ::send(mSendFd, (char*) vaddr + start, size - start, MSG_DONTWAIT);
            len = ::write(mReceiveFd, (char*) &sendTime + clkstart, clksize - clkstart);
            if(len < 0 && (errno != EINTR && errno != EAGAIN)) {
                return -errno;
            }
            if(len > 0) {
                clkstart += len;
            }
            err = len < 0 ? errno : 0;
        } while (clkstart < clksize);
    }
#endif
    
    if (err == EAGAIN || err == EWOULDBLOCK) {
        // EAGAIN means that we have non-blocking I/O but there was
        // no data to be read. Nothing the client should care about.
        return 0;
    }
    return err == 0 ? start : -err;
}

status_t RpcBitTube::writeToParcel(Parcel* reply) const
{
    //if (mReceiveFd < 0)
    //    return -EINVAL;

    reply->writeInt32(2);
    status_t result = reply->writeDupFileDescriptor(mReceiveFd);
    close(mReceiveFd);
    mReceiveFd = -1;
    return result;
}

// ----------------------------------------------------------------------------
}; // namespace android
