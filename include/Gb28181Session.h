#ifndef _GB28181SESSION_H
#define _GB28181SESSION_H

#include "rtpsession.h"
#include "rtpsourcedata.h"
#include "rtptcptransmitter.h"
#include "rtptcpaddress.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtplibraryversion.h"
#include "rtcpsrpacket.h"
#include "Common.h"
#include "mpeg-ps.h"

#include "SipDef.h"

#ifdef __cplusplus
extern "C"{
#endif
#include "jthread.h"
#ifdef __cplusplus
}
#endif
#include "GlobalCtl.h"
using namespace jrtplib;


//音视频接收端的数据包处理状态缓存结构
//目的是在处理（如接收、拼接、解复用、缓存、写文件）过程中，记录和管理当前的中间状态和缓冲区数据
//记录上一包及当前正在处理的 RTP 包的状态和数据的
typedef struct _PackProcStat
{
    _PackProcStat()
    {
        rSeq = -1;
        rTimeStamp = 0;
        rState = 1;
        rlen = 10*1024;
        rNow = 0;
        rBuf = (char*)malloc(rlen);
        unpackHnd = NULL;
        psFp = NULL;
        sCodec = 0;
        sBuf = (char*)malloc(rlen);
        slen = 10*1024;;
        sNow = 0;
        sKeyFrame = 0;
		sPts = 0;
		sFp = NULL;
		session = NULL;
    }
    ~_PackProcStat()
    {
        if(rBuf)
        {
            free(rBuf);
            rBuf = NULL;
        }
        if(psFp)
        {
            fclose(psFp);
            psFp = NULL;
        }
    }

    int rSeq;
    int rTimeStamp;
    int rState;
    int rlen;   //当前buf的总大小
    int rNow;   //当前已经收到的数据包大小
    char* rBuf;  //当前收取数据的buf
    char* sBuf;
    int slen;
    int sNow;
    void* unpackHnd;
    FILE* psFp;
    int sCodec;
    int sKeyFrame;
	int sPts;
	FILE* sFp;
	void* session;

}PackProcStat;

//RTPsession是jrtplib库的一个核心类，是用于管理和处理RTP会话的，提供了一套方便的方法还有功能来发送和接收实时音频和数据

//不同的RTP源可能代表不同的摄像头、不同的音视频流、或者同一设备不同的流。
class Gb28181Session : public RTPSession,public Session
{
    public:
        Gb28181Session(const DeviceInfo& devInfo);
        ~Gb28181Session();


        //创建上级的rtp会话
        int CreateRtpSession(string dstip,int dstport);
        int RtpTcpInit(string dstip,int dstport,int time);
		int SendPacket(int media,char* data,int datalen,int codecId);
    protected:
        enum
        {
            //当前帧未完整，我们需要继续的接收rtp包来进行一个组包
            RtpPack_FrameContinue = 0,
            //帧结束表示帧边界
            RtpPack_FrameCurFinsh,
            //下一个帧的开始
            RtpPack_FrameNextStart,
        };
        //重写
        //这个是轮询线程一直循环时，会有两个事件来触发到这个接口
        //有新的RTP或者RTCP的数据时
        //当本机在发送rtcp包的电热器事件触发的时候
        void OnPollThreadStep();
        void ProcessRTPPacket(RTPSourceData& srcdat,RTPPacket& pack);
        void OnRTPPacketProcPs(int mark,int curSeq,int timestamp,unsigned char* payloadData,int payloadLen);
        //重写，当有新的数据源添加到表中时回调用这个函数
        //收到一个新的RTP数据包时调用这个函数

        //源端不同消息的标识
        void OnNewSource(RTPSourceData *srcdat)
        {
			LOG(INFO)<<"OnNewSource";
			LOG(INFO)<<"srcdat->IsOwnSSRC():"<<srcdat->IsOwnSSRC();

            //判断这个数据源是否在本地的源表当中添加，是的话直接返回。
            //判断是不是本地，该函数只接收远端不接收本地的
            if(srcdat->IsOwnSSRC())
                return;
            
            uint32_t ip;
            uint16_t port;
			LOG(INFO)<<"00";


            //判断是不是有效的RTP数据源信息
            //判断是rtp还是rtcp
            if(srcdat->GetRTPDataAddress() != 0)
            {
				LOG(INFO)<<"11";
                const RTPIPv4Address* addr = (const RTPIPv4Address*)srcdat->GetRTPDataAddress();
                ip = addr->GetIP();
                port = addr->GetPort();
            }
            else if(srcdat->GetRTCPDataAddress() != 0)
            {
				LOG(INFO)<<"22";
                const RTPIPv4Address* addr = (const RTPIPv4Address*)srcdat->GetRTCPDataAddress();
                ip = addr->GetIP();
                port = addr->GetPort()-1;
            }
            else
			{
				LOG(INFO)<<"33";
				return;
			}
                

            //将这个rtp/rtcp会话添加到源表当中
            RTPIPv4Address dest(ip,port);
            AddDestination(dest);
            struct in_addr inaddr;
            inaddr.s_addr = htonl(ip);
            LOG(INFO)<<"Adding destination "<<string(inet_ntoa(inaddr))<<":"<<port;
        }

        //重写
        void OnRemoveSource(RTPSourceData *srcdat)
        {
            if(srcdat->IsOwnSSRC())
                return;

            if(srcdat->ReceivedBYE())
                return;
            
            uint32_t ip;
            uint16_t port;
            if(srcdat->GetRTPDataAddress() != 0)
            {
                const RTPIPv4Address* addr = (const RTPIPv4Address*)srcdat->GetRTPDataAddress();
                ip = addr->GetIP();
                port = addr->GetPort();
            }
            else if(srcdat->GetRTCPDataAddress() != 0)
            {
                const RTPIPv4Address* addr = (const RTPIPv4Address*)srcdat->GetRTCPDataAddress();
                ip = addr->GetIP();
                port = addr->GetPort()-1;
            }
            else
                return;
            
            RTPIPv4Address dest(ip,port);
            DeleteDestination(dest);
			
			struct in_addr inaddr;
			inaddr.s_addr = htonl(ip);
			LOG(INFO) << __FUNCTION__ << " Deleting destination " << std::string(inet_ntoa(inaddr)) << ":" << port;
        }


        //重写
        void OnBYEPacket(RTPSourceData *srcdat)
        {
            if(srcdat->IsOwnSSRC())
                return;
            
            uint32_t ip;
            uint16_t port;
            if(srcdat->GetRTPDataAddress() != 0)
            {
                const RTPIPv4Address* addr = (const RTPIPv4Address*)srcdat->GetRTPDataAddress();
                ip = addr->GetIP();
                port = addr->GetPort();
            }
            else if(srcdat->GetRTCPDataAddress() != 0)
            {
                const RTPIPv4Address* addr = (const RTPIPv4Address*)srcdat->GetRTCPDataAddress();
                ip = addr->GetIP();
                port = addr->GetPort()-1;
            }
            else
                return;
            
            RTPIPv4Address dest(ip,port);
            DeleteDestination(dest);
			
			struct in_addr inaddr;
			inaddr.s_addr = htonl(ip);
			
			LOG(INFO) << " Deleting destination " << std::string(inet_ntoa(inaddr)) << ":" << port;
        }

    private:
        PackProcStat* m_proc;
		int m_count;
        int m_rtpTcpFd;
        int m_listenFd;
};

#endif