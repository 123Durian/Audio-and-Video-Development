#ifndef _SIPDEF_H
#define _SIPDEF_H
#include <string>
#include <string.h>  
using namespace std;
#define SIP_STACK_SIZE   1024*256
#define SIP_ALLOC_POOL_1M  1024*1024*1

#define SIP_NOTIFY   "Notify"
#define SIP_HEARTBEAT "keepalive"
#define SIP_RESPONSE "Response"
#define SIP_CATALOG "Catalog"
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

    Camera_Code = 131,//普通摄像头
    Ipc_Code = 132,//网络摄像头
    VGA_Code = 133,
    AlarmInput_Code = 134,
    AlarmOutput_Code = 135,

    CenterServer_Code = 200,
};

struct DeviceInfo
{
    string devid;
    string playformId;//下级id
    string streamName;//play还是back
    string setupType;//当需要指定rtp的传输层协议为TCP的时候需要用到这个
    int protocal;//指定的rtp的传输层是udp还是tcp
    int startTime;//play_back的时候会用到
    int endTime;//play_back的时候会用到
    struct bufferevent* bev//加一个event的变量
};

struct StreamHeader
{
    int type;   //媒体类型   1=音频  2=视频
    int length;   //该帧数据的长度
    int videoH;   //视频分别率-高
    int videoW;   //视频分别率-宽
    //这里我们可以定义个char数组，可以存放包含音频的参数信息，然后client可以根据type来解出相应的参数来
    char format[4];    
    //这里面可以存储视频的编码格式，帧率和帧类型，或者音频的采样，采样率以及采样宽等参数，
    //这些参数可以让前端方便的进行解码和播放,前端还需要什么业务可以协商后在这个header中进行添加
    //目前我们这个项目中重点是视频部分，不涉及音频，音频部分也比较简单，
    //如果有同学涉及到音频开发可以私下找我，网上查找到关于音频的处理已经完全够用，远没有视频复杂
};

enum Command_Type
{
    //获取注册信息
	Command_Session_Register = 1,
	Command_Session_Catalog,
	Command_Session_RealPlay,
	Command_Session_RecordList,
	Command_Session_PlayBack,
	Command_Session_PlayStop
};
#endif