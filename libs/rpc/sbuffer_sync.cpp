#include <rpc/sbuffer_sync.h>
#include <fstream>
#include <utils/Log.h>
#include "time.h" 

namespace android {
// ---------------------------------------------------------------------------
    
/*void BufferSyncUtil::dataSyncReq(RpcEndpoint* endpoint, int socketFd, char* target, int offset, char* buffer, int size, SyncBase* syncBase) {
    RpcRequest* request = new RpcRequest(BUFF_SERVICE_ID, BUFF_DATA_METHOD_ID, socketFd, true);
    uid_t uidval = IPCThreadState::self()->getCallingUid();
    pid_t pidval = IPCThreadState::self()->getCallingPid();
    request->putArg((char*) &uidval, sizeof(uidval));
    request->putArg((char*) &pidval, sizeof(pidval));
    request->putArg((char*) &size, sizeof(size));
    request->putArg(buffer, size);
    u4 targetAddr = sharedAddrMap[(u4) target] + offset;
    request->putArg((char*) &targetAddr, sizeof(targetAddr));
    int ctlId = (syncBase != NULL) ? syncBase->ctlId : 0;
    request->putArg((char*) &ctlId, sizeof(ctlId));
    
    // handle the control synchronization
    if (syncBase != NULL) {
        ctlSyncReqMap[ctlId](request, syncBase);
    }
    
    RpcResponse* response = endpoint->doRpc(request);
    delete response;
}

static void restoreToken(uid_t uid, pid_t pid) {
    // TODO: make sure that the pid will not collide to the local process
    int64_t token = uid;
    token <<= 32;
    token += (0x80000000 | pid);
    IPCThreadState::self()->restoreCallingIdentity(token);
}

RpcResponse* BufferSyncUtil::dataSyncRes(RpcRequest* request) {
    uid_t uidval;
    request->getArg((char*) &uidval, sizeof(uidval));
    pid_t pidval;
    request->getArg((char*) &pidval, sizeof(pidval));
    restoreToken(uidval, pidval);
    int size;
    request->getArg((char*) &size, sizeof(size));
    char buffer[size];
    request->getArg((char*) buffer, size);
    u4 targetAddr;
    request->getArg((char*) &targetAddr, sizeof(targetAddr));
    int ctlId;
    request->getArg((char*) &ctlId, sizeof(ctlId));
    
    // synchronize the data contents
    char* targetBuf = (char*) targetAddr;
    memcpy(targetBuf, buffer, size);
    
    // handle the control synchronization
    ctlSyncResMap[ctlId](request);
    
    RpcResponse* response = new RpcResponse(false);
    return response;
}

void BufferSyncUtil::ctlSyncReq(RpcEndpoint* endpoint, int socketFd, SyncBase* syncBase) {
    RpcRequest* request = new RpcRequest(BUFF_SERVICE_ID, BUFF_CTL_METHOD_ID, socketFd, true);
    uid_t uidval = IPCThreadState::self()->getCallingUid();
    pid_t pidval = IPCThreadState::self()->getCallingPid();
    request->putArg((char*) &uidval, sizeof(uidval));
    request->putArg((char*) &pidval, sizeof(pidval));
    int ctlId = (syncBase != NULL) ? syncBase->ctlId : 0;
    request->putArg((char*) &ctlId, sizeof(ctlId));
    
    // handle the control synchronization
    if (syncBase != NULL) {
        ctlSyncReqMap[ctlId](request, syncBase);
    }
    
    RpcResponse* response = endpoint->doRpc(request);
    delete response;
}

RpcResponse* BufferSyncUtil::ctlSyncRes(RpcRequest* request) {
    uid_t uidval;
    request->getArg((char*) &uidval, sizeof(uidval));
    pid_t pidval;
    request->getArg((char*) &pidval, sizeof(pidval));
    restoreToken(uidval, pidval);
    int ctlId;
    request->getArg((char*) &ctlId, sizeof(ctlId));
    
    // handle the control synchronization
    ctlSyncResMap[ctlId](request);
    
    RpcResponse* response = new RpcResponse(false);
    return response;
}

u4 BufferSyncUtil::getMappingAddr(u4 addr) {
    return sharedAddrMap[addr];
}

** synchronize a control struct by feeding the fields to be synchronized in flags
  * this works only for type struct, and which has only less than 64 fields
  * This implementation makes the assumption with each synchronization as 4 bytes,
  * another implementation shall be used if varible sync length shall be used
  *
void genericCtlSyncReq(RpcRequest* request, SyncBase* syncBase) {
    if (syncBase == NULL) {
        return;
    }
    GenericSync* genericSync = (GenericSync*) syncBase;
    int counts = genericSync->counts;
    char* baseAddr = genericSync->baseAddr;
    int* offset = genericSync->offset;
    u4* value= genericSync->value;
    request->putArg((char*) &counts, sizeof(counts));
    u4 targetStartAddr = BufferSyncUtil::getMappingAddr((u4) baseAddr);
    for(int i = 0; i < counts; i++) {
        u4 targetAddr = targetStartAddr + offset[i];
        request->putArg((char*) &targetAddr, sizeof(targetAddr));
        request->putArg((char*) &value[i], sizeof(u4));
    }
}

** The pairing for generic control sync req, this method read an address and update the address *
void genericCtlSyncRes(RpcRequest* request) {
    int counts;
    request->getArg((char*) &counts, sizeof(counts));
    for (int i = 0; i < counts; i++) {
        u4 addr;
        request->getArg((char*) &addr, sizeof(addr));
        u4 value;
        request->getArg((char*) &value, sizeof(value));
        memcpy((char*) addr, (char*) &value, sizeof(value));
    }
}*/

extern std::ifstream* readRpcConfBase(const char* fileName, RpcUtilBase* rpcUtilBaseInst);
extern void initRpcEndpointBase(RpcUtilBase* rpcUtilBaseInst);

RpcUtilBase AudioAppUtilInst;

static inline void getProcessName(char* buffer, int* size) {
    std::ifstream cmdfile("/proc/self/cmdline", std::ifstream::binary);
    cmdfile.read(buffer, *size);
}

void initAudioAppBufferEndpoint() {
    char filepath[128];
    strcpy(filepath, "/data/data/");
    char processname[64];
    int size = 64;
    getProcessName(processname, &size);
    char* pname = processname;
    if (strncmp(processname, "/system/bin/", strlen("/system/bin/")) == 0) {
        pname += strlen("/system/bin/");
    }
    strcat(filepath, pname);
    strcat(filepath, "/buffer.service.config.properties");
    std::ifstream* confFile = readRpcConfBase(filepath, &AudioAppUtilInst);
    if(confFile == NULL) {
        return;
    }
    confFile->close();
    
    if (AudioAppUtilInst.isShareEnabled && !AudioAppUtilInst.isServer) {
        initRpcEndpointBase(&AudioAppUtilInst);
        RpcBufferUtil::bufferUtilInit(AudioAppUtilInst.rpcclient);
    }
}

std::map<void*, SyncDescriptor*> RpcBufferUtil::syncDescriptorMap;
std::map<u4, u4>* RpcBufferUtil::sharedAddrMap = NULL;
RpcEndpoint* RpcBufferUtil::endpoint = NULL;

static inline void checkLockExist(SyncDescriptor* syncdes, int offset) {
    if (syncdes->lockMap.find(offset) == syncdes->lockMap.end()) {
        // this is to avoid the duplicate creation of the lock for the field
        pthread_mutex_lock(&syncdes->globalLock);
        if (syncdes->lockMap.find(offset) == syncdes->lockMap.end()) {
            pthread_mutex_t* ownerLock = new pthread_mutex_t();
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
            pthread_mutex_init(ownerLock, &attr);
            syncdes->lockMap[offset] = ownerLock;
            pthread_mutex_t* writingLock = new pthread_mutex_t();
            pthread_mutex_init(writingLock, &attr);
            syncdes->writingLockMap[offset] = writingLock;
        }
        pthread_mutex_unlock(&syncdes->globalLock);
    }
}

void RpcBufferUtil::genericPullReq(GenericPull* gensync) {
    SyncDescriptor* syncdes = syncDescriptorMap[gensync->baseAddr];
    RpcRequest* request = new RpcRequest(BUFF_SERVICE_ID, BUFF_STRUCT_PULL_METHOD_ID, syncdes->socketFd, true);
    char* baseAddr = gensync->baseAddr;
    int offset = gensync->offset;
    int size = gensync->size;
    u4 targetBaseAddr = (*sharedAddrMap)[(u4) baseAddr];
    request->putArg((char*) &targetBaseAddr, sizeof(targetBaseAddr));
    request->putArg((char*) &offset, sizeof(int));
    request->putArg((char*) &size, sizeof(int));
    
    RpcResponse* response = endpoint->doRpc(request);
    response->getRet(baseAddr + offset, size);
    setBit(offset, syncdes->dirty, 0);
    delete response;
}

RpcResponse* RpcBufferUtil::genericPullRes(RpcRequest* request) {
    u4 baseAddrVal;
    request->getArg((char*) &baseAddrVal, sizeof(baseAddrVal));
    char* baseAddr = (char*) baseAddrVal;
    SyncDescriptor* syncdes = syncDescriptorMap[baseAddr];
    RpcResponse* response = new RpcResponse(true);
    u4 offset;
    request->getArg((char*) &offset, sizeof(offset));
    int size;
    request->getArg((char*) &size, sizeof(size));
    
    checkLockExist(syncdes, offset);
    // to avoid of sending the data while the field is writing so that it can prevent the writing or wait until the writing is finished
    pthread_mutex_lock(syncdes->lockMap[offset]);
    response->putRet(baseAddr + offset, size);
    setBit(offset, syncdes->dirty, 0);
    pthread_mutex_unlock(syncdes->lockMap[offset]);
    
    return response;
}

int RpcBufferUtil::genericPushReq(GenericPush* gensync, GenericBuf* genbuf) {
    SyncDescriptor* syncdes = syncDescriptorMap[gensync->baseAddr];
    RpcRequest* request = new RpcRequest(BUFF_SERVICE_ID, BUFF_STRUCT_PUSH_METHOD_ID, syncdes->socketFd, true);
    char* baseAddr = gensync->baseAddr;
    int offset = gensync->offset;
    int size= gensync->size;
    char* value = gensync->value;
    u4 targetBaseAddr = (*sharedAddrMap)[(u4) baseAddr];
    request->putArg((char*) &targetBaseAddr, sizeof(targetBaseAddr));
    request->putArg((char*) &offset, sizeof(int));
    request->putArg((char*) &size, sizeof(int));
    request->putArg(value, size);
    wrapBufData(genbuf, request);
    
    RpcResponse* response = endpoint->doRpc(request);
    setBit(offset, syncdes->dirty, 0);
    delete response;
    
    // the endpoint which writes with owner being held always return true
    return 1;
}

RpcResponse* RpcBufferUtil::genericPushRes(RpcRequest* request) {
    u4 baseAddrVal;
    request->getArg((char*) &baseAddrVal, sizeof(baseAddrVal));
    char* baseAddr = (char*) baseAddrVal;
    SyncDescriptor* syncdes = syncDescriptorMap[baseAddr];
    u4 offset;
    request->getArg((char*) &offset, sizeof(offset));
    int size;
    request->getArg((char*) &size, sizeof(size));
    char buffer[size];
    request->getArg(buffer, size);
    unwrapBufData(request);
        
    checkLockExist(syncdes, offset);
    // to avoid the case of reading incomplete data so that the read can only happen after the synchronization of data and the synchronization is atomic
    pthread_mutex_lock(syncdes->lockMap[offset]);
    memcpy(baseAddr + offset, buffer, size);
    setBit(offset, syncdes->dirty, 0);
    pthread_mutex_unlock(syncdes->lockMap[offset]);
    
    RpcResponse* response = new RpcResponse(false);
    return response;
}

int RpcBufferUtil::genericOwnerInvalidateReq(GenericCtl* gensync, GenericBuf* genbuf) {
    SyncDescriptor* syncdes = syncDescriptorMap[gensync->baseAddr];
    RpcRequest* request = new RpcRequest(BUFF_SERVICE_ID, BUFF_STRUCT_OWNER_INVALIDATE_METHOD_ID, syncdes->socketFd, true);
    char* baseAddr = gensync->baseAddr;
    int offset = gensync->offset;
    u4 targetBaseAddr = (*sharedAddrMap)[(u4) baseAddr];
    request->putArg((char*) &targetBaseAddr, sizeof(targetBaseAddr));
    request->putArg((char*) &offset, sizeof(int));
    wrapBufData(genbuf, request);
    
    RpcResponse* response = endpoint->doRpc(request);
    int result;
    response->getRet((char*) &result, sizeof(result));
    if (result) {
        setBit(offset, syncdes->owner, 1);
        setBit(offset, syncdes->dirty, 1);
    }
    delete response;
    return result;
}

RpcResponse* RpcBufferUtil::genericOwnerInvalidateRes(RpcRequest* request) {
    u4 baseAddrVal;
    request->getArg((char*) &baseAddrVal, sizeof(baseAddrVal));
    char* baseAddr = (char*) baseAddrVal;
    SyncDescriptor* syncdes = RpcBufferUtil::syncDescriptorMap[baseAddr];
    u4 offset;
    request->getArg((char*) &offset, sizeof(offset));
    unwrapBufData(request);
    checkLockExist(syncdes, offset);
    RpcResponse* response = new RpcResponse(true);
    int result = 0;
    // this is used to avoid the conflicts between writing a field and another endpoint requesting the ownership to prevent simultaneously writing
    // to avoid the deadlock where two endpoints write simultaneously, the one without owner give up its change of writing
    pthread_mutex_lock(syncdes->writingLockMap[offset]); // to avoid the change of the isWriting during checking
    int isWriting = ((char*) syncdes->isWriting)[offset];
    if ((isWriting > 0 && pthread_mutex_trylock(syncdes->lockMap[offset]) == 0)
        || (isWriting == 0 && pthread_mutex_lock(syncdes->lockMap[offset]) == 0)) {
        setBit(offset, syncdes->owner, 0);
        setBit(offset, syncdes->dirty, 1);
        pthread_mutex_unlock(syncdes->lockMap[offset]);
        result = 1;
    }
    pthread_mutex_unlock(syncdes->writingLockMap[offset]);
    response->putRet((char*) &result, sizeof(result));
    
    return response;
}

int RpcBufferUtil::genericOwnerPushReq(GenericPush* gensync, GenericBuf* genbuf) {
    SyncDescriptor* syncdes = syncDescriptorMap[gensync->baseAddr];
    RpcRequest* request = new RpcRequest(BUFF_SERVICE_ID, BUFF_STRUCT_OWNER_PUSH_METHOD_ID, syncdes->socketFd, true);
    char* baseAddr = gensync->baseAddr;
    int offset = gensync->offset;
    int size= gensync->size;
    char* value = gensync->value;
    u4 targetBaseAddr = (*sharedAddrMap)[(u4) baseAddr];
    request->putArg((char*) &targetBaseAddr, sizeof(targetBaseAddr));
    request->putArg((char*) &offset, sizeof(int));
    request->putArg((char*) &size, sizeof(int));
    request->putArg(value, size);
    wrapBufData(genbuf, request);
    
    RpcResponse* response = endpoint->doRpc(request);
    int result;
    response->getRet((char*) &result, sizeof(result));
    if (result) {
        setBit(offset, syncdes->owner, 1);
        setBit(offset, syncdes->dirty, 0);
    }
    delete response;
    return result;
}

RpcResponse* RpcBufferUtil::genericOwnerPushRes(RpcRequest* request) {
    u4 baseAddrVal;
    request->getArg((char*) &baseAddrVal, sizeof(baseAddrVal));
    char* baseAddr = (char*) baseAddrVal;
    SyncDescriptor* syncdes = syncDescriptorMap[baseAddr];
    u4 offset;
    request->getArg((char*) &offset, sizeof(offset));
    int size;
    request->getArg((char*) &size, sizeof(size));
    char buffer[size];
    request->getArg(buffer, size);
    unwrapBufData(request);
        
    checkLockExist(syncdes, offset);
    RpcResponse* response = new RpcResponse(true);
    int result = 0;
    // to avoid the case of reading incomplete data
    // this is used to avoid the conflicts between writing a field and another endpoint requesting the ownership to prevent simultaneously writing
    // to avoid the deadlock where two endpoints write simultaneously, the one without owner give up its change of writing
    pthread_mutex_lock(syncdes->writingLockMap[offset]); // to avoid the change of the isWriting during checking
    int isWriting = ((char*) syncdes->isWriting)[offset];
    if ((isWriting > 0 && pthread_mutex_trylock(syncdes->lockMap[offset]) == 0)
        || (isWriting == 0 && pthread_mutex_lock(syncdes->lockMap[offset]) == 0)) {
        setBit(offset, syncdes->owner, 0);
        memcpy(baseAddr + offset, buffer, size);
        setBit(offset, syncdes->dirty, 0);
        pthread_mutex_unlock(syncdes->lockMap[offset]);
        result = 1;
    }
    pthread_mutex_unlock(syncdes->writingLockMap[offset]);
    response->putRet((char*) &result, sizeof(result));
   
    return response;
}

int RpcBufferUtil::genericInvalidateReq(GenericCtl* gensync, GenericBuf* genbuf) {
    SyncDescriptor* syncdes = syncDescriptorMap[gensync->baseAddr];
    RpcRequest* request = new RpcRequest(BUFF_SERVICE_ID, BUFF_STRUCT_INVALIDATE_METHOD_ID, syncdes->socketFd, true);
    char* baseAddr = gensync->baseAddr;
    int offset = gensync->offset;
    u4 targetBaseAddr = (*sharedAddrMap)[(u4) baseAddr];
    request->putArg((char*) &targetBaseAddr, sizeof(targetBaseAddr));
    request->putArg((char*) &offset, sizeof(int));
    wrapBufData(genbuf, request);
    
    RpcResponse* response = endpoint->doRpc(request);
    setBit(offset, syncdes->dirty, 1);
    delete response;
    
    // the endpoint which writes with owner being held always return true
    return 1;
}

RpcResponse* RpcBufferUtil::genericInvalidateRes(RpcRequest* request) {
    u4 baseAddrVal;
    request->getArg((char*) &baseAddrVal, sizeof(baseAddrVal));
    char* baseAddr = (char*) baseAddrVal;
    SyncDescriptor* syncdes = syncDescriptorMap[baseAddr];
    u4 offset;
    request->getArg((char*) &offset, sizeof(offset));
    unwrapBufData(request);
    checkLockExist(syncdes, offset);
    // to avoid the conflicts between writing a field and reading a field. Invalidate the data so that the happens-before relationship can be reflected
    pthread_mutex_lock(syncdes->lockMap[offset]);
    setBit(offset, syncdes->dirty, 1);
    pthread_mutex_unlock(syncdes->lockMap[offset]);
    
    RpcResponse* response = new RpcResponse(false);
    return response;
}

void RpcBufferUtil::read(void* datablk, int offset, int size, void* value) {
    //ALOGE("rpc audio service buffer read 1");
    SyncDescriptor* syncdes = syncDescriptorMap[datablk];
    checkLockExist(syncdes, offset);
    // to prevent the endpoint to write the same field and prevent the incomplete update to this field
    pthread_mutex_lock(syncdes->lockMap[offset]);
    int owner = getBit(offset, syncdes->owner);
    int dirty = getBit(offset, syncdes->dirty);
    if (!owner && dirty) { // this endpoint is not the owner of the data and the data is dirty
        // do data synchronization
        GenericPull gensync((char*) datablk, offset, size);
        genericPullReq(&gensync);
    }
    char* srcAddr = ((char*) datablk) + offset;
    memcpy((char*) value, srcAddr, size);
    pthread_mutex_unlock(syncdes->lockMap[offset]);
    //ALOGE("rpc audio service buffer read 5");
}

int RpcBufferUtil::writePreprocess(void* datablk, int offset, int size, void* value, void* bufblk, int bufOffset, int bufSize)
{
    SyncDescriptor* syncdes = syncDescriptorMap[datablk];
    
    int owner = getBit(offset, syncdes->owner);
    int dirty = getBit(offset, syncdes->dirty);
    int wthrough = getBit(offset, syncdes->wthrough);
    int result;
    GenericBuf bufVal(bufblk, bufOffset, bufSize);
    GenericBuf* genbuf = NULL;
    if (bufblk != NULL) {
        genbuf = &bufVal;
    }
            
    if (!owner && !wthrough) {
        // request ownership of the fields
        GenericCtl gensync((char*) datablk, offset);
        result = genericOwnerInvalidateReq(&gensync, genbuf);
    } else if(!owner && wthrough) {
        // request ownership and write the data to the other endpoint
        GenericPush gensync((char*) datablk, offset, size, (char*) value);
        result = genericOwnerPushReq(&gensync, genbuf);
    } else if (owner && !dirty && !wthrough) {
        // invalidate the fields in the other endpoint
        GenericCtl gensync((char*) datablk, offset);
        result = genericInvalidateReq(&gensync, genbuf);
    } else if (wthrough) {
        // write the data to the other endpoint
        GenericPush gensync((char*) datablk, offset, size, (char*) value);
        result = genericPushReq(&gensync, genbuf);
    } else {
        if (genbuf != NULL) {
            dataSync(genbuf);
        }
        result = 1;
    }
    
    return result;
}

static inline void incIsWriting(SyncDescriptor* syncdes, int offset)
{
    pthread_mutex_lock(syncdes->writingLockMap[offset]);
    ((char*) syncdes->isWriting)[offset] = ((char*) syncdes->isWriting)[offset] + 1;
    pthread_mutex_unlock(syncdes->writingLockMap[offset]);
}

static inline void decIsWriting(SyncDescriptor* syncdes, int offset)
{
    pthread_mutex_lock(syncdes->writingLockMap[offset]);
    ((char*) syncdes->isWriting)[offset] = ((char*) syncdes->isWriting)[offset] - 1;
    pthread_mutex_unlock(syncdes->writingLockMap[offset]);
}

void RpcBufferUtil::write(void* datablk, int offset, int size, void* value, void* bufblk, int bufOffset, int bufSize) {
    //struct timeval start, finish; 
    //gettimeofday(&start, NULL);
    //ALOGE("rpc audio service buffer write 1");
    SyncDescriptor* syncdes = syncDescriptorMap[datablk];
    checkLockExist(syncdes, offset);
    incIsWriting(syncdes, offset);
    int result = 0;
    while (!result) {
        // does not allow the release of the ownership and prevent the reading of this field until end writing
        pthread_mutex_lock(syncdes->lockMap[offset]);
        result = writePreprocess(datablk, offset, size, value, bufblk, bufOffset, bufSize);
        
        if (!result) {
            ALOGE("rpc audio service buffer write 8 result: %d", result);
            // temporarily give up the lock so that the one with owner can invalidate the local
            pthread_mutex_unlock(syncdes->lockMap[offset]);
        }
    }
    
    // write the value of the fields
    char* destAddr = ((char*) datablk) + offset;
    memcpy(destAddr, (char*) value, size);
    //ALOGE("rpc audio service buffer write 9");
    
    decIsWriting(syncdes, offset);
    pthread_mutex_unlock(syncdes->lockMap[offset]);
    //gettimeofday(&finish, NULL);
     //ALOGE("rpc audio service the time for data sync %ld", (finish.tv_sec - start.tv_sec) * 1000000 + finish.tv_usec - start.tv_usec);
}

void RpcBufferUtil::wrapBufData(GenericBuf* genbuf, RpcRequest* request)
{
    bool hasData = true;
    if (genbuf == NULL) {
        hasData = false;
        request->putArg((char*) &hasData, sizeof(hasData));
        return;
    }
    request->putArg((char*) &hasData, sizeof(hasData));
    void* datablk = genbuf->baseAddr;
    int offset = genbuf->offset;
    int size = genbuf->size;
    //ALOGE("rpc audio service data sync start with addr: %d, offset: %d, size: %d", datablk, offset, size);
    u4 targetBaseAddr = (*sharedAddrMap)[(u4) datablk];
    request->putArg((char*) &targetBaseAddr, sizeof(targetBaseAddr));
    request->putArg((char*) &offset, sizeof(offset));
    request->putArg((char*) &size, sizeof(size));
    request->putArg(((char*) datablk) + offset, size);
}

void RpcBufferUtil::unwrapBufData(RpcRequest* request)
{
    //ALOGE("rpc audio service start to unwrap data");
    bool hasData;
    request->getArg((char*) &hasData, sizeof(hasData));
    if (!hasData) {
        return;
    }
    u4 baseAddrVal;
    request->getArg((char*) &baseAddrVal, sizeof(baseAddrVal));
    char* baseAddr = (char*) baseAddrVal;
    u4 offset;
    request->getArg((char*) &offset, sizeof(offset));
    int size;
    request->getArg((char*) &size, sizeof(size));
    char buffer[size];
    //ALOGE("rpc audio service data sync start with addr: %d, offset: %d, size: %d", baseAddr, offset, size);
    request->getArg(buffer, size);
    //ALOGE("rpc audio service get buffer data");
    memcpy(baseAddr + offset, buffer, size);
}

void RpcBufferUtil::dataSync(GenericBuf* genbuf)
{
    SyncDescriptor* syncdes = syncDescriptorMap[genbuf->baseAddr];
    RpcRequest* request = new RpcRequest(BUFF_SERVICE_ID, BUFF_DATA_SYNC_METHOD_ID, syncdes->socketFd, true);
    wrapBufData(genbuf, request);
    
    RpcResponse* response = endpoint->doRpc(request);
    delete response;
}

RpcResponse* RpcBufferUtil::dataSyncRes(RpcRequest* request) {
    unwrapBufData(request);

    RpcResponse* response = new RpcResponse(false);
    
    return response;
}

int32_t RpcBufferUtil::rpcAtomicOr(void* datablk, volatile int32_t* data, int32_t value)
{
    int offset = (char*) data - (char*) datablk;
    int size = sizeof(int32_t);
    SyncDescriptor* syncdes = syncDescriptorMap[datablk];
    checkLockExist(syncdes, offset);
    // as an optimization if the new value equals the old value, do not update explicitly
    pthread_mutex_lock(syncdes->lockMap[offset]);
    int32_t oldValue;
    read(datablk, offset, sizeof(int), &oldValue);
    int32_t andValue = oldValue | value;
    if (andValue == oldValue) {
        pthread_mutex_unlock(syncdes->lockMap[offset]);
        return oldValue;
    }
    pthread_mutex_unlock(syncdes->lockMap[offset]);
    // make it an atomic operation
    incIsWriting(syncdes, offset);
    int32_t curValue;
    int32_t newValue;
    int result = 0;
    while (!result) {
        // does not allow the release of the ownership and prevent the reading of this field until end writing
        pthread_mutex_lock(syncdes->lockMap[offset]);
        
        read(datablk, offset, sizeof(int), &curValue);
        newValue = curValue | value;
        
        result = writePreprocess(datablk, offset, size, &newValue);
        
        if (!result) {
            ALOGE("rpc audio service buffer write 8 result: %d", result);
            // temporarily give up the lock so that the one with owner can invalidate the local
            pthread_mutex_unlock(syncdes->lockMap[offset]);
        }
    }
    // write the value of the fields
    char* destAddr = ((char*) datablk) + offset;
    memcpy(destAddr, (char*) &newValue, size);
    
    decIsWriting(syncdes, offset);
    pthread_mutex_unlock(syncdes->lockMap[offset]);
    ALOGE("rpc auido service atomic and old: %d, new: %d, value: %d", curValue, newValue, value);
    
    return curValue;
}

int32_t RpcBufferUtil::rpcAtomicAnd(void* datablk, volatile int32_t* data, int32_t value)
{
    int offset = (char*) data - (char*) datablk;
    int size = sizeof(int32_t);
    SyncDescriptor* syncdes = syncDescriptorMap[datablk];
    checkLockExist(syncdes, offset);
    // as an optimization if the new value equals the old value, do not update explicitly
    pthread_mutex_lock(syncdes->lockMap[offset]);
    int32_t oldValue;
    read(datablk, offset, sizeof(int), &oldValue);
    int32_t andValue = oldValue & value;
    if (andValue == oldValue) {
        pthread_mutex_unlock(syncdes->lockMap[offset]);
        return oldValue;
    }
    pthread_mutex_unlock(syncdes->lockMap[offset]);
    // make it an atomic operation
    incIsWriting(syncdes, offset);
    int32_t curValue;
    int32_t newValue;
    int result = 0;
    while (!result) {
        // does not allow the release of the ownership and prevent the reading of this field until end writing
        pthread_mutex_lock(syncdes->lockMap[offset]);
        read(datablk, offset, sizeof(int), &curValue);
        newValue = curValue & value;
        
        result = writePreprocess(datablk, offset, size, &newValue);
        
        if (!result) {
            ALOGE("rpc audio service buffer write 8 result: %d", result);
            // temporarily give up the lock so that the one with owner can invalidate the local
            pthread_mutex_unlock(syncdes->lockMap[offset]);
        }
    }
    // write the value of the fields
    char* destAddr = ((char*) datablk) + offset;
    memcpy(destAddr, (char*) &newValue, size);
    
    decIsWriting(syncdes, offset);
    pthread_mutex_unlock(syncdes->lockMap[offset]);
    ALOGE("rpc auido service atomic and old: %d, new: %d, value: %d", curValue, newValue, value);
    
    return curValue;
}

void RpcBufferUtil::waitOnVal(void* datablk, int* futex, int value, const struct timespec *ts) {
        ALOGE("rpc audio service trying to wait");
    SyncDescriptor* syncdes = syncDescriptorMap[datablk];
    int offset = (char*) futex - (char*) datablk;
    checkLockExist(syncdes, offset);
    // does not allow the release of the ownership and prevent the reading of this field until end writing
    pthread_mutex_lock(syncdes->lockMap[offset]);
    //TODO: we may need to request ownership here because the data may change after reading the value    
    if (syncdes->addrCondMap.find(offset) == syncdes->addrCondMap.end()) {
        pthread_cond_t cond;
        pthread_cond_init(&cond, NULL);
        syncdes->addrCondMap[offset] = &cond;
    }
    int curValue;
    read(datablk, offset, sizeof(int), &curValue);
    if (curValue == value) {
        ALOGE("rpc audio service entering wait state");
        pthread_cond_timedwait(syncdes->addrCondMap[offset], syncdes->lockMap[offset], ts);
    }
    
    pthread_mutex_unlock(syncdes->lockMap[offset]);
}

void RpcBufferUtil::wakeLocal(void* datablk, int* futex) {
    SyncDescriptor* syncdes = syncDescriptorMap[datablk];
    int offset = (char*) futex - (char*) datablk;
    if (syncdes->addrCondMap.find(offset) == syncdes->addrCondMap.end()) {
        return;
    }
    checkLockExist(syncdes, offset);
    // does not allow the release of the ownership and prevent the reading of this field until end writing
    pthread_mutex_lock(syncdes->lockMap[offset]);
    pthread_cond_signal(syncdes->addrCondMap[offset]);
    pthread_mutex_unlock(syncdes->lockMap[offset]);
}

void RpcBufferUtil::wakeRemote(void* datablk, int* futex) {
    ALOGE("rpc audio service the thread starts to wake remote call, addr: %d", datablk);
    SyncDescriptor* syncdes = syncDescriptorMap[datablk];
    int offset = (char*) futex - (char*) datablk;
    RpcRequest* request = new RpcRequest(BUFF_SERVICE_ID, BUFF_STRUCT_WAKE_REMOTE, syncdes->socketFd, true);
    u4 targetBaseAddr = (*sharedAddrMap)[(u4) datablk];
    request->putArg((char*) &targetBaseAddr, sizeof(targetBaseAddr));
    request->putArg((char*) &offset, sizeof(offset));
    
    RpcResponse* response = endpoint->doRpc(request);
    delete response;
}

RpcResponse* RpcBufferUtil::wakeRemoteRes(RpcRequest* request) {
    u4 baseAddrVal;
    request->getArg((char*) &baseAddrVal, sizeof(baseAddrVal));
    char* baseAddr = (char*) baseAddrVal;
    ALOGE("rpc audio service the thread starts to be waked by remote, addr: %d", baseAddr);
    u4 offset;
    request->getArg((char*) &offset, sizeof(offset));
    
    RpcResponse* response = new RpcResponse(false);
    SyncDescriptor* syncdes = syncDescriptorMap[baseAddr];
    if (syncdes->addrCondMap.find(offset) == syncdes->addrCondMap.end()) {
        return response;
    }
    checkLockExist(syncdes, offset);
    // does not allow the release of the ownership and prevent the reading of this field until end writing
    pthread_mutex_lock(syncdes->lockMap[offset]);
    pthread_cond_signal(syncdes->addrCondMap[offset]);
    pthread_mutex_unlock(syncdes->lockMap[offset]);
    
    return response;
}

void RpcBufferUtil::pushSyncDesc(void* blkAddr, SyncDescriptor* syncdes) {
    syncDescriptorMap[blkAddr] = syncdes;
}

SyncDescriptor* RpcBufferUtil::getSyncDesc(void* blkAddr) {
    return syncDescriptorMap[blkAddr];
}

void RpcBufferUtil::pushAddrMap(u4 srcAddr, u4 destAddr) {
    (*sharedAddrMap)[srcAddr] = destAddr;
}

bool RpcBufferUtil::isRemoteShared(void* datablk) {
    if (sharedAddrMap == NULL) {
        return false;
    }
    return sharedAddrMap->find((u4) datablk) != sharedAddrMap->end();
}

void RpcBufferUtil::bufferUtilInit(RpcEndpoint* vEndpoint) {
    endpoint = vEndpoint;
    sharedAddrMap = new std::map<u4, u4>();
    
    // register the rpc functions
    endpoint->registerFunc(BUFF_SERVICE_ID, BUFF_DATA_SYNC_METHOD_ID, &dataSyncRes);
    endpoint->registerFunc(BUFF_SERVICE_ID, BUFF_STRUCT_PULL_METHOD_ID, &genericPullRes);
    endpoint->registerFunc(BUFF_SERVICE_ID, BUFF_STRUCT_PUSH_METHOD_ID, &genericPushRes);
    endpoint->registerFunc(BUFF_SERVICE_ID, BUFF_STRUCT_OWNER_INVALIDATE_METHOD_ID, &genericOwnerInvalidateRes);
    endpoint->registerFunc(BUFF_SERVICE_ID, BUFF_STRUCT_OWNER_PUSH_METHOD_ID, &genericOwnerPushRes);
    endpoint->registerFunc(BUFF_SERVICE_ID, BUFF_STRUCT_INVALIDATE_METHOD_ID, &genericInvalidateRes);
    endpoint->registerFunc(BUFF_SERVICE_ID, BUFF_STRUCT_WAKE_REMOTE, &wakeRemoteRes);
}

// ---------------------------------------------------------------------------
}; // namespace android
