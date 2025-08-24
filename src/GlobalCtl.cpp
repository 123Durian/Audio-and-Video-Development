#include "GlobalCtl.h"

GlobalCtl* GlobalCtl::m_pInstance = NULL;
GlobalCtl::SUBDOMAININFOLIST GlobalCtl::subDomainInfoList;
pthread_mutex_t GlobalCtl::globalLock = PTHREAD_MUTEX_INITIALIZER;
//static修饰的成员变量需要在.cpp中定义初始化
GlobalCtl::ListSession GlobalCtl::glistSession;
pthread_mutex_t GlobalCtl::gStreamLock = PTHREAD_MUTEX_INITIALIZER;
bool GlobalCtl::gStopPool = false;
bool GlobalCtl::gRcvIpc = false;
std::string GlobalCtl::catalogPayLoadIn;
GlobalCtl* GlobalCtl::instance()
{
    if(!m_pInstance)
    {
        m_pInstance = new GlobalCtl();
    }

    return m_pInstance;
}

//初始化GlobalCtl全局控制对象
bool GlobalCtl::init(void* param)
{
    gConfig = (SipLocalConfig*)param;
    if(!gConfig)
    {
        return false;
    }

    SubDomainInfo info;
    auto iter = gConfig->ubNodeInfoList.begin();
    for(;iter != gConfig->ubNodeInfoList.end();++iter)
    {
        info.sipId = iter->id;
        info.addrIp = iter->ip;
        info.sipPort = iter->port;
        info.protocal = iter->poto;
        info.auth = iter->auth;
        subDomainInfoList.push_back(info);
    }
    LOG(INFO)<<"supDomainInfoList.SIZE:"<<subDomainInfoList.size();

    if(!gThPool)
    {
        gThPool =  new ThreadPool();
        gThPool->createThreadPool(10);
    }

    if(!gSipServer)
    {
        gSipServer = new SipCore();
    }
    gSipServer->InitSip(gConfig->sipPort());
    return true;
}

//在本机list迭代器中查找发出请求的id
bool GlobalCtl::checkIsExist(string id)
{
    AutoMutexLock lck(&globalLock);
    SUBDOMAININFOLIST::iterator it;
    it = std::find(subDomainInfoList.begin(),subDomainInfoList.end(),id);
    if(it != subDomainInfoList.end())
    {
        return true;
    }
    return false;

}

bool GlobalCtl::checkIsVaild(string id)
{
    AutoMutexLock lck(&globalLock);
    SUBDOMAININFOLIST::iterator it;
    it = std::find(subDomainInfoList.begin(),subDomainInfoList.end(),id);
    if(it != subDomainInfoList.end() && it->registered)
    {
        return true;
    }
    return false;
}

void GlobalCtl::setExpires(string id,int expires)
{
    AutoMutexLock lck(&globalLock);
    SUBDOMAININFOLIST::iterator it;
    it = std::find(subDomainInfoList.begin(),subDomainInfoList.end(),id);
    if(it != subDomainInfoList.end())
    {
        it->expires = expires;
    }
}

void GlobalCtl::setRegister(string id,bool registered)
{
    AutoMutexLock lck(&globalLock);
    SUBDOMAININFOLIST::iterator it;
    it = std::find(subDomainInfoList.begin(),subDomainInfoList.end(),id);
    if(it != subDomainInfoList.end())
    {
        it->registered = registered;
    }
}

void GlobalCtl::setLastRegTime(string id,time_t t)
{
    AutoMutexLock lck(&globalLock);
    SUBDOMAININFOLIST::iterator it;
    it = std::find(subDomainInfoList.begin(),subDomainInfoList.end(),id);
    if(it != subDomainInfoList.end())
    {
        it->lastRegTime = t;
    }
}

bool GlobalCtl::getAuth(string id)
{
    AutoMutexLock lck(&globalLock);
    SUBDOMAININFOLIST::iterator it;
    it = std::find(subDomainInfoList.begin(),subDomainInfoList.end(),id);
    if(it != subDomainInfoList.end())
    {
        return it->auth;
    }
}


//返回传入参数个数的随机数
string GlobalCtl::randomNum(int length)
{
    #if 0
    //随机数种子
    //从操作系统或者硬件中获取真实的随机数种子，这里随机数种子每次的值都不一样
    random_device rd;
    //随机数生成器，该计算出不一样的值
    mt19937 gen(rd());
    //分布器类模板  设定一个0-15的均匀整数分布的区间范围
    uniform_int_distribution<> dis(0,15);
    stringstream ss;
    for(int i =0;i<length;++i)
    {
        //每次使用随机数生成器在指定的分布范围内获取一个随机数
        int value = dis(gen);
        //转换成16进制
        ss<< std::hex << value;
    }
    #endif
    stringstream ss;
    for(int i =0;i<length;++i)
    {   
        int value = random()%15;
        ss<< std::hex << value;
    }
    
    return ss.str();
}

//判断下级推送过来的设备是什么是吧
DevTypeCode GlobalCtl::getSipDevInfo(string id)
{
    DevTypeCode code_type = Error_code;
    string tmp = id.substr(10,3);
    //转换成int类型的
    int type = atoi(tmp.c_str());

    switch(type)
    {
        case Camera_Code://摄像头设备
            code_type = Camera_Code;
            break;
        case Ipc_Code://IPC设备
            code_type = Ipc_Code;
            break;   
        default:
            code_type = Error_code;
            break;
    }

    return code_type;
}