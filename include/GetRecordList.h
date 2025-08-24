#ifndef _GETRECORDLIST_H
#define _GETRECORDLIST_H

#include "SipTaskBase.h"

class GetRecordList
{
    public:
        GetRecordList();
        ~GetRecordList();

        //定义一个接口用于发送
        void RecordInfoGetPro(void* param);
};

#endif