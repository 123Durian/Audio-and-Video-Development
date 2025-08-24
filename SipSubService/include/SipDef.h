#ifndef _SIPDEF_H
#define _SIPDEF_H
#include <string>
#include <string.h>  
#define SIP_STACK_SIZE   1024*256
#define SIP_ALLOC_POOL_1M  1024*1024*1

#define SIP_NOTIFY   "Notify"
#define SIP_HEARTBEAT "keepalive"
#define SIP_QUERY   "Query"
#define SIP_CATALOG   "Catalog"
#define SIP_RECORDINFO "RecordInfo"

enum statusCode
{
    SIP_SUCCESS = 200,
    SIP_BADREQUEST = 400,  //请求的参数或者格式不对，请求非法
    SIP_FORBIDDEN = 404,   //请求的用户在本域中不存在

};

//类型编码枚举
enum DevTypeCode
{
    Error_code = -1,
    Dvr_Code = 111,
    ViderServer_Code = 112,
    Encoder_Code = 113,
    Decoder_Code = 114,
    AlarmDev_Code = 117,
    NVR_Code = 118,

    Camera_Code = 131,
    Ipc_Code = 132,
    VGA_Code = 133,
    AlarmInput_Code = 134,
    AlarmOutput_Code = 135,

    CenterServer_Code = 200,
};


#endif