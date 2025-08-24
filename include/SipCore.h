#ifndef _SIPCORE_H
#define _SIPCORE_H
#include "SipTaskBase.h"


//用于封装线程处理所需要的两个主要函数
typedef struct _threadParam
{
    _threadParam()
    {
        base = NULL;
        data = NULL;
    }
    ~_threadParam()
    {
        if(base)
        {
            delete base;
            base = NULL;
        }
        if(data)
        {
            pjsip_rx_data_free_cloned(data);
            base = NULL;
        }
    }
    SipTaskBase* base;
    pjsip_rx_data* data;
}threadParam;

class SipCore
{
    public:
        SipCore();
        ~SipCore();

        bool InitSip(int sipPort);

        pj_status_t init_transport_layer(int sipPort);

        pjsip_endpoint* GetEndPoint(){return m_endpt;}
        pj_pool_t* GetPool(){return m_pool;}
		void stopSip();
    public:
        static void* dealTaskThread(void* arg);

    private:
        pjsip_endpoint* m_endpt;
        pjmedia_endpt* m_mediaEndpt;
        pj_caching_pool m_cachingPool;
        pj_pool_t* m_pool;
};
#endif