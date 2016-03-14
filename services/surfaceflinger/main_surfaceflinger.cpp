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

#if defined(HAVE_PTHREADS)
#include <sys/resource.h>
#endif

#include <cutils/sched_policy.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include "SurfaceFlinger.h"

#include <rpc/share_rpc.h>
#include "RpcSurfaceFlingerServer.h"

using namespace android;

static void* initSurfaceRpc(void* args) {
    while (!isNetworkReady()) {
        ALOGE("rpc surface flinger the network is still not available");
        sleep(1);
    }
    initSurfaceRpcEndpoint();
    
    // rpc server start to register the rpc functions it can serve
    if (SurfaceRpcUtilInst.isShareEnabled && SurfaceRpcUtilInst.isServer) {
        // get the service handle and register it to rpc module
        sp<SurfaceFlinger>* gSurfaceFlinger = new sp<SurfaceFlinger>((SurfaceFlinger*) SurfaceRpcUtilInst.surfaceFlinger);
        SurfaceRpcUtilInst.idToObjMap[SurfaceRpcUtilInst.SURFACE_SERVICE_ID] = gSurfaceFlinger;
        // register server method
        registerSurfaceFlingerServer();
    }
    
    return NULL;
} 

int main(int, char**) {
    // When SF is launched in its own process, limit the number of
    // binder threads to 4.
    ProcessState::self()->setThreadPoolMaxThreadCount(4);

    // start the thread pool
    sp<ProcessState> ps(ProcessState::self());
    ps->startThreadPool();

    // instantiate surfaceflinger
    sp<SurfaceFlinger> flinger = new SurfaceFlinger();
    
    SurfaceRpcUtilInst.surfaceFlinger = flinger->get();

#if defined(HAVE_PTHREADS)
    setpriority(PRIO_PROCESS, 0, PRIORITY_URGENT_DISPLAY);
#endif
    set_sched_policy(0, SP_FOREGROUND);

    pthread_t rpcInitThread;
    pthread_create(&rpcInitThread, NULL, initSurfaceRpc, NULL);
    
    // initialize before clients can connect
    flinger->init();

    // publish surface flinger
    sp<IServiceManager> sm(defaultServiceManager());
    sm->addService(String16(SurfaceFlinger::getServiceName()), flinger, false);

    // run in this thread
    flinger->run();

    return 0;
}
