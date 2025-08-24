// #include "OpenStream.h"
// #include "SipDef.h"
// #include "SipMessage.h"
// #include "GlobalCtl.h"
// #include "Gb28181Session.h"






// //rtp负载类型定义
// static string rtpmap_ps = "96 PS/90000";

// OpenStream::OpenStream()
// {
//     m_pStreamTimer = new TaskTimer(600);
//     m_pCheckSessionTimer = new TaskTimer(5);
// }

// OpenStream::~OpenStream()
// {
//     LOG(INFO)<<"~OpenStream";
//     if(m_pStreamTimer)
//     {
//         delete m_pStreamTimer;
//         m_pStreamTimer = NULL;
//     }


//     if(m_pCheckSessionTimer)
//     {
//         delete m_pCheckSessionTimer;
//         m_pCheckSessionTimer = NULL;
//     }
// }

// void OpenStream::StreamServiceStart()
// {
//     if(m_pStreamTimer && m_pCheckSessionTimer)
//     {
//         m_pStreamTimer->setTimerFun(OpenStream::StreamGetProc,this);
//         m_pStreamTimer->start();

//         m_pCheckSessionTimer->setTimerFun(OpenStream::CheckSession,this);
//         m_pCheckSessionTimer->start();
//     }
// }


// // 定时检查当前视频流会话（Session）是否已超时，如果超时就自动关闭并释放资源。
// void OpenStream::CheckSession(void* param)
// {
//     AutoMutexLock lck(&(GlobalCtl::gStreamLock));
//     GlobalCtl::ListSession::iterator iter = GlobalCtl::glistSession.begin();
//     while(iter != GlobalCtl::glistSession.end())
//     {
//         timeval curttime;
//         gettimeofday(&curttime, NULL);
//         Session* pSession = *iter;

//         //超时之后就关闭这个会话
//         if(pSession != NULL && curttime.tv_sec - pSession->m_curTime.tv_sec >= 10)
//         {

//             //首先发送sip层的bye包
//             //然后发送rtp层的bye包

//             //析构了一个rtp对象
//             StreamStop(pSession->platformId,pSession->devid);
//             Gb28181Session *pGb28181Session = dynamic_cast<Gb28181Session*>(pSession);
//             //pGb28181Session->Destroy();  //rtp session destroy
// 			LOG(INFO)<<"pGb28181Session->Destroy()";
//             delete pGb28181Session;
//             //将该会话关闭
//             iter = GlobalCtl::glistSession.erase(iter);
//         }
//         else
//         {
//             ++iter;
//         }
//     }
// }



// void OpenStream::StreamStop(std::string platformId, std::string devId)
// {
// 	pj_thread_desc  desc;
// 	pjcall_thread_register(desc);
//     SipMessage msg;
//     {
//         AutoMutexLock lck(&(GlobalCtl::globalLock));
//         GlobalCtl::SUBDOMAININFOLIST::iterator iter = GlobalCtl::instance()->getSubDomainInfoList().begin();
//         for(;iter != GlobalCtl::instance()->getSubDomainInfoList().end();iter++)
//         {
//             if(iter->sipId == platformId)
//             {
//                 //header part
//                 msg.setFrom((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());
// 				msg.setTo((char*)devId.c_str(),(char*)iter->addrIp.c_str());
//                 msg.setUrl((char*)devId.c_str(),(char*)iter->addrIp.c_str(),iter->sipPort);
//                 msg.setContact((char*)devId.c_str(),(char*)iter->addrIp.c_str(),iter->sipPort);
//             }
//         }
//     }
//     pjsip_tx_data *tdata;
// 	pj_str_t from = pj_str(msg.FromHeader());
//     pj_str_t to = pj_str(msg.ToHeader());
//     pj_str_t requestUrl = pj_str(msg.RequestUrl());
//     pj_str_t contact = pj_str(msg.Contact());
// 	std::string method = "BYE";
// 	pjsip_method reqMethod = {PJSIP_OTHER_METHOD,{(char*)method.c_str(),method.length()}};
//     //创建bye请求
// 	pj_status_t status = pjsip_endpt_create_request(GBOJ(gSipServer)->GetEndPoint(), &reqMethod, &requestUrl, &from, &to, &contact, NULL, -1, NULL, &tdata);
//     if (status != PJ_SUCCESS)
//     {
//         LOG(ERROR)<<"request catalog error";
//         return;
//     }

//     //发送信令，将当前用户保存到token中，作为回调的参数
//     //发送bye请求
//     status = pjsip_endpt_send_request(GBOJ(gSipServer)->GetEndPoint(), tdata, -1, NULL, NULL);
//     if (status != PJ_SUCCESS)
//     {
//         LOG(ERROR)<<"send request error";
//     }
// 	return;
// }



// /*
// 开流sdp格式
// v=0
// o=11000000001310000059 0 0 IN IP4 [本机IP]
// s=Play
// c=IN IP4 [本机IP]
// t=0 0
// m=video [RTP端口] RTP/AVP 96
// a=rtpmap:96 PS/90000
// a=recvonly
// a=setup:[active|passive|actpass] （如果 info.protocal 为 TCP 时添加）
// */




// //开流
// //向指定的下级平台（或设备）发送 SIP INVITE 请求，建立视频流会话。
// void OpenStream::StreamGetProc(void* param)
// {
//     LOG(INFO)<<"GlobalCtl::gRcvIpc:"<<GlobalCtl::gRcvIpc;
//     //设备获取成功
//     if(!GlobalCtl::gRcvIpc)
//         return;
//     LOG(INFO)<<"start stream";



//     //定义一个存储开流信息的结构体
//     DeviceInfo info;
//     info.devid = "11000000001310000059";
//     info.playformId = "11000000002000000001";
//     info.streamName = "Play";
//     info.startTime = 0;
//     info.endTime = 0;

//     //tcp时候将这个改为1
//     info.protocal = 0;
// 	//info.setupType = "passive";
  
// #if 1
// 	{
//         //不支持同一设备及类型重复开流
//         AutoMutexLock lck(&(GlobalCtl::gStreamLock));
//         GlobalCtl::ListSession::iterator iter = GlobalCtl::glistSession.begin();
//         while(iter != GlobalCtl::glistSession.end())
//         {
//             if((*iter)->devid == info.devid && (*iter)->streamName == info.streamName)
//             {
//                 return;
//             }
//         }

//     }
// #endif
// 	int rtp_port = GBOJ(gConfig)->popOneRandNum();
// 	LOG(INFO)<<"rtp_port:"<<rtp_port;
//     //request INVTE
//     SipMessage msg;
//     {
//         AutoMutexLock lock(&(GlobalCtl::globalLock));
//         GlobalCtl::SUBDOMAININFOLIST::iterator iter = GlobalCtl::instance()->getSubDomainInfoList().begin();
//         for(;iter != GlobalCtl::instance()->getSubDomainInfoList().end(); iter++)
//         {
//             if(iter->sipId == info.playformId)
//             {
//                 msg.setFrom((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());
//                 msg.setTo((char*)iter->sipId.c_str(),(char*)iter->addrIp.c_str());
//                 msg.setUrl((char*)iter->sipId.c_str(),(char*)iter->addrIp.c_str(),iter->sipPort);
//                 msg.setContact((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str(),GBOJ(gConfig)->sipPort());
//             }
//         }
//     }

//     pj_str_t from = pj_str(msg.FromHeader());
//     pj_str_t to = pj_str(msg.ToHeader());
//     pj_str_t contact = pj_str(msg.Contact());
//     pj_str_t requestUrl = pj_str(msg.RequestUrl());

// //invite会话需要创建一个对话框
//     //创建一个dlg对象
//     pjsip_dialog* dlg;
//     //创建成功后 dlg 就是一个指向 pjsip_dialog 结构体的指针，它保存了 整条 SIP 会话的状态。
//     pj_status_t status = pjsip_dlg_create_uac(pjsip_ua_instance(),&from,&contact,&to,&requestUrl,&dlg);
//     if(PJ_SUCCESS != status)
//     {
//         LOG(ERROR)<<"pjsip_dlg_create_uac ERROR";
//         return;
//     }
//     //sdp part
//     //body部分是sdp协议

//     //创建一个SDP对象
//     pjmedia_sdp_session* sdp = NULL;
//     //pjsip中对结构体内存的创建
//     sdp = (pjmedia_sdp_session*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_session));
//     //Session Description Protocol Version (v): 0
//     //版本设置
//     sdp->origin.version = 0;
//     //Owner/Creator, Session Id (o): 33010602001310019325 0 0 IN IP4 10.64.49.44
//     //user需要开流的设备id（国标id）
//     sdp->origin.user = pj_str((char*)info.devid.c_str());
//     //名字用户，表示某个会话的唯一标识
//     sdp->origin.id = 0;
//     //网络的类型，in表示internet
//     sdp->origin.net_type = pj_str("IN");
//     //ip地址的类型
//     sdp->origin.addr_type = pj_str("IP4");
//     //发起开流的具体ip地址（本机ip）
//     sdp->origin.addr = pj_str((char*)GBOJ(gConfig)->sipIp().c_str());
//     //Session Name (s): Play
//     //会话的名称
//     sdp->name = pj_str((char*)info.streamName.c_str());
//    //Connection Information (c): IN IP4 10.64.49.218


//    //需要先创建一个conncet的对象，然后再这个对象中进行设定
//    //设置连接信息
//     sdp->conn = (pjmedia_sdp_conn*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_conn));
//     sdp->conn->net_type = pj_str("IN");
//     sdp->conn->addr_type = pj_str("IP4");
//     //收流端的ip也是本级开流端的ip
//     //如果之后信令和实际的媒体接收时两个模块，拿着一块就不一样
//     sdp->conn->addr = pj_str((char*)GBOJ(gConfig)->sipIp().c_str());
//     //Time Description, active time (t): 0 0


//     //设置事件戳
//     sdp->time.start = info.startTime;
//     sdp->time.stop = info.endTime;
//     //Media Description, name and address (m): video 5514 RTP/AVP 96
//     //我们只构建一个媒体通道
//     sdp->media_count = 1;
//     //创建并初始化一个 pjmedia_sdp_media 类型的结构体实例
//     pjmedia_sdp_media* m = (pjmedia_sdp_media*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_media));
//     sdp->media[0] = m;
//     //媒体类型
//     m->desc.media = pj_str("video");
//     //传输端口
//     //使得这个收流端口每次不一样
//     //虽然说可能是一个上级对应多个下级，但是在rtp传输和接收的时候，是一个rtp对应一个rtp的
//     m->desc.port = rtp_port;
//     //使用一个端口
//     // 使用多少个连续的端口号来传输该媒体流。
//     m->desc.port_count = 1;
//     //设置传输协议
//     if(info.protocal)
//     {
//         m->desc.transport = pj_str("TCP/RTP/AVP");
//     }
//     else
//     {
//         m->desc.transport = pj_str("RTP/AVP");
//     }
//     // 该媒体流支持的 payload 格式（format）的数量。
//     //动态负载型号
//     m->desc.fmt_count = 1;
//     m->desc.fmt[0] = pj_str("96");
//     /*
//     Media Attribute (a): rtpmap:96 PS/90000
//         Media Attribute (a): recvonly
//         Media Attribute (a): setup:active
//     */

//     //属性的个数，即就是a开头的个数
//     //创建媒体的属性
//     //这里的count标识属性的index
//     m->attr_count = 0;
//     //每添加一个属性count++
//     pjmedia_sdp_attr* attr = (pjmedia_sdp_attr*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_attr));
//     attr->name = pj_str("rtpmap");
//     attr->value = pj_str((char*)rtpmap_ps.c_str());
//     m->attr[m->attr_count++] = attr;

//     //添加第二个属性
//     attr = (pjmedia_sdp_attr*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_attr));
//     attr->name = pj_str("recvonly");
//     m->attr[m->attr_count++] = attr;
//     //如果是tcp则添加第三个属性
//     if(info.protocal)
//     {
//         attr = (pjmedia_sdp_attr*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_attr));
//         attr->name = pj_str("setup");
//         attr->value = pj_str((char*)info.setupType.c_str());
//         m->attr[m->attr_count++] = attr;
//     }


//     //在客户端（UAC）侧创建一个 SIP INVITE 会话。
//     pjsip_inv_session* inv;
//     status = pjsip_inv_create_uac(dlg,sdp,0,&inv);
//     if(PJ_SUCCESS != status)
//     {
//         //如果失败dlg终止对话框
//         pjsip_dlg_terminate(dlg);
//         LOG(ERROR)<<"pjsip_inv_create_uac ERROR";
//         return;
//     }
//     //那么在这里就可以使用Gb28181Session类来实例化一个session类
//     //在invite的时候就实例化gb2818
//     //通过inv回调OnMediaUpdate，这样就可以在发送invite请求的时候，里面的收留端口每次都是不一样的，rtp对应rtp
// //每次请求都启动了一个rtp进程
//     Session* pSession = new Gb28181Session(info);
//     //给基类的成员赋值
// 	pSession->rtp_loaclport = rtp_port;
//     //用于把用户自定义的“会话上下文对象”附加到 pjsip_inv_session 这个 SIP 会话对象中
//     //mod_data 是 pjsip_inv_session 用来模块或应用程序自己存储私有数据的空间。
//     //这样做的目的是方便你在后续的 INVITE 会话回调，中，可以通过 inv->mod_data[0] 快速找到对应的业务会话对象
//     inv->mod_data[0] = (void*)pSession;


//     //处理发送的信息txdata
//     //用于构造一条invite请求消息
//     // 根据 inv（已经初始化好的 INVITE 会话对象）自动构造一个完整的 SIP INVITE 请求包，并把这个包保存在 tdata 中。
//     pjsip_tx_data* tdata;
//     status = pjsip_inv_invite(inv,&tdata);
//     if(PJ_SUCCESS != status)
//     {
//         pjsip_dlg_terminate(dlg);
//         LOG(ERROR)<<"pjsip_inv_invite ERROR";
//         return;
//     }
    

//     //定义header中的subject字段
//     //定义的是一个字符串
//     pj_str_t subjectName = pj_str("Subject");
//     char subjectBuf[128] = {0};
//     //给字符串赋值
//     sprintf(subjectBuf,"%s:0,%s:0",info.devid.c_str(),GBOJ(gConfig)->sipId().c_str());
//     pj_str_t subjectValue = pj_str(subjectBuf);
//     //将key和value生成一个头节点
//     pjsip_generic_string_hdr* hdr = pjsip_generic_string_hdr_create(GBOJ(gSipServer)->GetPool(),&subjectName,&subjectValue);
//     //将这个头节点添加到tdata的message中
//     pjsip_msg_add_hdr(tdata->msg,(pjsip_hdr*)hdr);

//     status = pjsip_inv_send_msg(inv,tdata);
//     if(PJ_SUCCESS != status)
//     {
//         pjsip_dlg_terminate(dlg);
//         LOG(ERROR)<<"pjsip_inv_send_msg ERROR";
//         return;
//     }


//     //在每次的ivite发送之后在这里要push——back


//     //抛出一个问题：
//     //如果下级在推流中出现了某个异常，但是下级没有做异常处理。就是出现了某个问题导致下级推流中断，但是下级没有关闭rtp会话。
//     //这个时候双方一种保持着rtp会话，一直占用着rtp端口，但是没有推流。他们没有实际的交互。
//     //所以上级要有一个机制来检查下级推流超时


//     //rtp的会话和控制在全局的session这个类中，如果这个session如果对2818中的rtp会话进行管理，那么我们就需要用2818继承session。
//     AutoMutexLock lck(&(GlobalCtl::gStreamLock));
//     GlobalCtl::glistSession.push_back(pSession);
//     GlobalCtl::gRcvIpc = false;
// 	//sleep(3);
// 	//OpenStream::StreamStop("11000000002000000001","11000000001310000059");
//     return;
// }





#include "OpenStream.h"
#include "SipDef.h"
#include "SipMessage.h"
#include "GlobalCtl.h"
#include "Gb28181Session.h"

//rtp负载类型定义
static string rtpmap_ps = "96 PS/90000";

OpenStream::OpenStream(struct bufferevent* bev,void* arg)
    :ThreadTask(bev,arg)
{
    //m_pStreamTimer = new TaskTimer(600);
}

OpenStream::~OpenStream()
{
    LOG(INFO)<<"~OpenStream";
    // if(m_pStreamTimer)
    // {
    //     delete m_pStreamTimer;
    //     m_pStreamTimer = NULL;
    // }


    // if(m_pCheckSessionTimer)
    // {
    //     delete m_pCheckSessionTimer;
    //     m_pCheckSessionTimer = NULL;
    // }
}

#if 0
void OpenStream::StreamServiceStart()
{
    if(m_pStreamTimer && m_pCheckSessionTimer)
    {
        m_pStreamTimer->setTimerFun(OpenStream::StreamGetProc,this);
        m_pStreamTimer->start();

        m_pCheckSessionTimer->setTimerFun(OpenStream::CheckSession,this);
        m_pCheckSessionTimer->start();
    }
}

void OpenStream::CheckSession(void* param)
{
    AutoMutexLock lck(&(GlobalCtl::gStreamLock));
    GlobalCtl::ListSession::iterator iter = GlobalCtl::glistSession.begin();
    while(iter != GlobalCtl::glistSession.end())
    {
        timeval curttime;
        gettimeofday(&curttime, NULL);
        Session* pSession = *iter;
        if(pSession != NULL && curttime.tv_sec - pSession->m_curTime.tv_sec >= 10)
        {
            StreamStop(pSession->platformId,pSession->devid);
            Gb28181Session *pGb28181Session = dynamic_cast<Gb28181Session*>(pSession);
            //pGb28181Session->Destroy();  //rtp session destroy
			LOG(INFO)<<"pGb28181Session->Destroy()";
            delete pGb28181Session;
            iter = GlobalCtl::glistSession.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
}
#endif


void OpenStream::StreamStop(std::string platformId, std::string devId)
{
	pj_thread_desc  desc;
	pjcall_thread_register(desc);
    SipMessage msg;
    {
        AutoMutexLock lck(&(GlobalCtl::globalLock));
        GlobalCtl::SUBDOMAININFOLIST::iterator iter = GlobalCtl::instance()->getSubDomainInfoList().begin();
        for(;iter != GlobalCtl::instance()->getSubDomainInfoList().end();iter++)
        {
            if(iter->sipId == platformId)
            {
                //header part
                msg.setFrom((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());
				msg.setTo((char*)devId.c_str(),(char*)iter->addrIp.c_str());
                msg.setUrl((char*)devId.c_str(),(char*)iter->addrIp.c_str(),iter->sipPort);
                msg.setContact((char*)devId.c_str(),(char*)iter->addrIp.c_str(),iter->sipPort);
            }
        }
    }
    pjsip_tx_data *tdata;
	pj_str_t from = pj_str(msg.FromHeader());
    pj_str_t to = pj_str(msg.ToHeader());
    pj_str_t requestUrl = pj_str(msg.RequestUrl());
    pj_str_t contact = pj_str(msg.Contact());
	std::string method = "BYE";
	pjsip_method reqMethod = {PJSIP_OTHER_METHOD,{(char*)method.c_str(),method.length()}};
	pj_status_t status = pjsip_endpt_create_request(GBOJ(gSipServer)->GetEndPoint(), &reqMethod, &requestUrl, &from, &to, &contact, NULL, -1, NULL, &tdata);
    if (status != PJ_SUCCESS)
    {
        LOG(ERROR)<<"request catalog error";
        return;
    }

    //发送信令，将当前用户保存到token中，作为回调的参数
    status = pjsip_endpt_send_request(GBOJ(gSipServer)->GetEndPoint(), tdata, -1, NULL, NULL);
    if (status != PJ_SUCCESS)
    {
        LOG(ERROR)<<"send request error";
    }
	return;
}

void OpenStream::run()
{
    StreamGetProc(NULL);
}

void OpenStream::StreamGetProc(void* param)
{
    LOG(INFO)<<"GlobalCtl::gRcvIpc:"<<GlobalCtl::gRcvIpc;
    if(!GlobalCtl::gRcvIpc)
        return;
    LOG(INFO)<<"start stream";
    DeviceInfo info;
    info.devid = "11000000001310000059";
    info.playformId = "11000000002000000001";
    if(streamType == 3)
    {
        info.streamName = "Play";
        info.startTime = 0;
        info.endTime = 0;
        info.protocal = 0;
    }
    else if(streamType == 5)
    {
        info.streamName = "PlayBack";
        info.startTime = 5;
        info.endTime = 10;
        info.protocal = 0;
    }
    info.bev = m_bev;
    
	//info.setupType = "passive";
  
#if 1
	{
        //不支持同一设备及类型重复开流
        AutoMutexLock lck(&(GlobalCtl::gStreamLock));
        GlobalCtl::ListSession::iterator iter = GlobalCtl::glistSession.begin();
        while(iter != GlobalCtl::glistSession.end())
        {
            if((*iter)->devid == info.devid && (*iter)->streamName == info.streamName)
            {
                return;
            }
        }

    }
#endif
	int rtp_port = GBOJ(gConfig)->popOneRandNum();
	LOG(INFO)<<"rtp_port:"<<rtp_port;
    //request INVTE
    SipMessage msg;
    {
        AutoMutexLock lock(&(GlobalCtl::globalLock));
        GlobalCtl::SUBDOMAININFOLIST::iterator iter = GlobalCtl::instance()->getSubDomainInfoList().begin();
        for(;iter != GlobalCtl::instance()->getSubDomainInfoList().end(); iter++)
        {
            if(iter->sipId == info.playformId)
            {
                msg.setFrom((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());
                msg.setTo((char*)iter->sipId.c_str(),(char*)iter->addrIp.c_str());
                msg.setUrl((char*)iter->sipId.c_str(),(char*)iter->addrIp.c_str(),iter->sipPort);
                msg.setContact((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str(),GBOJ(gConfig)->sipPort());
            }
        }
    }

    pj_str_t from = pj_str(msg.FromHeader());
    pj_str_t to = pj_str(msg.ToHeader());
    pj_str_t contact = pj_str(msg.Contact());
    pj_str_t requestUrl = pj_str(msg.RequestUrl());

    pjsip_dialog* dlg;
    pj_status_t status = pjsip_dlg_create_uac(pjsip_ua_instance(),&from,&contact,&to,&requestUrl,&dlg);
    if(PJ_SUCCESS != status)
    {
        LOG(ERROR)<<"pjsip_dlg_create_uac ERROR";
        return;
    }
    //sdp part
    pjmedia_sdp_session* sdp = NULL;
    sdp = (pjmedia_sdp_session*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_session));
    //Session Description Protocol Version (v): 0
    sdp->origin.version = 0;
    //Owner/Creator, Session Id (o): 33010602001310019325 0 0 IN IP4 10.64.49.44
    sdp->origin.user = pj_str((char*)info.devid.c_str());
    sdp->origin.id = 0;
    sdp->origin.net_type = pj_str("IN");
    sdp->origin.addr_type = pj_str("IP4");
    sdp->origin.addr = pj_str((char*)GBOJ(gConfig)->sipIp().c_str());
    //Session Name (s): Play
    sdp->name = pj_str((char*)info.streamName.c_str());
   //Connection Information (c): IN IP4 10.64.49.218
    sdp->conn = (pjmedia_sdp_conn*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_conn));
    sdp->conn->net_type = pj_str("IN");
    sdp->conn->addr_type = pj_str("IP4");
    sdp->conn->addr = pj_str((char*)GBOJ(gConfig)->sipIp().c_str());
    //Time Description, active time (t): 0 0
    sdp->time.start = info.startTime;
    sdp->time.stop = info.endTime;
    //Media Description, name and address (m): video 5514 RTP/AVP 96
    sdp->media_count = 1;
    pjmedia_sdp_media* m = (pjmedia_sdp_media*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_media));
    sdp->media[0] = m;
    m->desc.media = pj_str("video");
    m->desc.port = rtp_port;
    m->desc.port_count = 1;
    if(info.protocal)
    {
        m->desc.transport = pj_str("TCP/RTP/AVP");
    }
    else
    {
        m->desc.transport = pj_str("RTP/AVP");
    }
    m->desc.fmt_count = 1;
    m->desc.fmt[0] = pj_str("96");
    /*
    Media Attribute (a): rtpmap:96 PS/90000
        Media Attribute (a): recvonly
        Media Attribute (a): setup:active
    */
    m->attr_count = 0;
    pjmedia_sdp_attr* attr = (pjmedia_sdp_attr*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_attr));
    attr->name = pj_str("rtpmap");
    attr->value = pj_str((char*)rtpmap_ps.c_str());
    m->attr[m->attr_count++] = attr;

    attr = (pjmedia_sdp_attr*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_attr));
    attr->name = pj_str("recvonly");
    m->attr[m->attr_count++] = attr;
    if(info.protocal)
    {
        attr = (pjmedia_sdp_attr*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_attr));
        attr->name = pj_str("setup");
        attr->value = pj_str((char*)info.setupType.c_str());
        m->attr[m->attr_count++] = attr;
    }

    pjsip_inv_session* inv;
    status = pjsip_inv_create_uac(dlg,sdp,0,&inv);
    if(PJ_SUCCESS != status)
    {
        pjsip_dlg_terminate(dlg);
        LOG(ERROR)<<"pjsip_inv_create_uac ERROR";
        return;
    }
    //那么在这里就可以使用Gb28181Session类来实例化一个session类
    Session* pSession = new Gb28181Session(info);
	pSession->rtp_loaclport = rtp_port;
    inv->mod_data[0] = (void*)pSession;

    pjsip_tx_data* tdata;
    status = pjsip_inv_invite(inv,&tdata);
    if(PJ_SUCCESS != status)
    {
        pjsip_dlg_terminate(dlg);
        LOG(ERROR)<<"pjsip_inv_invite ERROR";
        return;
    }
    pj_str_t subjectName = pj_str("Subject");
    char subjectBuf[128] = {0};
    sprintf(subjectBuf,"%s:0,%s:0",info.devid.c_str(),GBOJ(gConfig)->sipId().c_str());
    pj_str_t subjectValue = pj_str(subjectBuf);
    pjsip_generic_string_hdr* hdr = pjsip_generic_string_hdr_create(GBOJ(gSipServer)->GetPool(),&subjectName,&subjectValue);
    pjsip_msg_add_hdr(tdata->msg,(pjsip_hdr*)hdr);

    status = pjsip_inv_send_msg(inv,tdata);
    if(PJ_SUCCESS != status)
    {
        pjsip_dlg_terminate(dlg);
        LOG(ERROR)<<"pjsip_inv_send_msg ERROR";
        return;
    }

    AutoMutexLock lck(&(GlobalCtl::gStreamLock));
    GlobalCtl::glistSession.push_back(pSession);
    GlobalCtl::gRcvIpc = false;
	//sleep(3);
	//OpenStream::StreamStop("11000000002000000001","11000000001310000059");
    return;
}