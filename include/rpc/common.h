#ifndef RPC_COMMON_H
#define RPC_COMMON_H

#include <stdint.h>
#include "FifoBuffer.h"

//#define LOG_RPC_TIME

namespace android {

// ---------------------------------------------------------------------------

typedef uint8_t             u1;
typedef uint16_t            u2;
typedef uint32_t            u4;
typedef uint64_t            u8;
typedef int8_t              s1;
typedef int16_t             s2;
typedef int32_t             s4;
typedef int64_t             s8;

class RpcMessage
{
public:
    static const u1 MSG_TYPE_REQUEST = 1;
    static const u1 MSG_TYPE_RESPONSE = 2;
    u1 type;        /* the type of this message */
    u4 seqNo;       /* the sequence no of the rpc request */
    int socketFd;   /* the connection socket fd associated with this rpc message */
};

/* A struct which represents the data structure of a rpc request */
class RpcRequest : public RpcMessage
{
public:
    u4 serviceId;       /* the id of the service object to call */
    u4 methodId;        /* the id of the method to call */
    u4 argsSize;        /* the total size of the argument byte stream */
    FifoBuffer* args;   /* the actual content of the argument byte stream */
    
    RpcRequest() {
        RpcMessage::type = RpcMessage::MSG_TYPE_REQUEST;
        args = NULL;
    }
    
    RpcRequest(u4 vServiceId, u4 vMethodId, int vSocketFd, bool hasArg)
        : serviceId(vServiceId),
          methodId(vMethodId),
          argsSize(0) {
        RpcMessage::type = RpcMessage::MSG_TYPE_REQUEST;
        RpcMessage::socketFd = vSocketFd;
        if (hasArg) {
            args = fifoCreate();
        } else {
            args = NULL;
        }
    }
    
    void putArg(char* data, int size) {
        fifoPushData(args, data, size);
        argsSize += size;
    }
    
    void getArg(char* data, int size) {
        fifoReadBuffer(args, data, size);
    }
};

/* A struct representing the data structure of a rpc response */
class RpcResponse : public RpcMessage
{
public:
    int errorNo;       /* the error no of the rpc, "0" indicates a successful rpc invocation */
    u4 retSize;        /* the total size of the return object byte stream */
    FifoBuffer* ret;   /* the actual content of the return objects byte stream */
    
    RpcResponse() {
        RpcMessage::type = RpcMessage::MSG_TYPE_RESPONSE;
        ret = NULL;
    }
    
    RpcResponse(bool hasRet)
        : retSize(0) {
        RpcMessage::type = RpcMessage::MSG_TYPE_RESPONSE;
        if (hasRet) {
            ret = fifoCreate();
        } else {
            ret = NULL;
        }
    }
    
    ~RpcResponse() {
        fifoDestroy(ret);
        ret = NULL;
    }
    
    void putRet(char* data, int size) {
        fifoPushData(ret, data, size);
        retSize += size;
    }
    
    void getRet(char* data, int size) {
        fifoReadBuffer(ret, data, size);
    }
};

// ---------------------------------------------------------------------------
}; // namespace android

#endif
