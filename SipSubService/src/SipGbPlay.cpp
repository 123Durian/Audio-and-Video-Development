#include "SipGbPlay.h"
#include "GlobalCtl.h"
#include "Common.h"
#include "SipDef.h"
#include "Gb28181Session.h"
#include "SipMessage.h"


SipGbPlay::MediaStreamInfo SipGbPlay::mediaInfoMap;
pthread_mutex_t SipGbPlay::streamLock = PTHREAD_MUTEX_INITIALIZER;

SipGbPlay::SipGbPlay()
{

}

SipGbPlay::~SipGbPlay()
{

}

//收到来自下级的数据时候触发这个类


void SipGbPlay::run(pjsip_rx_data *rdata)
{
    pjsip_msg* msg = rdata->msg_info.msg;
    if(msg->line.req.method.id == PJSIP_INVITE_METHOD)
    {
        dealWithInvite(rdata);
    }
    else if(msg->line.req.method.id == PJSIP_BYE_METHOD)
    {
        //这里我们来实现下级处理上级的BYE请求
        dealWithBye(rdata);
    }
}

void SipGbPlay::dealWithBye(pjsip_rx_data *rdata)
{
    //下级首先获取对于哪个devic发送的bye---找devic -id
    //上级发送这个请求时会将devicid放到to字段中
    //下级根据这个id找到对应的sippscode的句柄
	int code = SIP_SUCCESS;

    //解析to字段
	std::string devId = parseToId(rdata->msg_info.msg);
	LOG(INFO)<<"======BYE  devId:"<<devId;
	do
	{
		if(devId == "")
		{
			code = SIP_BADREQUEST;
			break;
		}
		AutoMutexLock lck(&streamLock);
		auto iter = mediaInfoMap.find(devId);
		if(iter != mediaInfoMap.end())
		{
            //我们先判断下SipPsCode句柄是否不为空
			if(iter->second != NULL)
			{
				iter->second->stopFlag = true;
			}
            //delete iter->second;
            //最后还需要从map中删除当前的键值对
			iter = mediaInfoMap.erase(iter);
		}
		else
		{
			code = SIP_FORBIDDEN;
		}
	}while(false);
	
    //处理完之后发送请求
	pj_status_t status = pjsip_endpt_respond(GBOJ(gSipServer)->GetEndPoint(), NULL, rdata, code, NULL,NULL, NULL, NULL);
	if (PJ_SUCCESS != status)
	{
		LOG(ERROR)<<"create response failed";
		return;
	}
	
}

void SipGbPlay::dealWithInvite(pjsip_rx_data *rdata)
{
    /*
    下级处理首先需要解析到SDP，再给一个响应200OK，并且还要携带SDP
    */
   

    //检查请求开流的上级平台ID是否是注册在线的
    string fromId = parseFromId(rdata->msg_info.msg);
    bool flag = false;
    int status_code = 200;
    string id;
    MediaInfo sdpInfo;
    SipPsCode* ps = NULL;
    do
    {
       {
            AutoMutexLock lock(&(GlobalCtl::globalLock));
            GlobalCtl::SUPDOMAININFOLIST::iterator iter = GlobalCtl::instance()->getSupDomainInfoList().begin();
            for(;iter != GlobalCtl::instance()->getSupDomainInfoList().end(); iter++)
            {//存在并且是注册在线的
                if(iter->sipId == fromId && iter->registered)
                {
                    //flag等于true
                    flag = true;
                    break;
                }
            }
        }

        //如果不存在就直接返回
        if(!flag)
        {
            status_code = SIP_FORBIDDEN;
            break;
        }

        
        pjmedia_sdp_session* sdp = NULL;
        //看收到的sdp的body部分是否有效
        if(rdata->msg_info.msg->body)
        {
            //解析出SDP
            pjsip_rdata_sdp_info* sdp_info = pjsip_rdata_get_sdp_info(rdata);
            sdp = sdp_info->sdp;
        }

        //sdp的媒体个数等于0说明是不正确的
        if(sdp && sdp->media_count == 0)
        {
            status_code = SIP_BADREQUEST;
            break;
        }

        //从sdp的o字段解析出实际开流设备的id
        string devId(sdp->origin.user.ptr,sdp->origin.user.slen);
        DevTypeCode type = GlobalCtl::getSipDevInfo(devId);
        if(type == Error_code)
        {
            status_code = SIP_FORBIDDEN;
            break;
        }
        //判断设备对不对
        id = devId;

        //截取S字段，用于判断开流是实时流还是回放流
        string tmp(sdp->name.ptr,sdp->name.slen);
        //提出出来之后进行赋值操作
        sdpInfo.sessionName = tmp;
        if(sdpInfo.sessionName == "PlayBack")
        {
            //需要事件戳
            sdpInfo.startTime = sdp->time.start;
            sdpInfo.endTime = sdp->time.stop;
        }


        //解析connect info

        //连接有ip
        pjmedia_sdp_conn* c = sdp->conn;
        string dst_ip(c->addr.ptr,c->addr.slen);
        //设置上级ip
        sdpInfo.dstRtpAddr = dst_ip;

        //获取收流服务器端口
        //媒体有端口
        pjmedia_sdp_media* m = sdp->media[sdp->media_count-1];
        int sdp_port = m->desc.port;
        sdpInfo.dstRtpPort = sdp_port;

        //指定rtp的传输层协议
        string protol(m->desc.transport.ptr,m->desc.transport.slen);
        sdpInfo.sdp_protol = protol;
        int poto = 0;
        if(sdpInfo.sdp_protol == "TCP/RTP/AVP")
        {
            //设置setup
            poto = 1;
            pjmedia_sdp_attr* attr = pjmedia_sdp_attr_find2(m->attr_count,m->attr,"setup",NULL);
            string setup(attr->value.ptr,attr->value.slen);
            sdpInfo.setUp = setup;
        }
		
		sdpInfo.localRtpPort = GBOJ(gConfig)->popOneRandNum();
        //对端的ip,port,本地的端口，协议，setup,开始时间和结束时间

        //每个用户一个新的ps封装
        ps = new SipPsCode(dst_ip,sdp_port,sdpInfo.localRtpPort,poto,sdpInfo.setUp,sdpInfo.startTime,sdpInfo.endTime);
        {
            //需要在ps对象实例化后就插入到map中
            AutoMutexLock lck(&streamLock);
			mediaInfoMap.insert(pair<string, SipPsCode*>(devId, ps));
        }

    } while (0);

    //解析完之后进行一个回复
    resWithSdp(rdata,status_code,id,sdpInfo);

    //取流
    sendPsRtpStream(&ps);
    
    
    
}

void SipGbPlay::sendPsRtpStream(SipPsCode** ps)
{
    //创建ps和rtp对象
    int ret = (*ps)->initPsEncode();
    if(ret < 0)
    {
        LOG(ERROR)<<"initPsEncode error:"<<ret;
        return;
    }

    //创建成功之后开始取流
    //取流之后要封装为ps流，然后通过rtp会话发出
    ret = recvFrame(&(*ps));
    if(ret < 0)
    {
        LOG(ERROR)<<"recvFrame error";
    }
    return;


}


//16个字节
//描述音视频流头部信息的结构体
struct StreamHeader
{
	//媒体类型，1-》audio 2-》video
	char type;//会向四个字节对齐
	//显示时间戳  毫秒级
	int pts;
	//本帧长度, 指后续负载长度, bytes
    //该长度不包括头部
	int length;
	//是否为I帧
	int keyFrame;
};

//回放的话就注意起始和结束

int SipGbPlay::recvFrame(SipPsCode** ps)
{
    // string out_video_path = "/home/ap/safm/ccbc/bin/forkCancer/out222222222.h264";
    // FILE* h264_fp = fopen(out_video_path.c_str(),"wb");
    // FILE* fp = fopen("/home/ap/safm/ccbc/bin/forkCancer/stream.file","rb");
    // if(!fp)
        // return -1;
	
	string out_video_path = "../../conf/out.h264";
    FILE* h264_fp = fopen(out_video_path.c_str(),"wb");

    //将这个文件当成下级从流媒体服务器拉过来的流
    //然后我们要读取这个流，对这个流中的每一帧数据进行一个header的解析
    FILE* fp = fopen("../../conf/stream.file","rb");
    if(!fp)
        return -1;
	
	//在这里我们先对录像时间进行个转换，转成毫秒,因为数据的头部里的pts为毫秒级别
	int start = 0;
	int end = 0;
	bool backflag = false;  //定义个回放flag
    //回放时候先判断起始时间和结束时间是不是有效的
	if((*ps)->m_sTime >= 0 && (*ps)->m_eTime > 0)
	{
		start = (*ps)->m_sTime * 1000;
		end = (*ps)->m_eTime * 1000;
		backflag = true;
	}
    int ret = 0;
    //创建一个数组
    unsigned char* buf = new unsigned char[sizeof(StreamHeader)];
    //读取文件数据并判断是否到达文件末尾
    while(!feof(fp))
    {
        //需要判断下结束的flag
        //为true则发送rtp层的bye，并退出当前取流和推流的线程
        if((*ps)->stopFlag)
        {
            delete *ps;
            *ps = NULL;
            break;
        }
        //将动态分配的内存初始化为0
        memset(buf,0,sizeof(StreamHeader));
        //从文件中读取一个 StreamHeader 结构体大小的数据块，保存到内存中 buf 指向的缓冲区里。
        int size = fread(buf,1,sizeof(StreamHeader),fp);
        if(size < 0)
        {
            ret = -1;
            break;
        }

        //类型转换
        StreamHeader* header = (StreamHeader*)buf;
        //打印这一帧所显示的时间戳
		LOG(INFO)<<"header->pts:"<<header->pts;
        //根据帧头中的 length 字段动态申请一段内存，用来保存这一帧的实际音视频数据。
        //该length表示的是一帧ps数据的长度

        //header解析出来之后马上把他对应的负载部分拿出来，不然就解析不到负载的部分
		unsigned char* data = new unsigned char[header->length];
        //从文件中读取 length 字节的数据到 data 中。也就是说，读取对应的音视频负载。
		fread(data,1,header->length,fp);
        if(header->type == 2)
        {//video
			if(backflag)
			{
                //如果是回放判断起始时间和终止时间
				if(header->pts < start)
				{
					continue;  //协议头里的pts小于起始时间，那么我们就不推流
				}
				else if(header->pts > end)
				{
					break;   //当pts大于录像结束时间后我们断流
				}
			}
            

            //ps demetex
            //将读取到的视频数据传给解码/推流模块
            //实现ps流的封装
        
            if((*ps)->incomeVideoData(data,header->length,header->pts,header->keyFrame)<0)
            {
                continue;
                //只有当当前h264数据不完整的时候才会出现封装错误
                //接着封装下一个
            }
			//LOG(INFO)<<"header->length:"<<header->length;
            //如果成功，就将这帧数据写入 H264 文件中
            size = fwrite(data,1,header->length,h264_fp);
            //LOG(INFO)<<"write size:"<<size;
            delete data;
        }
        else
        {
            continue;
        }
        usleep(90000);
    }

    //下级在正常情况下不会主动断流，只有说设备有问题或者推流过程中码流有问题才会断流
    //上级还需要监听下级推流的这个过程是否又断流了
    delete buf;
    fclose(h264_fp);
    fclose(fp);
    //delete表示下级在处理流的时候出现了异常，导致了资源的释放，rtp会话这个资源也可以释放
    if((*ps) != NULL)
    {
        delete *ps;
        *ps = NULL;
    }
   

    return ret;
}

void SipGbPlay::resWithSdp(pjsip_rx_data *rdata,int status_code,string devid,MediaInfo sdpInfo)
{
    //组织响应body部分的sdp内容
    pjsip_tx_data* tdata;
    //组织响应消息的结构体
    pjsip_endpt_create_response(GBOJ(gSipServer)->GetEndPoint(),rdata,status_code,NULL,&tdata);
    //定义消息类型，一个是主类型一个是子类型
    pj_str_t type = {"Application",11};
    pj_str_t sdptype = {"SDP",3};
    if(status_code != SIP_SUCCESS)
    {
        //将对端发过来的sdp原封不动发过去
        tdata->msg->body = pjsip_msg_body_create(tdata->pool,&type,&sdptype,&(pjsip_rdata_get_sdp_info(rdata)->body));
    }
    else
    {
        //用字符串流的方式
        stringstream ss;
        ss<<"v="<<"0"<<"\r\n";
        ss<<"o="<<devid<<" 0 0 IN IP4 "<<GBOJ(gConfig)->sipIp()<<"\r\n";
        ss<<"s="<<"Play"<<"\r\n";
        ss<<"c="<<"IN IP4 "<<GBOJ(gConfig)->sipIp()<<"\r\n";
        ss<<"t="<<"0 0"<<"\r\n";
        //端口号写的都是本机的
        ss<<"m=video "<<30000<<" "<<sdpInfo.sdp_protol<<" 96"<<"\r\n";
        ss<<"a=rtpmap:96 PS/90000"<<"\r\n";
        ss<<"a=sendonly"<<"\r\n";
        //一方是主动另一方就是被动
        if(sdpInfo.setUp != "")
        {
            if(sdpInfo.setUp == "passive")
            {
                ss<<"a=setup:"<<"active"<<"\r\n";
            }
            else if(sdpInfo.setUp == "active")
            {
                ss<<"a=setup:"<<"passive"<<"\r\n";
            }
        }


        //将构造好的 SDP 内容设置到 SIP 消息体中
        //将上述定义的sdp转换成string
        string sResp = ss.str();
        //将string转换成pjsip自定义类型
        pj_str_t sdpData = pj_str((char*)sResp.c_str());
        //构造了一个 SIP 消息体对象（pjsip_msg_body），并将其赋值给待发送 SIP 消息（tdata->msg）的 body 字段
        tdata->msg->body = pjsip_msg_body_create(tdata->pool,&type,&sdptype,&sdpData);
    }


    //构造一个 SIP Contact 头部字段，并添加到待发送的 SIP 响应消息中，然后获取响应目标地址信息
	SipMessage msg;
    //本级的
    //contact是每一次发送不管是请求还是响应都要改成本机的
	msg.setContact((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str(),GBOJ(gConfig)->sipPort());
	const pj_str_t contactHeader = pj_str("Contact");
	const pj_str_t param = pj_str(msg.Contact());
	pjsip_generic_string_hdr* customHeader = pjsip_generic_string_hdr_create(tdata->pool,&contactHeader,&param);
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)customHeader);
	pjsip_response_addr res_addr;
    //res_addr	输出参数，函数会把对端的响应地址信息写到这个结构体里
	pj_status_t status = pjsip_get_response_addr(tdata->pool, rdata, &res_addr);
    if (PJ_SUCCESS != status)
    {
        pjsip_tx_data_dec_ref(tdata);
        return;
    }
    

    //发送响应
    status = pjsip_endpt_send_response(GBOJ(gSipServer)->GetEndPoint(), &res_addr, tdata, NULL, NULL);
    if (PJ_SUCCESS != status)
    {
        pjsip_tx_data_dec_ref(tdata);
        return;
    }

    return;
}

//media模块注册时候强制需要初始化的两个回调
void SipGbPlay::OnStateChanged(pjsip_inv_session *inv, pjsip_event *e)
{

}
void SipGbPlay::OnNewSession(pjsip_inv_session *inv, pjsip_event *e)
{

}
void SipGbPlay::OnMediaUpdate(pjsip_inv_session *inv_ses, pj_status_t status)
{
    
}