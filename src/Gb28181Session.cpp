#include "Gb28181Session.h"
#include "SipDef.h"
#include "mpeg4-avc.h"
#include "h264_stream.hpp"
#include "h265_stream.hpp"
#include "mpeg4-hevc.h"  
#include "ECSocket.h"
using namespace EC;


//定义个结构体
typedef struct 
{
    uint16_t width;
    uint16_t height;
    float max_framerate;
} Picinfo;



//这里定义个接口来实现从h264的编码中获取分辨率和帧率


//第一个参数是h264码流
//第三个参数为传出参数
int GetH264pic(const uint8_t* data,uint16_t size,Picinfo* info)
{

    //数据为空或者size==0表示传入的数据无效，直接返回
    if (data == NULL || size == 0) {
        return 1;
    }
    if (info == NULL) {
        return 1;
    }
    //将传出参数的内存全部清空
    memset(info, 0, sizeof(Picinfo));

    int ret = 999;
    char s_buffer[4 * 1024];
    //将定义的这个数组清零
    memset(s_buffer, 0, sizeof(s_buffer));

    //定义并初始化两个结构体变量，用于存储 MPEG-4 中的 AVC（H.264）和 HEVC（H.265）格式相关的数据
    struct mpeg4_avc_t avc;
    memset(&avc, 0, sizeof(mpeg4_avc_t));
    struct mpeg4_hevc_t hevc;
    memset(&hevc, 0, sizeof(mpeg4_hevc_t));

    //将 H.264 码流封装进 MP4 容器

    /*
    第一个参数为输出参数，第二个参数为h264格式数据
    第四个参数为输出缓冲区

    主要是将nal前面的起始码去掉，转换为四个字节的数据长度

    s_buffer（也就是你说的 buf）保存的是 转换后的 H.264 视频码流数据，格式是符合 MP4 封装规范的

    传出参数中保存啦还没有转换为mp4格式的数据的sps和pps，还有视频编码器所使用的配置文件，视频编码器复杂度和性能需求的级别。
    */
	ret =(int)mpeg4_annexbtomp4(&avc, data, size, s_buffer, sizeof(s_buffer));
	if (avc.nb_sps <= 0) {
		return 1;
	}

	h264_stream_t* h4 = h264_new();

    /*
    解析SPS数据
    第一个参数：创建好的h264解析结构体指针，
    最后一个参数：标识本次解析的是h264数据



    SPS 是整个视频序列级别的大体规则----分辨率和帧率都在sps中
    PPS 是每一帧具体编码时的细节设置。
    */
	ret =h264_configure_parse(h4, avc.sps[0].data, avc.sps[0].bytes, H264_SPS);
	if (ret != 0) {
		h264_free(h4);
		return 1;
	}
	info->width = h4->info->width;
	info->height = h4->info->height;
	info->max_framerate = h4->info->max_framerate;
	h264_free(h4);
	
	return 0;
}

//需要用codecid来判别裸流是音频还是视频数据
//stream 流的编号  用不上
//flags 视频关键帧的标识


//接收解封装后的音视频裸数据 H.264并拼接、缓存或转发发送。
/*
参数：
1：上下文架构体
2：分辨流的索引
3：区分音频流的编解码器还是视频流的编解码器
4：关键帧标记
5：显示时间戳，区分每一帧的数据
6：解码时间戳
7：裸数据指针
8：裸数据长度
*/


//将要发送的数据存到buf中

//组h264帧
static void ps_demux_callback(void* param, int stream, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    /*
    在这个函数中，我们可以将音视频流推送给上级client，前端是来接收流和相应的参数，把接受流和参数给到解码器，解码后的数据交给SDL进行播放。

    将这个回调传入的data保存到buf后，再添加前端所需要的header部分就行
    header中需要区分：当前的这一帧数据是音频还是视频，是关键帧还是p帧
    */
    
	PackProcStat* pProc = (PackProcStat*)param;
    //然后需要在结构体里再添加几个成员

    //数据流类型(音/视频)
    int media = -1;
    //视频帧类型(I/P帧)
    int frameType = 1;
    
    //我们先判别下当前的codecid是否有效
	if(codecid == 0) // unknown codec id
	{
        //当前负载的裸流数据是无效的
		return;
	}
	
    //首先先处理同一类型的流数据被拆成多个包的情况
    //当前回调被第一次调用时，不会走这个逻辑

    //将同一类型的同一帧数据存在buf中，等待组装完整在统一处理

    //比较当前收到的数据帧（data）是否与上一次接收的数据是“同一帧”的继续部分。
	
    //将同一个H264帧放在一起
    if(pProc->sCodec == codecid && pProc->sPts == pts)
	{
        //我们需要将当前的裸流保存到buffer，后续我们再处理buffer的数据
		if(pProc->slen < pProc->sNow + (int)bytes)
		{
            //扩容
			pProc->slen = pProc->sNow + (int)bytes + 1024;
			pProc->sBuf = (char *)realloc(pProc->sBuf, pProc->slen);
		}
		memcpy(pProc->sBuf + pProc->sNow, data, bytes);

        //更改结构体成员
		pProc->sNow += bytes;
        //是不是关键帧的
		pProc->sKeyFrame = flags;
		return;
	}
//同一个媒体不管被拆成多少个包，他就是连续性的

    //当前这个接口第一次调用时，会走这个逻辑
    //后续这个逻辑是来处理当前传入的data为下一帧数据或者不同类型的数据，
	else 
	{

        //如果之前缓冲区有数据（不太可能是第一次，因为刚开始无数据），先发送旧帧数据
		//如果发送buffer的数据大于0，那么我们就进行发送，发送前还需要进行header的设定，一会我们再做

        //第一次时候这个==0
		if(pProc->sNow > 0)
		{

            //判断当前处理的帧是视频还是音频，保存（可选）数据到文件，如果是视频还判断是否为关键帧，最终通过会话对象 pGbSesson 将数据打包并发送出去。
			do{
                //如果是视频数据
				if(pProc->sCodec == STREAM_VIDEO_H264
					|| pProc->sCodec == STREAM_VIDEO_H265)
				{
                    //标记媒体类型为视频
					media = 2;
					//frameType = pProc->sKeyFrame > 1 ? FORMAT_VIDEO_I : FORMAT_VIDEO_P;
#if 1
					if(pProc->sFp == NULL)
					{
						pProc->sFp = fopen("/home/zengyao/conf/send.h264", "w+");
					}

					if(pProc->sFp != NULL)
					{
						fwrite(pProc->sBuf, 1, pProc->sNow, pProc->sFp);
					}
#endif
				}
				else if(pProc->sCodec == STREAM_AUDIO_AAC
						|| pProc->sCodec == STREAM_AUDIO_G711
						|| pProc->sCodec == STREAM_AUDIO_G711U)
				{
                    //标记为音频数据
					media = 1;
				}
				else
				{
				// 	if(pProc->unknownCodecCnt == 0)
				// 		LOG(INFO) << " unknown codec: " << pProc->sCodec;

				// 	if(pProc->unknownCodecCnt ++ > 200)
				// 		pProc->unknownCodecCnt = 0;

					break;
				}

				//unsigned long long microsecIn = pProc->sDts * 1000 / 90000;

                //标记完之后直接将数据发送出去
                Gb28181Session* pGbSesson = (Gb28181Session*)pProc->session;
				int sendLen = pGbSesson->SendPacket(media, (char *)pProc->sBuf, pProc->sNow, pProc->sCodec);
			}while(0);
		}
		//LOG(INFO)<<"33333333";
		// copy new data to send buffer


        //重新设置结构体
		pProc->sNow = 0;
		pProc->sKeyFrame = 0;
        //将当前数据copy到buffer中
		if(pProc->slen < pProc->sNow + (int)bytes)
		{
			pProc->slen = pProc->sNow + bytes + 1024;
			pProc->sBuf = (char *)realloc(pProc->sBuf, pProc->slen);
		}
		memcpy(pProc->sBuf + pProc->sNow, data, bytes);
		pProc->sNow += bytes;
		pProc->sKeyFrame = flags;

		pProc->sPts = pts;
		
		pProc->sCodec = codecid;
	}

	return;
}

//需要使用codecId来区分码流的编码方式，然后来进行相应的解码获取分别率以及帧率

//该代码的核心是给每一帧数据封装一个自定义的头部
int Gb28181Session::SendPacket(int media,char* data,int datalen,int codecId)
{
    //为什么突然有一个header？？？
    //先计算下推送包的整个长度，header部分+负载部分
    

    //这个header的格式是自己定义的

    //给每一帧数据前面会加上一个header
    int len = sizeof(struct StreamHeader) + datalen;


    //创建一个缓存
    char* streamBuf = new char[len];
    memset(streamBuf,0,len);

    //先给header部分的参数进行赋值

    //只影响访问起始那部分内存，大小是 sizeof(StreamHeader)，超出部分不会被当成结构体的一部分
    //header就是streambuf的头部分是吧

    //我们在进行h264裸流之后在进行协议头的封装，这个协议投需要跟上级客户端进行一个协商
    StreamHeader* header = (StreamHeader*)streamBuf;
    header->length = datalen;
    if(media == 2)
    {
        header->type = media;
        //需要进行IDR帧的判断，并从SPS序列集中解出帧率和分别率
        //判断下当前流的codeid是否属于h264编码
        if(codecId == STREAM_VIDEO_H264)
        {
            //h264码流是由多个NALU组成
            int nalType = 0,keyFrame = 0;
			int videoW = 0, videoH = 0;
			int videoFps = 25;
            //先保存下当前流的编码格式，将int类型转为char，4-》1，其实就是存储了ascll码的值
            //header->format[0] = 1;
            //然后再获取nalu类型，我们之前说过nalu的startcode为三个字节或者四个字节

            //先解析一个startcode获取nalu的类型


            //在解析一下nalu的type，这个type就是区分当前这个帧里面是否是p帧，i帧，或idr帧

            //[start code] + [NAL unit header] + [NAL unit payload]
            //在NAL unit header中确定naltype
            if(data[0] == 0x0 && data[1] == 0x0 && data[2] == 0x0 && data[3] == 0x01)
			{
                
                nalType = data[4] & 0x1F;
			}
            else if(data[0] == 0x0 && data[1] == 0x0 && data[2] == 0x01)
            {
                
                nalType = data[3] & 0x1F;
            }
			else
			{
				LOG(ERROR) << "Invalid h264 data, please check!";
				return -1;
			}


//判断该帧是不是idr帧，如果是idr帧，那么我们要获取到idr帧中的sps参数集合
//然后我们要用sps中的参数集计算出当前视频流中的分辨率，帧率

//sps的naltype对应的是7，pbs对应的是8
//i帧的naltype对应的是5

//sps是解码说明书
//解码器必须先收到 SPS 和 PPS，才能解码后面的图像帧
//sps描述整个视频序列的结构，比如分辨率、帧率、颜色格式等
//pps一张图像或一组图像的编码参数，比如熵编码方式、参考帧数量等



            //如果说nalType为7，那么就代表这帧数据包含了SPS序列集，也就是IDR帧
            //那么定义个变量标识下当前帧为关键帧
            if(nalType == 7)
            {
                //当前帧为idr帧
                keyFrame = 1;
            }
            else //这里只需要将包含了SPS参数集的数据看作为关键帧，其他都为非关键帧
            {
                keyFrame = 0;
            }

            if(keyFrame == 1)
            {
				//这里查询帧率和分辨率信息的频率不要太频繁，这里我们设置每50个关键帧查询一次

                //解出视频的分辨率和帧率


                /*
                h264的三种数据类型
                u(1):表示无符号的二进制，h264编码时直接将0/1直接写到数据流中，解码时读取一个比特位，并将其解释为0/1
                ue(v)：表示无符号指数哥伦布编码，是一种变长的编码方式。
                se(v)：有符号编码

                //编码是为了提高数据的压缩率

//了解之后直接用现成的源码
                */




                //我们不能说每组idr都来解析一下分辨率还有帧率，这样的话浪费性能
                //就是我们要尽可能的提高性能
                //但是我们又怕他中间的分辨率和帧率参数有变
                //所以我们每50个idr帧取一次
				if(m_count == 0 || m_count >50)
				{
					Picinfo info;
					if(GetH264pic((unsigned char *)data, datalen, &info) == 0)
					{
						if(info.height != 0 && info.width != 0)
						{
							header->videoH = info.height;
							header->videoW = info.width;
							
							LOG(INFO)<<"header->videoH:"<<header->videoH<<",header->videoW:"<<header->videoW;
							m_count++;
						}
						if(info.max_framerate > 0)
						{
							//header->format[0] = info.max_framerate;
							LOG(INFO)<<"info.max_framerate:"<<info.max_framerate;
						}
						else if(info.max_framerate == 0)
						{
							//header->format[0] = 25;
						}
						if(m_count >50)
						{
							m_count = 0;
						}
					}
					else
					{
						LOG(ERROR) << "can't analysis video data !! data: " << (void *)data << ", dataLen: " << datalen;
					}
				}
				else 
				{
					m_count++;
				}
				
				//最后再保存下帧类型
				//header->format[1] = keyFrame;  
				//将编码器类型也保存下
				//header->format[2] = codecId;   
            }
			

        }

        //将数据部分拷贝过来
        memcpy(streamBuf+sizeof(StreamHeader),data,datalen);
        
        if(bev != NULL)
        {
            bufferevent_write(bev,streamBuf,len);
            LOG(INFO)<<"send total data len:"<<len<<",payload data len:"<<datalen;
        }
    }
    
    return 0;
}


Gb28181Session::Gb28181Session(const DeviceInfo& devInfo)
:Session(devInfo)
{
    m_proc = new PackProcStat();
	m_count = 0;
    m_rtpTcpFd = -1;
    m_listenFd = -1;
}

Gb28181Session::~Gb28181Session()
{
	//发送BYE数据包并离开会话 不用等待
	BYEDestroy(RTPTime(0, 0), 0, 0);
    if(m_rtpTcpFd != -1)
    {
        close(m_rtpTcpFd);
        m_rtpTcpFd = -1;
    }

    if(m_listenFd != -1)
    {
        close(m_listenFd);
        m_listenFd = -1;
    }
	
    //在2818的析构函数中对rtp端口进行回收
	GBOJ(gConfig)->pushOneRandNum(rtp_loaclport);
}


//定期处理收到的 RTP 数据包的回调函数
void Gb28181Session::OnPollThreadStep()
{
    //确保线程安全地访问 RTP 接收缓冲区的数据
    //开始安全访问RTP缓冲
    //开始对接收到的数据进行访问
    //目的是通知JRTPLIB库开始准备访问接收到的数据
    BeginDataAccess();

    //检查是否有传入的数据包
    if(GotoFirstSourceWithData())//找到第一个有数据的源（可以理解为是一个设备）
    {
        do
        {
            RTPSourceData* srcdat = NULL;//表示当前正在处理的RTP数据源
            RTPPacket* pack = NULL;//表示当前正在处理的RTP数据包
            srcdat = GetCurrentSourceInfo();//获取当前RTP数源的信息
            while((pack = GetNextPacket()) != NULL)//遍历当前源的所有待处理RTP包
            {
                //定义自定义函数处理收到的RTP包
                //这里处理慢会导致丢包
                ProcessRTPPacket(*srcdat,*pack);

                //释放当前RTP包所占用的资源
                DeletePacket(pack);
            }

        } while (GotoNextSourceWithData());  
    }
    EndDataAccess();

}

//具体的处理逻辑
//第一次组帧是将rtp组合成一个完整的PS流

void Gb28181Session::ProcessRTPPacket(RTPSourceData& srcdat,RTPPacket& pack)
{
    //首先从pack中解析出rtp相关的参数

    //可以知道RTP数据包中承载的是哪种编码的数据包
    int payloadType = pack.GetPayloadType();
    int payloadLen = pack.GetPayloadLength();
    //判断帧边界的
    int mark = pack.HasMarker();
    //当前的序列号
    int curSeq = pack.GetExtendedSequenceNumber();
    //时间戳
    int timestamp = pack.GetTimestamp();

    //获取负载数据，负载数据就是真实的ps或者h264数据
    unsigned char* payloadData = pack.GetPayloadData();

    //负载的类型不是ps流或者不是h264流的话，我们认为这个负载类型我们处理不了
    if(payloadType != 96  && payloadType != 98)
    {
        LOG(ERROR)<<"rtp unknown payload type:"<<payloadType;
        return;
    }
    //在这里更新下下级最后有rtp包推送的时间
    //更新上级最后一次接收rtp包的时间
	gettimeofday(&m_curTime, NULL);
	//那么就在接收rtp包的时机给这个session进行赋值
    //这里需要先判断下
    if(m_proc && m_proc->session == NULL)
    {
        //给m_proc赋this
        m_proc->session = (void*)this;
    }


    //如果是ps的话就进行ps流的解析
    if(payloadType == 96)
    {
        //我们要把它组成完整帧，因为我门还要进行ps的解封装，调用这个解封装的接口必须是完整帧数据才可以。
        //所以我们要手动的组织完整帧数据。
        

        //该代码实现的是ps流的解封装
        OnRTPPacketProcPs(mark,curSeq,timestamp,payloadData,payloadLen);
    }
    else if(payloadType == 98)
    {
        //就直接根据上面代码中的长度将帧解析出来，然后保存，也不用去管他是不是完整帧，我们都保存下来。

    }
}



//该代码会一直接收rtp包

//默认了“新的帧的第一个RTP包是正常收到的，没有丢包”
void Gb28181Session::OnRTPPacketProcPs(int mark,int curSeq,int timestamp,unsigned char* payloadData,int payloadLen)
{

/*
我们需要将当前的rtp包的序列号，时间戳以及负载流进行保存。
在解析下一个rtp包时，我们需要将当前的序列号和上一个序列号做一个差值，检查有没有丢包
*/




	//LOG(INFO)<<"mark:"<<mark;
    int FrameStat = mark;

    //m_proc实现参数的保存


    //当前同一个线程多个rtp包进来时序号和时间戳只初始化一次

    if(m_proc->rSeq == -1)
        m_proc->rSeq = curSeq;
    
    if(m_proc->rTimeStamp == 0)
        m_proc->rTimeStamp = timestamp;


        //当前减去上一个大于1说明丢包了
    if(curSeq - m_proc->rSeq > 1)
    {
        //状态为2表示丢包
        m_proc->rState = 2;
        LOG(ERROR)<<"rtp drop pack diff:"<<curSeq - m_proc->rSeq;
    }

    //mark不能作为帧边界
    //该代码是双重判断
    if(FrameStat == 0)
    {
		//LOG(INFO)<<"m_proc->rTimeStamp:"<<m_proc->rTimeStamp"<< timestamp:"<<timestamp;
		//通过比较 RTP 包的时间戳（timestamp）是否发生变化，来判断是否遇到了“新的一帧”的开始。



        //此时就用时间戳作为帧边界
        if(timestamp != m_proc->rTimeStamp)
        {
            FrameStat = RtpPack_FrameNextStart;
        }
    }


    //判断完整之后更新变量
    m_proc->rSeq = curSeq;
    m_proc->rTimeStamp = timestamp;

    //不丢包
    if(m_proc->rState == 1)
    {
        //如果当前数据包不是下一帧的开始我们都要进行组包的操作，要把rtp负载的data部分保存到我们的缓冲区里面
        //继续组帧
        if(FrameStat == RtpPack_FrameContinue || FrameStat == RtpPack_FrameCurFinsh)
        {

            //rlen表示当前buffer的总大小，rnow表示当前已经收到的数据包大小
            //rbuf当前收取数据的buffer
            if(m_proc->rlen < payloadLen + m_proc->rNow)
            {
                //内存不够需要动态分配内存
                m_proc->rlen = payloadLen + m_proc->rNow + 1024;
                m_proc->rBuf = (char*)realloc(m_proc->rBuf,m_proc->rlen);
            }
            //当前缓冲区写入位置（偏移量）
            memcpy(m_proc->rBuf + m_proc->rNow,payloadData,payloadLen);
            m_proc->rNow += payloadLen;
        }
    }
    else if(m_proc->rState == 2)
    {
        m_proc->rState = 1;

        //数据缓存清理掉
        //如果发现丢包，说明该帧已经不可用了，不能恢复不能解码，将这一帧数据直接丢弃。
        memset(m_proc->rBuf,0,m_proc->rNow);
        //重新初始化
        m_proc->rNow = 0;
        m_proc->rSeq = -1;
        m_proc->rTimeStamp = 0;

        if(FrameStat == RtpPack_FrameNextStart)
        {
            //虽然已经丢包了，但是此时是一个新的帧的开始
            //新帧的开始就重新开始接收
            if(m_proc->rlen < payloadLen + m_proc->rNow)
            {
                //就是说：m_proc->rBuf里面始终存出的都是一个完整帧的数据
                m_proc->rlen = payloadLen + m_proc->rNow + 1024;
                m_proc->rBuf = (char*)realloc(m_proc->rBuf,m_proc->rlen);
            }
            memcpy(m_proc->rBuf + m_proc->rNow,payloadData,payloadLen);
            m_proc->rNow += payloadLen;
        }
        return;
        
    }
	//LOG(INFO)<<"FrameStat:"<<FrameStat;

    //只有帧结束或者下一帧数据时才进入
    //当检测到一帧数据已经接收完整（或检测到新一帧开始）时，进行PS流数据的写文件、解封装处理，并准备接收下一帧数据
    //解封装ps->h264
    if(FrameStat)
    {
        //文件描述符
        if(m_proc->psFp == NULL)
        {
            //创建
            m_proc->psFp = fopen("../../conf/data.ps","w+");
        }
        if(m_proc->psFp)
        {
            //写入，将缓存区中的数据写入
            fwrite(m_proc->rBuf,1,m_proc->rNow,m_proc->psFp);
        }
        //ps demutex
        //该参数表示一直指向解封装器的句柄
        if(m_proc->unpackHnd == NULL)
		{
			LOG(INFO)<<"ps_demuxer_create";
            //这里需要定义一个回调接口，用来接收处理的解封装后的音视频数据
            //创建一个ps解封装器
            //第一个参数是回调函数指针，用于接收解封装后的音视频帧。
            //当解复用器成功从 PS 数据中解析出一个完整的音视频帧时，就调用这个函数进行处理。


            //第二个参数是该回调函数传入的参数
			m_proc->unpackHnd = (void *)ps_demuxer_create((ps_dumuxer_onpacket)ps_demux_callback, (void *)m_proc);
		}

        //将缓存区中存储的 PS（Program Stream）流数据逐块传递给 PS 解封装器（demuxer）进行解析处理
		if(m_proc->unpackHnd)
		{
			//LOG(INFO)<<"PS SIZE:"<<m_proc->rNow;
            //向后移动的字节数
			int offset = 0;
            //遍历整个缓存中的数据
			while(offset < m_proc->rNow)
			{
				//这里需将缓存的数据传入到ps解封装接口中，

                //第一个参数为解封装器实例
                //第二个参数是指向待处理数据的指针
                //第三个参数是还剩下多少数据
                //返回值表示成功解析的字节数


                //该buf中存放的是一个完整帧的ps流

                //buf中存储的对于h264来说是一个完整帧的数据，对于ps来说，这里面存了多个ps数据。
                //因为每个rtp负载了一个ps流的数据，我们是以ps完帧的标准组织的h264数据
                //这一帧数据是有多个携带了ps的数据包

                //所以说要循环的将buf中的数据传入解封装的接口进行解封装
                //然后返回一个处理好的ps长度

                //第二个参数表示解析的起始位置
                //第三个参数表示本次要解析的长度。
				int ret = ps_demuxer_input((struct ps_demuxer_t *)m_proc->unpackHnd, (const uint8_t*)m_proc->rBuf+offset , m_proc->rNow-offset);
				if(ret == 0)
				{
                    //不是完整的数据，解封装失败
					LOG(ERROR) << "wrong payload data !!!!! can't demux the PS data";
				}
				offset += ret;
			}
			
		}


        memset(m_proc->rBuf,0,m_proc->rNow);
        m_proc->rNow = 0;

        if(FrameStat == RtpPack_FrameNextStart)
        {
            if(m_proc->rlen < payloadLen + m_proc->rNow)
            {
                m_proc->rlen = payloadLen + m_proc->rNow + 1024;
                m_proc->rBuf = (char*)realloc(m_proc->rBuf,m_proc->rlen);
            }
            memcpy(m_proc->rBuf + m_proc->rNow,payloadData,payloadLen);
            m_proc->rNow += payloadLen;
        }

    }

    return;

}


//创建rtp会话，不过在创建会话前要先配置参数
int Gb28181Session::CreateRtpSession(string dstip,int dstport)
{
	LOG(INFO)<<"CreateRtpSession";
//配置一个rtp会话参数对象

    //先定义一个rtpsession的参数配置对象
    RTPSessionParams sessParams;
    //设置会话的时间戳，这里时间戳的单位是取决于传输的音视频数据的采样率
    //我们在invite请求的时候已经指定了采样率，这个也是国标规定的一个
//保证 RTP 包时间戳和媒体采样率同步。
    sessParams.SetOwnTimestampUnit(1.0/90000.0);
    //设置是否自己发送的数据包，这个主要在本地测试rtp数据包推送的情况
    sessParams.SetAcceptOwnPackets(true);
    //设置会话是否使用轮询线程，可以避免阻塞主线程
    //轮询线程的作用是不断检查网络套接字（socket）是否有新的数据包到达，或者是否有定时事件需要处理
    //类似于linux中epoll
    sessParams.SetUsePollThread(true);
    //线程安全
    sessParams.SetNeedThreadSafety(true);
    //设置最小的rtcp的发送间隔
    sessParams.SetMinimumRTCPTransmissionInterval(RTPTime(5,0));
	int ret = -1;
    if(protocal == 0)
    {//创建并配置一个基于 UDP 的 RTP 传输会话


        //创建一个 UDP 传输参数对象，用于设置 RTP 传输的相关参数
        RTPUDPv4TransmissionParams transparams;
        //设置接收缓冲区大小为1MB，确保网络层能缓存足够多的数据包，减少丢包风险。
        transparams.SetRTPReceiveBuffer(1024*1024);
        //设置 RTP 会话监听的本地端口号（rtp_loaclport），这个端口是接收对端发送的 RTP 包的端口。


        //派生类直接使用基类的数据成员
        transparams.SetPortbase(rtp_loaclport);
        

        //创建 RTP 会话，传入之前设置的会话参数（sessParams）和传输参数
        ret = Create(sessParams,&transparams);
		LOG(INFO)<<"ret:"<<ret;
        if(ret < 0)
        {
            LOG(ERROR)<<"udp create fail";
        }
        else
        {
            LOG(INFO)<<"udp create ok,bind:"<<rtp_loaclport;
        }
    }
    else
    {
        //设置单个RTP包的最大为64K
        //TCP是流式协议，需要显示定义包边界
        sessParams.SetMaximumPacketSize(65535);
        RTPTCPTransmissionParams transParams;
        //创建一个rtp会话
        ret = Create(sessParams, &transParams, RTPTransmitter::TCPProto);
        if(ret < 0)
        {
            LOG(ERROR) << "Rtp tcp error: " << RTPGetErrorString(ret);
            return -1;
        }

        //会话创建成功后，接下来需要创建tcp连接
        //返回一个已经建立连接的fd
        //返回一个创建好的文件描述符。
        int sessFd = RtpTcpInit(dstip,dstport,5);
		LOG(INFO)<<"sessFd:"<<sessFd;
        if(sessFd < 0)
        {
            LOG(ERROR)<<"RtpTcpInit faild";
            return -1;
        }
        else
        {
            //将一个目标地址（远端接收者）添加到 RTP 会话中，指定该地址为数据的发送目的地。
            AddDestination(RTPTCPAddress(sessFd));
        }
    }
    

    return ret;
}

int Gb28181Session::RtpTcpInit(string dstip,int dstport,int time)
{
    int timeout = time*1000;
    if(setupType == "active")
    {
        m_rtpTcpFd = ECSocket::createConnByActive(dstip,dstport,rtp_loaclport,&timeout);
    }
    else if(setupType == "passive")
    {
        m_rtpTcpFd = ECSocket::createConnByPassive(&m_listenFd,rtp_loaclport,&timeout);
    }

    return m_rtpTcpFd;
}