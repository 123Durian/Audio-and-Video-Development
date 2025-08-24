#ifndef _SIPREGISTER_H
#define _SIPREGISTER_H
#include "SipTaskBase.h"
#include "TaskTimer.h"


//继承基类
class SipRegister : public SipTaskBase
{
    public:
        SipRegister();
        ~SipRegister();


        //重新并实现基类的虚函数
        virtual pj_status_t run(pjsip_rx_data *rdata);
        pj_status_t RegisterRequestMessage(pjsip_rx_data *rdata);
        pj_status_t dealWithAuthorRegister(pjsip_rx_data *rdata);
        pj_status_t dealWithRegister(pjsip_rx_data *rdata);
        void registerServiceStart();
        static void RegisterCheckProc(void* param);
    private:
        TaskTimer* m_regTimer;

};

#endif