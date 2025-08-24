#ifndef _SIPREGISTER_H
#define _SIPREGISTER_H
#include "GlobalCtl.h"
 #include "TaskTimer.h"
class SipRegister
{
    public:
        SipRegister();
        ~SipRegister();
        void registerServiceStart();
        static void RegisterProc(void* param);
        //新增接口用于发送注册请求
        int gbRegister(GlobalCtl::SupDomainInfo& node);
    private:
        TaskTimer* m_regTimer;
};

#endif