#include <rpc/rpc_profiling.h>

namespace android {

// ---------------------------------------------------------------------------

// methods allowed to be profiling
std::set<u8> profilingMethods;

// recording the time used for remote execution in the client
std::map<u4, u8> clientRemoteTimes;

// recording the timestamp when the client sends the rpc request data
std::map<u4, timeval*> clientRemoteStartTimes;

// recording the timestamp when the server starts to its processing
std::map<u4, timeval*> serverRpcStartTime;

// recording the time for the actual method execution in the server
std::map<u4, u8> serverMethExecTimes;

// ---------------------------------------------------------------------------

}; // namespace android

