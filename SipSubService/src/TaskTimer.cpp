#include "TaskTimer.h"
#include "ECThread.h"
#include "Common.h"
#include "GlobalCtl.h"
using namespace EC;

//该类设置了一个时间间隔，启动一个线程，线程会每隔设定的描述调用一次
//用户注册的回调函数，知道调用stop（）停止定时器


TaskTimer::TaskTimer(int timeSecond)
{
    m_timeSecond = timeSecond;  //定时器触发间隔，单位秒
    m_timerFun = NULL;  //回调函数指针，初始化为空
    m_funParam = NULL;//回调函数参数，初始化为空
    m_timerStop = false;//定时器停止标志，false表示未停止
}

TaskTimer::~TaskTimer()
{
    //销毁时调用stop停止定时器线程
    stop();
}

void TaskTimer::start()
{
    //启动定时器线程
    pthread_t pid;
    int ret = EC::ECThread::createThread(TaskTimer::timer,(void*)this,pid);
    if(ret != 0)
    {
        LOG(ERROR)<<"create thread failed";
    }
}
void TaskTimer::stop()
{
    m_timerStop = true;
}
void TaskTimer::setTimerFun(timerCallBack fun,void* param)
{
    //绑定用户提供的回调函数和参数

    m_timerFun = fun;//注册回调函数
    m_funParam = param;//注册回调参数
}


//实现设定时间间隔周期性调用回调函数的功能
void* TaskTimer::timer(void* context)
{
    TaskTimer* pthis = (TaskTimer*)context;
    if(NULL == pthis)
    {
        return NULL;
    }

    //当前时间和上一次回调时间
    unsigned long curTm = 0;
    unsigned long lastTm = 0;

    while(!(pthis->m_timerStop))
    {
        //获得当前系统时间，分秒和微妙两部分
        struct timeval current;
        gettimeofday(&current,NULL);
        //计算转换成猫喵单位时间戳，方便时间间隔计算
        curTm = current.tv_sec*1000 + current.tv_usec / 1000;

        //判断距离上一次回调执行时间是否已经达到或超过设定的定时秒数
        if((curTm-lastTm) >= ((pthis->m_timeSecond)*1000))
        {
            lastTm = curTm;
            if(pthis->m_timerFun != NULL)
            {
                //定义了一个结构体变量，用于描述当前线程的信息
                pj_thread_desc desc;
                //将当前线程注册到pjsip的线程管理系统
                pjcall_thread_register(desc);
                pthis->m_timerFun(pthis->m_funParam);
            }
        }
        else
        {
            usleep(1000*1000);
            continue;
        }
    }

    return NULL;

}