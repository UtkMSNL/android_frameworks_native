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

#ifndef ANDROID_GUI_RPC_SENSOR_CHANNEL_H
#define ANDROID_GUI_RPC_SENSOR_CHANNEL_H

#include <stdint.h>
#include <sys/types.h>

#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <cutils/log.h>
#include "BitTube.h"

namespace android {
// ----------------------------------------------------------------------------
void initBitTubeServer();

class RpcBitTube : public BitTube
{
public:

    // creates a BitTube with a default (4KB) send buffer
    RpcBitTube();

    // creates a BitTube with a a specified send and receive buffer size
    explicit RpcBitTube(int sendFd, int receiveFd, size_t bufsize);

    explicit RpcBitTube(const Parcel& data);
    virtual ~RpcBitTube();

    // check state after construction
    status_t initCheck() const;

    // get receive file-descriptor
    int getFd() const;

    // get the send file-descriptor.
    int getSendFd() const;
    
    // parcels this BitTube
    status_t writeToParcel(Parcel* reply) const;

protected:
    void init(size_t rcvbuf, size_t sndbuf);

    // send a message. The write is guaranteed to send the whole message or fail.
    virtual ssize_t write(void const* vaddr, size_t size);

    // receive a message. the passed buffer must be at least as large as the
    // write call used to send the message, excess data is silently discarded.
    virtual ssize_t read(void* vaddr, size_t size);
};

// ----------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_GUI_RPC_SENSOR_CHANNEL_H
