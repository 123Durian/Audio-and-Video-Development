#ifndef _GLOBALCTL_H
#define _GLOBALCTL_H
#include "Common.h"
#include "SipLocalConfig.h"
#include "ThreadPool.h"
#include "SipCore.h"
#include "SipDef.h"


//单例类对象
class GlobalCtl;
#define  GBOJ(obj)  GlobalCtl::instance()->obj

//pjlib的线程注册
static pj_status_t pjcall_thread_register(pj_thread_desc& desc)
{
    pj_thread_t* tread = 0;
    if(!pj_thread_is_registered())
    {
        return pj_thread_register(NULL,desc,&tread);
    }
    return PJ_SUCCESS;
}
class GlobalCtl
{
    public:
        static GlobalCtl* instance();
        bool init(void* param);

        SipLocalConfig* gConfig = NULL;
        ThreadPool* gThPool = NULL;
        SipCore* gSipServer = NULL;

        
        typedef struct _SupDomainInfo
        {
            _SupDomainInfo()
            {
                sipId = "";
                addrIp = "";
                sipPort = 0;
                protocal = 0;//0是udp
                registered = 0;
                expires = 0;
                usr = "";
                pwd = "";
                isAuth = false;
                realm = "";
            }
            string sipId;
            string addrIp;
            int sipPort;
            int protocal;
            int registered;
            int expires;
            bool isAuth;
            string usr;
            string pwd; 
            string realm;
        }SupDomainInfo;
        typedef list<SupDomainInfo> SUPDOMAININFOLIST;

        //list对外提供的接口
        SUPDOMAININFOLIST& getSupDomainInfoList()
        {
            return supDomainInfoList;
        }

        static void get_global_mutex()
        {
            pthread_mutex_lock(&globalLock);
        }

        static void free_global_mutex()
        {
            pthread_mutex_unlock(&globalLock);
        }

        static pthread_mutex_t globalLock;
        static DevTypeCode getSipDevInfo(string id);

        static bool gStopPool;
    private:
        GlobalCtl(void)
        {

        }
        ~GlobalCtl(void);
        GlobalCtl(const GlobalCtl& global);
        const GlobalCtl& operator=(const GlobalCtl& global);

        static GlobalCtl* m_pInstance;
        static SUPDOMAININFOLIST supDomainInfoList;
        
};

#endif