#include "RpcSurfaceFlingerClient.h"
#include "RpcSurfaceFlingerCommon.h"

#include <rpc/share_rpc.h>
#include <utils/Log.h>
#include <map>
#include <list>


#include "SkBitmap.h"
#include "SkData.h"
#include "SkPixelRef.h"
#include "SkImageEncoder.h"
#include "SkImageInfo.h"
#include "SkColorPriv.h"
#include "SkDither.h"
#include "SkUnPreMultiply.h"
#include "SkStream.h"

#include "time.h" 

//#define TYPE_ADD_CLIENT 1
#define TYPE_REMOVE_CLIENT 2
//#define TYPE_ADD_LAYER 3
#define TYPE_REMOVE_LAYER 4
#define TYPE_SYNC_LAYER 5
#define TYPE_UPDATE_LAYER_STATE 6

// allow maximum of 5 frames in queue, new comings are discarded
#define MAX_ALLOWED_IN_QUEUE 5

namespace android {

static std::list<SurfaceRpcRequest*> reqList;
// this is a list to cache the layers whose image has not been compressed yet
static std::list<SurfaceRpcRequest*> layerSyncList;
static pthread_mutex_t queueLock;
static pthread_cond_t queueCond;
static pthread_mutex_t layerSyncLock;
static pthread_cond_t layerSyncCond;

// stored the number of frames in queue for each layer
static std::map<int, int> frameInQueue;

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
    ALOGI("rpc surface flinger start doRemoveClient");
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
    ALOGI("rpc surface flinger start to remove a layer");
    int layerId = layerRequest->id;
    
    RpcRequest* request = new RpcRequest(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_REMOVE_LAYER, SurfaceRpcUtilInst.rpcclient->socketFd, true);
    request->putArg((char*) &layerId, sizeof(layerId));
    
    RpcResponse* response = SurfaceRpcUtilInst.rpcclient->doRpc(request);
    delete response; 
    ALOGI("rpc surface flinger remove a layer");
}

void doSyncLayer(SurfaceRpcRequest* layerRequest)
{
    ALOGI("rpc surface flinger start to dosynclayer");
    CLIENT_METH_PROFILING_START(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_SYNC_LAYER)
    BufferDef* def = (BufferDef*) layerRequest->payload;
    int clientId = def->clientId;
    int layerId = layerRequest->id;
    size_t size = def->size;
    uint8_t* data;
    if (def->data != NULL) {
        data = def->data;
    } else {
        data = (uint8_t*) def->skdata->data();
    }
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
    CLIENT_METH_PROFILING_END(response->seqNo)
    delete response; 
    frameInQueue[layerId] -= 1;
    ALOGI("rpc surface flinger finish sync layer, size is: %d, format is: %d",  size, format);
}

void doUpdateLayerState(SurfaceRpcRequest* layerRequest)
{
    ALOGI("rpc surface flinger start doUpdateLayerState");
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
    
    RpcRequest* request = new RpcRequest(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_UPDATE_LAYER_STATE, SurfaceRpcUtilInst.rpcclient->socketFd, true);
    request->putArg((char*) &clientId, sizeof(clientId));
    request->putArg((char*) &layerId, sizeof(layerId));
    request->putArg((char*) &what, sizeof(what));
    if (what & layer_state_t::ePositionChanged) {
        request->putArg((char*) &x, sizeof(x));
        request->putArg((char*) &y, sizeof(y));
    }
    if (what & layer_state_t::eLayerChanged) {
        request->putArg((char*) &z, sizeof(z));
    }
    if (what & layer_state_t::eBlurChanged) {
        request->putArg((char*) &blur, sizeof(blur));
    }
    /*if ((cwhat & layer_state_t::eBlurMaskSurfaceChanged) && !(lwhat & layer_state_t::eBlurMaskSurfaceChanged)) {
        requestInList->blurMaskSurface = request->blurMaskSurface;
    }*/
    if (what & layer_state_t::eBlurMaskSamplingChanged) {
        request->putArg((char*) &blurMaskSampling, sizeof(blurMaskSampling));
    }
    if (what & layer_state_t::eBlurMaskAlphaThresholdChanged) {
        request->putArg((char*) &blurMaskAlphaThreshold, sizeof(blurMaskAlphaThreshold));
    }
    if (what & layer_state_t::eSizeChanged) {
        request->putArg((char*) &w, sizeof(w));
        request->putArg((char*) &h, sizeof(h));
    }
    if (what & layer_state_t::eAlphaChanged) {
        request->putArg((char*) &alpha, sizeof(alpha));
    }
    if (what & layer_state_t::eMatrixChanged) {
        layer_state_t::matrix22_t matrix = *def->matrix;
        //what = what & ~layer_state_t::eMatrixChanged;
        request->putArg((char*) &matrix, sizeof(matrix));
    }
    if (what & layer_state_t::eTransparentRegionChanged) {
        Region transparentRegion = *def->transparentRegion;
        //what = what & ~layer_state_t::eTransparentRegionChanged;
        int tregionSize = transparentRegion.getFlattenedSize();
        char regionBuffer[tregionSize];
        transparentRegion.flatten((void*) regionBuffer, tregionSize);
        request->putArg((char*) &tregionSize, sizeof(tregionSize));
        request->putArg(regionBuffer, tregionSize);
    }
    if ((what & layer_state_t::eVisibilityChanged) || 
        (what & layer_state_t::eOpacityChanged) ||
        (what & layer_state_t::eTransparencyChanged)) {
        request->putArg((char*) &flags, sizeof(flags));
        request->putArg((char*) &mask, sizeof(mask));
    }
    if (what & layer_state_t::eCropChanged) {
        Rect crop = *def->crop;
        //what = what & ~layer_state_t::eCropChanged;
        request->putArg((char*) &crop, sizeof(crop));
    }
    if (what & layer_state_t::eLayerStackChanged) {
        request->putArg((char*) &layerStack, sizeof(layerStack));
    }
    
    RpcResponse* response = SurfaceRpcUtilInst.rpcclient->doRpc(request);
    delete response;
    ALOGI("rpc surface flinger update layer state");
}

/* this method check if the current state update request can be avoided
   the reason is that the current frame sync is too slow to sync all the frames on time,
   therefore, multiple state update before its frame sync needs only to take the last one 
 */
/*static bool isDiscardReq(SurfaceRpcRequest* request)
{
    int layerId = request->id;
    for (std::list<SurfaceRpcRequest*>::iterator it=reqList.begin(); it != reqList.end(); ++it) {
        SurfaceRpcRequest* requestInList = *it;
        if (requestInList->type == TYPE_SYNC_LAYER && requestInList->id == layerId) {
            LayerStateDef* cdef = (LayerStateDef*) request->payload;
            LayerStateDef* ldef = (LayerStateDef*) requestInList->payload;
            uint32_t cwhat = cdef->what;
            uint32_t lwhat = ldef->what;
            ldef->what |= cwhat;
            // combine the request effect
            if ((cwhat & layer_state_t::ePositionChanged) && !(lwhat & layer_state_t::ePositionChanged)) {
                ldef->x = cdef->x;
                ldef->y = cdef->y;
            }
            if ((cwhat & layer_state_t::eLayerChanged) && !(lwhat & layer_state_t::eLayerChanged)) {
                ldef->z = cdef->z;
            }
            if ((cwhat & layer_state_t::eBlurChanged) && !(lwhat & layer_state_t::eBlurChanged)) {
                ldef->blur = cdef->blur;
            }
            //if ((cwhat & layer_state_t::eBlurMaskSurfaceChanged) && !(lwhat & layer_state_t::eBlurMaskSurfaceChanged)) {
            //    ldef->blurMaskSurface = cdef->blurMaskSurface;
            //}
            if ((cwhat & layer_state_t::eBlurMaskSamplingChanged) && !(lwhat & layer_state_t::eBlurMaskSamplingChanged)) {
                ldef->blurMaskSampling = cdef->blurMaskSampling;
            }
            if ((cwhat & layer_state_t::eBlurMaskAlphaThresholdChanged) && !(lwhat & layer_state_t::eBlurMaskAlphaThresholdChanged)) {
                ldef->blurMaskAlphaThreshold = cdef->blurMaskAlphaThreshold;
            }
            if ((cwhat & layer_state_t::eSizeChanged) && !(lwhat & layer_state_t::eSizeChanged)) {
                ldef->w = cdef->w;
                ldef->h = cdef->h;
            }
            if ((cwhat & layer_state_t::eAlphaChanged) && !(lwhat & layer_state_t::eAlphaChanged)) {
                ldef->alpha = cdef->alpha;
            }
            if ((cwhat & layer_state_t::eMatrixChanged) && !(lwhat & layer_state_t::eMatrixChanged)) {
                ALOGE("rpc surface flinger start matrix, %p, %p", ldef->matrix, cdef->matrix);
                ldef->matrix = new layer_state_t::matrix22_t();
                memcpy(ldef->matrix, cdef->matrix, sizeof(layer_state_t::matrix22_t));
                ALOGE("rpc surface flinger end matrix");
            }
            if ((cwhat & layer_state_t::eTransparentRegionChanged) && !(lwhat & layer_state_t::eTransparentRegionChanged)) {
                ALOGE("rpc surface flinger start region");
                ldef->transparentRegion = new Region(*cdef->transparentRegion);
                ALOGE("rpc surface flinger end region");
                //ldef->transparentRegion.orSelf(cdef->transparentRegion);
                //Region tmpRegion(cdef->transparentRegion);
                //ldef->transparentRegion = tmpRegion;
            }
            if (((cwhat & layer_state_t::eVisibilityChanged) && !(lwhat & layer_state_t::eVisibilityChanged)) || 
                ((cwhat & layer_state_t::eOpacityChanged) && !(lwhat & layer_state_t::eOpacityChanged)) ||
                ((cwhat & layer_state_t::eTransparencyChanged) && !(lwhat & layer_state_t::eTransparencyChanged))) {
                ldef->flags = cdef->flags;
                ldef->mask = cdef->mask;
            }
            if ((cwhat & layer_state_t::eCropChanged) && !(lwhat & layer_state_t::eCropChanged)) {
                ALOGE("rpc surface flinger start crop");
                ldef->crop = new Rect(cdef->crop->left, cdef->crop->top, cdef->crop->right, cdef->crop->bottom);
                ALOGE("rpc surface flinger end crop");
            }
            if ((cwhat & layer_state_t::eLayerStackChanged) && !(lwhat & layer_state_t::eLayerStackChanged)) {
                ldef->layerStack = cdef->layerStack;
            }
            return true;
        }
        if (requestInList->type == TYPE_SYNC_LAYER && requestInList->id == layerId) {
            return false;
        }
    }
    return false;
}*/

static void compressGraphData(SurfaceRpcRequest* request)
{
    BufferDef* def = (BufferDef*) request->payload;
    int format = def->format;
    int stride = def->stride;
    int height = def->height;
    // TODO: handling other pixel formats
    if (format != PIXEL_FORMAT_RGBA_8888 && format != PIXEL_FORMAT_RGB_888) {
        return;
    }
    ALOGE("rpc surface flinger start compressGraphData");
    struct timeval start, finish;
    gettimeofday(&start, NULL); 
    // use skia to compress bitmap
    SkColorType colorType = kRGBA_8888_SkColorType;
    SkBitmap bitmap;
    bitmap.setInfo(SkImageInfo::Make(stride, height, colorType, kPremul_SkAlphaType));
    if (format == PIXEL_FORMAT_RGB_888) {
        uint8_t* pixels = (uint8_t*) malloc(stride * height * 4);
        uint8_t* data = def->data;
        for (int i = 0; i < stride * height; i++) {
            pixels[i * 4] = data[i * 3];
            pixels[i * 4 + 1] = data[i * 3 + 1];
            pixels[i * 4 + 2] = data[i * 3 + 2];
            pixels[i * 4 + 3] = 0xff;
        }
        // free the memory in the request
        free(data);
        def->data = pixels;
    }
    bitmap.setPixels(def->data, NULL);
    
    // compress to png
    SkImageEncoder::Type fm = SkImageEncoder::kPNG_Type;
    SkImageEncoder* encoder = SkImageEncoder::Create(fm);
    def->skdata = encoder->encodeData(bitmap, 100);
    // free the memory in the request
    free(def->data);
    // change the format to rgba_8888 and get the new data in the bitmap
    def->format = PIXEL_FORMAT_RGBA_8888;
    def->size = def->skdata->size();
    def->data = NULL;
    gettimeofday(&finish, NULL); 
    ALOGE("rpc surface flinger finish compressGraphData, format: %d, time: %ld", format, (finish.tv_sec - start.tv_sec) * 1000000 + finish.tv_usec - start.tv_usec);
}

static void* sfthLoop(void* args)
{
    while (true) {
        pthread_mutex_lock(&queueLock);
        if (reqList.empty()) {
            pthread_cond_wait(&queueCond, &queueLock);
        }
        SurfaceRpcRequest* request = reqList.front();
        reqList.pop_front();
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
            case TYPE_UPDATE_LAYER_STATE: 
                            //if (!isDiscardReq(request)) { commented out because of runtime error
                                doUpdateLayerState(request);
                            //}
                            break;
        }
        delete request;
    }
}


static void* compressLoop(void* args)
{
    while (true) {
        pthread_mutex_lock(&layerSyncLock);
        if (layerSyncList.empty()) {
            pthread_cond_wait(&layerSyncCond, &layerSyncLock);
        }
        SurfaceRpcRequest* request = layerSyncList.front();
        layerSyncList.pop_front();
        pthread_mutex_unlock(&layerSyncLock);
        
        switch (request->type) {
            case TYPE_REMOVE_CLIENT: 
                            break;
            case TYPE_REMOVE_LAYER: 
                            break;
            case TYPE_SYNC_LAYER: 
                            if (PNG_COMRESSION_ENABLE) {
                                compressGraphData(request);
                            }
                            break;
            case TYPE_UPDATE_LAYER_STATE: 
                            break;
        }
        
        pthread_mutex_lock(&queueLock);
        reqList.push_back(request);
        pthread_cond_signal(&queueCond);
        pthread_mutex_unlock(&queueLock);
    }
}

__attribute__ ((visibility ("default"))) void initFlingerClient()
{
    pthread_mutex_init(&queueLock, NULL);
    pthread_cond_init(&queueCond, NULL);
    pthread_t rpcThread;
    pthread_create(&rpcThread, NULL, sfthLoop, NULL);
    if (PNG_COMRESSION_ENABLE) {
        pthread_mutex_init(&layerSyncLock, NULL);
        pthread_cond_init(&layerSyncCond, NULL);
        pthread_t compressThread;
        pthread_create(&compressThread, NULL, compressLoop, NULL);
    }
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
    if (PNG_COMRESSION_ENABLE) {
        pthread_mutex_lock(&layerSyncLock);
        layerSyncList.push_back(request);
        pthread_cond_signal(&layerSyncCond);
        pthread_mutex_unlock(&layerSyncLock);
    } else {
        pthread_mutex_lock(&queueLock);
        reqList.push_back(request);
        pthread_cond_signal(&queueCond);
        pthread_mutex_unlock(&queueLock);
    }
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
    if (PNG_COMRESSION_ENABLE) {
        pthread_mutex_lock(&layerSyncLock);
        layerSyncList.push_back(request);
        pthread_cond_signal(&layerSyncCond);
        pthread_mutex_unlock(&layerSyncLock);
    } else {
        pthread_mutex_lock(&queueLock);
        reqList.push_back(request);
        pthread_cond_signal(&queueCond);
        pthread_mutex_unlock(&queueLock);
    }
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
    int layerId = SurfaceRpcUtilInst.layerToIds[layer];
    if (frameInQueue.find(layerId) == frameInQueue.end()) {
        frameInQueue[layerId] = 0;
    }
    if (frameInQueue[layerId] >= MAX_ALLOWED_IN_QUEUE) {
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
    const ssize_t bpp = bytesPerPixel(format);
    size_t size;
    if (bpp > 0) {
        size = stride * height * bpp;
    } else {
        if (format == 0x7fa30c03) { // qualcomm proprietary format　QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka, See: https://mailman.videolan.org/pipermail/vlc-commits/2013-September/022245.html          and See: ACodec.cpp  
            const size_t bpp = 1;
            const size_t bpr = stride * bpp;
            size = bpr * height + ((bpr + 1) / 2) * ((height + 1) / 2) * 2;
        }
    }
    def->size = size;
    def->data = (uint8_t*) malloc(size);
    memcpy(def->data, data, size);
    if (data) {
        buffer->unlock();
    }
    SurfaceRpcRequest* request = new SurfaceRpcRequest(TYPE_SYNC_LAYER, layerId, def);
    if (PNG_COMRESSION_ENABLE) {
        pthread_mutex_lock(&layerSyncLock);
        layerSyncList.push_back(request);
        frameInQueue[layerId] += 1;
        pthread_cond_signal(&layerSyncCond);
        pthread_mutex_unlock(&layerSyncLock);
    } else {
        pthread_mutex_lock(&queueLock);
        reqList.push_back(request);
        frameInQueue[layerId] += 1;
        pthread_cond_signal(&queueCond);
        pthread_mutex_unlock(&queueLock);
    }
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
    if (def->what & layer_state_t::eMatrixChanged) {
        def->matrix = new layer_state_t::matrix22_t();
        memcpy(def->matrix, &state.matrix, sizeof(layer_state_t::matrix22_t));
    }
    if (def->what & layer_state_t::eCropChanged) {
        def->crop = new Rect(state.crop.left, state.crop.top, state.crop.right, state.crop.bottom);
    }
    if (def->what & layer_state_t::eTransparentRegionChanged) {
        def->transparentRegion = new Region(state.transparentRegion);
    }
    
    SurfaceRpcRequest* request = new SurfaceRpcRequest(TYPE_UPDATE_LAYER_STATE, layerId, def);
    if (PNG_COMRESSION_ENABLE) {
        pthread_mutex_lock(&layerSyncLock);
        layerSyncList.push_back(request);
        pthread_cond_signal(&layerSyncCond);
        pthread_mutex_unlock(&layerSyncLock);
    } else {
        pthread_mutex_lock(&queueLock);
        reqList.push_back(request);
        pthread_cond_signal(&queueCond);
        pthread_mutex_unlock(&queueLock);
    }
}

}; // namespace android
