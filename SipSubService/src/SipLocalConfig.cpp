#include "SipLocalConfig.h"

//该文件主要功能
//读取本地和SIP服务器配置信息（上级节点），类似于微信总部的登陆服务器
//动态管理RTP端口
//读取上级节点信息列表（可以中继，转发等），是可以连向总部的多个中转



//读取配置文件路径
#define CONFIGFILE_PATH "../../conf/SipSubService.conf"

//配置文件中的宏定义
#define LOCAL_SECTION "localserver"
#define SIP_SECTION "sipserver"


//定义了一组常量字符串，通常用于作为配置项的key
static const string keyLocalIp = "local_ip";
static const string keyLocalPort = "local_port";
static const string keySipId = "sip_id";
static const string keySipIp = "sip_ip";
static const string keySipPort = "sip_port";
static const std::string keyRtpPortBegin   = "rtp_port_begin";
static const std::string keyRtpPortEnd   = "rtp_port_end";

static const string keySupNodeNum = "supnode_num";
static const string keySupNodeId = "sip_supnode_id";
static const string keySupNodeIp = "sip_supnode_ip";
static const string keySupNodePort = "sip_supnode_port";
static const string keySupNodePoto = "sip_supnode_poto";
static const string keySupNodeExpires = "sip_supnode_expires";
static const string keySupNodeAuth = "sip_supnode_auth";
static const string keySupNodeUsr = "sip_supnode_usr";
static const string keySupNodePwd = "sip_supnode_pwd";
static const string keySupNodeRealm = "sip_supnode_realm";

SipLocalConfig::SipLocalConfig()
:m_conf(CONFIGFILE_PATH)
{
    m_localIp = "";
    m_localPort = 0;
    m_sipId = "";
    m_sipIp = "";
    m_sipPort = 0;
    m_supNodeIp = "";
    m_supNodePort = 0;
    m_supNodePoto = 0;
    m_supNodeAuth = 0;
}
SipLocalConfig::~SipLocalConfig()
{

}

int SipLocalConfig::ReadConf()
{
    int ret = 0;
    m_conf.setSection(LOCAL_SECTION);
    //读取本地的ip
    m_localIp = m_conf.readStr(keyLocalIp);
    if(m_localIp.empty())
    {
        ret = -1;
        LOG(ERROR)<<"localIp is wrong";
        return ret;
    }
    //读取本地的端口
    m_localPort = m_conf.readInt(keyLocalPort);
    if(m_localPort <= 0)
    {
        ret = -1;
        LOG(ERROR)<<"localPort is wrong";
        return ret;
    }

    m_conf.setSection(SIP_SECTION);、
    //读取SIP服务器的id,ip,port
    m_sipId = m_conf.readStr(keySipId);
    if(m_sipId.empty())
    {
        ret = -1;
        LOG(ERROR)<<"sipId is wrong";
        return ret;
    }

    m_sipIp = m_conf.readStr(keySipIp);
    if(m_sipIp.empty())
    {
        ret = -1;
        LOG(ERROR)<<"sipIp is wrong";
        return ret;
    }
    m_sipPort = m_conf.readInt(keySipPort);
    if(m_sipPort <= 0)
    {
        ret = -1;
        LOG(ERROR)<<"sipPort is wrong";
        return ret;
    }
    LOG(INFO)<<"localip:"<<m_localIp<<",localport:"<<m_localPort<<",sipid:"<<m_sipId<<",sipip:"<<m_sipIp\
    <<",sipport:"<<m_sipPort;
	
    //读取RTP的起始端口和结束端口
	m_rtpPortBegin = m_conf.readInt(keyRtpPortBegin);
    if(m_rtpPortBegin<=0)
    {
       ret =-1;
       LOG(ERROR)<<"rtpPortBegin is NULL";
       return ret;
    }

    m_rtpPortEnd = m_conf.readInt(keyRtpPortEnd);
    if(m_rtpPortEnd<=0)
    {
       ret =-1;
       LOG(ERROR)<<"rtpPortEnd is NULL";
       return ret;
    }
	initRandPort();
	
    //读取上级节点的配置
    int num = m_conf.readInt(keySupNodeNum);
    LOG(INFO)<<"num:"<<num;
    SupNodeInfo info;
    for(int i = 1;i<num+1;++i)
    {
        //进行拼接操作
        string id = keySupNodeId + to_string(i);
        string ip = keySupNodeIp + to_string(i);
        string port = keySupNodePort + to_string(i);
        string poto = keySupNodePoto + to_string(i);
        string auth = keySupNodeAuth + to_string(i);
        string expires = keySupNodeExpires + to_string(i);
        string usr = keySupNodeUsr+to_string(i);
        string pwd = keySupNodePwd+to_string(i);
        string realm = keySupNodeRealm + to_string(i);
        
        info.id = m_conf.readStr(id);
        info.ip = m_conf.readStr(ip);
        info.port = m_conf.readInt(port);
        info.poto = m_conf.readInt(poto);
        LOG(INFO)<<"info.poto:"<<info.poto;
        info.expires = m_conf.readInt(expires);
        info.usr = m_conf.readStr(usr);
        info.pwd = m_conf.readStr(pwd);
        info.auth = m_conf.readInt(auth);
        info.realm = m_conf.readStr(realm);
        upNodeInfoList.push_back(info);

        // LOG(INFO)<<"subnodeip:"<<m_supNodeIp<<",subnodeport:"<<m_supNodePort\
        // <<",subnodepoto:"<<m_supNodePoto<<",subnodeauth:"<<m_supNodeAuth;
    }
    LOG(INFO)<<"upNodeInfoList.SIZE:"<<upNodeInfoList.size();


    return ret;
}

void SipLocalConfig::initRandPort()
{
    //先将配置的起始端口和2做个取余，如果有余那么代表配置的起始端口为奇数，那么就做++
    while(m_rtpPortBegin % 2)  
    {
        m_rtpPortBegin++;
    }

    //队列中最后的端口反着来，和2做个整除，代表配置的rtpPortEnd为偶数，那么我们就做--
    while(m_rtpPortEnd % 2 == 0)
    {
        m_rtpPortEnd--;
    }

    //下面就将起始和结束端口的区间放入到队列中
    AutoMutexLock lck(&m_rtpPortLock);
    for(int i=m_rtpPortBegin;i< (m_rtpPortEnd + 1);i++)
    {
        m_RandNum.push(i);
    }
    LOG(INFO)<<"rand size:"<<m_RandNum.size();
}

//下面需要定义接口来取出rtp端口和回收rtp端口
//从队列中取出一个rtp port，取出一个偶数号
int SipLocalConfig::popOneRandNum()
{
    int rtpPort = 0;
    AutoMutexLock lck(&m_rtpPortLock);
    if(m_RandNum.size()>0)
    {
        //取完一个就扔出队列，每次取两个号
        rtpPort = m_RandNum.front();
        m_RandNum.pop();
        if(rtpPort % 2) //这里再做下判断
        {
            //如果第一个是奇数，那么重新取
            rtpPort = m_RandNum.front();
            m_RandNum.pop();
        }
        //取第二个号
        m_RandNum.pop();
    }
    return rtpPort;
}

//回收rtp port
int SipLocalConfig::pushOneRandNum(int num)
{
    if(num < m_rtpPortBegin || num > m_rtpPortEnd)
    {
        return -1;
    }
    AutoMutexLock lck(&m_rtpPortLock);
    m_RandNum.push(num);
    m_RandNum.push(num+1);
	LOG(INFO)<<"push rtp port:"<<num<<",rtcp port:"<<num+1;
	return 0;
}