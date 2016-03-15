#ifndef ANDROID_RPC_SURFACE_FLINGER_CLIENT_H
#define ANDROID_RPC_SURFACE_FLINGER_CLIENT_H

#include <ui/PixelFormat.h>
#include <utils/String8.h>
#include <utils/StrongPointer.h>
#include <gui/BufferQueue.h>

#include <pthread.h>
#include <cstdlib>
#include <queue>

namespace android {

struct SurfaceRpcRequest
{
    int type; // the type of the request
    int id; // the associate id to perform
    void* payload; // the data associated with this request
    
    SurfaceRpcRequest(int vType, int vId = 0, void* vPayload = NULL)
        : type(vType), id(vId), payload(vPayload) {}
        
    ~SurfaceRpcRequest()
    {
        if (payload != NULL) {
            delete [] reinterpret_cast <char*> (payload);
        }
    }
};

struct ClientDef
{
    void* client;
    
    ClientDef(void* vClient)
        : client(vClient) {}
};

struct LayerDef
{
    String8 name;
    uint32_t width;
    uint32_t height;
    uint32_t flags;
    PixelFormat format;
    void* client;
    void* layer;
    
    LayerDef(String8 vName, uint32_t vWidth, uint32_t vHeight, uint32_t vFlags, PixelFormat vFormat, void* vClient, void* vLayer)
        : name(vName), width(vWidth), height(vHeight), flags(vFlags), format(vFormat), client(vClient), layer(vLayer) {}
};

struct BufferDef
{
    int clientId;
    uint8_t* data;
    size_t size;
    int width;
    int height;
    int stride;
    int format;
    int usage;
    
    BufferDef(int vClientId, int vWidth, int vHeight, int vStride, int vFormat, int vUsage)
        : clientId(vClientId), width(vWidth), height(vHeight), stride(vStride), format(vFormat), usage(vUsage) {}
    
    ~BufferDef()
    {
        free(data);
    }
};

void addClient(void* client);

void removeClient(void* client);

void addLayer(const String8& name, uint32_t w, uint32_t h, uint32_t flags, PixelFormat format, void* client, void* layer);

void removeLayer(void* layer);

void syncLayer(sp<GraphicBuffer> buffer, void* client, void* layer);

void initFlingerClient();

}; // namespace android

#endif // ANDROID_RPC_SURFACE_FLINGER_CLIENT_H
