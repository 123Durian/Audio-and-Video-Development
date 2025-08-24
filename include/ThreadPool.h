#ifndef _THREADPOOL_H
#define _THREADPOOL_H
#include "Common.h"
#include "ECThread.h"
#include <queue>
#include <unistd.h>
#include <semaphore.h>
#include "EventMsgHandle.h"
using namespace EC;

class ThreadTask
{
    public:
        ThreadTask(struct bufferevent* bev,void* arg = NULL)
            :m_bev(bev)
        {
            if(arg != NULL)
            {
                streamType = *(int*)arg;
            }
            
        }
        virtual ~ThreadTask(){}
        virtual void run()=0;
    protected:
        struct bufferevent* m_bev;
        //表示预览还是回放流进行区分
        int streamType;
};

class ThreadPool
{
    public:
        ThreadPool();
        ~ThreadPool();

        int createThreadPool(int threadCount);
        int waitTask();
        int postTask(ThreadTask* task);

        int waitInfo();
        int postInfo();

        static void* mainThread(void* argc);
        static queue<ThreadTask*> m_taskQueue;
        static pthread_mutex_t m_queueLock;
    private:
        sem_t m_signalSem;
        sem_t m_signalSem_Info;
};
#endif