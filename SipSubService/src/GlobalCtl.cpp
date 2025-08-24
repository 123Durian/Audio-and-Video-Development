#include "GlobalCtl.h"

//表示开始并没有创建实例
GlobalCtl* GlobalCtl::m_pInstance = NULL;
//隐士构造一个新的容器
GlobalCtl::SUPDOMAININFOLIST GlobalCtl::supDomainInfoList;
pthread_mutex_t GlobalCtl::globalLock = PTHREAD_MUTEX_INITIALIZER;
bool GlobalCtl::gStopPool = false;

//创建单例类
//全局类
GlobalCtl* GlobalCtl::instance()
{
    if(!m_pInstance)
    {
        m_pInstance = new GlobalCtl();
    }

    return m_pInstance;
}

bool GlobalCtl::init(void* param)
{
    //将传入的指针参数转换为特定类型的指针
    //传入参数是SipLocalConfigk类型的对象指针
    gConfig = (SipLocalConfig*)param;
    if(!gConfig)
    {
        return false;
    }
    

    //定义了一个变量类型是info
    SupDomainInfo info;
    //遍历链表
    //保存读取的上级节点的信息
    auto iter = gConfig->upNodeInfoList.begin();
    for(;iter != gConfig->upNodeInfoList.end();++iter)
    {
        info.sipId = iter->id;
        info.addrIp = iter->ip;
        info.sipPort = iter->port;
        info.protocal = iter->poto;
        info.expires = iter->expires;
        if(iter->auth)
        {
            info.isAuth = (iter->auth = 1)?true:false;
            info.usr = iter->usr;
            info.pwd = iter->pwd;
            info.realm = iter->realm;//上级给下级固定好的
        }
        supDomainInfoList.push_back(info);
    }
    LOG(INFO)<<"supDomainInfoList.SIZE:"<<supDomainInfoList.size();

    //创建线程池
    if(!gThPool)
    {
        gThPool =  new ThreadPool();
        gThPool->createThreadPool(10);
    }

    //创建SIP服务器对象
    if(!gSipServer)
    {
        gSipServer = new SipCore();
    }
    gSipServer->InitSip(gConfig->sipPort());//传入下级的端口号
    return true;
}

//根据传入的国标id提取出一个设备type
DevTypeCode GlobalCtl::getSipDevInfo(string id)
{
    DevTypeCode code_type = Error_code;
    string tmp = id.substr(10,3);
    int type = atoi(tmp.c_str());

    switch(type)
    {
        case Camera_Code:
            code_type = Camera_Code;
            break;
        case Ipc_Code:
            code_type = Ipc_Code;
            break;   
        default:
            code_type = Error_code;
            break;
    }

    return code_type;
}