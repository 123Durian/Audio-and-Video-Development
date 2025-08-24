#include "Gb28181Session.h"
#include "ECSocket.h"
using namespace EC;
FILE* fp = NULL;
Gb28181Session::Gb28181Session()
{
    m_rtpRRTime = 0;
    m_rtpTcpFd = -1;
	m_listenFd = -1;
}

Gb28181Session::~Gb28181Session()
{
    if(m_rtpTcpFd != -1)
    {
        DeleteDestination(RTPTCPAddress(m_rtpTcpFd));   //rtp-tcp时需要删除连接
        close(m_rtpTcpFd);
        m_rtpTcpFd = -1;
    }

    if(m_listenFd != -1)
    {
        close(m_listenFd);
        m_listenFd = -1;
    }
    //发送BYE数据包并离开会话 不用等待

    //下级在推送回访流的时候到达结束时间没有异常，此时下级主动给上级发送一个bye包
	BYEDestroy(RTPTime(0, 0), 0, 0);
}
//创建下级rtp会话，不太一样的地方是，下级主动推送
//在推送时要负载数据类型，目前我们指定了负载数据类型为ps流
//所以下级在创建rtp会话前，需要初始化ps封装器相关资源
//这里我们要用到ps封装器的第三方库




//这里需要传入一个tcp的标识
int Gb28181Session::CreateRtpSession(int poto,string setup,string dstip,int dstport,int rtpPort)
{
    uint32_t destip = inet_addr(dstip.c_str());
    destip = ntohl(destip);

    RTPSessionParams sessParams;
    sessParams.SetOwnTimestampUnit(1.0/90000.0);
    sessParams.SetAcceptOwnPackets(true);
    sessParams.SetUsePollThread(true);
    sessParams.SetNeedThreadSafety(true);
    sessParams.SetMinimumRTCPTransmissionInterval(RTPTime(5,0));
    int ret = -1;
    if(poto == 0)
    {
        RTPUDPv4TransmissionParams transparams;
        //设置本地端口
        transparams.SetPortbase(rtpPort);

        //下面两个参数设置完之后由rtcp决定，已达到最佳的性能和效果
        transparams.SetRTPSendBuffer(JRTP_SET_SEND_BUFFER);//rtp发送的缓冲区大小
        transparams.SetMulticastTTL(JRTP_SET_MULITTTL);//经过一个路由器就会减1


        //下级是推流方，目前创建到这不知道将数据推给谁
        //由于下级是主动推流，刚开始rtp表是空的，所以需要主动将对端的ip和端口号添加进来
        ret = Create(sessParams,&transparams);
        if(ret < 0)
        {
            LOG(ERROR)<<"udp create fail";
        }
        else
        {
            //如果会话创建成功就将对端的ip和端口号进行添加
            //添加的目的是知道将数据发送给谁
            //rtp发送时会广播将数据发送给AddDestination中所有地址
            RTPIPv4Address dest(destip,dstport);
            AddDestination(dest);
            LOG(INFO)<<"udp create ok rtp:"<<rtpPort;
        }
    }
    else
    {
        sessParams.SetMaximumPacketSize(65535);
        RTPTCPTransmissionParams transParams;
        ret = Create(sessParams, &transParams, RTPTransmitter::TCPProto);
        if(ret < 0)
        {
            LOG(ERROR) << "Rtp tcp error: " << RTPGetErrorString(ret);
            return -1;
        }

        //会话创建成功后，接下来我们需要创建tcp连接
        //上级和下级是反这来的
        int sessFd = RtpTcpInit(dstip,dstport,rtpPort,setup,5);
        if(sessFd < 0)
        {
            LOG(ERROR)<<"RtpTcpInit faild";
            return -1;
        }
        else
        {
            AddDestination(RTPTCPAddress(sessFd));
        }
    }

    

    return ret;
}

int Gb28181Session::RtpTcpInit(string dstip,int dstport,int localPort,string setup,int time)
{
	LOG(INFO)<<"setup:"<<setup;
    int timeout = time*1000;
    //在这里我们就需要判断SDP中的setup字段值来调用不同的接口
    if(setup == "active")
    {
		m_rtpTcpFd = ECSocket::createConnByPassive(&m_listenFd,localPort,&timeout);
    }
    else if(setup == "passive")
    {
        m_rtpTcpFd = ECSocket::createConnByActive(dstip,dstport,localPort,&timeout);
    }
	LOG(INFO)<<"m_rtpTcpFd:"<<m_rtpTcpFd<<",m_listenFd:"<<m_listenFd;
    return m_rtpTcpFd;
}


//
void Gb28181Session::OnRTCPCompoundPacket(RTCPCompoundPacket *pack, const RTPTime &receivetime, const RTPAddress *senderaddress)
{
	RTCPPacket *rtcpPack;
    //将复合包的指针移动到第一个子包
    pack->GotoFirstPacket();
    while((rtcpPack = pack->GetNextPacket()) != 0)
    {
        ////判断这个rtcp的格式是否有误
        //即当前这个rtcp的包是否是符号rtcp协议的有效包
        if(rtcpPack->IsKnownFormat())
        {
            //获取当前rtcp包的类型，是rr包还是sr包
            switch (rtcpPack->GetPacketType())
            {
            case RTCPPacket::RR:
                {
                    //这里我们每次收到RR包，我们就更新下时间戳，我们用时间来检测RR包的机制
                    //获取当前系统时间单位是秒
                    time_t now_time = time(NULL);
                    //记录上一次收到rr包的时间戳
                    m_rtpRRTime = now_time;
                    //一般是5秒不到会回复一次
                    LOG(INFO)<<"=====m_rtpRRTime:"<<m_rtpRRTime;
                    break;
                }

            }
        }
    }
}

void* SipPsCode::Alloc(void* param, size_t bytes)
{
    return malloc(bytes);
}

void SipPsCode::Free(void* param, void* packet)
{
	delete fp;
	fp = NULL;
    return free(packet);
}


//ps封装好数据之后会执行该函数
void SipPsCode::onPsPacket(void* param, int stream, void* packet, size_t bytes)
{
    //LOG(INFO)<<bytes<<" packet demutex";
    SipPsCode* self = (SipPsCode*)param;
    //发送数据
    self->sendPackData(packet,bytes);

}

//用来将编码后的视频（例如 H.264）或音频（如 G.711）等媒体流封装成标准的 PS 包，然后再通过 RTP 协议传输。
int SipPsCode::initPsEncode()
{
    //首先定义ps封装器参数设定的结构体
    //ps_muxer_func_t 这个结构体一般是用来定义 PS（Program Stream）封装器中的回调函数集合，用于管理内存分配、释放和写入操作。
    struct ps_muxer_func_t func;
    //负责内存分配的回调
    func.alloc = Alloc;
    //负责释放之前分配的内存
    func.free = Free;
    //通常负责处理封装完成后的 PS 数据包
    func.write = onPsPacket;


    //初始化ps的封装器
    m_muxer = ps_muxer_create(&func,this);
    if(NULL == m_muxer)
    {
        LOG(ERROR)<<"ps_muxer_create failed";
        return -1;
    }
    sleep(1);
    //下级rtp会话创建
    if(gbRtpInit()<0)
    {
        LOG(ERROR)<<"rtp init error";
        return -1;
    }

    return 0;

}

int SipPsCode::gbRtpInit()
{
    //下级rtp的会话创建
    m_gbRtpHandle = new Gb28181Session();
    //只要一个类在另一个类使用它的方法之前已经定义好了（或至少声明了），就可以在同一个文件中让两个类互相调用或使用对方的方法。
    return m_gbRtpHandle->CreateRtpSession(this->m_poto,this->m_connType,this->m_dstip,this->m_dstport,this->m_localRtpPort);
}



//对收到的每一帧h264进行ps封装
//主要是将数据封装成PS的格式并通过RTP发送
int SipPsCode::incomeVideoData(unsigned char* avdata,int len,int pts,int isIframe)
{
    //如果还没有添加任何流
    if(m_avStreamIndex == -1)
    {

        //在整个 PS 封装器 m_muxer 中只会添加一个 H.264 视频流通道（stream）。
        //该函数实现ps封装器m_muxer是一个封装器，最后两个参数是流的附加数据和附加的字节，都没有
        //第二个参数是流的类型，这里表示H.264视频流
        //这行代码的作用是告诉复用器 m_muxer，将要添加一个类型为 H.264 的视频流，也就是“注册”一个视频流的通道或标识
        //此时还没有数据
        m_avStreamIndex = ps_muxer_add_stream(m_muxer,STREAM_VIDEO_H264,NULL,0);
        LOG(INFO)<<"add stream index:"<<m_avStreamIndex;
    }

    if(m_gbRtpHandle->CheckAlive())
    {
        LOG(ERROR) << "[OnRecv] The upper service connection is disconnected and send ps packet is stopped";
        this->stopFlag = true;
        return -1;
    }

    //关键帧：ps h | ps sys h| ps sys map|pes h|h264 raw data
    //非关键帧： ps h | pes h|h264 raw data
    //音频：pes h | aac raw data

    //ps流的封装
    //第二个参数是该流的索引
    //第三个参数是判断当前这个流的数据是i帧还是非i帧
    //倒数第二个参数是我们要封装的数据
    //最后一个参数表示该数据的长度
    int ret = ps_muxer_input(m_muxer,m_avStreamIndex,isIframe,pts,pts,avdata,len);
    if(ret < 0)
    {
        LOG(INFO)<<"error to push frame:"<<ret;
    }

    return ret;
        
}


//音频处理
int SipPsCode::incomeAudioData(unsigned char* audata,int len,int pts)
{
    if(m_auStreamIndex == -1)
    {
        m_auStreamIndex = ps_muxer_add_stream(m_muxer,STREAM_AUDIO_AAC,NULL,0);
        LOG(INFO)<<"add stream index:"<<m_auStreamIndex;
    }

    int ret = ps_muxer_input(m_muxer,m_avStreamIndex,0,pts,pts,audata,len);
    if(ret < 0)
    {
        LOG(INFO)<<"error to push frame:"<<ret;
    }

    return ret;
}

int SipPsCode::sendPackData(void* packet, size_t bytes)
{
    //定义最大数据部分长度
    int ps_buff_len = 1300;
    //累计处理的字节数
    int size = 0;
    //状态码，用于后续判断是否成功
    int status = 0;
    while(size < bytes)
    {
        //计算本次要处理的数据长度
        int packlen = (bytes-size) >= ps_buff_len ? ps_buff_len : (bytes-size);
        if(bytes < ps_buff_len)
        {
            //可以直接发的

            //向 RTP 通道发送一帧数据包
            //第一个参数表示要发送的数据帧
            //第二个参数表示数据帧的长度
            //动态负载类型
            //该帧的标志位，可能用于标记是否为关键帧（I帧）或当前包是否为帧的结束（marker 位）
            //mark对于视频来说是一帧数据的结尾，对于音频来说是开始
            //时间戳参数，应该是一个常量或者宏，用于指示当前 RTP 包的时间戳。该值可以用于同步音视频、估算播放时序等。表示当前帧（或音频数据块）在原始媒体时间线上的位置，用于播放端进行音视频同步与顺序播放

            //直接发送到远端
            status = m_gbRtpHandle->SendPacket(packet,bytes,96,true,PS_SEND_TIMESTAME);
            if(status < 0)
            {
                LOG(ERROR)<<RTPGetErrorString(status);
            }
        }
        else
        {
            //rtp分包发送逻辑
            if((bytes-size) > ps_buff_len)
            {
                //从第一个参数位置开始，发送第二个参数大小的数据包
                //false表示不是最后一个数据包
                status = m_gbRtpHandle->SendPacket((packet+size),packlen,96,false,0);
                if(status < 0)
                {
                    LOG(ERROR)<<RTPGetErrorString(status);
                }
            }
            else
            {
                status = m_gbRtpHandle->SendPacket((packet+size),packlen,96,true,PS_SEND_TIMESTAME);
                if(status < 0)
                {
                    LOG(ERROR)<<RTPGetErrorString(status);
                }
            }

        }
        size += packlen;
    }
    return status;
}