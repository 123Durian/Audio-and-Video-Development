#ifndef _ECTHREAD_H
#define _ECTHREAD_H
#include <pthread.h>
#include <string>
#include <sys/prctl.h>


//自定义一个命名空间，之后再使用命名空间中的代码时，都需要先声明此命名空间
//全部定义成static无需对外提供实例
namespace EC
{
    typedef void* (*ECThreadFunc)(void*);
    class ECThread
    {
        public:
            static int createThread(ECThreadFunc startRoutine,void* args,pthread_t& id);
            static int detachSelf();

            //退出当前线程
            static void exitSelf(void* rval);
            //等待指定的线程退出
            static int waitThread(const pthread_t& id);
            //向指定线程发出退出信号
            static int terminateThread(const pthread_t& id);
        private:
            ECThread(){}
            ~ECThread(){}
    };
}


#endif