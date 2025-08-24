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
#include "mpeg-ps.h"
#include "Common.h"
#include "GlobalCtl.h"
#ifdef __cplusplus
extern "C"{
#endif
#include "jthread.h"
#ifdef __cplusplus
}
#endif

#define JRTP_SET_SEND_BUFFER   ((30)*1024)
#define JRTP_SET_MULITTTL   (255)
#define PS_SEND_TIMESTAME   (3600)
using namespace jrtplib;



//主要是管理RTP传输
class Gb28181Session : public RTPSession
{
    public:
        Gb28181Session();
        ~Gb28181Session();

        //我们这里再实现个接口进行检测
        //检测心跳超时

        //网络在线检测
        int CheckAlive()
        {
            time_t cm_time = time(NULL);  // 获取当前系统时间，单位是秒
            if(m_rtpRRTime == 0)
            {
                m_rtpRRTime = cm_time;   // 如果之前没有记录时间，则初始化为当前时间
            }

            if((cm_time - m_rtpRRTime) > 20)  // 如果当前时间和上次记录时间的差值超过20秒
            {
                return 1;  // 表示“不活跃”或“超时”，即连接已断开
            }

            return 0;  // 否则返回0，表示连接活跃
        }



        
        int CreateRtpSession(int poto,string setup,string dstip,int dstport,int rtpPort);
        int RtpTcpInit(string dstip,int dstport,int localPort,string setup,int time);
    protected:
        /*
        接下来我们还需要完善下级对上级回复的rtcp的机制做检测，
        我们之前说过,如果rtp使用的传输层协议为udp推流时，需要上级发送rtcp的RR包，也就是recv report,
        关于这个RR包的内容我们在之前的课程已经详细的说过了，这里就不再重复说明，
        主要就是发送端要实时的了解接收端接收数据的情况，根据RR包报告的参数来做相应的调整，实现提高数据包的完整性，
        好  下面我们来实现检测的功能
        我们先需要重写RTPSession的虚函数
        */
        //这个接口只有对方发送了rtcp包才会被触发


        ////重新rtpsession中的在这该虚函数

        //第一个参数是一个recp复合包
        //第二个参数是本地接收的时间
        //第三个参数是对方的ip+port
        void OnRTCPCompoundPacket(RTCPCompoundPacket *pack,const RTPTime &receivetime, const RTPAddress *senderaddress);
        void OnNewSource(RTPSourceData *srcdat)
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
            AddDestination(dest);

            struct in_addr inaddr;
            inaddr.s_addr = htonl(ip);
            LOG(INFO)<<"Adding destination "<<string(inet_ntoa(inaddr))<<":"<<port;
        }

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
        }
        //当上级发送了rtp的bye包，那么下级这边就会启用这个回调来删除会话
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
            LOG(INFO) << __FUNCTION__ << " Deleting destination " << std::string(inet_ntoa(inaddr)) << ":" << port;
        }
    private:
        int m_rtpRRTime;
        int m_rtpTcpFd;
	    int m_listenFd;
};


//PS封装器的作用是把视频、音频、字幕等多路多媒体数据打包成一个统一的流格式，方便存储和传输。


class SipPsCode
{
    public:
        SipPsCode(string dstip,int dstport,int rtpPort,int poto,string setup,int s,int e)
        {
            m_dstip = dstip;
            m_dstport = dstport;
            m_avStreamIndex = -1;
            m_auStreamIndex = -1;
			m_localRtpPort = rtpPort;
			stopFlag = false;
			m_sTime = s;
			m_eTime = e;
            m_poto = poto;
            m_connType = setup;
        }

        ~SipPsCode()
        {
            if(m_muxer)
            {
                ps_muxer_destroy(m_muxer);
            }

            if(m_gbRtpHandle)
            {
                delete m_gbRtpHandle;
                m_gbRtpHandle = NULL;
            }
			GBOJ(gConfig)->pushOneRandNum(m_localRtpPort);
			stopFlag = false;
        }


        //初始化ps封装器
        int initPsEncode();
        //创建rtp会话
        int gbRtpInit();

        static void* Alloc(void* param, size_t bytes);
        static void Free(void* param, void* packet);
        static void onPsPacket(void* param, int stream, void* packet, size_t bytes);

        int incomeVideoData(unsigned char* avdata,int len,int pts,int isIframe);
        int incomeAudioData(unsigned char* audata,int len,int pts);
        int sendPackData(void* packet, size_t bytes);

        bool stopFlag;//这个句柄结束ps的打包和rtp的发送
		int m_sTime;
		int m_eTime;
        int m_poto = 0;
        string m_connType;
    private:
        string m_dstip;
        int m_dstport;
        Gb28181Session* m_gbRtpHandle;
        ps_muxer_t* m_muxer;
        int m_avStreamIndex;
        int m_auStreamIndex;
		int m_localRtpPort;
        


};

#endif