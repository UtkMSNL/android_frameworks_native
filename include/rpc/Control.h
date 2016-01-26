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

// ---------------------------------------------------------------------------
}; // namespace android

#endif
