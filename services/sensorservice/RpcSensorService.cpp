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

#include <stdint.h>
#include <math.h>
#include <sys/types.h>

#include <cutils/properties.h>

#include <utils/SortedVector.h>
#include <utils/KeyedVector.h>
#include <utils/threads.h>
#include <utils/Atomic.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/Singleton.h>
#include <utils/String16.h>

#include <binder/BinderService.h>
#include <binder/IServiceManager.h>
#include <binder/PermissionCache.h>

#include <gui/ISensorServer.h>
#include <gui/ISensorEventConnection.h>
#include <gui/SensorEventQueue.h>
#include <gui/RpcBitTube.h>

#include <hardware/sensors.h>
#include <hardware_legacy/power.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "BatteryService.h"
#include "CorrectedGyroSensor.h"
#include "GravitySensor.h"
#include "LinearAccelerationSensor.h"
#include "OrientationSensor.h"
#include "RotationVectorSensor.h"
#include "SensorFusion.h"
#include "RpcSensorService.h"

#include "time.h"
#include <sys/time.h>

namespace android {
// ---------------------------------------------------------------------------

#define TCP_NODELAY   1 /* Turn off Nagle's algorithm. */

/*
 * Notes:
 *
 * - what about a gyro-corrected magnetic-field sensor?
 * - run mag sensor from time to time to force calibration
 * - gravity sensor length is wrong (=> drift in linear-acc sensor)
 *
 */
#ifdef CPU_TIME
extern clock_t requestStartClock, requestSendClock, responseStartClock, responseSendClock, reqResGetStart, reqResFinishClock;
#else
extern struct timeval requestStartClock, requestSendClock, responseStartClock, responseSendClock, reqResGetStart, reqResFinishClock;
#endif

const char* RpcSensorService::WAKE_LOCK_NAME = "SensorService";

RpcSensorService::RpcSensorService()
{
}

void RpcSensorService::onFirstRef()
{
    // todo: check if the sensor supports batch operation
    ALOGD("nuSensorService starting...");
    mSocketBufferSize = SOCKET_BUFFER_SIZE_NON_BATCHED;
    FILE *fp = fopen("/proc/sys/net/core/wmem_max", "r");
    char line[128];
    if (fp != NULL && fgets(line, sizeof(line), fp) != NULL) {
        line[sizeof(line) - 1] = '\0';
        sscanf(line, "%u", &mSocketBufferSize);
        if (mSocketBufferSize > MAX_SOCKET_BUFFER_SIZE_BATCHED) {
            mSocketBufferSize = MAX_SOCKET_BUFFER_SIZE_BATCHED;
        }
    }
    ALOGD("Max socket buffer size %u", mSocketBufferSize);
    if (fp) {
        fclose(fp);
    }
}

bool RpcSensorService::threadLoop()
{
    ALOGD("nuSensorService thread starting...");
    // do nothing
    return false;
}

RpcSensorService::~RpcSensorService()
{
}

Vector<Sensor> RpcSensorService::getSensorList()
{
    struct timeval methStartTime;
    gettimeofday(&methStartTime, NULL);
#ifdef CPU_TIME
    requestStartClock = clock();
#else
    gettimeofday(&requestStartClock, NULL);
#endif
    RpcRequest* request = new RpcRequest();
    request->serviceId = 1;
    request->methodId = 1;
    request->socketFd = RpcUtilInst.rpcclient->socketFd;
    request->argsSize = 0;
    RpcResponse* response = RpcUtilInst.rpcclient->doRpc(request);
    
    Vector<Sensor> sensorList;
    while(!fifoEmpty(response->ret)) {
        Sensor sensor;
        size_t len;
        fifoReadBuffer(response->ret, (char*) &len, sizeof(len));
        char nameBuf[len];
        fifoReadBuffer(response->ret, nameBuf, len);
        String8 name;
        name.setTo(nameBuf, len);
        sensor.setName(name);

        fifoReadBuffer(response->ret, (char*) &len, sizeof(len));
        char vendorBuf[len];
        fifoReadBuffer(response->ret, vendorBuf, len);
        String8 vendor;
        vendor.setTo(vendorBuf, len);
        sensor.setVendor(vendor);
        
        int32_t version;
        fifoReadBuffer(response->ret, (char*) &version, sizeof(version));
        sensor.setVersion(version);
        int32_t handle;
        fifoReadBuffer(response->ret, (char*) &handle, sizeof(handle));
        sensor.setHandle(handle);
        int32_t type;
        fifoReadBuffer(response->ret, (char*) &type, sizeof(type));
        sensor.setType(type);
        float   minValue;
        fifoReadBuffer(response->ret, (char*) &minValue, sizeof(minValue));
        sensor.setMinValue(minValue);
        float   maxValue;
        fifoReadBuffer(response->ret, (char*) &maxValue, sizeof(maxValue));
        sensor.setMaxValue(maxValue);
        float   resolution;
        fifoReadBuffer(response->ret, (char*) &resolution, sizeof(resolution));
        sensor.setResolution(resolution);
        float   power;
        fifoReadBuffer(response->ret, (char*) &power, sizeof(power));
        sensor.setPowerUsage(power);
        int32_t minDelay;
        fifoReadBuffer(response->ret, (char*) &minDelay, sizeof(minDelay));
        sensor.setMinDelay(minDelay);
        int32_t fifoReservedEventCount;
        fifoReadBuffer(response->ret, (char*) &fifoReservedEventCount, sizeof(fifoReservedEventCount));
        sensor.setFifoReservedEventCount(fifoReservedEventCount);
        int32_t fifoMaxEventCount;
        fifoReadBuffer(response->ret, (char*) &fifoMaxEventCount, sizeof(fifoMaxEventCount));
        sensor.setFifoMaxEventCount(fifoMaxEventCount);

        fifoReadBuffer(response->ret, (char*) &len, sizeof(len));
        char stringTypeBuf[len];
        fifoReadBuffer(response->ret, stringTypeBuf, len);
        String8 stringType;
        stringType.setTo(stringTypeBuf, len);
        sensor.setStringType(stringType);

        fifoReadBuffer(response->ret, (char*) &len, sizeof(len));
        char reqPermissionBuf[len];
        fifoReadBuffer(response->ret, reqPermissionBuf, len);
        String8 requiredPermission;
        requiredPermission.setTo(reqPermissionBuf, len);
        sensor.setRequiredPermission(requiredPermission);
        
        int32_t maxDelay;
        fifoReadBuffer(response->ret, (char*) &maxDelay, sizeof(maxDelay));
        sensor.setMaxDelay(maxDelay);
        int32_t flags;
        fifoReadBuffer(response->ret, (char*) &flags, sizeof(flags));
        sensor.setFlags(flags);
        
        sensorList.add(sensor);
    }
    
    //ALOGE("rpc sensor service get remote sensor count is: %d", sensorList.size());
    
    fifoDestroy(response->ret);
    delete response;
    
#ifdef CPU_TIME
    reqResFinishClock = clock();
#else
    gettimeofday(&reqResFinishClock, NULL);
#endif
    struct timeval methEndTime;
    gettimeofday(&methEndTime, NULL);
    long timeUsed = (methEndTime.tv_sec - methStartTime.tv_sec) * 1000000 + methEndTime.tv_usec - methStartTime.tv_usec;
#ifdef CPU_TIME
    ALOGE("rpc sensor service experiment get sensor list: %f, method time: %f, total method time: %ld",  ((double (reqResFinishClock - reqResGetStart)) / CLOCKS_PER_SEC) * 1000000,  ((double (reqResFinishClock - requestStartClock)) / CLOCKS_PER_SEC) * 1000000, timeUsed);
#else
    ALOGE("rpc sensor service experiment get sensor list: %ld, method time: %ld, total method time: %ld",  (reqResFinishClock.tv_sec - reqResGetStart.tv_sec) * 1000000 + reqResFinishClock.tv_usec - reqResGetStart.tv_usec,  (reqResFinishClock.tv_sec - requestStartClock.tv_sec) * 1000000 + reqResFinishClock.tv_usec - requestStartClock.tv_usec, timeUsed);
#endif
    return sensorList;
}

sp<ISensorEventConnection> RpcSensorService::createSensorEventConnection()
{
    struct timeval methStartTime;
    gettimeofday(&methStartTime, NULL);
#ifdef CPU_TIME
    requestStartClock = clock();
#else
    gettimeofday(&requestStartClock, NULL);
#endif
    // create the network socket to remote sensor service server
    int receiveFd = socket(AF_INET, SOCK_STREAM, 0);
    if(receiveFd == -1) {
        ALOGE("rpc sensor service create server socket failed! %s", strerror(errno));
        return NULL;
    }
    union {
        struct sockaddr_in addrin;
        struct sockaddr addr;
    } addr;
    addr.addrin.sin_family = AF_INET;
    addr.addrin.sin_port = htons(RpcUtilInst.sensorChannelPort);
    inet_aton(RpcUtilInst.serverAddr, &addr.addrin.sin_addr);
    if(connect(receiveFd, &addr.addr, sizeof(struct sockaddr))) {
        if(receiveFd != -1) {
            ALOGE("rpc sensor service connect to the server failed! %s", strerror(errno));
            close(receiveFd);
            return NULL;
        }
        int nodelay = 1;
        setsockopt(receiveFd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
    }
    // read the sender fd
    int sendFd, len;
    size_t start = 0;
    do {
        len = ::read(receiveFd, (char*) &sendFd + start, sizeof(sendFd) - start);
        if(len < 0 && errno != EINTR && errno != EAGAIN) {
            close(receiveFd);
            ALOGE("rpc sensor service reading server sending fd failed! %s", strerror(errno));
            return NULL;
        }
        if(len > 0) {
            start += len;
        }
    } while(start < sizeof(sendFd));
    
#ifdef CPU_TIME
    clock_t connFinishClock = clock();
#else
    struct timeval connFinishClock;
    gettimeofday(&connFinishClock, NULL);
#endif
            
    // create sensor event connection in rpc server
    RpcRequest* request = new RpcRequest();
    request->serviceId = 1;
    request->methodId = 2;
    request->socketFd = RpcUtilInst.rpcclient->socketFd;
    request->argsSize = 0;
    request->args = fifoCreate();
    uid_t uid = IPCThreadState::self()->getCallingUid();
    fifoPushData(request->args, (char*) &uid, sizeof(uid));
    request->argsSize += sizeof(uid);
    fifoPushData(request->args, (char*) &sendFd, sizeof(sendFd));
    request->argsSize += sizeof(sendFd);
    RpcResponse* response = RpcUtilInst.rpcclient->doRpc(request);
    
    int remoteServiceId;
    fifoReadBuffer(response->ret, (char*) &remoteServiceId, sizeof(remoteServiceId));
    
    fifoDestroy(response->ret);
    delete response;
    
    sp<ISensorEventConnection> result(new RpcSensorService::RpcSensorEventConnectionProxy(this, remoteServiceId, sendFd, receiveFd));
    //ALOGE("rpc sensor service connection creation succeed with id: %d, uid is: %d, sending fd: %d, receive fd: %d", remoteServiceId, uid, sendFd, receiveFd);
    
#ifdef CPU_TIME
    reqResFinishClock = clock();
#else
    gettimeofday(&reqResFinishClock, NULL);
#endif
    struct timeval methEndTime;
    gettimeofday(&methEndTime, NULL);
    long timeUsed = (methEndTime.tv_sec - methStartTime.tv_sec) * 1000000 + methEndTime.tv_usec - methStartTime.tv_usec;
#ifdef CPU_TIME
    ALOGE("rpc sensor service experiment create sensor event connection: %f, method time: %f, channel connection time: %f, total method time: %ld",  ((double (reqResFinishClock - reqResGetStart)) / CLOCKS_PER_SEC) * 1000000,  ((double (reqResFinishClock - requestStartClock)) / CLOCKS_PER_SEC) * 1000000,  ((double (connFinishClock - requestStartClock)) / CLOCKS_PER_SEC) * 1000000, timeUsed);
#else
    ALOGE("rpc sensor service experiment create sensor event connection: %ld, method time: %ld, channel connection time: %ld, total method time: %ld",  (reqResFinishClock.tv_sec - reqResGetStart.tv_sec) * 1000000 + reqResFinishClock.tv_usec - reqResGetStart.tv_usec,  (reqResFinishClock.tv_sec - requestStartClock.tv_sec) * 1000000 + reqResFinishClock.tv_usec - requestStartClock.tv_usec,  (connFinishClock.tv_sec - requestStartClock.tv_sec) * 1000000 + connFinishClock.tv_usec - requestStartClock.tv_usec, timeUsed);
#endif
    return result;
}

RpcSensorService::RpcSensorEventConnectionProxy::RpcSensorEventConnectionProxy(
        const sp<RpcSensorService>& service, int remoteServiceId, int sendFd, int receiveFd)
    : mService(service), mRemoteServiceId(remoteServiceId)
{
    mChannel = new RpcBitTube(-1, receiveFd, service->mSocketBufferSize);
    // todo: handle channel creation
}

/* todo: clean up connection
RpcSensorService::RpcSensorService::~RpcSensorService()
{
    ALOGD_IF(DEBUG_CONNECTIONS, "~SensorEventConnection(%p)", this);
    mService->cleanupConnection(this);
}*/

RpcSensorService::RpcSensorEventConnectionProxy::~RpcSensorEventConnectionProxy()
{
#ifdef CPU_TIME
    requestStartClock = clock();
#else
    gettimeofday(&requestStartClock, NULL);
#endif
    //ALOGE("rpc sensor service rpc event connection proxy defactor");
    // destroy connection in rpc server
    RpcRequest* request = new RpcRequest();
    request->serviceId = mRemoteServiceId;
    request->methodId = 4;
    request->socketFd = RpcUtilInst.rpcclient->socketFd;
    request->argsSize = 0;
    RpcResponse* response = RpcUtilInst.rpcclient->doRpc(request);
    
    fifoDestroy(response->ret);
    delete response;
        
#ifdef CPU_TIME
    reqResFinishClock = clock();
#else
    gettimeofday(&reqResFinishClock, NULL);
#endif
#ifdef CPU_TIME
    ALOGE("rpc sensor service experiment defactoring connection: %f, method time: %f", ((double (reqResFinishClock - reqResGetStart)) / CLOCKS_PER_SEC) * 1000000,  ((double (reqResFinishClock - requestStartClock)) / CLOCKS_PER_SEC) * 1000000);
#else
    ALOGE("rpc sensor service experiment defactoring connection: %ld, method time: %ld", (reqResFinishClock.tv_sec - reqResGetStart.tv_sec) * 1000000 + reqResFinishClock.tv_usec - reqResGetStart.tv_usec,  (reqResFinishClock.tv_sec - requestStartClock.tv_sec) * 1000000 + reqResFinishClock.tv_usec - requestStartClock.tv_usec);
#endif
}

void RpcSensorService::RpcSensorEventConnectionProxy::onFirstRef()
{
}

sp<BitTube> RpcSensorService::RpcSensorEventConnectionProxy::getSensorChannel() const
{
    return mChannel;
}

status_t RpcSensorService::RpcSensorEventConnectionProxy::enableDisable(
        int handle, bool enabled, nsecs_t samplingPeriodNs, nsecs_t maxBatchReportLatencyNs,
        int reservedFlags)
{
    struct timeval methStartTime;
    gettimeofday(&methStartTime, NULL);
#ifdef CPU_TIME
    requestStartClock = clock();
#else
    gettimeofday(&requestStartClock, NULL);
#endif
    RpcRequest* request = new RpcRequest();
    request->serviceId = mRemoteServiceId;
    request->methodId = 1;
    request->socketFd = RpcUtilInst.rpcclient->socketFd;
    request->argsSize = 0;
    request->args = fifoCreate();
    fifoPushData(request->args, (char*) &handle, sizeof(handle));
    request->argsSize += sizeof(handle);
    fifoPushData(request->args, (char*) &enabled, sizeof(enabled));
    request->argsSize += sizeof(enabled);
    fifoPushData(request->args, (char*) &samplingPeriodNs, sizeof(samplingPeriodNs));
    request->argsSize += sizeof(samplingPeriodNs);
    fifoPushData(request->args, (char*) &maxBatchReportLatencyNs, sizeof(maxBatchReportLatencyNs));
    request->argsSize += sizeof(maxBatchReportLatencyNs);
    fifoPushData(request->args, (char*) &reservedFlags, sizeof(reservedFlags));
    request->argsSize += sizeof(reservedFlags);
    RpcResponse* response = RpcUtilInst.rpcclient->doRpc(request);
    status_t err;
    fifoReadBuffer(response->ret, (char*) &err, sizeof(err));
    
    fifoDestroy(response->ret);
    delete response;
    
    //ALOGE("rpc sensor service finish enabling and disabling the sensor handle: %d, service id: %d, sampling: %lld, batch: %ld", handle, mRemoteServiceId, samplingPeriodNs, maxBatchReportLatencyNs);
    
#ifdef CPU_TIME
    reqResFinishClock = clock();
#else
    gettimeofday(&reqResFinishClock, NULL);
#endif
    struct timeval methEndTime;
    gettimeofday(&methEndTime, NULL);
    long timeUsed = (methEndTime.tv_sec - methStartTime.tv_sec) * 1000000 + methEndTime.tv_usec - methStartTime.tv_usec;
#ifdef CPU_TIME
    ALOGE("rpc sensor service experiment enable disable: %f, method time: %f, total method time: %ld", ((double (reqResFinishClock - reqResGetStart)) / CLOCKS_PER_SEC) * 1000000,  ((double (reqResFinishClock - requestStartClock)) / CLOCKS_PER_SEC) * 1000000, timeUsed);
#else
    ALOGE("rpc sensor service experiment enable disable: %ld, method time: %ld, total method time: %ld", (reqResFinishClock.tv_sec - reqResGetStart.tv_sec) * 1000000 + reqResFinishClock.tv_usec - reqResGetStart.tv_usec,  (reqResFinishClock.tv_sec - requestStartClock.tv_sec) * 1000000 + reqResFinishClock.tv_usec - requestStartClock.tv_usec, timeUsed);
#endif
    
    return err;
}

status_t RpcSensorService::RpcSensorEventConnectionProxy::setEventRate(
        int handle, nsecs_t samplingPeriodNs)
{
    struct timeval methStartTime;
    gettimeofday(&methStartTime, NULL);
#ifdef CPU_TIME
    requestStartClock = clock();
#else
    gettimeofday(&requestStartClock, NULL);
#endif
    RpcRequest* request = new RpcRequest();
    request->serviceId = mRemoteServiceId;
    request->methodId = 2;
    request->socketFd = RpcUtilInst.rpcclient->socketFd;
    request->argsSize = 0;
    request->args = fifoCreate();
    fifoPushData(request->args, (char*) &handle, sizeof(handle));
    request->argsSize += sizeof(handle);
    fifoPushData(request->args, (char*) &samplingPeriodNs, sizeof(samplingPeriodNs));
    request->argsSize += sizeof(samplingPeriodNs);
    RpcResponse* response = RpcUtilInst.rpcclient->doRpc(request);
    status_t err;
    fifoReadBuffer(response->ret, (char*) &err, sizeof(err));
    
    fifoDestroy(response->ret);
    delete response;
    
    //ALOGE("rpc sensor service finish setting event rate: %d", handle);
    
#ifdef CPU_TIME
    reqResFinishClock = clock();
#else
    gettimeofday(&reqResFinishClock, NULL);
#endif
    struct timeval methEndTime;
    gettimeofday(&methEndTime, NULL);
    long timeUsed = (methEndTime.tv_sec - methStartTime.tv_sec) * 1000000 + methEndTime.tv_usec - methStartTime.tv_usec;
#ifdef CPU_TIME
    ALOGE("rpc sensor service experiment set event rate: %f, method time: %f, total method time: %ld",  ((double (reqResFinishClock - reqResGetStart)) / CLOCKS_PER_SEC) * 1000000,  ((double (reqResFinishClock - requestStartClock)) / CLOCKS_PER_SEC) * 1000000, timeUsed);
#else
    ALOGE("rpc sensor service experiment set event rate: %ld, method time: %ld, total method time: %ld",  (reqResFinishClock.tv_sec - reqResGetStart.tv_sec) * 1000000 + reqResFinishClock.tv_usec - reqResGetStart.tv_usec,  (reqResFinishClock.tv_sec - requestStartClock.tv_sec) * 1000000 + reqResFinishClock.tv_usec - requestStartClock.tv_usec, timeUsed);
#endif
    
    return err;
}

status_t  RpcSensorService::RpcSensorEventConnectionProxy::flush() {
    struct timeval methStartTime;
    gettimeofday(&methStartTime, NULL);
#ifdef CPU_TIME
    requestStartClock = clock();
#else
    gettimeofday(&requestStartClock, NULL);
#endif
    RpcRequest* request = new RpcRequest();
    request->serviceId = mRemoteServiceId;
    request->methodId = 3;
    request->socketFd = RpcUtilInst.rpcclient->socketFd;
    request->argsSize = 0;
    RpcResponse* response = RpcUtilInst.rpcclient->doRpc(request);
    status_t err;
    fifoReadBuffer(response->ret, (char*) &err, sizeof(err));
    
    fifoDestroy(response->ret);
    delete response;
    
    //ALOGE("rpc sensor service finish flushing");
    
#ifdef CPU_TIME
    reqResFinishClock = clock();
#else
    gettimeofday(&reqResFinishClock, NULL);
#endif
    struct timeval methEndTime;
    gettimeofday(&methEndTime, NULL);
    long timeUsed = (methEndTime.tv_sec - methStartTime.tv_sec) * 1000000 + methEndTime.tv_usec - methStartTime.tv_usec;
#ifdef CPU_TIME
    ALOGE("rpc sensor service experiment flush: %f, method time: %f, total method time: %ld",  ((double (reqResFinishClock - reqResGetStart)) / CLOCKS_PER_SEC) * 1000000,  ((double (reqResFinishClock - requestStartClock)) / CLOCKS_PER_SEC) * 1000000, timeUsed);
#else
    ALOGE("rpc sensor service experiment flush: %ld, method time: %ld, total method time: %ld",  (reqResFinishClock.tv_sec - reqResGetStart.tv_sec) * 1000000 + reqResFinishClock.tv_usec - reqResGetStart.tv_usec,  (reqResFinishClock.tv_sec - requestStartClock.tv_sec) * 1000000 + reqResFinishClock.tv_usec - requestStartClock.tv_usec, timeUsed);
#endif
    
    return err;
}

// ---------------------------------------------------------------------------
}; // namespace android

