#include "GetPlamtInfo.h"
#include "GlobalCtl.h"

GetPlamtInfo::GetPlamtInfo(struct bufferevent* bev)
    :ThreadTask(bev)
{

}
void GetPlamtInfo::run()
{
    //从存放下级平台信息的list当中摘出下级信息并返还给客户端
    Json::Value jsonIn;
    int index = 0;
    AutoMutexLock lock(&(GlobalCtl::globalLock));
    GlobalCtl::SUBDOMAININFOLIST::iterator iter = GlobalCtl::instance()->getSubDomainInfoList().begin();
    for(;iter != GlobalCtl::instance()->getSubDomainInfoList().end(); iter++)
    {
        if(iter->registered)
        {
            //json数据组织
            //json数据的存放字段也是需要和客户端协商之后决定的
            jsonIn["SubDomain"][index]["sipId"] = iter->sipId;
            index++;
        }
    }

    //完了之后需要将json字段压缩成字符串传递给client

    //用于快速将json转换成字符串
    Json::FastWriter fase_writer;
    //转换成字符串保存到payLoadIn中
    string payLoadIn = fase_writer.write(jsonIn);
    LOG(INFO)<<"payLoadIn:"<<payLoadIn;
    int bodyLen = payLoadIn.length();
    int len = sizeof(int)+bodyLen;
    char* buf = new char[len];
    //将bodylen拷贝到前四个字节
    memcpy(buf,&bodyLen,sizeof(int));
    //字符串拷贝
    memcpy(buf+sizeof(int),payLoadIn.c_str(),bodyLen);
    //发送数据
    bufferevent_write(m_bev,buf,len);
    delete buf;
}