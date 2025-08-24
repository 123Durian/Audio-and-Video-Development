// #ifndef _OPENSTREAM_H
// #define _OPENSTREAM_H
// #include "TaskTimer.h"
// #include "Common.h"


// //上级新增的类用于发送invite请求
// class OpenStream
// {
//     public:
//         OpenStream();
//         ~OpenStream();

//         void StreamServiceStart();
//         static void StreamGetProc(void* param);
// 		static void CheckSession(void* param);
//         //上级的bye接口
// 		static void StreamStop(string platformId, string devId);
//     private:
//         TaskTimer* m_pStreamTimer;
//         TaskTimer* m_pCheckSessionTimer;
// };

// #endif


#ifndef _OPENSTREAM_H
#define _OPENSTREAM_H
#include "TaskTimer.h"
#include "Common.h"
#include "ThreadPool.h"
class OpenStream : public ThreadTask
{
    public:
        OpenStream(struct bufferevent* bev,void* arg);
        ~OpenStream();
        virtual void run();
        //void StreamServiceStart();
        void StreamGetProc(void* param);
		//void CheckSession(void* param);
		void StreamStop(string platformId, string devId);
    private:
        TaskTimer* m_pStreamTimer;
        TaskTimer* m_pCheckSessionTimer;
};

#endif