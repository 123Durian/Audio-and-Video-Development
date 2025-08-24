#include "SipHeartBeat.h"
#include "Common.h"
#include "SipMessage.h"
#include "XmlParser.h"




//心跳代码，该目的是为了检查链路是否通畅


//SIP 下级的心跳保活就是不断发起事务，并根据收到的事件来判断心跳是否成功，链路是否正常。
int SipHeartBeat::m_snIndex = 0;

static void response_callback(void *token, pjsip_event *e)
{
    //从事件里找到它关联的那个 SIP 事务
    pjsip_transaction* tsx = e->body.tsx_state.tsx;
    GlobalCtl::SupDomainInfo* node = (GlobalCtl::SupDomainInfo*)token;
    if(tsx->status_code != 200)
    {
        node->registered = false;
    }
    return;
}

SipHeartBeat::SipHeartBeat()
{
    m_heartTimer = new TaskTimer(10);
}

SipHeartBeat::~SipHeartBeat()
{
    if(m_heartTimer)
    {
        delete m_heartTimer;
        m_heartTimer = NULL;
    }
}
-
void SipHeartBeat::gbHeartBeatServiceStart()
{
    if(m_heartTimer)
    {
        m_heartTimer->setTimerFun(HeartBeatProc,this);
        m_heartTimer->start();
    }
}

void SipHeartBeat::HeartBeatProc(void* param)
{
    //对注册在线的上级执行心跳包的发送
    SipHeartBeat* pthis = (SipHeartBeat*)param;
    AutoMutexLock lock(&(GlobalCtl::globalLock));
    GlobalCtl::SUPDOMAININFOLIST::iterator iter = GlobalCtl::instance()->getSupDomainInfoList().begin();
    for(;iter != GlobalCtl::instance()->getSupDomainInfoList().end(); iter++)
    {
        if(iter->registered)
        {
            if(pthis->gbHearteat(*iter)<0)
            {
                LOG(ERROR)<<"keep alive error for:"<<iter->sipId;
            }
        }
    }
}

int SipHeartBeat::gbHeartBeat(GlobalCtl::SupDomainInfo& node)
{
    //心跳包默认用UDP
    //这里的from和to与注册是不一样的
    SipMessage msg;
    //下级
    msg.setFrom((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());
    //上级
    msg.setTo((char*)node.sipId.c_str(),(char*)node.addrIp.c_str());
    msg.setUrl((char*)node.sipId.c_str(),(char*)node.addrIp.c_str(),node.sipPort);

    pj_str_t from = pj_str(msg.FromHeader());
    pj_str_t to = pj_str(msg.ToHeader());
    pj_str_t line = pj_str(msg.RequestUrl());
    //message是发送及时消息而不是请求
    string method = "MESSAGE";
    //在 PJSIP 中构造一个表示 MESSAGE 方法的 SIP 请求方法对象 reqMethod
    pjsip_method reqMethod = {PJSIP_OTHER_METHOD,{(char*)method.c_str(),method.length()}};
    pjsip_tx_data* tdata;
    //使用 reqMethod（如 MESSAGE）创建一个新的 SIP 请求报文，并把构造好的请求保存在 tdata 中，供后续发送使用
    //目的是给tdata赋值
    pj_status_t status = pjsip_endpt_create_request(GBOJ(gSipServer)->GetEndPoint(),&reqMethod,&line,&from,&to,NULL,NULL,-1,NULL,&tdata);
    if(PJ_SUCCESS != status)
    {
        LOG(ERROR)<<"pjsip_endpt_create_request ERROR";
        return -1;
    }


    //注册一般不需要body，如果血药body就自己添加
    //添加时候发送数据是tdata->pool，响应数据是rdata->tp_info.pool

    //body部分手动组织body部分
    char strIndex[16] = {0};
    sprintf(strIndex,"%d",m_snIndex++);
    #if 0
    //使用字符串拼接的方式写body
    string keepalive = "<?xml version=\"1.0\"?>\r\n";
    //Root Element
    keepalive += "<Notify>\r\n";
    keepalive += "<CmdType>keepalive</CmdType>\r\n";
    keepalive += "<SN>";
    keepalive += strIndex;
    keepalive += "</SN>\r\n";
    //本级的国标id
    keepalive += "<DeviceID>";
    keepalive += GBOJ(gConfig)->sipId();
    keepalive += "</DeviceID>\r\n";
    keepalive += "<Status>OK</Status>\r\n";
    keepalive += "</Notify>\r\n";
    #endif

    //是一个自定义的类，通常用于处理 XML 数据
    XmlParser parse;
    
    tinyxml2::XMLElement* rootNode = parse.AddRootode((char*)"Notify");
    parse.InsertSubNode(rootNode,(char*)"CmdType",(char*)"keepalive");
    parse.InsertSubNode(rootNode,(char*)"SN",strIndex);
    parse.InsertSubNode(rootNode,(char*)"DeviceID",GBOJ(gConfig)->sipId().c_str());
    parse.InsertSubNode(rootNode,(char*)"Status",(char*)"OK");
    char* xmlbuf = new char[1024];
    memset(xmlbuf,0,1024);
    //前面的parse就是m_doc
    parse.getXmlData(xmlbuf);
    //LOG(INFO)<<"xmlbuf:"<<xmlbuf;

    //设置主类型
    pj_str_t type = pj_str("Application");
    //设置子类型
    pj_str_t subtype = pj_str("MANSCDP+xml");
    //设置实际消息内容
    pj_str_t xmldata = pj_str(xmlbuf);
    tdata->msg->body = pjsip_msg_body_create(tdata->pool,&type,&subtype,&xmldata);

    status = pjsip_endpt_send_request(GBOJ(gSipServer)->GetEndPoint(),tdata,-1,&node,&response_callback);
    if(PJ_SUCCESS != status)
    {
        LOG(ERROR)<<"pjsip_endpt_send_request ERROR";
        return -1;
    }
    return 0;

}