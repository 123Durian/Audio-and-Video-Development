#ifndef _TASKTIMER_H
#define _TASKTIMER_H
#include <unistd.h>
#include <sys/time.h>

//定时器，本质是定时开启线程执行任务
typedef void (*timerCallBack)(void* param);
class TaskTimer
{
    public:
        TaskTimer(int timeSecond);
        ~TaskTimer();

        void start();
        void stop();

        void setTimerFun(timerCallBack fun,void* param);

        static void* timer(void* context);
    private:
        timerCallBack m_timerFun;
        void* m_funParam;
        int m_timeSecond;
        bool m_timerStop;
};

#endif