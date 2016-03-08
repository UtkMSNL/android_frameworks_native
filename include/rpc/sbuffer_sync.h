#ifndef RPC_SBUFFER_SYNC_H
#define RPC_SBUFFER_SYNC_H

#include "share_rpc.h"
#include <pthread.h>
#include <utils/Log.h>

#define BUFF_SERVICE_ID 999999
#define BUFF_DATA_SYNC_METHOD_ID 1
//#define BUFF_CTL_METHOD_ID 2
#define BUFF_STRUCT_PULL_METHOD_ID 3
#define BUFF_STRUCT_PUSH_METHOD_ID 4
#define BUFF_STRUCT_OWNER_INVALIDATE_METHOD_ID 5
#define BUFF_STRUCT_OWNER_PUSH_METHOD_ID 6
#define BUFF_STRUCT_INVALIDATE_METHOD_ID 7
#define BUFF_STRUCT_WAKE_REMOTE 8

namespace android {
// ---------------------------------------------------------------------------

/*struct SyncBase {
    int ctlId;
};

struct GenericSync : public SyncBase {
    int counts;
    char* baseAddr;
    int* offset;
    u4* value;
};

typedef void (*CtlSyncReq)(RpcRequest*, SyncBase*);
typedef void (*CtlSyncRes)(RpcRequest*);
typedef void (*DataSyncReq)(RpcEndpoint*, int, char*, int, int, u8, CtlSyncReq);
typedef void (*DataSyncRes)(RpcRequest*);

class BufferSyncUtil {
public:    
    static void dataSyncReq(RpcEndpoint* endpoint, int socketFd, char* target, int offset, char* buffer, int size, SyncBase* syncBase);
    
    static RpcResponse* dataSyncRes(RpcRequest* request);
    
    static void ctlSyncReq(RpcEndpoint* endpoint, int socketFd, SyncBase* syncBase);
    
    static RpcResponse* ctlSyncRes(RpcRequest* request);
    
    static u4 getMappingAddr(u4 addr);
    
private:
    static std::map<u4, u4> sharedAddrMap;
    
    static std::map<int, CtlSyncReq> ctlSyncReqMap;

    static std::map<int, CtlSyncRes> ctlSyncResMap;
};

void genericCtlSyncReq(RpcRequest* request, SyncBase* syncBase);
void genericCtlSyncRes(RpcRequest* request);*/

struct RpcUtilBase;
class RpcEndpoint; 

/** a struct descriptor for the synchronization of a structured area */
struct SyncDescriptor {
    /* the communication socket fd for this structure */
    int socketFd;
    
    /* indicates the ownership of the fields */
    u4* owner;
    
    /* indicates the dirtiness of the fields */
    u4* dirty;
    
    /* indicates if the field uses a write-through manner */
    u4* wthrough;
    
    /* indicates if the field is currently writing in this endpoint */
    u4* isWriting;
    
    /* lock control to avoid the race condition when two endpoints are requesting the updates to the same field */
    std::map<int, pthread_mutex_t*> lockMap;
    
    /* lock to control the change of the writing flag */
    std::map<int, pthread_mutex_t*> writingLockMap;
    
    /* map between condition for wait and its offset in the data block */
    std::map<int, pthread_cond_t*> addrCondMap;
    
    /** map betwen write race condition and its offset in the data block */
    std::map<int, pthread_cond_t*> writeCondMap;
    
    /* global lock which control the initiation of the individual lock of the fields */
    pthread_mutex_t globalLock;
    
    SyncDescriptor() {
        pthread_mutex_init(&globalLock, NULL);
    }
    
    SyncDescriptor(int ctlSize) {
        pthread_mutex_init(&globalLock, NULL);
        int sz = (ctlSize - 1) / (sizeof(u4) * 8) + 1;
        // every bit represents one byte in the struct
        owner = (u4*) calloc(sz, sizeof(u4));
        dirty = (u4*) calloc(sz, sizeof(u4));
        wthrough = (u4*) calloc(sz, sizeof(u4));
        // every byte represent one byte in the struct so that multiple write can wait
        isWriting = (u4*) calloc(sz, ctlSize);
    }
};

struct GenericCtl {
    char* baseAddr;
    int offset;
    
    GenericCtl(char* vBaseAddr, int vOffset)
        : baseAddr(vBaseAddr), offset(vOffset) {};
};

struct GenericPull {
    char* baseAddr;
    int offset;
    int size;
    
    GenericPull(char* vBaseAddr, int vOffset, int vSize)
        : baseAddr(vBaseAddr), offset(vOffset), size(vSize) {};
};

struct GenericPush {
    char* baseAddr;
    int offset;
    int size;
    char* value;
    
    GenericPush(char* vBaseAddr, int vOffset, int vSize, char* vValue)
        : baseAddr(vBaseAddr), offset(vOffset), size(vSize), value(vValue) {};
};

struct GenericBuf {
    void* baseAddr;
    int offset;
    int size;
    
    GenericBuf(void* vBaseAddr, int vOffset, int vSize) 
        : baseAddr(vBaseAddr), offset(vOffset), size(vSize) {};
};

/*struct GenericCtl {
    int counts;
    char* baseAddr;
    int* offset;
    
    GenericCtl(int vCounts, char* vBaseAddr, int* vOffset)
        : counts(vCounts), baseAddr(vBaseAddr), offset(vOffset) {};
};

struct GenericPull {
    int counts;
    char* baseAddr;
    int* offset;
    int* size;
    
    GenericPull(int vCounts, char* vBaseAddr, int* vOffset, int* vSize)
        : counts(vCounts), baseAddr(vBaseAddr), offset(vOffset), size(vSize) {};
};

struct GenericPush {
    int counts;
    char* baseAddr;
    int* offset;
    int* size;
    char** value;
    
    GenericPush(int vCounts, char* vBaseAddr, int* vOffset, int* vSize, char** vValue)
        : counts(vCounts), baseAddr(vBaseAddr), offset(vOffset), size(vSize), value(vValue) {};
};*/

static inline int getBit(int offset, u4* bits) {
    int u4bits = 8 * sizeof(u4);
    return 0x1 & (bits[offset / u4bits] >> (offset % u4bits));
}

static inline void setBit(int offset, u4* bits, int value) {
    int u4bits = 8 * sizeof(u4);
    if (value) {
        bits[offset / u4bits] = bits[offset / u4bits] | (value << (offset % u4bits));
    } else {
        u4 operand = (0xfffffffe << (offset % u4bits)) | (0xfffffffe >> (u4bits - offset % u4bits));
        bits[offset / u4bits] = bits[offset / u4bits] & operand;
    }
}


class RpcBufferUtil {
public:
    static void read(void* datablk, int offset, int size, void* value);
    static void write(void* datablk, int offset, int size, void* value, void* bufblk = NULL, int bufOffset = 0, int bufSize = 0);
    
    //static void read(void* datablk, int counts, int* offset, int* size, void** value);
    //static void write(void* datablk, int counts, int* offset, int* size, void** value);
    
    /** synchronize the specified data block to the other endpoint */
    static void dataSync(GenericBuf* genbuf);
    
    /** implementation of the atomic or operation to the field */
    static int32_t rpcAtomicOr(void* datablk, volatile int32_t* data, int32_t value);
    
    /** implementation of the atomic and operation to the field */
    static int32_t rpcAtomicAnd(void* datablk, volatile int32_t* data, int32_t value);
    
    /** put the thread to wait if the memory value equals the given value */
    static void waitOnVal(void* datablk, int* futex, int value, const struct timespec *ts);
    
    /** wake up local threads through futex */
    static void wakeLocal(void* datablk, int* futex);
    
    /** wake up remote endpoints through futex */
    static void wakeRemote(void* datablk, int* futex);
    
    /** push a mapping entry of synchronization descriptor of a block */
    static void pushSyncDesc(void* blkAddr, SyncDescriptor* syncdes);
    
    /** retrieve the block description information with the block address */
    static SyncDescriptor* getSyncDesc(void* blkAddr);
    
    /** push a mapping entry between the source and destination address */
    static void pushAddrMap(u4 srcAddr, u4 destAddr);
    
    static bool isRemoteShared(void* datablk);
    
    static void bufferUtilInit(RpcEndpoint* vEndpoint);
        
private:
    static void genericPullReq(GenericPull* gensync);
    static RpcResponse* genericPullRes(RpcRequest* request);
    static int genericPushReq(GenericPush* gensync, GenericBuf* genbuf);
    static RpcResponse* genericPushRes(RpcRequest* request);
    static int genericOwnerInvalidateReq(GenericCtl* gensync, GenericBuf* genbuf);
    static RpcResponse* genericOwnerInvalidateRes(RpcRequest* request);
    static int genericOwnerPushReq(GenericPush* gensync, GenericBuf* genbuf);
    static RpcResponse* genericOwnerPushRes(RpcRequest* request);
    static int genericInvalidateReq(GenericCtl* gensync, GenericBuf* genbuf);
    static RpcResponse* genericInvalidateRes(RpcRequest* request);
    static RpcResponse* dataSyncRes(RpcRequest* request);
    static RpcResponse* wakeRemoteRes(RpcRequest* request);
    
    static void wrapBufData(GenericBuf* genbuf, RpcRequest* request);
    static void unwrapBufData(RpcRequest* request);
    static int writePreprocess(void* datablk, int offset, int size, void* value, void* bufblk = NULL, int bufOffset = 0, int bufSize = 0);
    
    static std::map<void*, SyncDescriptor*> syncDescriptorMap;
    static std::map<u4, u4>* sharedAddrMap;
    static RpcEndpoint* endpoint;
};

void initAudioAppBufferEndpoint();

extern RpcUtilBase AudioAppUtilInst;

template<typename T>
inline T ReadRpcBuffer(void* blkAddr, T* data) {
    if (!RpcBufferUtil::isRemoteShared(blkAddr)) {
        return *data;
    }
    int offset = (char*) data - (char*) blkAddr;
    int size = sizeof(T);
    RpcBufferUtil::read(blkAddr, offset, size, (void*) data);
    
    return *data;
}

template<typename T>
inline void WriteRpcBuffer(void* blkAddr, T* data, T* value, void* bufblk = NULL, int bufOffset = 0, int bufSize = 0) {
    if (!RpcBufferUtil::isRemoteShared(blkAddr)) {
        *data = *value;
        return;
    }
    int offset = (char*) data - (char*) blkAddr;
    int size = sizeof(T);
    RpcBufferUtil::write(blkAddr, offset, size, (void*) value, bufblk, bufOffset, bufSize);
}

inline int32_t RpcBufferAtomicOr(void* blkAddr, volatile int32_t* data, int32_t value) {
    if (!RpcBufferUtil::isRemoteShared(blkAddr)) {
        return android_atomic_or(value, data);
    }
    return RpcBufferUtil::rpcAtomicOr(blkAddr, data, value);
}

inline int32_t RpcBufferAtomicAnd(void* blkAddr, volatile int32_t* data, int32_t value) {
    if (!RpcBufferUtil::isRemoteShared(blkAddr)) {
        return android_atomic_and(value, data);
    }
    return RpcBufferUtil::rpcAtomicAnd(blkAddr, data, value);
}

// ---------------------------------------------------------------------------
}; // namespace android

#endif
