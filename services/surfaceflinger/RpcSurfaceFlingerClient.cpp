#include "RpcSurfaceFlingerClient.h"
#include "RpcSurfaceFlingerCommon.h"

#include <rpc/share_rpc.h>
#include <utils/Log.h>

#include "Layer.h"

//#define TYPE_ADD_CLIENT 1
#define TYPE_REMOVE_CLIENT 2
//#define TYPE_ADD_LAYER 3
#define TYPE_REMOVE_LAYER 4
#define TYPE_SYNC_LAYER 5
#define TYPE_UPDATE_LAYER_STATE 6

namespace android {

static std::queue<SurfaceRpcRequest*> reqQueue;
static pthread_mutex_t queueLock;
static pthread_cond_t queueCond;

/*void doAddClient(SurfaceRpcRequest* clientRequest)
{
    ALOGE("rpc surface flinger doAddClient start");
    ClientDef* def = (ClientDef*) clientRequest->payload;
    void* client = def->client;
    
    RpcRequest* request = new RpcRequest(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_ADD_CLIENT, SurfaceRpcUtilInst.rpcclient->socketFd, false);
    
    RpcResponse* response = SurfaceRpcUtilInst.rpcclient->doRpc(request);
    int clientId;
    response->getRet((char*) &clientId, sizeof(clientId));
    delete response;
    SurfaceRpcUtilInst.clientToIds[client] = clientId;
    ALOGE("rpc surface flinger doAddClient finish");
}*/

void doRemoveClient(SurfaceRpcRequest* clientRequest)
{
    int clientId = clientRequest->id;
    
    RpcRequest* request = new RpcRequest(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_REMOVE_CLIENT, SurfaceRpcUtilInst.rpcclient->socketFd, true);
    request->putArg((char*) &clientId, sizeof(clientId));
    
    RpcResponse* response = SurfaceRpcUtilInst.rpcclient->doRpc(request);
    delete response; 
    ALOGI("rpc surface flinger remove a client");
}

/*void doAddLayer(SurfaceRpcRequest* layerRequest)
{
    ALOGE("rpc surface flinger doAddLayer start");
    LayerDef* def = (LayerDef*) layerRequest->payload;
    void* client = def->client;
    void* layer = def->layer;
    if (SurfaceRpcUtilInst.clientToIds.find(client) == SurfaceRpcUtilInst.clientToIds.end()) {
        return;
    }
    String8 name = def->name;
    uint32_t width = def->width;
    uint32_t height = def->height;
    uint32_t flags = def->flags;
    PixelFormat format = def->format;
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
    ALOGE("rpc surface flinger doAddLayer finish");
}*/

void doRemoveLayer(SurfaceRpcRequest* layerRequest)
{
    int layerId = layerRequest->id;
    
    RpcRequest* request = new RpcRequest(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_REMOVE_LAYER, SurfaceRpcUtilInst.rpcclient->socketFd, true);
    request->putArg((char*) &layerId, sizeof(layerId));
    
    RpcResponse* response = SurfaceRpcUtilInst.rpcclient->doRpc(request);
    delete response; 
    ALOGI("rpc surface flinger remove a layer");
}

void doSyncLayer(SurfaceRpcRequest* layerRequest)
{
    BufferDef* def = (BufferDef*) layerRequest->payload;
    int clientId = def->clientId;
    int layerId = layerRequest->id;
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
    ALOGI("rpc surface flinger sync a layer");
}

void doUpdateLayerState(SurfaceRpcRequest* layerRequest)
{
    LayerStateDef* def = (LayerStateDef*) layerRequest->payload;
    int clientId = def->clientId;
    int layerId = layerRequest->id;
    //sp<IBinder> surface = def->surface;
    uint32_t what = def->what;
    float x = def->x;
    float y = def->y;
    uint32_t z = def->z;
    uint32_t w = def->w;
    uint32_t h = def->h;
    uint32_t layerStack = def->layerStack;
    float blur = def->blur;
    //sp<IBinder> blurMaskSurface = def->blurMaskSurface;
    what = what & ~layer_state_t::eBlurMaskSurfaceChanged;
    int32_t blurMaskSampling = def->blurMaskSampling;
    float blurMaskAlphaThreshold = def->blurMaskAlphaThreshold;
    float alpha = def->alpha;
    uint8_t flags = def->flags;
    uint8_t mask = def->mask;
    //uint8_t reserved = def->reserved;
    layer_state_t::matrix22_t matrix = def->matrix;
    //what = what & ~layer_state_t::eMatrixChanged;
    Rect crop = def->crop;
    //what = what & ~layer_state_t::eCropChanged;
    //Region transparentRegion = def->transparentRegion;
    what = what & ~layer_state_t::eTransparentRegionChanged;
    
    RpcRequest* request = new RpcRequest(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_UPDATE_LAYER_STATE, SurfaceRpcUtilInst.rpcclient->socketFd, true);
    request->putArg((char*) &clientId, sizeof(clientId));
    request->putArg((char*) &layerId, sizeof(layerId));
    request->putArg((char*) &what, sizeof(what));
    request->putArg((char*) &x, sizeof(x));
    request->putArg((char*) &y, sizeof(y));
    request->putArg((char*) &z, sizeof(z));
    request->putArg((char*) &w, sizeof(w));
    request->putArg((char*) &h, sizeof(h));
    request->putArg((char*) &layerStack, sizeof(layerStack));
    request->putArg((char*) &blur, sizeof(blur));
    request->putArg((char*) &blurMaskSampling, sizeof(blurMaskSampling));
    request->putArg((char*) &blurMaskAlphaThreshold, sizeof(blurMaskAlphaThreshold));
    request->putArg((char*) &alpha, sizeof(alpha));
    request->putArg((char*) &flags, sizeof(flags));
    request->putArg((char*) &mask, sizeof(mask));
    request->putArg((char*) &matrix, sizeof(matrix));
    request->putArg((char*) &crop, sizeof(crop));
    
    RpcResponse* response = SurfaceRpcUtilInst.rpcclient->doRpc(request);
    delete response; 
    ALOGI("rpc surface flinger update layer state");
}

static void* sfthLoop(void* args)
{
    while (true) {
        pthread_mutex_lock(&queueLock);
        if (reqQueue.empty()) {
            pthread_cond_wait(&queueCond, &queueLock);
        }
        SurfaceRpcRequest* request = reqQueue.front();
        reqQueue.pop();
        pthread_mutex_unlock(&queueLock);
        switch (request->type) {
//            case TYPE_ADD_CLIENT: doAddClient(request);
//                            break;
            case TYPE_REMOVE_CLIENT: doRemoveClient(request);
                            break;
//            case TYPE_ADD_LAYER: doAddLayer(request);
//                            break;
            case TYPE_REMOVE_LAYER: doRemoveLayer(request);
                            break;
            case TYPE_SYNC_LAYER: doSyncLayer(request);
                            break;
            case TYPE_UPDATE_LAYER_STATE: doUpdateLayerState(request);
                            break;
        }
        delete request;
    }
}

__attribute__ ((visibility ("default"))) void initFlingerClient()
{
    pthread_mutex_init(&queueLock, NULL);
    pthread_cond_init(&queueCond, NULL);
    pthread_t rpcThread;
    pthread_create(&rpcThread, NULL, sfthLoop, NULL);
}

void addClient(void* client)
{
    if (!SurfaceRpcUtilInst.isShareEnabled || !SurfaceRpcUtilInst.isConnected || SurfaceRpcUtilInst.isServer) {
        return;
    }
    RpcRequest* request = new RpcRequest(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_ADD_CLIENT, SurfaceRpcUtilInst.rpcclient->socketFd, false);
    
    RpcResponse* response = SurfaceRpcUtilInst.rpcclient->doRpc(request);
    int clientId;
    response->getRet((char*) &clientId, sizeof(clientId));
    delete response;
    SurfaceRpcUtilInst.clientToIds[client] = clientId;
    ALOGI("rpc surface flinger add a client");
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
    SurfaceRpcUtilInst.clientToIds.erase(client);
    SurfaceRpcRequest* request = new SurfaceRpcRequest(TYPE_REMOVE_CLIENT, clientId);
    pthread_mutex_lock(&queueLock);
    reqQueue.push(request);
    pthread_cond_signal(&queueCond);
    pthread_mutex_unlock(&queueLock);
}

void addLayer(const String8& name, uint32_t width, uint32_t height, uint32_t flags, PixelFormat format, void* client, void* layer)
{
    if (!SurfaceRpcUtilInst.isShareEnabled || !SurfaceRpcUtilInst.isConnected || SurfaceRpcUtilInst.isServer) {
        return;
    }
    if (SurfaceRpcUtilInst.clientToIds.find(client) == SurfaceRpcUtilInst.clientToIds.end()) {
        return;
    }
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
    ALOGI("rpc surface flinger add a layer");
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

void updateLayerState(void* client, void* layer, layer_state_t state)
{
    if (!SurfaceRpcUtilInst.isShareEnabled || !SurfaceRpcUtilInst.isConnected || SurfaceRpcUtilInst.isServer) {
        return;
    }
    if (SurfaceRpcUtilInst.clientToIds.find(client) == SurfaceRpcUtilInst.clientToIds.end() ||
        SurfaceRpcUtilInst.layerToIds.find(layer) == SurfaceRpcUtilInst.layerToIds.end()) {
        return;
    }
    int clientId = SurfaceRpcUtilInst.clientToIds[client];
    int layerId = SurfaceRpcUtilInst.layerToIds[layer];
    LayerStateDef* def = new LayerStateDef();
    def->clientId = clientId;
    //def->surface = state.surface;;
    def->what = state.what;
    def->x = state.x;
    def->y = state.y;
    def->z = state.z;
    def->w = state.w;
    def->h = state.h;
    def->layerStack = state.layerStack;
    def->blur = state.blur;
    //def->blurMaskSurface = state.blurMaskSurface;
    def->blurMaskSampling = state.blurMaskSampling;
    def->blurMaskAlphaThreshold = state.blurMaskAlphaThreshold;
    def->alpha = state.alpha;
    def->flags = state.flags;
    def->mask = state.mask;
    //def->reserved = state.reserved;
    def->matrix = state.matrix;
    def->crop = state.crop;
    //def->transparentRegion = state.transparentRegion;
    
    SurfaceRpcRequest* request = new SurfaceRpcRequest(TYPE_UPDATE_LAYER_STATE, layerId, def);
    pthread_mutex_lock(&queueLock);
    reqQueue.push(request);
    pthread_cond_signal(&queueCond);
    pthread_mutex_unlock(&queueLock);
}

}; // namespace android
