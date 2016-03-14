#ifndef ANDROID_RPC_SURFACE_FLINGER_SERVER_H
#define ANDROID_RPC_SURFACE_FLINGER_SERVER_H

#include <gui/BufferQueue.h>
#include <map>

namespace android {

std::map<int, sp<GraphicBuffer>*> bufferSlots;

void registerSurfaceFlingerServer();

}; // namespace android

#endif // ANDROID_RPC_SURFACE_FLINGER_SERVER_H