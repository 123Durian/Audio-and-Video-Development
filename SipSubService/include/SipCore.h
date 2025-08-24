#ifndef _SIPCORE_H
#define _SIPCORE_H
#include "SipTaskBase.h"


//在线程中传递和管理 SIP 消息处理任务的参数
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

        static void* dealTaskThread(void* arg);

    private:
        pjsip_endpoint* m_endpt;
        pj_caching_pool m_cachingPool;
        pjmedia_endpt* m_mediaEndpt;
        pj_pool_t* m_pool;
};
#endif