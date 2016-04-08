#include "RpcSurfaceFlingerServer.h"
#include "RpcSurfaceFlingerCommon.h"

#include "SurfaceFlinger.h"
#include "Client.h"
#include "Layer.h"

#include "SkBitmap.h"
#include "SkImageDecoder.h"
#include "SkStream.h"
#include "SkUtils.h"

#include <rpc/share_rpc.h>
#include <ui/PixelFormat.h>
#include <utils/StrongPointer.h>
#include <utils/Log.h>
#include <private/gui/LayerState.h>

#include "time.h" 

namespace android {

RpcResponse* SurfaceFlinger_addClient(RpcRequest* request)
{
    sp<SurfaceFlinger> flinger = *((sp<SurfaceFlinger>*) SurfaceRpcUtilInst.idToObjMap[request->serviceId]);
    sp<Client> client((Client*) (flinger->createConnection().get()));
    int clientId = SurfaceRpcUtilInst.nextServiceObjId++;
    SurfaceRpcUtilInst.idToClients[clientId] = new sp<Client>(client);
    
    RpcResponse* response = new RpcResponse(true);
    response->putRet((char*) &clientId, sizeof(clientId));
    ALOGI("rpc surface flinger add a client server");
    
    return response;
}

RpcResponse* SurfaceFlinger_removeClient(RpcRequest* request)
{
    int clientId;
    request->getArg((char*) &clientId, sizeof(clientId));
    delete (sp<ISurfaceComposerClient>*) SurfaceRpcUtilInst.idToClients[clientId];
    SurfaceRpcUtilInst.idToClients.erase(clientId);
    
    ALOGI("rpc surface flinger remove a client server");
    RpcResponse* response = new RpcResponse(false);
    return response; 
}

RpcResponse* SurfaceFlinger_addLayer(RpcRequest* request)
{
    size_t len;
    request->getArg((char*) &len, sizeof(len));
    char nameBuf[len];
    request->getArg(nameBuf, len);
    String8 name(nameBuf, len);
    uint32_t width;
    request->getArg((char*) &width, sizeof(width));
    uint32_t height;
    request->getArg((char*) &height, sizeof(height));
    uint32_t flags;
    request->getArg((char*) &flags, sizeof(flags));
    PixelFormat format;
    request->getArg((char*) &format, sizeof(format));
    int clientId;
    request->getArg((char*) &clientId, sizeof(clientId));
    sp<Client> client = *((sp<Client>*) SurfaceRpcUtilInst.idToClients[clientId]);
    sp<SurfaceFlinger> flinger = *((sp<SurfaceFlinger>*) SurfaceRpcUtilInst.idToObjMap[request->serviceId]);
    sp<IBinder> handle;
    sp<IGraphicBufferProducer> gbp;
    flinger->createLayer(name, client, width, height, format, flags, &handle, &gbp);
    int layerId = SurfaceRpcUtilInst.nextServiceObjId++;
    SurfaceRpcUtilInst.idToLayers[layerId] = new sp<IBinder>(handle);
    // initialize the buffer slots for this layer
    bufferSlots[layerId] = new sp<GraphicBuffer>[BufferQueue::NUM_BUFFER_SLOTS];
    
    // set layer z
    sp<Layer> layer = client->getLayerUser(handle);
    //((sp<ISurfaceComposerClient>) client)->setLayer(handle, 0xffffffff);
    
    ALOGI("rpc surface flinger add a layer server, ptr[%p]", layer.get());
    RpcResponse* response = new RpcResponse(true);
    response->putRet((char*) &layerId, sizeof(layerId));
    
    return response;
}

RpcResponse* SurfaceFlinger_removeLayer(RpcRequest* request)
{
    int layerId;
    request->getArg((char*) &layerId, sizeof(layerId));
    // delet buffer slots
    delete bufferSlots[layerId];
    bufferSlots.erase(layerId);
    // delte layer object
    delete (sp<IBinder>*) SurfaceRpcUtilInst.idToLayers[layerId];
    SurfaceRpcUtilInst.idToLayers.erase(layerId);
    
    ALOGI("rpc surface flinger remove a layer server");
    RpcResponse* response = new RpcResponse(false);
    return response; 
}

u4 receivedFrames = 0;
struct timeval lastTime;
RpcResponse* SurfaceFlinger_syncLayer(RpcRequest* request)
{
    SERVER_METH_PROFILING_START(request->seqNo)
    int clientId;
    request->getArg((char*) &clientId, sizeof(clientId));
    int layerId;
    request->getArg((char*) &layerId, sizeof(layerId));
    size_t size;
    request->getArg((char*) &size, sizeof(size));
    uint8_t* data = (uint8_t*) malloc(size);
    request->getArg((char*) data, size);
    int width;
    request->getArg((char*) &width, sizeof(width));
    int height;
    request->getArg((char*) &height, sizeof(height));
    int stride;
    request->getArg((char*) &stride, sizeof(stride));
    int format;
    request->getArg((char*) &format, sizeof(format));
    int usage;
    request->getArg((char*) &usage, sizeof(usage));
    
    sp<Client> client = *((sp<Client>*) SurfaceRpcUtilInst.idToClients[clientId]);
    sp<IBinder> handler = *((sp<IBinder>*) SurfaceRpcUtilInst.idToLayers[layerId]);
    sp<Layer> layer = client->getLayerUser(handler);
    sp<IGraphicBufferProducer> producer = layer->getProducer();
    // get the buffer slots for this layer
    sp<GraphicBuffer>* slots = bufferSlots[layerId];
    
    RpcResponse* response = new RpcResponse(false);
    int buf = -1;
    sp<Fence> fence;
    status_t err = producer->dequeueBuffer(&buf, &fence, false, width, height, format, usage);
    if (err < 0) {
        ALOGE("rpc camera service dequeue buffer failed in listener");
        free(data);
        return response;
    }
    sp<GraphicBuffer>& gbuf(slots[buf]);
    if (err & IGraphicBufferProducer::RELEASE_ALL_BUFFERS) {
        for (int i = 0; i < BufferQueue::NUM_BUFFER_SLOTS; i++) {
            slots[i] = 0;
        }
    }
    if ((err & IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION) || gbuf == 0) {
        err = producer->requestBuffer(buf, &gbuf);
        if (err != NO_ERROR) {
            ALOGE("rpc camera service dequeueBuffer: IGraphicBufferProducer::requestBuffer failed: %d", err);
            producer->cancelBuffer(buf, fence);
            free(data);
            return response;
        }
    }
    if (fence->isValid()) {
        err = fence->waitForever("RpcBufferConsumerListener::onFrameAvailable::dequeue");
        if (err != OK) {
            ALOGW("failed to wait for buffer fence in dequeue: %d", err);
            // keep going
        }
    }
    
    // decode the png graph for PIXEL_FORMAT_RGBA_8888, refer: nativeDecodeByteArray()@BitmapFactory.cpp 
    SkBitmap* outputBitmap = NULL;
    if (PNG_COMRESSION_ENABLE) {
        if (format == PIXEL_FORMAT_RGBA_8888) {
            SkMemoryStream* stream = new SkMemoryStream(data, size, false);
            SkAutoUnref aur(stream);
            SkImageDecoder::Mode decodeMode = SkImageDecoder::kDecodePixels_Mode;
            SkImageDecoder* decoder = SkImageDecoder::Factory(stream);
            decoder->setSampleSize(1);
            decoder->setDitherImage(true);
            decoder->setPreferQualityOverSpeed(false);
            decoder->setRequireUnpremultipliedColors(false);
            outputBitmap = new SkBitmap();
            
            SkAutoTDelete<SkImageDecoder> add(decoder);
            SkColorType prefColorType = kN32_SkColorType;
            if (decoder->decode(stream, outputBitmap, prefColorType, decodeMode)
                    != SkImageDecoder::kSuccess) {
                ALOGE("rpc surface flinger decode png to arga_8888 failed");
                return response;
            }
            free(data);
            size = outputBitmap->getSize();
            data = (uint8_t*) outputBitmap->getPixels();
        }
    }
    
    sp<GraphicBuffer> dst(gbuf);
    uint8_t* dst_bits = NULL;
    err = dst->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, (void**)&dst_bits);
    ALOGE_IF(err, "error locking dst buffer %s", strerror(-err));
    if (dst_bits) {
        memcpy(dst_bits, data, size);
        ALOGI("rpc surface flinger copied data: width[%d], height[%d], stride[%d], format[%d], usage[%d], size[%d]", width, height, stride, format, usage, size);
        dst->unlock();
    }
    // queue the producer buffer
    int64_t timestamp;
    timestamp = systemTime(SYSTEM_TIME_MONOTONIC);
    // Make sure the crop rectangle is entirely inside the buffer.
    Rect crop(width, height);

    IGraphicBufferProducer::QueueBufferOutput output;
    // NATIVE_WINDOW_SCALING_MODE_FREEZE this doesn't work, we need to make it scaling
    IGraphicBufferProducer::QueueBufferInput input(timestamp, true, crop,
            NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW, 0 ^ 0, false,
            Fence::NO_FENCE, 0);
    err = producer->queueBuffer(buf, input, &output);
    if (err != OK)  {
        ALOGE("rpc surface flinger queueBuffer: error queuing buffer, %d", err);
    }
    if (outputBitmap != NULL) {
        delete outputBitmap;
    } else {
        free(data);
    }
    
    // code for profiling
    if (receivedFrames == 0) {
        gettimeofday(&lastTime, NULL);
    }
    receivedFrames++;
    if (receivedFrames > 0 && receivedFrames % 1000 == 0) {
        struct timeval finish;
        gettimeofday(&finish, NULL);
        double secs = (double) ((finish.tv_sec - lastTime.tv_sec) + (finish.tv_usec - lastTime.tv_usec) * 0.000001);
        ALOGE("[rpc evaluation], achieved fps for LCD is: %f, width: %d, height: %d, size: %d", (1000 / secs), width, height, size);
        lastTime = finish;
    }
    
    SERVER_METH_PROFILING_END(request->seqNo)
    ALOGI("rpc surface flinger sync a layer server");
    return response;
}

RpcResponse* SurfaceFlinger_updateLayerState(RpcRequest* request)
{
    int clientId;
    request->getArg((char*) &clientId, sizeof(clientId));
    int layerId;
    request->getArg((char*) &layerId, sizeof(layerId));
    layer_state_t state;
    request->getArg((char*) &state.what, sizeof(state.what));
    uint32_t what = state.what;
    if (what & layer_state_t::ePositionChanged) {
        request->getArg((char*) &state.x, sizeof(state.x));
        request->getArg((char*) &state.y, sizeof(state.y));
    }
    if (what & layer_state_t::eLayerChanged) {
        request->getArg((char*) &state.z, sizeof(state.z));
    }
    if (what & layer_state_t::eBlurChanged) {
        request->getArg((char*) &state.blur, sizeof(state.blur));
    }
    /*if ((cwhat & layer_state_t::eBlurMaskSurfaceChanged) && !(lwhat & layer_state_t::eBlurMaskSurfaceChanged)) {
        requestInList->blurMaskSurface = request->blurMaskSurface;
    }*/
    if (what & layer_state_t::eBlurMaskSamplingChanged) {
        request->getArg((char*) &state.blurMaskSampling, sizeof(state.blurMaskSampling));
    }
    if (what & layer_state_t::eBlurMaskAlphaThresholdChanged) {
        request->getArg((char*) &state.blurMaskAlphaThreshold, sizeof(state.blurMaskAlphaThreshold));
    }
    if (what & layer_state_t::eSizeChanged) {
        request->getArg((char*) &state.w, sizeof(state.w));
        request->getArg((char*) &state.h, sizeof(state.h));
    }
    if (what & layer_state_t::eAlphaChanged) {
        request->getArg((char*) &state.alpha, sizeof(state.alpha));
    }
    if (what & layer_state_t::eMatrixChanged) {
        request->getArg((char*) &state.matrix, sizeof(state.matrix));
    }
    if (what & layer_state_t::eTransparentRegionChanged) {
        int tregionSize;
        request->getArg((char*) &tregionSize, sizeof(tregionSize));
        char regionBuffer[tregionSize];
        request->getArg(regionBuffer, tregionSize);
        state.transparentRegion.unflatten((void*) regionBuffer, tregionSize);
    }
    if ((what & layer_state_t::eVisibilityChanged) || 
        (what & layer_state_t::eOpacityChanged) ||
        (what & layer_state_t::eTransparencyChanged)) {
        request->getArg((char*) &state.flags, sizeof(state.flags));
        request->getArg((char*) &state.mask, sizeof(state.mask));
    }
    if (what & layer_state_t::eCropChanged) {
        request->getArg((char*) &state.crop, sizeof(state.crop));
    }
    if (what & layer_state_t::eLayerStackChanged) {
        request->getArg((char*) &state.layerStack, sizeof(state.layerStack));
    }
    
    sp<Client> client = *((sp<Client>*) SurfaceRpcUtilInst.idToClients[clientId]);
    sp<IBinder> handler = *((sp<IBinder>*) SurfaceRpcUtilInst.idToLayers[layerId]);
    sp<SurfaceFlinger> flinger = *((sp<SurfaceFlinger>*) SurfaceRpcUtilInst.idToObjMap[request->serviceId]);
    state.surface = handler;
    Vector<ComposerState> cstates;
    Vector<DisplayState> display;
    ComposerState cstate;
    cstate.client = client;
    cstate.state = state;
    cstates.add(cstate);
    flinger->setTransactionState(cstates, display, 0);

    ALOGI("rpc surface flinger update layer state server");
    RpcResponse* response = new RpcResponse(false);
    return response; 
}

__attribute__ ((visibility ("default"))) void registerSurfaceFlingerServer()
{
    SurfaceRpcUtilInst.rpcserver->registerFunc(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_ADD_CLIENT, &SurfaceFlinger_addClient);
    SurfaceRpcUtilInst.rpcserver->registerFunc(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_REMOVE_CLIENT, &SurfaceFlinger_removeClient);
    SurfaceRpcUtilInst.rpcserver->registerFunc(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_ADD_LAYER, &SurfaceFlinger_addLayer);
    SurfaceRpcUtilInst.rpcserver->registerFunc(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_REMOVE_LAYER, &SurfaceFlinger_removeLayer);
    SurfaceRpcUtilInst.rpcserver->registerFunc(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_SYNC_LAYER, &SurfaceFlinger_syncLayer);
    SurfaceRpcUtilInst.rpcserver->registerFunc(SurfaceRpcUtilInst.SURFACE_SERVICE_ID, SF_METH_UPDATE_LAYER_STATE, &SurfaceFlinger_updateLayerState);
}

}; // namespace android
