#ifndef RPC_CONTROL_H
#define RPC_CONTROL_H

#include "common.h"
#include "share_rpc.h"

namespace android {

// ---------------------------------------------------------------------------

void message_loop(RpcEndpoint* endpoint);

void addOutRpcMsg(RpcMessage* rpcMsg);

void setupConnection(int socketFd);

void controlInit();

void acquireLock(u8 idxId);

void releaseLock(u8 idxId);

// ---------------------------------------------------------------------------
}; // namespace android

#endif
