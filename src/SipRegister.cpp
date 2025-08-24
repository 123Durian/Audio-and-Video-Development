#include "SipRegister.h"
#include "Common.h"
#include "SipDef.h"
#include "GlobalCtl.h"


/*
sip注册上级响应格式
SIP/2.0 200 OK
Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bK7f7a0fbb
From: <sip:alice@192.168.1.100>;tag=12345
To: <sip:alice@192.168.1.100>;tag=xyz67890   ; ← 服务器补全 To 的 tag
Call-ID: asd88asd77a@192.168.1.100
CSeq: 1 REGISTER
Contact: <sip:alice@192.168.1.100:5060>;expires=3600
Date: Fri, 26 Jul 2025 03:00:00 GMT
Server: MySIPServer/1.0
Content-Length: 0

这里面中除了data剩下的都是自动填充的
通过代码： pj_status_t status = pjsip_endpt_create_response(GBOJ(gSipServer)->GetEndPoint(),rdata,status_code,NULL,&txdata);
*/




//回调函数只要是为了设置最后一个参数结构体
static pj_status_t auth_cred_callback(pj_pool_t *pool,
					    const pj_str_t *realm,
					    const pj_str_t *acc_name,
					    pjsip_cred_info *cred_info )
{
    //获取本地用户名
    pj_str_t usr = pj_str((char*)GBOJ(gConfig)->usr().c_str());
    if(pj_stricmp(acc_name,&usr) != 0)
    {
        LOG(ERROR)<<"usr name wrong";
        return PJ_FALSE;
    }
    //获取本地密码
    //将本地的信息存放到结构体中
    pj_str_t pwd = pj_str((char*)GBOJ(gConfig)->pwd().c_str());
    cred_info->realm = *realm;
    cred_info->username = usr;
    cred_info->data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    cred_info->data = pwd;

    return PJ_SUCCESS;
}

SipRegister::SipRegister()
{
    m_regTimer = new TaskTimer(10);
    // m_regTimer-setTimerFun(RegisterCheckProc,this);
    // m_regTimer->start();
}

SipRegister::~SipRegister()
{
    if(m_regTimer)
    {
        delete m_regTimer;
        m_regTimer = NULL;
    }
}

void SipRegister::registerServiceStart()
{
    if(m_regTimer)
    {
        m_regTimer->setTimerFun(RegisterCheckProc,this);
        m_regTimer->start();
    }
}

//上级定时检查下级
void SipRegister::RegisterCheckProc(void* param)
{
    //获取当前系统时间
    time_t regTime = 0;
    struct sysinfo info;
    memset(&info,0,sizeof(info));
    int ret = sysinfo(&info);
    if(ret == 0)
    {
        regTime = info.uptime;
    }
    else
    {
        regTime = time(NULL);
    }
    AutoMutexLock lock(&(GlobalCtl::globalLock));
    GlobalCtl::SUBDOMAININFOLIST::iterator iter = GlobalCtl::instance()->getSubDomainInfoList().begin();
    for(;iter != GlobalCtl::instance()->getSubDomainInfoList().end(); iter++)
    {
        if(iter->registered)
        {
            LOG(INFO)<<"regTime:"<<regTime<<",lastRegTime:"<<iter->lastRegTime;
            if(regTime - iter->lastRegTime >= iter->expires)
            {
                iter->registered = false;
                LOG(INFO)<<"register time was gone";
            }
        }
    }
//lasttime用于处理是否超过时限
}

//健全（是否需要认证）由上级SIP服务器决定，下级SIP客户端必须服从。
//接收注册的业务处理
pj_status_t SipRegister::run(pjsip_rx_data *rdata)
{
    return RegisterRequestMessage(rdata);
}

pj_status_t SipRegister::RegisterRequestMessage(pjsip_rx_data *rdata)
{
    pjsip_msg* msg = rdata->msg_info.msg;
    //判断是健全还是不健全的
    if(GlobalCtl::getAuth(parseFromId(msg)))
    {//健全
        return dealWithAuthorRegister(rdata);
    }
    else
    {//非健全
        return dealWithRegister(rdata);
    }
    
}


//健全注册
//首先判断当前上级给下级是否给了健全
pj_status_t SipRegister::dealWithAuthorRegister(pjsip_rx_data *rdata)
{
    pjsip_msg* msg = rdata->msg_info.msg;
    //读取fromID
    string fromId = parseFromId(msg);
    int expiresValue = 0;
    //链表头部
    pjsip_hdr hdr_list;
    pj_list_init(&hdr_list);
    //设置默认的sip响应码
    int status_code = 401;
    pj_status_t status;
    bool registered = false;
    //查找健全字段
    if(pjsip_msg_find_hdr(msg,PJSIP_H_AUTHORIZATION,NULL) == NULL)
    {
        //没找到的话发送401，就是让下级重新发送请求，包含这个字段
        //该结构体指针用于处理 SIP 中 401（未授权）响应中服务器返回的认证挑战信息。

        //创建401响应中WWW-Authenticate 头部结构体
        pjsip_www_authenticate_hdr* hdr = pjsip_www_authenticate_hdr_create(rdata->tp_info.pool);
        hdr->scheme = pj_str("digest");
        //nonce，参与认证响应，必须返回
        string nonce = GlobalCtl::randomNum(32);
        LOG(INFO)<<"nonce:"<<nonce;
        hdr->challenge.digest.nonce = pj_str((char*)nonce.c_str());
        //realm
        hdr->challenge.digest.realm = pj_str((char*)GBOJ(gConfig)->realm().c_str()); 
        //opaque，附加信息，不参与认证响应，也必须返回
        string opaque = GlobalCtl::randomNum(32);
        LOG(INFO)<<"opaque:"<<opaque;
        hdr->challenge.digest.opaque = pj_str((char*)opaque.c_str());
        //加密方式
        hdr->challenge.digest.algorithm = pj_str("MD5");



        //将该认证头部添加到头部链表中
        pj_list_push_back(&hdr_list,hdr);
    }
    else
    {
        //上级需要定义一个描述服务器身份验证的结构体
        //需要调用一个接口初始化这个结构体，并且还需要设置一个回调接口来进行本级身份验证参数的赋值



        //服务器收到后，发现 opaque 和之前发出的一致，可以确认这是之前挑战的合法回复。
        //保证客户端每次认证请求都是“新的”

        //初始化一个服务器认证对象
        pjsip_auth_srv auth_srv;
        //最后让这个结构体里面的认证信息与rdata进行对比
        pj_str_t realm = pj_str((char*)GBOJ(gConfig)->realm().c_str());

        //初始化一个SIP 服务器端认证处理器（auth_srv），用于后续对客户端请求中的 Authorization 信息进行验证。
        //auth_srv 去对一个 客户端请求中的 Authorization 头部进行验证，看对方提供的用户名/密码是否正确。


        //auth_cred_callback 是你写的回调函数指针，PJSIP 后续会调用它来根据用户名查询密码或者凭据。
        //auth_srv是一个输出参数
        //该代码：你会得到一个已经初始化好的认证服务器对象 auth_srv，接下来可以用它来验证客户端的认证请求。
        //auth_srv 对象已经准备好，它包含了你设置的认证域 realm 和获取密码的回调 auth_cred_callback。
        status = pjsip_auth_srv_init(rdata->tp_info.pool,&auth_srv,&realm,&auth_cred_callback,0);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"pjsip_auth_srv_init failed";
            status_code = 401;
        }

        //进行本级的身份信息与下级的健全信息进行验证
        //匹配认证头
        //匹配成功之后调用回调函数获得该用户的密码和hash值

        //标识已经认证成功
        //验证客户端请求中带过来的 Authorization 字段（用户名、密码摘要等）是否合法。


        /*
        该代码执行流程
        1：解析客户端的 Authorization 字段
        2：校验 realm 匹配
        3：调用你注册的回调函数获取密码
        4：拿到都关系之后重新计算HA1，HA2，和response
        5：将你算出的 response 与客户端 Authorization 中带来的 response 比较。
        6：处理 nonce/opaque 的有效性。如果不是有效的就重新发
        */
        pjsip_auth_srv_verify(&auth_srv,rdata,&status_code);
        LOG(INFO)<<"status_code:"<<status_code;
        if(SIP_SUCCESS == status_code)
        {
            //从msg中获取有效期限
            pjsip_expires_hdr* expires = (pjsip_expires_hdr*)pjsip_msg_find_hdr(msg,PJSIP_H_EXPIRES,NULL);
            expiresValue = expires->ivalue;
            GlobalCtl::setExpires(fromId,expiresValue);

            //data字段hdr部分组织
            time_t t;
            t = time(0);
            char bufT[32] = {0};
            strftime(bufT,sizeof(bufT),"%y-%m-%d%H:%M:%S",localtime(&t));
            pj_str_t value_time = pj_str(bufT);
            pj_str_t key = pj_str("Date");
            pjsip_date_hdr* date_hrd = (pjsip_date_hdr*)pjsip_date_hdr_create(rdata->tp_info.pool,&key,&value_time);
            pj_list_push_back(&hdr_list,date_hrd); 
            registered = true;

        }
    }

    //就是上面代码中hdr_list中只是定义了一个新添加的头部的，其它需要的pjsip_endpt_respond()会自动添加是吧
    status = pjsip_endpt_respond(GBOJ(gSipServer)->GetEndPoint(),NULL,rdata,status_code,NULL,&hdr_list,NULL,NULL);
    if(PJ_SUCCESS != status)
    {
        LOG(ERROR)<<"pjsip_endpt_respond failed";
        return status;
    }

    if(registered)
    {
        if(expiresValue > 0)
        {
            time_t regTime = 0;
            struct sysinfo info;
            memset(&info,0,sizeof(info));
            int ret = sysinfo(&info);
            if(ret == 0)
            {
                regTime = info.uptime;
            }
            else
            {
                regTime = time(NULL);
            }
            GlobalCtl::setRegister(fromId,true);
            GlobalCtl::setLastRegTime(fromId,regTime);
        }
        else if(expiresValue == 0)
        {
            GlobalCtl::setRegister(fromId,false);
            GlobalCtl::setLastRegTime(fromId,0);
        }
    }
}

pj_status_t SipRegister::dealWithRegister(pjsip_rx_data *rdata)
{
    //id是上级给下级分配的
    //先解析出下级ID
    string random = GlobalCtl::randomNum(32);
    LOG(INFO)<<"random:"<<random;

    pjsip_msg* msg = rdata->msg_info.msg;
    //初始化响应状态码
    int status_code = 200;
    //"alice@192.168.1.100"
    string fromId = parseFromId(msg);
    LOG(INFO)<<"fromId:"<<fromId;
    pj_int32_t expiresValue = 0;
    //检查id是否存在
    //判断是否是合法的下级节点
    if(!(GlobalCtl::checkIsExist(fromId)))
    {
        //不在的话资源禁止访问
        status_code = SIP_FORBIDDEN;
    }
    else
    {
        //id存在就要获取有效期
        pjsip_expires_hdr* expires = (pjsip_expires_hdr*)pjsip_msg_find_hdr(msg,PJSIP_H_EXPIRES,NULL);
        expiresValue = expires->ivalue;
        //给时效赋值
        //用于后面判断下级的有效期是不是到了
        GlobalCtl::setExpires(fromId,expiresValue);
    }

    //发送响应的结构
    pjsip_tx_data* txdata;
    //创建txdata数据结构
    //基于收到的请求和状态码创建响应消息
    //会智能的区分哪些头部字段需要自动复用，哪些需要手动添加或修改
    pj_status_t status = pjsip_endpt_create_response(GBOJ(gSipServer)->GetEndPoint(),rdata,status_code,NULL,&txdata);
    if(PJ_SUCCESS != status)
    {
        LOG(ERROR)<<"create response failed";
        return status;
    }

    //获取当前时间并格式化当前时间
    time_t t;
    //获取当前系统时间，返回的是秒数
    t = time(0);
    char bufT[32] = {0};
    //localtime(&t)：将 t 的时间戳转换为当前时区的本地时间，返回的是一个 tm* 类型的结构体
    strftime(bufT,sizeof(bufT),"%y-%m-%d%H:%M:%S",localtime(&t));

    //创建Key value字段
    pj_str_t value_time = pj_str(bufT);
    pj_str_t key = pj_str("Date");
    //创建一个header
    pjsip_date_hdr* date_hrd = (pjsip_date_hdr*)pjsip_date_hdr_create(rdata->tp_info.pool,&key,&value_time);
    //将该header插入到header list当中
    pjsip_msg_add_hdr(txdata->msg,(pjsip_hdr*)date_hrd);

    pjsip_response_addr res_addr;
    //获取响应的地址
    //代码会根据rdata自动解析出相应地址是吧
    status = pjsip_get_response_addr(txdata->pool,rdata,&res_addr);
    if(PJ_SUCCESS != status)
    {
        pjsip_tx_data_dec_ref(txdata);
        LOG(ERROR)<<"get response addr failed";
        return status;
    }
    //发送响应
    //GBOJ(gSipServer)->GetEndPoint()负责根据指定的目标地址res_addr直接发送响应消息txdata
    status = pjsip_endpt_send_response(GBOJ(gSipServer)->GetEndPoint(),&res_addr,txdata,NULL,NULL);
    if(PJ_SUCCESS != status)
    {
        pjsip_tx_data_dec_ref(txdata);
        LOG(ERROR)<<"send response msg failed";
        return status;
    }
    if(status_code == 200)
    {
        if(expiresValue > 0)
        {
            //注册
            //记录当前设备的注册时间，并更新其注册状态。
            time_t regTime = 0;//初始化时间变量
            struct sysinfo info;//用于获取系统信息
            memset(&info,0,sizeof(info));
            int ret = sysinfo(&info);
            if(ret == 0)//成功获取系统信息
            {
                regTime = info.uptime;
            }
            else
            {
                regTime = time(NULL);
            }
            //将其设置成true
            GlobalCtl::setRegister(fromId,true);
            GlobalCtl::setLastRegTime(fromId,regTime);
        }
        else if(expiresValue == 0)
        {
            //注销
            GlobalCtl::setRegister(fromId,false);
            GlobalCtl::setLastRegTime(fromId,0);
        }
    }

    return status;
}   