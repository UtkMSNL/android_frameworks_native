#ifndef RPC_THREADPOOL_H
#define RPC_THREADPOOL_H

#include <pthread.h>
#include <semaphore.h>
#include <iostream>
#include <vector>

namespace android {
// ---------------------------------------------------------------------------

/*
WorkerThread class
This class needs to be sobclassed by the user.
*/
class WorkerThread
{
public:
    unsigned virtual executeThis() {
        return 0;
    }
    virtual ~WorkerThread(){}
};
/*
ThreadPool class manages all the ThreadPool related activities. This includes keeping track of idle threads and synchronizations between all threads.
*/
class ThreadPool
{
public:
    ThreadPool();
    ThreadPool(int maxThreadsTemp);
    virtual ~ThreadPool();
    void destroyPool(int maxPollSecs);
    bool assignWork(WorkerThread *worker);
    bool fetchWork(WorkerThread **worker);
    void initializeThreads();
    static void *threadExecute(void *param); // pthread_create() must call a static function
    static pthread_mutex_t mutexSync;
    static pthread_mutex_t mutexWorkCompletion; // Mutex for operating the number of work completion
private:
    int maxThreads;
    pthread_cond_t  condCrit;
    sem_t availableWork;
    sem_t availableThreads;
    std::vector<WorkerThread *> workerQueue;
    int topIndex;
    int bottomIndex;
    int incompleteWork;
    int queueSize;
};

// ---------------------------------------------------------------------------
}; // namespace android

#endif
