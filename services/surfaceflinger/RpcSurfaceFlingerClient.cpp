#include "RpcSurfaceFlingerClient.h"
#include "RpcSurfaceFlingerCommon.h"

#include <rpc/share_rpc.h>
#include <queue>
#include <pthread.h>

#define TYPE_ADD_CLIENT 1
#define TYPE_REMOVE_CLIENT 2
#define TYPE_ADD_LAYER 3
#define TYPE_REMOVE_LAYER 4
#define TYPE_SYNC_LAYER 5

namespace android {

void doAddClient(SurfaceRpcRequest* clientRequest)
{
    ClientDef* def = (ClientDef*) clientRequest->payload;
    void* client = def->client;
    
    RpcRequest* request = new RpcRequest(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_ADD_CLIENT, SurfaceRpcUtilInst.rpcclient->socketFd, false);
    
    RpcResponse* response = SurfaceRpcUtilInst.rpcclient->doRpc(request);
    int clientId;
    response->getRet((char*) &clientId, sizeof(clientId));
    delete response;
    SurfaceRpcUtilInst.clientToIds[client] = clientId;
}

void doRemoveClient(SurfaceRpcRequest* clientRequest)
{
    int clientId = clientRequest->id;
    
    RpcRequest* request = new RpcRequest(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_REMOVE_CLIENT, SurfaceRpcUtilInst.rpcclient->socketFd, true);
    request->putArg((char*) &clientId, sizeof(clientId));
    
    RpcResponse* response = SurfaceRpcUtilInst.rpcclient->doRpc(request);
    delete response; 
}

void doAddLayer(SurfaceRpcRequest* layerRequest)
{
    LayerDef* def = (LayerDef*) layerRequest->payload;
    String8 name = def->name;
    uint32_t width = def->width;
    uint32_t height = def->height;
    uint32_t flags = def->flags;
    PixelFormat foramt = def->format;
    void* client = def->client;
    void* layer = def->layer;
    int clientId = SurfaceRpcUtilInst.clientToIds[client];
    
    RpcRequest* request = new RpcRequest(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_ADD_LAYER, SurfaceRpcUtilInst.rpcclient->socketFd, true);
    size_t len = name.length();
    request->putArg((char*) &len, sizeof(len));
    request->putArg((char*) name.string(), len);
    request->putArg((char*) &width, sizeof(width));
    request->putArg((char*) &height, sizeof(height));
    request->putArg((char*) &flags, sizeof(flags));
    request->putArg((char*) &format, sizeof(format));
    request->putArg((char*) &clientId, sizeof(clientId));
    
    RpcResponse* response = SurfaceRpcUtilInst.rpcclient->doRpc(request);
    int layerId;
    response->getRet((char*) &layerId, sizeof(layerId));
    delete response;
    SurfaceRpcUtilInst.layerToIds[layer] = layerId;
}

void doRemoveLayer(SurfaceRpcRequest* layerRequest)
{
    int layerId = layerRequest->id;
    
    RpcRequest* request = new RpcRequest(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_REMOVE_LAYER, SurfaceRpcUtilInst.rpcclient->socketFd, true);
    request->putArg((char*) &layerId, sizeof(layerId));
    
    RpcResponse* response = SurfaceRpcUtilInst.rpcclient->doRpc(request);
    delete response; 
}

void doSyncLayer(SurfaceRpcRequest* layerRequest)
{
    BufferDef* def = (BufferDef*) layerRequest->payload;
    int clientId = def->clientId;
    int layerId = layerRequest->layerId;
    size_t size = def->size;
    uint8_t* data = def->data;
    int width = def->width;
    int height = def->height;
    int stride = def->stride;
    int format = def->format;
    int usage = def->usage;
    
    RpcRequest* request = new RpcRequest(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_SYNC_LAYER, SurfaceRpcUtilInst.rpcclient->socketFd, true);
    request->putArg((char*) &clientId, sizeof(clientId));
    request->putArg((char*) &layerId, sizeof(layerId));
    request->putArg((char*) &size, sizeof(size));
    request->putArg((char*) data, size);
    request->putArg((char*) &width, sizeof(width));
    request->putArg((char*) &height, sizeof(height));
    request->putArg((char*) &stride, sizeof(stride));
    request->putArg((char*) &format, sizeof(format));
    request->putArg((char*) &usage, sizeof(usage));
    
    RpcResponse* response = SurfaceRpcUtilInst.rpcclient->doRpc(request);
    delete response; 
}

static void* sfthLoop(void* args)
{
    while (true) {
        pthread_mutex_lock(&queueLock);
        if (reqQueue.isEmpty()) {
            pthread_cond_wait(&queueCond, &queueLock);
        }
        SurfaceRpcRequest* request = reqQueue.front();
        reqQueue.pop();
        pthread_mutex_unlock(&queueLock);
        switch (request->type) {
            case TYPE_ADD_CLIENT: doAddClient(request);
                            break;
            case TYPE_REMOVE_CLIENT: doRemoveClient(request);
                            break;
            case TYPE_ADD_LAYER: doAddLayer(request);
                            break;
            case TYPE_REMOVE_LAYER: doRemoveLayer(request);
                            break;
            case TYPE_SYNC_LAYER: doSyncLayer(request);
                            break;
        }
        delete request;
    }
}

void init()
{
    pthread_mutex_init(&queueLock, NULL);
    pthread_cond_init(queueCond, NULL);
    pthread_t rpcThread;
    pthread_create(&rpcThread, NULL, sfthLoop, NULL);
}

void addClient(void* client)
{
    if (!SurfaceRpcUtilInst.isShareEnabled || !SurfaceRpcUtilInst.isConnected || SurfaceRpcUtilInst.isServer) {
        return;
    }
    ClientDef* def = new ClientDef(client);
    SurfaceRpcRequest* request = new SurfaceRpcRequest(TYPE_ADD_CLIENT, 0, def);
    pthread_mutex_lock(&queueLock);
    reqQueue.push(request);
    pthread_cond_signal(&queueCond);
    pthread_mutex_unlock(&queueLock);
}

void removeClient(void* client)
{
    if (!SurfaceRpcUtilInst.isShareEnabled || !SurfaceRpcUtilInst.isConnected || SurfaceRpcUtilInst.isServer) {
        return;
    }
    if (SurfaceRpcUtilInst.clientToIds.find(client) == SurfaceRpcUtilInst.clientToIds.end()) {
        return;
    }
    int clientId = SurfaceRpcUtilInst.clientToIds[client];
    SurfaceRpcRequest* request = new SurfaceRpcRequest(TYPE_REMOVE_CLIENT, clientId);
    pthread_mutex_lock(&queueLock);
    reqQueue.push(request);
    pthread_cond_signal(&queueCond);
    pthread_mutex_unlock(&queueLock);
}

int addLayer(const String8& name, uint32_t w, uint32_t h, uint32_t flags, PixelFormat format, void* client, void* layer)
{
    if (!SurfaceRpcUtilInst.isShareEnabled || !SurfaceRpcUtilInst.isConnected || SurfaceRpcUtilInst.isServer) {
        return;
    }
    if (SurfaceRpcUtilInst.clientToIds.find(client) == SurfaceRpcUtilInst.clientToIds.end()) {
        return;
    }
    LayerDef* def = new LayerDef(name, w, h, flags, foramt, client, layer);
    SurfaceRpcRequest* request = new SurfaceRpcRequest(TYPE_ADD_LAYER, 0, def);
    pthread_mutex_lock(&queueLock);
    reqQueue.push(request);
    pthread_cond_signal(&queueCond);
    pthread_mutex_unlock(&queueLock);
}

void removeLayer(void* layer)
{
    if (!SurfaceRpcUtilInst.isShareEnabled || !SurfaceRpcUtilInst.isConnected || SurfaceRpcUtilInst.isServer) {
        return;
    }
    if (SurfaceRpcUtilInst.layerToIds.find(layer) == SurfaceRpcUtilInst.layerToIds.end()) {
        return;
    }
    int layerId = SurfaceRpcUtilInst.layerToIds[layer];
    SurfaceRpcUtilInst.layerToIds.erase(layer);
    SurfaceRpcRequest* request = new SurfaceRpcRequest(TYPE_REMOVE_LAYER, layerId);
    pthread_mutex_lock(&queueLock);
    reqQueue.push(request);
    pthread_cond_signal(&queueCond);
    pthread_mutex_unlock(&queueLock);
}

void syncLayer(sp<GraphicBuffer> buffer, void* client, void* layer)
{
    if (!SurfaceRpcUtilInst.isShareEnabled || !SurfaceRpcUtilInst.isConnected || SurfaceRpcUtilInst.isServer) {
        return;
    }
    if (SurfaceRpcUtilInst.clientToIds.find(client) == SurfaceRpcUtilInst.clientToIds.end() ||
        SurfaceRpcUtilInst.layerToIds.find(layer) == SurfaceRpcUtilInst.layerToIds.end()) {
        return;
    }
    uint8_t* data;
    status_t err = buffer->lock(GRALLOC_USAGE_SW_READ_OFTEN, (void**)&data);
    if (err) {
        return;
    }
    int width = buffer->width;
    int height = buffer->height;
    int stride = buffer->stride;
    int format = buffer->format;
    int usage = buffer->usage;
    int clientId = SurfaceRpcUtilInst.clientToIds[client];
    BufferDef* def = new BufferDef(clientId, width, height, stride, format, usage);
    const size_t bpp = bytesPerPixel(format);
    const size_t size = height * stride * bpp;
    def->size = size;
    def->data = (uint8_t*) malloc(size);
    memcpy(def->data, data, size);
    if (data) {
        buffer->unlock();
    }
    int layerId = SurfaceRpcUtilInst.layerToIds[layer];
    SurfaceRpcRequest* request = new SurfaceRpcRequest(TYPE_SYNC_LAYER, layerId, def);
    pthread_mutex_lock(&queueLock);
    reqQueue.push(request);
    pthread_cond_signal(&queueCond);
    pthread_mutex_unlock(&queueLock);
}

}; // namespace android