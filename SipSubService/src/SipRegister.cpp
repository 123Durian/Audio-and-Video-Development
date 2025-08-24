 #include "SipRegister.h"
 #include "Common.h"
 #include "SipMessage.h"


 /*
 请求时候发送的消息格式：


REGISTER sip:192.168.1.10:5060 SIP/2.0
Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bK7f7a0fbb
Max-Forwards: 70
From: <sip:alice@192.168.1.100>;tag=12345
To: <sip:alice@192.168.1.100>
Call-ID: asd88asd77a@192.168.1.100
CSeq: 1 REGISTER
Contact: <sip:alice@192.168.1.100:5060>
Expires: 3600
User-Agent: PJSIP 2.10.0
Authorization: Digest username="alice", realm="example.com", nonce="abcdef1234567890", uri="sip:192.168.1.10:5060", response="d41d8cd98f00b204e9800998ecf8427e", algorithm=MD5
Content-Length: 0
 
 */



















 //sip注册的目的：SIP终端（摄像头，网关等）需要向SIP服务器注册其IP,端口，标识。
 //注册过程实际上是告知服务器：“我在这，别人可以通过这个地址联系我”

 //在SIP网络中，常常会采用分级框架
 
 
 static void client_cb(struct pjsip_regc_cbparam *param)
 {
    LOG(INFO)<<"code:"<<param->code;
    if(param->code == 200)//注册成功
    {
        GlobalCtl::SupDomainInfo* subinfo = (GlobalCtl::SupDomainInfo*)param->token;
        subinfo->registered =true;
    }
    return;
 }

 SipRegister::SipRegister()
 {

    m_regTimer = new TaskTimer(3);
    
 }
SipRegister::~SipRegister()
{
    if(m_regTimer)
    {
        delete m_regTimer;
        m_regTimer = NULL;
    }
}

//用于启动sip服务器的定时注册功能
void SipRegister::registerServiceStart()
{
    if(m_regTimer)
    {
        m_regTimer->setTimerFun(SipRegister::RegisterProc,(void*)this);
        m_regTimer->start();
    }
}



//通过上级的registered字段判断是否注册

//该代码传入的参数是SipRegister类型指针
void SipRegister::RegisterProc(void* param)
{
    SipRegister* pthis = (SipRegister*)param;
    //对全局互斥锁加锁，因为list共享

    //构造函数自动加锁
    //当 AutoMutexLock 对象生命周期结束，析构函数自动解锁
    AutoMutexLock lock(&(GlobalCtl::globalLock));
    //遍历上级SIP节点
    GlobalCtl::SUPDOMAININFOLIST::iterator iter = GlobalCtl::instance()->getSupDomainInfoList().begin();
    for(;iter != GlobalCtl::instance()->getSupDomainInfoList().end(); iter++)
    {
        //判断是否已经注册
        if(!(iter->registered))
        {
            if(pthis->gbRegister(*iter)<0)
            {
                LOG(ERROR)<<"register error for:"<<iter->sipId;
            }
        }
    }
}






int SipRegister::gbRegister(GlobalCtl::SupDomainInfo& node)
{
    SipMessage msg;
    //message header的from字段和to字段
    //from字段格式
    //<sip:id@ip:port>
    //to字段和from字段的内容是一样的
    //to是告诉对方自己的信息




//构造一个完整的SIP消息的关键头部字段，为后续发送注册或请求消息做准备


    //GBOJ表示本地
    msg.setFrom((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());
    msg.setTo((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());
    //node表示远端
    if(node.protocal == 1)//TCP协议
    {
        msg.setUrl((char*)node.sipId.c_str(),(char*)node.addrIp.c_str(),node.sipPort,(char*)"tcp");
    }
    else//UDP协议
    {
        msg.setUrl((char*)node.sipId.c_str(),(char*)node.addrIp.c_str(),node.sipPort);
    }
    msg.setContact((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str(),GBOJ(gConfig)->sipPort());
 

    //将上面数据转换成pj_str_t类型
    //把SipMessage类的字符串成员转换成PJSIP可以标识和操作的字符串格式
    //这样后续构建SIP消息，发送请求时，可以直接用pj_str_t类型变量
    pj_str_t from = pj_str(msg.FromHeader());
    pj_str_t to = pj_str(msg.ToHeader());
    pj_str_t line = pj_str(msg.RequestUrl());
    pj_str_t contact = pj_str(msg.Contact());

    //表示执行状态
    pj_status_t status = PJ_SUCCESS;
    do
    {


        //sip注册客户端结构体
        pjsip_regc* regc;

        //创建一个新的SIP注册客户端对象，为后续的SIP服务器发送注册请求做准备
        status = pjsip_regc_create(GBOJ(gSipServer)->GetEndPoint(),&node,&client_cb,&regc);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"pjsip_regc_create error";
            break;
        }

        //初始化一个已经创建好的注册客户端对象regc
        //为发送注册请求准备具体的消息头信息和参数

        //regc之前已经创建的注册客户都安对象指针
        //$line目的url
        //1表示注册请求
        //node.expires,注册过期时间
        status = pjsip_regc_init(regc,&line,&from,&to,1,&contact,node.expires);
        if(PJ_SUCCESS != status)
        {
            //失败就释放资源
            pjsip_regc_destroy(regc);
            LOG(ERROR)<<"pjsip_regc_init error";
            break;
        }
        //如果需要认证就设置认证信息


        //HA1：MD5(username : realm : password)
        //HA2：HA2 = MD5(method : uri)
        if(node.isAuth)
        {
            //下级需要配置上级指定的用户名和密码，每个上级用户名和密码不一样

            /*
            格式：
            Authorization: Digest username="alice",
               realm="example.com",
               nonce="d4f9c45a6d3c36b13be2d9d53a5e8e4a",
               uri="sip:example.com",
               response="3a1b67cc845d4f945e2b981bc0b21a8f",
               opaque="z1x2c3v4b5n6m7a8s9d0qwertyuioplk",
               algorithm=MD5
            */

            //获取上级用户名和密码等信息
            //定义一个认证信息的结构体
            pjsip_cred_info cred;
            //将结构体内数据清零
            pj_bzero(&cred,sizeof(pjsip_cred_info));
            //设置认证的方式（SIP中最常用的认证机制）
            cred.scheme = pj_str("digest");
            //认证域
            cred.realm = pj_str((char*)node.realm.c_str());
            //用户名
            cred.username = pj_str((char*)node.usr.c_str());
            //凭据类型
            cred.data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
            //密码
            cred.data = pj_str((char*)node.pwd.c_str());

            //为注册客户端设置认证凭据
            pjsip_regc_set_credentials(regc,1,&cred);
            if(PJ_SUCCESS != status)
            {
                pjsip_regc_destroy(regc);
                LOG(ERROR)<<"pjsip_regc_set_credentials error";
                break;
            }
        }
        //构造注册请求
        pjsip_tx_data* tdata = NULL;
        status = pjsip_regc_register(regc,PJ_TRUE,&tdata);
        if(PJ_SUCCESS != status)
        {
            pjsip_regc_destroy(regc);
            LOG(ERROR)<<"pjsip_regc_register error";
            break;
        }


        //发送注册请求
        //有一个回调函数，在执行成功之后会执行这个函数
        status = pjsip_regc_send(regc,tdata);
        if(PJ_SUCCESS != status)
        {
            pjsip_regc_destroy(regc);
            LOG(ERROR)<<"pjsip_regc_send error";
            break;
        }
    } while (0);
    
    int ret = 0;
    if(PJ_SUCCESS != status)
    {
        ret = -1;
    }
    return ret;
    

}