#include "RpcSurfaceFlingerServer.h"
#include "RpcSurfaceFlingerCommon.h"

#include "SurfaceFlinger.h"

#include <rpc/share_rpc.h>
#include <ui/PixelFormat.h>

namespace android {

RpcResponse* SurfaceFlinger_addClient(RpcRequest* request)
{
    sp<SurfaceFlinger> flinger = *((sp<SurfaceFlinger>*) SurfaceRpcUtilInst.idToObjMap[request->serviceId]);
    sp<ISurfaceComposerClient> client = flinger->createConnection();
    int clientId = SurfaceRpcUtilInst.nextServiceObjId++;
    SurfaceRpcUtilInst.idToClients[clientId] = new sp<ISurfaceComposerClient>(client);
    
    RpcResponse* response = new RpcResponse(true);
    response->putRet((char*) &clientId, sizeof(clientId));
    
    return response;
}

RpcResponse* SurfaceFlinger_removeClient(RpcRequest* request)
{
    int clientId;
    request->getArg((char*) &clientId, sizeof(clientId));
    delete SurfaceRpcUtilInst.idToClients[clientId];
    SurfaceRpcUtilInst.idToClients.erase(clientId);
    
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
    sp<Client> client = *SurfaceRpcUtilInst.idToClients[clientId];
    sp<SurfaceFlinger> flinger = *((sp<SurfaceFlinger>*) SurfaceRpcUtilInst.idToObjMap[request->serviceId]);
    sp<IBinder> handle;
    sp<IGraphicBufferProducer> gbp;
    flinger->createLayer(name, client, width, height, format, flags, &handle, &gbp);
    int layerId = SurfaceRpcUtilInst.nextServiceObjId++;
    SurfaceRpcUtilInst.idToLayers[layerId] = new sp<IBinder>(handle);
    // initialize the buffer slots for this layer
    bufferSlots[layerId] = new sp<GraphicBuffer>[BufferQueue::NUM_BUFFER_SLOTS];
    
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
    delete SurfaceRpcUtilInst.idToLayers[layerId];
    SurfaceRpcUtilInst.idToLayers.erase(layerId);
    
    RpcResponse* response = new RpcResponse(false);
    return response; 
}

RpcResponse* SurfaceFlinger_syncLayer(RpcRequest* request)
{
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
    
    sp<Client> client = *((sp<Client>*) SurfaceRpcUtilInst.idToClients[clientId];
    sp<IBinder> handler = *((sp<IBinder>*) SurfaceRpcUtilInst.idToLayers[layerId];
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
    sp<GraphicBuffer> dst(gbuf);
    uint8_t* dst_bits = NULL;
    err = dst->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, (void**)&dst_bits);
    ALOGE_IF(err, "error locking dst buffer %s", strerror(-err));
    if (dst_bits) {
        memcpy(dst_bits, data, size);
        ALOGE("rpc surface flinger copied data: width[%d], height[%d], stride[%d], format[%d], usage[%d]", width, height, stride, format, usage);
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
    return response;
}

}; // namespace android