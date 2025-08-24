#include "SipDirectory.h"
#include "GlobalCtl.h"
#include <fstream>
#include <sstream>
#include "SipMessage.h"
#include "XmlParser.h"


SipDirectory::SipDirectory(tinyxml2::XMLElement* root)
	:SipTaskBase()
{
	m_pRootElement = root;
}

SipDirectory::~SipDirectory()
{

}


//先响应再推送

void SipDirectory::run(pjsip_rx_data *rdata)
{
	LOG(INFO)<<"SipDirectory::run";
    //这个sn要交给其它接口
    int sn = -1;
    //解析请求
    resDir(rdata,&sn);
    //需要给上级推送目录设备资源
    //数据的来源---数据库
    //数据库中的数据是下级的客户端
    //需要有一个模块专门实现数据库相关的业务，对对外提供数据库增删改查的接口
    //目前使用golong技术栈做数据业务很普遍
    //所以我们的国标服务器会和golong的模块进行交互

    

    //实际中：libcurl  http/https协议去请求  golang 接口，返回数据库中共享资源表中的数据

    //这里是json文件保存到本地，里面是目录树文件的数据


    //json  xml  protobuf   .proto
    Json::Value jsonOut;
    //读取本地的json文件
    directoryQuery(jsonOut);

    //有了json数据现在就是要推送了

    /*
    Device ListNum=2,表示推送两个节点即两个item
    我们发送的一个message包只包含一个
    下级在推送目录数的时候可以选择传输层协议是UDP还是TCP，实际生成中目录数的结构很多，为了高效传输尽量选择UDP传输
    如果选择TCP那么上级在接收的时候会一直处于加载的状态，影响比较大。
    如果选择UDP一个message中包含了多个item的话，一个message的长度可能会大于一个MTU的值。会在网络层进行分片
    但是UDP是一个不可靠传输，就是不提供丢包检测和重传机制，如果分片时丢包了，上级就收不到一个完整的message。
    */

    //要发送的数据
    char* sendData = new char[BODY_SIZE];
    //nodeInfo的个数
    int sum = jsonOut["data"]["nodeInfo"].size();
    int begin = 0;
    int sendCnt = 1;
    while(begin<sum)
    {
        memset(sendData,0,BODY_SIZE);
        constructMANSCDPXml(jsonOut["data"]["nodeInfo"],&begin,sendCnt,sendData,sn);
        sendSipDirMsg(rdata,sendData);
        usleep(15*1000);
    }

    return;
    


}


//将一个 JSON 格式的设备信息列表（listdata）转化为符合 MANSCDP 协议的 XML 目录响应结构，并写入 sendData 缓冲区中。
//list中可能有多个元素
void SipDirectory::constructMANSCDPXml(Json::Value listdata,int* begin,int itemCount,char* sendData,int sn)
{
    //设置上级请求时候的参数
    XmlParser parse;
    tinyxml2::XMLElement* rootNode = parse.AddRootNode((char*)"Response");
    parse.InsertSubNode(rootNode,(char*)"CmdType",(char*)"Catalog");
    char tmpStr[32] = {0};
    sprintf(tmpStr,"%d",sn);
    parse.InsertSubNode(rootNode,(char*)"SN",tmpStr);
    parse.InsertSubNode(rootNode,(char*)"DeviceID",GBOJ(gConfig)->sipId().c_str());

    //推送的目录数一共有多少个节点
    memset(tmpStr,0,sizeof(tmpStr));
    sprintf(tmpStr,"%d",listdata.size());
    parse.InsertSubNode(rootNode,(char*)"SumNum",tmpStr);

    //当前的message有多少个节点
    tinyxml2::XMLElement* itemNode = parse.InsertSubNode(rootNode,(char*)"DeviceList",(char*)"");
    memset(tmpStr,0,sizeof(tmpStr));
    sprintf(tmpStr,"%d",itemCount);
    //设置上面节点的属性
    parse.SetNodeAttributes(itemNode,(char*)"Num",tmpStr);
    //从某个位置开始，最多遍历 itemCount 个元素，或者在列表末尾前停止。

/*
大写的device是服务器对应的id，小写的device是国标id
*/
    int i = *begin;
    int index = 0;
    for(;i<listdata.size();i++)
    {
        if(index == itemCount)
        {
            break;
        }

        Json::Value &devNode = listdata[i];
        tinyxml2::XMLElement* node = parse.InsertSubNode(itemNode,(char*)"item",(char*)"");

        //asString().c_str() 是两个函数的链式调用，作用是：将 JSON 字段的值转换为 C 风格字符串（const char*）。
        parse.InsertSubNode(node,(char*)"DeviceID",(char*)devNode["deviceID"].asString().c_str());
        parse.InsertSubNode(node,(char*)"Name",(char*)devNode["name"].asString().c_str());
        //当为设备时  为必选
        if(devNode["manufacturer"] == "")
        {
            devNode["manufacturer"] = "unknow";
        }
        parse.InsertSubNode(node,(char*)"Manufacturer",(char*)devNode["manufacturer"].asString().c_str());
        //为设备时  必选  设备型号
        if(devNode["model"] == "")
        {
            devNode["model"] = "unknow";
        }
        parse.InsertSubNode(node,(char*)"Model",(char*)devNode["model"].asString().c_str());

        parse.InsertSubNode(node,(char*)"Owner",(char*)"unknow");
        
        //行政区域，国标ID的前几个
        string civilCode = devNode["deviceID"].asString().substr(0,6);
        parse.InsertSubNode(node,(char*)"CivilCode",(char*)civilCode.c_str());

        char info[32] = {0};
        int parental = devNode["parental"].asInt();
        sprintf(info,"%d",parental);
        parse.InsertSubNode(node,(char*)"Parental",info);

        parse.InsertSubNode(node,(char*)"ParentID",(char*)devNode["parentID"].asString().c_str());
        
        int safeway = devNode["safetyWay"].asInt();
        memset(info,0,32);
        sprintf(info,"%d",safeway);
        parse.InsertSubNode(node,(char*)"SafetyWay",info);

        int registerway = devNode["registerWay"].asInt();
        memset(info,0,32);
        sprintf(info,"%d",registerway);
        parse.InsertSubNode(node,(char*)"RegisterWay",info);

        int secrecy = devNode["secrecy"].asInt();
        memset(info,0,32);
        sprintf(info,"%d",secrecy);
        parse.InsertSubNode(node,(char*)"Secrecy",info);

        parse.InsertSubNode(node,(char*)"Status",(char*)devNode["status"].asString().c_str());
        
        index++;
    }
    *begin = i;
    parse.getXmlData(sendData);

    return;
}


void SipDirectory::sendSipDirMsg(pjsip_rx_data *rdata,char* sendData)
{
    LOG(INFO)<<sendData;
    pjsip_msg* msg = rdata->msg_info.msg;
    string fromId = parseFromId(msg);
    SipMessage sipMsg;
    AutoMutexLock lock(&(GlobalCtl::globalLock));
    GlobalCtl::SUPDOMAININFOLIST::iterator iter = GlobalCtl::instance()->getSupDomainInfoList().begin();
    for(;iter != GlobalCtl::instance()->getSupDomainInfoList().end(); iter++)
    {
        if(iter->sipId == fromId)
        {
            sipMsg.setFrom((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());
            sipMsg.setTo((char*)iter->sipId.c_str(),(char*)iter->addrIp.c_str());
            sipMsg.setUrl((char*)iter->sipId.c_str(),(char*)iter->addrIp.c_str(),iter->sipPort);
        }
        else
        {
            return;
        }
    }

    pj_str_t from = pj_str(sipMsg.FromHeader());
    pj_str_t to = pj_str(sipMsg.ToHeader());
    pj_str_t line = pj_str(sipMsg.RequestUrl());
    string method = "MESSAGE";
    pjsip_method reqMethod = {PJSIP_OTHER_METHOD,{(char*)method.c_str(),method.length()}};
    pjsip_tx_data* tdata;
    pj_status_t status = pjsip_endpt_create_request(GBOJ(gSipServer)->GetEndPoint(),&reqMethod,&line,&from,&to,NULL,NULL,-1,NULL,&tdata);
    if(PJ_SUCCESS != status)
    {
        LOG(ERROR)<<"pjsip_endpt_create_request ERROR";
        return ;
    }

    pj_str_t type = pj_str("Application");
    pj_str_t subtype = pj_str("MANSCDP+xml");
    pj_str_t xmldata = pj_str(sendData);
    tdata->msg->body = pjsip_msg_body_create(tdata->pool,&type,&subtype,&xmldata);

    status = pjsip_endpt_send_request(GBOJ(gSipServer)->GetEndPoint(),tdata,-1,NULL,NULL);
    if(PJ_SUCCESS != status)
    {
        LOG(ERROR)<<"pjsip_endpt_send_request ERROR";
        return;
    }
    return;
}


//解析 SIP 请求中的 XML 内容，检查设备 ID 是否匹配，并提取序列号 SN，随后发送 SIP 响应。
//发送请求时主要添加了两个东西，一个是设备号一个是SN,在这里判断设备号并获取SN，随机发出响应
void SipDirectory::resDir(pjsip_rx_data *rdata,int* sn)
{
    int status_code = 200;

    do
    {
        //给根节点赋值
        tinyxml2::XMLElement* pRootElement = m_pRootElement;
        if(!pRootElement)
        {
            status_code = 400;
            break;
        }

        //设备号的判断
        string devid,strSn;
        tinyxml2::XMLElement* pElement = pRootElement->FirstChildElement("DeviceID");
        if(pElement && pElement->GetText())
            devid = pElement->GetText();
        
        if(devid != GBOJ(gConfig)->sipId())
        {
            status_code = 400;
            break;
        }

        //获取SN
        pElement = pRootElement->FirstChildElement("SN");
        if(pElement)
            strSn = pElement->GetText();
        *sn = atoi(strSn.c_str());

    }while(0);

    pj_status_t status = pjsip_endpt_respond(GBOJ(gSipServer)->GetEndPoint(),NULL,rdata,status_code,NULL,NULL,NULL,NULL);
    if(PJ_SUCCESS != status)
    {
        LOG(ERROR)<<"pjsip_endpt_respond error";
    }
    return;

}

void SipDirectory::directoryQuery(Json::Value& jsonOut)
{
    std::ifstream file("../../conf/catalog.json");
    std::stringstream buffer;
    //将整个文件内容读入字符串流 buffer
    buffer<<file.rdbuf();
    //将字符串流 buffer 中的全部内容提取出来，保存为一个 std::string 类型的变量 payload。
    string payload = buffer.str();

    //尝试将字符串 payload 解析为 JSON 格式，并把结果写入 jsonOut；如果解析失败，就记录错误日志。
    if(JsonParse(payload).toJson(jsonOut) == false)
    {
        LOG(ERROR)<<"JsonParse error";
    }
    return;
}