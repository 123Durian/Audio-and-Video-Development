#ifndef _GETCATALOG_H
#define _GETCATALOG_H



//上级目录数请求
#include "SipTaskBase.h"
#include "ThreadPool.h"

class GetCatalog: public ThreadTask
{
    public:
        GetCatalog();
        ~GetCatalog();
        virtual void run();

        //用来发起目录数请求
        void DirectoryGetPro(void* param);
};

#endif