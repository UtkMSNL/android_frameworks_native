#ifndef RPC_SHARERPC_H
#define RPC_SHARERPC_H

#include <map>
#include <vector>

#include <utils/StrongPointer.h>

#include "common.h"
#include "thread_pool.h"
#include "FifoBuffer.h"

namespace android {
// ---------------------------------------------------------------------------

void addOutRpcMsg(RpcMessage* rpcMsg);

typedef RpcResponse* (*RpcCallFunc)(RpcRequest*);

class RpcEndpoint
{
public:
    /** initialize the rpc contexts */
    void init();

    /** client to call a remote method with rpc */
    RpcResponse* doRpc(RpcRequest* rpcReq);

    /** server received rpc request and performs the method invocation */
    void serverHandleRpc(RpcRequest* rpcReq);
    
    /** register a rpc method so that the client can do rpc on that */
    void registerFunc(u4 serviceId, u4 methodId, RpcCallFunc callFunc);

private:    
    std::map<u8, RpcCallFunc> registeredFuncs;
    /** Next response id associated with a rpc request */
    u4 nextSequenceNo;
    /** Lock for retrieving the next response id */
    pthread_mutex_t nextSeqNoLock;
};

class RpcClient : public RpcEndpoint
{
public:
    int socketFd;       /* the socket fd to the server */
    void startClient(struct sockaddr* addr);
};

class RpcServer : public RpcEndpoint
{
public:
    std::vector<int> cliSocketFds; /* the socket fds to the list of client */
    void startServer(int port);
};

class RpcWorkerThread : public WorkerThread
{
public:
    RpcEndpoint* endpoint;
    RpcRequest* rpcRequest;
    
    RpcWorkerThread(RpcEndpoint* m_endpoint, RpcRequest* m_rpcRequest) : endpoint(m_endpoint), rpcRequest(m_rpcRequest) {}
    
    unsigned virtual executeThis() {
        endpoint->serverHandleRpc(rpcRequest);
        fifoDestroy(rpcRequest->args);
        delete rpcRequest;
        return 0;
    }
};

struct RpcUtil {
    int isShareEnabled;
    
    int isServer;
    
    int isConnected;
        
    char* serverAddr;
    
    int serverPort;
    
    RpcServer* rpcserver;
    
    RpcClient* rpcclient;
    
    void* sensorService;
    
    std::map<int, void*> idToObjMap;
    
    int nextServiceObjId;
    
    int sensorChannelPort;
};

extern RpcUtil RpcUtilInst;

void readRpcConf(int* isServer, char* serverAddr, int* serverPort, int* sensorChannelPort);

void initRpcEndpoint();

struct RpcPairFds {
    int sendFd;
    
    int receiveFd;
    
    RpcPairFds() : sendFd(-1), receiveFd(-1) {}
};

extern RpcPairFds sensorChannelFds;

// ---------------------------------------------------------------------------
}; // namespace android

#endif
