#include "SipRecordList.h"
#include "GlobalCtl.h"
#include "XmlParser.h"
#include "SipMessage.h"

SipRecordList::SipRecordList()
{

}

SipRecordList::~SipRecordList()
{

}

void SipRecordList::run(pjsip_rx_data *rdata)
{

    //首先解析上级发过来的recordinfo获取相应的字段值
    int sn = -1;
    string devid = "";
    //首先需要解析上级请求的recordinfo,获取相应的字段值，并回复
    resRecordInfo(rdata,sn,devid);  
    //然后再进行相应的recordinfo的推送，这里面其实就包含了内部对于前端查询和中心查询的业务，
    //和catalog一样，录像记录的返回肯定是通过其他模板查询返回的，
    sendRecordList(rdata,sn,devid);
}

void SipRecordList::resRecordInfo(pjsip_rx_data *rdata,int& sn,string& devid)
{
    int status_code = 200;
    pjsip_msg* msg = rdata->msg_info.msg;
    tinyxml2::XMLDocument* xml = new tinyxml2::XMLDocument();
    if(xml)
    {
        //解析data
        xml->Parse((char*)msg->body->data);
    }

    do
    {
        tinyxml2::XMLElement* pRootElement = xml->RootElement();
        if(!pRootElement)
        {
            status_code = 400;
            break;
        }
		
        //取出设备id
        tinyxml2::XMLElement* pElement = pRootElement->FirstChildElement("DeviceID");
        if(pElement && pElement->GetText())
            devid = pElement->GetText();
		


        //取出sn编号
		string strSn;
        pElement = pRootElement->FirstChildElement("SN");
        if(pElement)
            strSn = pElement->GetText();
        sn = atoi(strSn.c_str());

        //然后在这里需要把解析的sn和devid作为参数传出

        //对于IndistinctQuery字段的获取我们这里就不做了，这个和实际业务挂钩，我们默认就是模糊查询

    }while(0);

    pj_status_t status = pjsip_endpt_respond(GBOJ(gSipServer)->GetEndPoint(),NULL,rdata,status_code,NULL,NULL,NULL,NULL);
    if(PJ_SUCCESS != status)
    {
        LOG(ERROR)<<"pjsip_endpt_respond error";
    }
    return;
}


//下级实现recordinfo的推送
//我们推送的body部分需要对应的sn号和deviceid

//调用一个写死的回放记录用户演示
void SipRecordList::sendRecordList(pjsip_rx_data *rdata,int sn,string devid)
{
    //在这里其实就需要发送http请求调用内部的其他模块的接口从数据库获取相应的回放记录，在这里我们不涉及其他模块，所以省略，
    //如果有同学在实际项目中涉及到了，不懂的可以来问我
    //好   那么下面我们这里就发送一条写死的回放记录演示下就行
    XmlParser parse;
    tinyxml2::XMLElement* rootNode = parse.AddRootNode((char*)"Response");
    parse.InsertSubNode(rootNode,(char*)"CmdType",(char*)"RecordInfo");
    char tmpStr[32] = {0};
    sprintf(tmpStr,"%d",sn);
    parse.InsertSubNode(rootNode,(char*)"SN",tmpStr);
    parse.InsertSubNode(rootNode,(char*)"DeviceID",devid.c_str());
    memset(tmpStr,0,sizeof(tmpStr));
    sprintf(tmpStr,"%d",1);
    //表示录像总数
    parse.InsertSubNode(rootNode,(char*)"SumNum",tmpStr);
    tinyxml2::XMLElement* itemNode = parse.InsertSubNode(rootNode,(char*)"RecordList",(char*)"");
    memset(tmpStr,0,sizeof(tmpStr));
    sprintf(tmpStr,"%d",1);
    //表示RecordList中有多少条录像记录
    //是 <RecordList> 的一个属性
    //就是当前的是 <RecordList> 有多少个item
    parse.SetNodeAttributes(itemNode,(char*)"Num",tmpStr);

    tinyxml2::XMLElement* node = parse.InsertSubNode(itemNode,(char*)"item",(char*)"");
    parse.InsertSubNode(node,(char*)"DeviceID",(char*)devid.c_str());
    parse.InsertSubNode(node,(char*)"Name",(char*)devid.c_str());
    parse.InsertSubNode(node,(char*)"Type","time"); //在回复的时候就不能和上级的type一样了，
                                                    //内部查询后返回的是什么记录类型就是什么类型
    int sTime = 10;
    int eTime = 20;
	memset(tmpStr, 0, sizeof(tmpStr));
	sprintf(tmpStr, "%d", sTime);
	parse.InsertSubNode(node, (char*)"StartTime", tmpStr);
	
	memset(tmpStr, 0, sizeof(tmpStr));
	sprintf(tmpStr, "%d", eTime);
	parse.InsertSubNode(node, (char*)"EndTime", tmpStr);
    
    char* sendData = new char[BODY_SIZE];
    parse.getXmlData(sendData);

    //request
    pjsip_msg* msg = rdata->msg_info.msg;
    string fromId = parseFromId(msg);
    SipMessage sipMsg;
    AutoMutexLock lock(&(GlobalCtl::globalLock));
    GlobalCtl::SUPDOMAININFOLIST::iterator iter = GlobalCtl::instance()->getSupDomainInfoList().begin();
    for(;iter != GlobalCtl::instance()->getSupDomainInfoList().end(); iter++)
    {
        //找到是哪个上级请求的，找到之后再遍历头部节点
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
    pj_str_t xmldata = pj_str(sendData);=
    tdata->msg->body = pjsip_msg_body_create(tdata->pool,&type,&subtype,&xmldata);

    status = pjsip_endpt_send_request(GBOJ(gSipServer)->GetEndPoint(),tdata,-1,NULL,NULL);
    if(PJ_SUCCESS != status)
    {
        LOG(ERROR)<<"pjsip_endpt_send_request ERROR";
        return;
    }
    return;

}