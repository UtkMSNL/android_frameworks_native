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
    int socketFdInServer; /* the socket fd of this connection in the server */
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

struct RpcUtilBase {
    int isShareEnabled;
    
    int isInited;
    
    int isServer;
    
    int isConnected;
        
    char* serverAddr;
    
    int serverPort;
    
    RpcServer* rpcserver;
    
    RpcClient* rpcclient;
    
    void* sensorService;
    
    std::map<int, void*> idToObjMap;
    
    int nextServiceObjId;
    
    RpcUtilBase() : isShareEnabled(0), isInited(0), isConnected(0) {}
};

struct RpcUtil : public RpcUtilBase {
    
    int sensorChannelPort;
};

struct AudioRpcUtil : public RpcUtilBase {
    u4 AUDIO_SERVICE_ID;
    
    u4 AUDIO_POLICY_SERVICE_ID;
    
    void* audioFlinger;
    
    void* audioPolicyService;
};

struct CameraRpcUtil : public RpcUtilBase {
    u4 CAMERA_SERVICE_ID;
    
    u4 CAMERA_PREVIEW_REFRESH_ID;
    
    void* cameraService;
};

struct SurfaceRpcUtil : public RpcUtilBase
{
    u4 SURFACE_SERVICE_ID;
    
    void* surfaceFlinger;
    
    std::map<int, void*> idToLayers;
    
    std::map<void*, int> layerToIds;
    
    std::map<int, void*> idToClients;
    
    std::map<void*, int> clientToIds;

};

extern RpcUtil RpcUtilInst;

extern AudioRpcUtil AudioRpcUtilInst;

extern CameraRpcUtil CameraRpcUtilInst;

extern RpcUtilBase AppRpcUtilInst;

extern SurfaceRpcUtil SurfaceRpcUtilInst;

void readRpcConf(int* isServer, char* serverAddr, int* serverPort, int* sensorChannelPort);

void initRpcEndpoint();

bool isNetworkReady(char* fileName);

void initAudioRpcEndpoint();

void initCameraRpcEndpoint();

void initSurfaceRpcEndpoint();

void initAppConf();

// ---------------------------------------------------------------------------
}; // namespace android

#endif
