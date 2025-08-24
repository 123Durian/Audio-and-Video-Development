#ifndef _SIPDIRECTORY_H
#define _SIPDIRECTORY_H
#include "SipTaskBase.h"
class SipDirectory : public SipTaskBase
{
    public:
        SipDirectory(tinyxml2::XMLElement* root);
        ~SipDirectory();
        virtual pj_status_t run(pjsip_rx_data *rdata);
        void SaveDir(int& status_code);
    private:
        tinyxml2::XMLElement* m_pRootElement;

        static Json::Value m_jsonIn;
        static int m_jsonIndex;
};
#endif