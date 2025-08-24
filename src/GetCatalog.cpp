#include "GetCatalog.h"
#include "GlobalCtl.h"
#include "SipMessage.h"
#include "XmlParser.h"
GetCatalog::GetCatalog(struct bufferevent* bev)
    :ThreadTask(bev)
{
    //DirectoryGetPro(NULL);
}

GetCatalog::~GetCatalog()
{

}

void GetCatalog::run()
{
    //调用 DirectoryGetPro 函数（参数为 NULL）获取设备目录数据
    //轮询的给下级发送请求
    DirectoryGetPro(NULL);
    //等待线程中其它任务执行结束
    //接收和向客户端发送数据是两个线程，所以要等待

    ////重新设置信号量用于等待
    GBOJ(gThPool)->waitInfo();
    GlobalCtl::get_global_mutex();
    //获取全局变量中存储目录字节的字节数
    //gCatalogPayload表示所有设备的信息
    int bodyLen = GlobalCtl::gCatalogPayload.length();
    int len = sizeof(int)+bodyLen;
    char* buf = new char[len];
    memcpy(buf,&bodyLen,sizeof(int));
    memcpy(buf+sizeof(int),GlobalCtl::gCatalogPayload.c_str(),bodyLen);
    bufferevent_write(m_bev,buf,len);
    GlobalCtl::free_global_mutex();
    delete buf;

    return;
}



//发送目录数请求

//轮询的遍历每个下级，给每个注册在线的下级发起目录数请求
//如果一个包发送失败他会返回尝试几次的
void GetCatalog::DirectoryGetPro(void* param)
{
    SipMessage msg;
    XmlParser parse;
    AutoMutexLock lock(&(GlobalCtl::globalLock));
    GlobalCtl::SUBDOMAININFOLIST::iterator iter = GlobalCtl::instance()->getSubDomainInfoList().begin();
    for(;iter != GlobalCtl::instance()->getSubDomainInfoList().end(); iter++)
    {
        if(iter->registered)
        {
            //上级
            msg.setFrom((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());
            //下级
            msg.setTo((char*)iter->sipId.c_str(),(char*)iter->addrIp.c_str());
            msg.setUrl((char*)iter->sipId.c_str(),(char*)iter->addrIp.c_str(),iter->sipPort);

            pj_str_t from = pj_str(msg.FromHeader());
            pj_str_t to = pj_str(msg.ToHeader());
            pj_str_t requestUrl = pj_str(msg.RequestUrl());

            string method = "MESSAGE";
            pjsip_method reqMethod = {PJSIP_OTHER_METHOD,{(char*)method.c_str(),method.length()}};
            pjsip_tx_data* tdata;
            pj_status_t status = pjsip_endpt_create_request(GBOJ(gSipServer)->GetEndPoint(),&reqMethod,&requestUrl,&from,&to,NULL,NULL,-1,NULL,&tdata);
            if(PJ_SUCCESS != status)
            {
                LOG(ERROR)<<"pjsip_endpt_create_request ERROR";
                return;
            }

            //添加body
            tinyxml2::XMLElement* rootNode = parse.AddRootNode((char*)"Query");
            parse.InsertSubNode(rootNode,(char*)"CmdType",(char*)"Catalog");
            //随机一个sn号
            int sn = random() % 1024;
            char tmpStr[32] = {0};
            sprintf(tmpStr,"%d",sn);
            parse.InsertSubNode(rootNode,(char*)"SN",tmpStr);
            //下级中心信令的ID
            parse.InsertSubNode(rootNode,(char*)"DeviceID",iter->sipId.c_str());
            char* xmlbuf = new char[1024];
            memset(xmlbuf,0,1024);
            //获取上面缩写的xml的格式
            parse.getXmlData(xmlbuf);
            LOG(INFO)<<"xml:"<<xmlbuf;
            
            pj_str_t type = pj_str("Application");
            pj_str_t subtype = pj_str("MANSCDP+xml");
            pj_str_t xmldata = pj_str(xmlbuf);
            tdata->msg->body = pjsip_msg_body_create(tdata->pool,&type,&subtype,&xmldata);

            status = pjsip_endpt_send_request(GBOJ(gSipServer)->GetEndPoint(),tdata,-1,NULL,NULL);
            if(PJ_SUCCESS != status)
            {
                LOG(ERROR)<<"pjsip_endpt_send_request ERROR";
                return;
            }
        
        }
    }
    return;
}