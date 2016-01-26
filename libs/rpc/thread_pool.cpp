#include <stdlib.h>
#include <unistd.h>
#include <rpc/thread_pool.h>

namespace android {
// ---------------------------------------------------------------------------

// The initialization of static class member must have the type and class declaration, and it must be done outside the class definition body
pthread_mutex_t ThreadPool::mutexSync = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ThreadPool::mutexWorkCompletion = PTHREAD_MUTEX_INITIALIZER;

ThreadPool::ThreadPool()
{
    ThreadPool(10);
}

ThreadPool::ThreadPool(int maxThreads)
{
    if (maxThreads < 1)  
        maxThreads = 1;
    pthread_mutex_lock(&mutexSync);
    this->maxThreads = maxThreads;
    this->queueSize = maxThreads;
    workerQueue.resize(maxThreads, NULL); // adjust the size of the worker queue
    topIndex = 0;
    bottomIndex = 0;
    incompleteWork = 0;
    sem_init(&availableWork, 0, 0); // semaphore used for the amount of work tasks in the queue
    sem_init(&availableThreads, 0, queueSize);  // semaphore used for the amount of available working thread
    pthread_mutex_unlock(&mutexSync);
}

// Thread initialization
void ThreadPool::initializeThreads()
{
    for(int i = 0; i < maxThreads; ++i)
    {
        pthread_t tempThread;
        pthread_create(&tempThread, NULL, ThreadPool::threadExecute, (void*)this );
    }
}

ThreadPool::~ThreadPool()
{
    workerQueue.clear();
}

void ThreadPool::destroyPool(int maxPollSecs = 2)
{
    while(incompleteWork > 0)
    {
        //cout << "Work is still incomplete=" << incompleteWork << endl;
        sleep(maxPollSecs);
    }
    std::cout << "All Done!! Wow! That was a lot of work!" << std::endl;
    sem_destroy(&availableWork);
    sem_destroy(&availableThreads);
    pthread_mutex_destroy(&mutexSync);
    pthread_mutex_destroy(&mutexWorkCompletion);
}

// Assign task to the top and notify that there are tasks to handle
bool ThreadPool::assignWork(WorkerThread *workerThread)
{
    pthread_mutex_lock(&mutexWorkCompletion);
    incompleteWork++;
    //cout << "assignWork...incomapleteWork=" << incompleteWork << endl;
    pthread_mutex_unlock(&mutexWorkCompletion);
    sem_wait(&availableThreads);
    pthread_mutex_lock(&mutexSync);
    workerQueue[topIndex] = workerThread;
    //cout << "Assigning Worker[" << workerThread->id << "] Address:[" << workerThread << "] to Queue index [" << topIndex << "]" << endl;
    if(queueSize !=1 )
        topIndex = (topIndex + 1) % (queueSize - 1);
    sem_post(&availableWork);
    pthread_mutex_unlock(&mutexSync);
    return true;
}

// It will be notified when there are available tasks and tasks will taken from bottom index
bool ThreadPool::fetchWork(WorkerThread **workerArg)
{
    sem_wait(&availableWork);
    pthread_mutex_lock(&mutexSync);
    WorkerThread * workerThread = workerQueue[bottomIndex];
    workerQueue[bottomIndex] = NULL;
    *workerArg = workerThread;
    if(queueSize != 1)
        bottomIndex = (bottomIndex + 1) % (queueSize - 1);
    sem_post(&availableThreads);
    pthread_mutex_unlock(&mutexSync);
    return true;
}

// The function executed by each thread, where the actual work "executeThis" will be overriden by subclass of WorkerThread
void *ThreadPool::threadExecute(void *param)
{
    WorkerThread *worker = NULL;
    while(((ThreadPool *)param)->fetchWork(&worker))
    {
        if(worker)
        {
            worker->executeThis();
            //cout << "worker[" << worker->id << "]\tdelete address: [" << worker << "]" << endl;
            delete worker;
            worker = NULL;
        }
        pthread_mutex_lock( &(((ThreadPool *)param)->mutexWorkCompletion) );
        //cout << "Thread " << pthread_self() << " has completed a Job !" << endl;
        ((ThreadPool *)param)->incompleteWork--;
        pthread_mutex_unlock( &(((ThreadPool *)param)->mutexWorkCompletion) );
    }
    return 0;
}


// ---------------------------------------------------------------------------
}; // namespace android
