#ifndef _SIPTASKBASE_H
#define _SIPTASKBASE_H
#include <pjlib-util.h>
#include <pjmedia.h>
#include <pjsip.h>
#include <pjsip_ua.h>
#include <pjsip/sip_auth.h>
#include <pjlib.h>
#include "Common.h"
#include <sys/sysinfo.h>

//根据请求的method来处理响应的业务
//使用多态，这个类是基类
class SipTaskBase
{
    public:
        SipTaskBase(){}
        virtual ~SipTaskBase()
        {
            LOG(INFO)<<"~SipTaskBase";
        }

        //纯虚函数
        virtual pj_status_t run(pjsip_rx_data *rdata) = 0;

        static tinyxml2::XMLElement* parseXmlData(pjsip_msg* msg,string& rootType,const string xmlkey,string& xmlvalue);
    protected:
        string parseFromId(pjsip_msg* msg);
};
#endif