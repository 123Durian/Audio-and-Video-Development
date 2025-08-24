#ifndef _GETPLAMTINFO_H_
#define _GETPLAMTINFO_H_
#include "ThreadPool.h"

class GetPlamtInfo : public ThreadTask
{
    public:
        GetPlamtInfo(struct bufferevent* bev);
        ~GetPlamtInfo(){};

        virtual void run();
};
#endif