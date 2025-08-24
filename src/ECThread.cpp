#include "ECThread.h"
using namespace EC;


//创建分离线程

//线程一旦结束，系统会自动回收它的资源，无需其他线程（如主线程）使用 pthread_join 来等待或回收它。
int ECThread::createThread(ECThreadFunc startRoutine,void* args,pthread_t& id)
{
    int ret = 0;
    //初始化线程属性对象
    pthread_attr_t threadAttr;
    pthread_attr_init(&threadAttr);
    do
    {
        //设置线程属性对象为分离状态
        ret = pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);
        if(ret != 0)
            break;
            
        // 创建线程
        ret = pthread_create(&id,&threadAttr,startRoutine,args);
        if(ret != 0)
            break;
    } while (0);
    pthread_attr_destroy(&threadAttr);
    if(ret != 0)
       ret = -1;


    return ret;
    
}

// 将当前线程编程分离线程
int ECThread::detachSelf()
{
    int ret = pthread_detach(pthread_self());
    if(ret != 0)
        return -1;
    return 0;
}

//退出当前线程
void ECThread::exitSelf(void* rval)
{
    pthread_exit(rval);
}
//等待线程结束
int waitThread(const pthread_t& id,void** rval)
{
    int ret = pthread_join(id,rval);
    if(ret != 0)
        return -1;
    return 0;
}

//终止指定线程
int terminateThread(const pthread_t& id)
{
    int ret = pthread_cancel(id);
    if(ret != 0)
        return -1;
    return 0;
}