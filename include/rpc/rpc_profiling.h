#ifndef RPC_RPC_PROFILING_H
#define RPC_RPC_PROFILING_H

#include "common.h"

#include <utils/Log.h>

#include <set>
#include <map>
#include "time.h" 

namespace android {
// ---------------------------------------------------------------------------

// methods allowed to be profiling
extern std::set<u8> profilingMethods;
// recording the time used for remote execution in the client
extern std::map<u4, u8> clientRemoteTimes;
// recording the timestamp when the client sends the rpc request data
extern std::map<u4, timeval*> clientRemoteStartTimes;
// recording the timestamp when the server starts to its processing
extern std::map<u4, timeval*> serverRpcStartTime;
// recording the time for the actual method execution in the server
extern std::map<u4, u8> serverMethExecTimes;

inline u8 getFuncId(u4 serviceId, u4 methodId)
{
    u8 funcId = serviceId;
    funcId <<= 4;
    funcId += methodId;
    
    return funcId;
}

inline bool isProfilingAllowed(u4 serviceId, u4 methodId)
{
    u8 funcId = getFuncId(serviceId, methodId);
    return profilingMethods.find(funcId) != profilingMethods.end();
}

#define CLIENT_METH_PROFILING_START(_serviceId, _methodId)     \
    u8 funcId = getFuncId(_serviceId, _methodId);                           \
    profilingMethods.insert(funcId);                                           \
    struct timeval start;                                                   \
    gettimeofday(&start, NULL);
    
#define CLIENT_METH_PROFILING_END(_seqNo)                                   \
    struct timeval finish;                                                  \
    gettimeofday(&finish, NULL);                                            \
    u8 totalTime = (finish.tv_sec - start.tv_sec) * 1000000 + finish.tv_usec - start.tv_usec;   \
    ALOGE("[rpc evaluation], rpc execution time, seqno: %d, total: %lld, remote: %lld", _seqNo, totalTime, clientRemoteTimes[_seqNo]);  \
    clientRemoteTimes.erase(_seqNo);
    
#define CLIENT_REMOTE_PROFILING_START(_seqNo, _serviceId, _methodId)       \
    if (isProfilingAllowed(_serviceId, _methodId)) {                \
        struct timeval* start = new timeval();                      \
        gettimeofday(start, NULL);                                  \
        clientRemoteStartTimes[_seqNo] = start;                     \
    }

#define CLIENT_REMOTE_PROFILING_END(_seqNo)                         \
    if (clientRemoteStartTimes.find(_seqNo) != clientRemoteStartTimes.end()) {    \
        struct timeval finish;                                      \
        gettimeofday(&finish, NULL);                                \
        struct timeval start = *clientRemoteStartTimes[_seqNo];     \
        u8 rtime = (finish.tv_sec - start.tv_sec) * 1000000 + finish.tv_usec - start.tv_usec;    \
        clientRemoteTimes[_seqNo] = rtime;                          \
        delete clientRemoteStartTimes[_seqNo];                      \
        clientRemoteStartTimes.erase(_seqNo);                       \
    }    
    
#define SERVER_RPC_PROFILING_START(_seqNo) \
    struct timeval* start = new timeval();  \
    gettimeofday(start, NULL);                                  \
    serverRpcStartTime[_seqNo] = start;
    
#define SERVER_RPC_PROFILING_END(_seqNo)                                \
    /* check if this method is in profiling */                          \
    if (serverMethExecTimes.find(_seqNo) != serverMethExecTimes.end()) {  \
        struct timeval finish;                                          \
        gettimeofday(&finish, NULL);                                    \
        struct timeval start = *serverRpcStartTime[_seqNo];             \
        u8 serverTime = (finish.tv_sec - start.tv_sec) * 1000000 + finish.tv_usec - start.tv_usec;    \
        ALOGE("[rpc evaluation], rpc server execution time, seqno: %d, total: %lld, method: %lld", _seqNo, serverTime, serverMethExecTimes[_seqNo]);    \
        serverMethExecTimes.erase(_seqNo);   \
    }                                                       \
    delete serverRpcStartTime[_seqNo];                      \
    serverRpcStartTime.erase(_seqNo);

#define SERVER_METH_PROFILING_START(_seqNo)                 \
    struct timeval start;                                   \
    gettimeofday(&start, NULL);                             \

#define SERVER_METH_PROFILING_END(_seqNo)                   \
    struct timeval finish;                                  \
    gettimeofday(&finish, NULL);                            \
    u8 execTime = (finish.tv_sec - start.tv_sec) * 1000000 + finish.tv_usec - start.tv_usec;   \
    serverMethExecTimes[_seqNo] = execTime;

#define COMPRESSION_PROFILING_START()                               \
    struct timeval compstart;                                       \
    gettimeofday(&compstart, NULL);
    
#define COMPRESSION_PROFILING_END(_seqNo, _operation)               \
    struct timeval compfinish;                                      \
    gettimeofday(&compfinish, NULL);                                \
    u8 compTotalTime = (compfinish.tv_sec - compstart.tv_sec) * 1000000 + compfinish.tv_usec - compstart.tv_usec;   \
    ALOGE("[rpc evaluation], rpc %s time, seqno is: %d, time is: %lld", _operation, _seqNo, compTotalTime);
    
// ---------------------------------------------------------------------------
}; // namespace android

#endif
