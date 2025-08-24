#include "ThreadPool.h"

//get

//pthread_mutex_t ThreadPool::m_queueLock = PTHREAD_MUTEX_INITIALIZER;
//用于声明两个静态成员变量
pthread_mutex_t ThreadPool::m_queueLock;
queue<ThreadTask*> ThreadPool::m_taskQueue;


//初始化互斥锁和信号量
ThreadPool::ThreadPool()
{
    pthread_mutex_init(&m_queueLock,NULL);
    //初始化一个 POSIX 信号量
    sem_init(&m_signalSem,0,0);
}
ThreadPool::~ThreadPool()
{
    pthread_mutex_destroy(&m_queueLock);
    sem_destroy(&m_signalSem);
}

//创建线程池，就是按照按照个数循环创建线程
int ThreadPool::createThreadPool(int threadCount)
{
    //创建线程
    if(threadCount <= 0)
    {
        LOG(ERROR)<<"thread count error";
        return -1;
    }

    for(int i=0;i<threadCount;++i)
    {
        pthread_t pid;
        if(EC::ECThread::createThread(ThreadPool::mainThread,(void*)this,pid) <0)
        {
            LOG(ERROR)<<"create thread error";
        }

        LOG(INFO)<<"thread:"<<pid<<" was created";
    }
}


//不断等待任务，一旦有任务就取出并执行，之后继续等待下一个任务。
//从队列头部取出该任务，并执行该任务。
void* ThreadPool::mainThread(void* argc)
{
    //将参数转换为线程池类型
    ThreadPool* pthis = (ThreadPool*)argc;

    do
    {

        //等待，不让其无限循环
        int ret = pthis->waitTask();
        if(ret == 0)
        {
            ThreadTask* task = NULL;
            //给队列加锁
            pthread_mutex_lock(&m_queueLock);
            if(m_taskQueue.size() > 0)
            {
                task = m_taskQueue.front();
                m_taskQueue.pop();
            }
            pthread_mutex_unlock(&m_queueLock);

            if(task)
            {
                task->run();
                delete task;
            }
        }
        
    } while (true);
    
}


//使工作线程阻塞等待任务到来
int ThreadPool::waitTask()
{
    int ret = 0;
    //如果信号量值大于 0，sem_wait 会将信号量减 1 并立即返回
    ret = sem_wait(&m_signalSem);
    if(ret != 0)
    {
        LOG(ERROR)<<"the api exec error";
    }
    return ret;
}


//生产者线程
int ThreadPool::postTask(ThreadTask* task)
{
    if(task)
    {
        pthread_mutex_lock(&m_queueLock);
        m_taskQueue.push(task);
        pthread_mutex_unlock(&m_queueLock);
        //发出“有新任务”的信号
        sem_post(&m_signalSem);
    }
}