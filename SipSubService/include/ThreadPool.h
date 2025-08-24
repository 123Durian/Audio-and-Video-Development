#ifndef _THREADPOOL_H
#define _THREADPOOL_H
#include "Common.h"
#include "ECThread.h"
#include <queue>
#include <unistd.h>
#include <semaphore.h>
using namespace EC;

class ThreadTask
{
    public:
        ThreadTask(){}
        virtual ~ThreadTask(){}
        virtual void run()=0;
};

class ThreadPool
{
    public:
        ThreadPool();
        ~ThreadPool();

        int createThreadPool(int threadCount);
        int waitTask();
        int postTask(ThreadTask* task);

        static void* mainThread(void* argc);
        static queue<ThreadTask*> m_taskQueue;
        static pthread_mutex_t m_queueLock;
    private:
        sem_t m_signalSem;
};
#endif