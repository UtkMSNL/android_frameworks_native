/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_RPC_SENSOR_SERVICE_H
#define ANDROID_RPC_SENSOR_SERVICE_H

#include <stdint.h>
#include <sys/types.h>

#include <utils/Vector.h>
#include <utils/SortedVector.h>
#include <utils/KeyedVector.h>
#include <utils/threads.h>
#include <utils/RefBase.h>

#include <binder/BinderService.h>

#include <gui/Sensor.h>
#include <gui/BitTube.h>
#include <gui/ISensorServer.h>
#include <gui/ISensorEventConnection.h>

#include <rpc/share_rpc.h>
#include "SensorService.h"
#include "SensorInterface.h"

// ---------------------------------------------------------------------------

#define DEBUG_CONNECTIONS   false
// Max size is 1 MB which is enough to accept a batch of about 10k events.
#define MAX_SOCKET_BUFFER_SIZE_BATCHED 1024 * 1024
#define SOCKET_BUFFER_SIZE_NON_BATCHED 4 * 1024

namespace android {
// ---------------------------------------------------------------------------

class RpcSensorService :
        public BinderService<RpcSensorService>,
        public BnSensorServer
{
public:
    friend class BinderService<RpcSensorService>;

    static const char* WAKE_LOCK_NAME;

    static char const* getServiceName() ANDROID_API { return "sensorservice"; }
    RpcSensorService() ANDROID_API;
    virtual ~RpcSensorService();

    virtual void onFirstRef();

    // Thread interface
    virtual bool threadLoop();

    // ISensorServer interface
    virtual Vector<Sensor> getSensorList();
    virtual sp<ISensorEventConnection> createSensorEventConnection();

    class RpcSensorEventConnectionProxy : public BnSensorEventConnection {
        virtual ~RpcSensorEventConnectionProxy();
        virtual void onFirstRef();
        virtual sp<BitTube> getSensorChannel() const;
        virtual status_t enableDisable(int handle, bool enabled, nsecs_t samplingPeriodNs,
                                       nsecs_t maxBatchReportLatencyNs, int reservedFlags);
        virtual status_t setEventRate(int handle, nsecs_t samplingPeriodNs);
        virtual status_t flush();
        
        sp<RpcSensorService> const mService;
        sp<BitTube> mChannel;
        int mRemoteServiceId;
        
    public:
        RpcSensorEventConnectionProxy(const sp<RpcSensorService>& service, int remoteServiceId, int sendFd, int receiveFd);
    };
    
    size_t mSocketBufferSize;
    
    /*static status_t publish(bool allowIsolated = false) {
        sp<IServiceManager> sm(defaultServiceManager());
        return sm->addService(
                String16(SERVICE::getServiceName()),
                new SERVICE(), allowIsolated);
    }

    static void publishAndJoinThreadPool(bool allowIsolated = false) {
        publish(allowIsolated);
        joinThreadPool();
    }

    static void instantiate() { publish(); }*/
};

// ---------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_RPC_SENSOR_SERVICE_H
