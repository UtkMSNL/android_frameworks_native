#include <pthread.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <fstream>
#include <cstdlib>
#include <rpc/Control.h>
#include <rpc/share_rpc.h>
#include <binder/IPCThreadState.h>
#include <utils/Log.h>
#include "time.h"
#include <sys/time.h>

#include <rpc/sbuffer_sync.h>

//#define CPU_TIME 0

namespace android {

// ---------------------------------------------------------------------------

#ifdef LOG_RPC_TIME
extern struct timeval start, finish;
#endif

#ifdef CPU_TIME
clock_t requestStartClock, requestSendClock, responseStartClock, responseSendClock, reqResGetStart, reqResFinishClock;
#else
struct timeval requestStartClock, requestSendClock, responseStartClock, responseSendClock, reqResGetStart, reqResFinishClock;
#endif

extern std::map<u8, pthread_mutex_t*> idxMtxMaps;
extern std::map<u8, pthread_cond_t*> idxCondMaps;

extern std::map<u8, RpcResponse*> arrivedResps;

void RpcEndpoint::init() {
    nextSequenceNo = 0;
    pthread_mutex_init(&nextSeqNoLock, NULL);
    controlInit();
}

RpcResponse* RpcEndpoint::doRpc(RpcRequest* rpcReq) {
    struct timeval start, finish;
    gettimeofday(&start, NULL);
    
    pthread_mutex_lock(&nextSeqNoLock); {
        rpcReq->seqNo = nextSequenceNo++;
    } pthread_mutex_unlock(&nextSeqNoLock);
    u8 idxId = rpcReq->socketFd;
    idxId <<= 4;
    idxId += rpcReq->seqNo;
    acquireLock(idxId);
    pthread_mutex_lock(idxMtxMaps[idxId]); 
    addOutRpcMsg(rpcReq);
    
    while(true) {
        pthread_cond_wait(idxCondMaps[idxId], idxMtxMaps[idxId]);
    //ALOGE("rpc audio service the do rpc signaling the thread idx: %lld, sockFd: %d, seq: %d", idxId, rpcReq->socketFd, rpcReq->seqNo);
        if(arrivedResps.find(idxId) != arrivedResps.end()) {
#ifdef LOG_RPC_TIME
            gettimeofday(&finish, NULL);
            ALOGE("rpc received to singal ending duration: %ld", (finish.tv_sec - start.tv_sec) * 1000000 + finish.tv_usec - start.tv_usec);
            start = finish;
#endif
            break;
        }
    }
    RpcResponse* rpcRes = arrivedResps[idxId];
    arrivedResps.erase(idxId);
    pthread_mutex_unlock(idxMtxMaps[idxId]);
    releaseLock(idxId);
    
    gettimeofday(&finish, NULL);
    ALOGI("rpc audio service the do rpc time: %ld", (finish.tv_sec - start.tv_sec) * 1000000 + finish.tv_usec - start.tv_usec);// idx: %lld, sockFd: %d, seq: %d, mtx: %d, cond: %d", idxId, rpcReq->socketFd, rpcReq->seqNo, idxMtxMaps[idxId], idxCondMaps[idxId]);
    return rpcRes;
}

void RpcEndpoint::serverHandleRpc(RpcRequest* rpcReq) {
    u8 funcId = rpcReq->serviceId;
    funcId <<= 4;
    funcId += rpcReq->methodId;
    RpcCallFunc callFunc = registeredFuncs[funcId];
    RpcResponse* rpcRes = (*callFunc)(rpcReq);
    rpcRes->seqNo = rpcReq->seqNo;
    rpcRes->socketFd = rpcReq->socketFd;
    addOutRpcMsg(rpcRes);
#ifdef LOG_RPC_TIME
//    finish = clock();
//    std::cout<<"server handle rpc duration: " << ((double)(finish - start) / CLOCKS_PER_SEC) << std::endl;
//    start = finish;
#endif
}

void RpcEndpoint::registerFunc(u4 serviceId, u4 methodId, RpcCallFunc callFunc) {
    u8 funcId = serviceId;
    funcId <<= 4;
    funcId += methodId;
    registeredFuncs[funcId] = callFunc;
}

static int connectServer(RpcClient* client, struct sockaddr* addr) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if(s == -1) {
        return -1;
    }
    if(connect(s, addr, sizeof(struct sockaddr))) {
        if(s != -1) {
            ALOGE("rpc audio service connection is failed, %s", strerror(errno));
            close(s);
            return -1;
        }
    }
    u1 magic_value = 0x55;
    if(1 != write(s, &magic_value, 1)) {
        close(s);
        return -1;
    }
    if(1 != read(s, &magic_value, 1)) {
        close(s);
        return -1;
    }
    int socketFdInServer;
    if(sizeof(socketFdInServer) != read(s, &socketFdInServer, sizeof(socketFdInServer))) {
        close(s);
        return -1;
    }
    if(magic_value != 0x55) {
        perror("Bad magic value from server");
        close(s);
        return -1;
    }
    client->socketFdInServer = socketFdInServer;
    client->socketFd = s;
    setupConnection(s);
    return 0;
}

static void* startMessageLoop(void* endpoint) {
    message_loop((RpcEndpoint*) endpoint);
    return NULL;
}

void RpcClient::startClient(struct sockaddr* addr) {
    init();
    int result = connectServer(this, addr);
    if(!result) {
        pthread_t tempThread;
        pthread_create(&tempThread, NULL, startMessageLoop, this);
    }
}

typedef struct arg_struct {
    int port;
    RpcServer* server;
    
    arg_struct(int m_port, RpcServer* m_server) : port(m_port), server(m_server) {}
} arg_struct;

static void* bindServer(void* args) {
    union {
        struct sockaddr_in addrin;
        struct sockaddr addr;
    } addr;
    
    arg_struct* argstruct = (arg_struct*) args;
    RpcServer* server = argstruct->server;
    int port = argstruct->port;
  
    int iter;
    int s = -1;
    for(iter = 0; ; iter = iter < 7 ? iter + 1 : 7) {
        s = socket(AF_INET, SOCK_STREAM, 0);
        if(s == -1) {
            perror("socket creation failed");
            return NULL;
        }
        int one = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

        addr.addrin.sin_family = AF_INET;
        addr.addrin.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.addrin.sin_port = htons(port);
        if(bind(s, &addr.addr, sizeof(addr.addrin)) < 0) {
            perror("bind server address failed");
            return NULL;
        }
        if(listen(s, 5) < 0) {
            perror("listen to port failed");
            return NULL;
        }
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
                u1 magic_value = 0x55;
                if (1 != write(s_cli, &magic_value, 1)) {
                    close(s_cli);
                    continue;
                }
                if (sizeof(s_cli) != write(s_cli, &s_cli, sizeof(s_cli))) {
                    close(s_cli);
                    continue;
                }
                if (1 != read(s_cli, &magic_value, 1)) {
                    close(s_cli);
                    continue;
                }
                if (magic_value != 0x55) {
                    perror("Bad magic value from server");
                    close(s_cli);
                    continue;
                }
                server->cliSocketFds.push_back(s_cli);
                setupConnection(s_cli);
            }
        }
    }
}

void RpcServer::startServer(int port) {
    init();
    pthread_t bindThread;
    arg_struct* argstruct = new arg_struct(port, this);
    pthread_create(&bindThread, NULL, bindServer, argstruct);
    pthread_t msgThread;
    pthread_create(&msgThread, NULL, startMessageLoop, this);
}

/*static void* initRTTServer(void* arg) {
    union {
        struct sockaddr_in addrin;
        struct sockaddr addr;
    } addr;
    
    int port = 59999;
  
    int iter;
    int s = -1;
    for(iter = 0; ; iter = iter < 7 ? iter + 1 : 7) {
        s = socket(AF_INET, SOCK_STREAM, 0);
        if(s == -1) {
            perror("socket creation failed");
            return NULL;
        }
        int one = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

        addr.addrin.sin_family = AF_INET;
        addr.addrin.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.addrin.sin_port = htons(port);
        if(bind(s, &addr.addr, sizeof(addr.addrin)) < 0) {
            perror("bind server address failed");
            return NULL;
        }
        if(listen(s, 5) < 0) {
            perror("listen to port failed");
            return NULL;
        }
        //ALOGI("Ready to accept connections on %d", addr.addrin.sin_port);
        union {
        struct sockaddr_in addrin;
        struct sockaddr addr;
        } cli_addr;
        socklen_t cli_len = sizeof(cli_addr.addrin);
        int s_cli = accept(s, &cli_addr.addr, &cli_len);
        int value = 1;
        if(setsockopt(s_cli, IPPROTO_TCP, TCP_NODELAY, (void*)&value, sizeof(int))) {
            ALOGE("rpc sensor service set tcp nodelay error");
        }
        while(1) {
            char payload;
            int len = read(s_cli, (char*) &payload, 1);
            if(len == 1) {
                len = 0;
                while(len != 1) {
                    len = write(s_cli, (char*) &payload, 1);
                }
            }
        }
    }
}

static void* initRTTClient(void* arg) {
    int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if(socketFd == -1) {
        ALOGE("rpc sensor service create server socket failed! %s", strerror(errno));
        return NULL;
    }
    union {
        struct sockaddr_in addrin;
        struct sockaddr addr;
    } addr;
    addr.addrin.sin_family = AF_INET;
    addr.addrin.sin_port = htons(59999);
    inet_aton(RpcUtilInst.serverAddr, &addr.addrin.sin_addr);
    if(connect(socketFd, &addr.addr, sizeof(struct sockaddr))) {
        if(socketFd != -1) {
            ALOGE("rpc sensor service connect to the server failed! %s", strerror(errno));
            close(socketFd);
            return NULL;
        }
    }
    // calculate the round trip in every 10 seconds
    int value = 1;
    if(setsockopt(socketFd, IPPROTO_TCP, TCP_NODELAY, (void*)&value, sizeof(int))) {
        ALOGE("rpc sensor service set tcp nodelay error");
    }
    while(1) {
        clock_t start = clock();
        char payload = 0x55;
        int len = write(socketFd, (char*) &payload, 1);
        if(len == 1) {
            len = 0;
            while(len != 1) {
                len = read(socketFd, (char*) &payload, 1);
            }
            clock_t finish = clock();
            ALOGE("rpc sensor service experiment round trip time: %ld", (finish - start) * 1000 / CLOCKS_PER_SEC);
            sleep(30);
        }
    }
}*/

std::ifstream* readRpcConfBase(const char* fileName, RpcUtilBase* rpcUtilBaseInst) {
    rpcUtilBaseInst->isInited = 1;
    std::string line;
    std::ifstream confFile(fileName);
    // check the existence of the file
    if (!confFile.good()) {
        rpcUtilBaseInst->isShareEnabled = 0;
        confFile.close();
        return NULL;
    }
    // line indicates if share is enabled
    std::getline(confFile, line);
    rpcUtilBaseInst->isShareEnabled = std::atoi(line.c_str());
    // indicates if it is a server
    std::getline(confFile, line);
    rpcUtilBaseInst->isServer = std::atoi(line.c_str());
    // indicates the server address
    std::getline(confFile, line);
    rpcUtilBaseInst->serverAddr = new char[16];
    strcpy(rpcUtilBaseInst->serverAddr, line.c_str());
    // indicates the server port
    std::getline(confFile, line);
    rpcUtilBaseInst->serverPort = std::atoi(line.c_str());
    
    return &confFile;
}

void initRpcEndpointBase(RpcUtilBase* rpcUtilBaseInst) {
    if(!rpcUtilBaseInst->isShareEnabled) {
        return;
    }
    rpcUtilBaseInst->nextServiceObjId = 10000;
    
    if(rpcUtilBaseInst->isServer) {
        RpcServer* server = new RpcServer();
        server->startServer(rpcUtilBaseInst->serverPort);
        rpcUtilBaseInst->rpcserver = server;
        rpcUtilBaseInst->isConnected = 0;
        //pthread_t rttseverThread;
        //pthread_create(&rttseverThread, NULL, initRTTServer, NULL);
    } else {
        RpcClient* client = new RpcClient();
        union {
            struct sockaddr_in addrin;
            struct sockaddr addr;
        } addr;
        addr.addrin.sin_family = AF_INET;
        addr.addrin.sin_port = htons(rpcUtilBaseInst->serverPort);
        inet_aton(rpcUtilBaseInst->serverAddr, &addr.addrin.sin_addr);
        client->startClient(&addr.addr);
        rpcUtilBaseInst->rpcclient = client;
        rpcUtilBaseInst->isConnected = 1;
        //pthread_t rttclientThread;
        //pthread_create(&rttclientThread, NULL, initRTTClient, NULL);
    }
}

RpcUtil RpcUtilInst;

void readRpcConf() {
    // initialize the service id
    std::ifstream* confFile = readRpcConfBase("/data/data/system_server/native.service.config.properties", &RpcUtilInst);
    if(confFile == NULL) {
        return;
    }
    std::string line;
    // indicates the channel port
    std::getline(*confFile, line);
    RpcUtilInst.sensorChannelPort = std::atoi(line.c_str());
    confFile->close();
    ALOGE("rpc sensor service conf isenabled: %d, isServer: %d, serverAddr: %s, port: %d, channelPort: %d", RpcUtilInst.isShareEnabled, RpcUtilInst.isServer, RpcUtilInst.serverAddr, RpcUtilInst.serverPort, RpcUtilInst.sensorChannelPort);
}

void initRpcEndpoint() {
    readRpcConf();
    initRpcEndpointBase(&RpcUtilInst);
}

AudioRpcUtil AudioRpcUtilInst;

void readAudioRpcConf() {
    AudioRpcUtilInst.AUDIO_SERVICE_ID = 2;
    AudioRpcUtilInst.AUDIO_POLICY_SERVICE_ID = 3;
    // initialize the service id
    std::ifstream* confFile = readRpcConfBase("/data/data/media_server/audio.service.config.properties", &AudioRpcUtilInst);
    if(confFile == NULL) {
        return;
    }
    confFile->close();
}

bool isNetworkReady() {
    // use the existence of a file to check if the network is ready
    std::ifstream netfile("/data/data/media_server/net_ready");
    bool result = netfile.good();
    netfile.close();
    std::remove("/data/data/media_server/net_ready");
    return result;
}

void initAudioRpcEndpoint() {
    readAudioRpcConf();
    initRpcEndpointBase(&AudioRpcUtilInst);
    
    ALOGE("rpc audio service conf isenabled: %d, isServer: %d, serverAddr: %s, port: %d", AudioRpcUtilInst.isShareEnabled, AudioRpcUtilInst.isServer, AudioRpcUtilInst.serverAddr, AudioRpcUtilInst.serverPort);
    
    if (AudioRpcUtilInst.isShareEnabled && AudioRpcUtilInst.isServer) {
        RpcBufferUtil::bufferUtilInit(AudioRpcUtilInst.rpcserver);
    }
}

CameraRpcUtil CameraRpcUtilInst;

void readCameraRpcConf() {
    CameraRpcUtilInst.CAMERA_SERVICE_ID = 4;
    CameraRpcUtilInst.CAMERA_PREVIEW_REFRESH_ID = 5;
    // initialize the service id
    std::ifstream* confFile = readRpcConfBase("/data/data/media_server/camera.service.config.properties", &CameraRpcUtilInst);
    if(confFile == NULL) {
        return;
    }
    confFile->close();
}

void initCameraRpcEndpoint() {
    readCameraRpcConf();
    initRpcEndpointBase(&CameraRpcUtilInst);
    
    ALOGE("rpc camera service conf isenabled: %d, isServer: %d, serverAddr: %s, port: %d", CameraRpcUtilInst.isShareEnabled, CameraRpcUtilInst.isServer, CameraRpcUtilInst.serverAddr, CameraRpcUtilInst.serverPort);
}

// ---------------------------------------------------------------------------

}; // namespace android
