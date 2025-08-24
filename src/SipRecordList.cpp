#include "SipRecordList.h"
#include "XmlParser.h"
#include "SipDef.h"
#include "GlobalCtl.h"


//上级解析下级推送的回放记录负载的item
SipRecordList::SipRecordList(tinyxml2::XMLElement* root)
    :SipTaskBase()
{
    m_pRootElement = root;
}

SipRecordList::~SipRecordList()
{

}

pj_status_t SipRecordList::run(pjsip_rx_data *rdata)
{
    int status_code = SIP_SUCCESS;
    //解析message-body-xml数据
    SaveRecordList(status_code);
    //响应
    pj_status_t status = pjsip_endpt_respond(GBOJ(gSipServer)->GetEndPoint(),NULL,rdata,status_code,NULL,NULL,NULL,NULL);
    if(PJ_SUCCESS != status)
    {
        LOG(ERROR)<<"pjsip_endpt_respond error";
    }

    return status;
}

//对body进行解析并保存我们所需要字段的值
void SipRecordList::SaveRecordList(int& status_code)
{
    //接收到的根节点
    tinyxml2::XMLElement* pRootElement = m_pRootElement;
    if(!pRootElement)
    {
        status_code = SIP_BADREQUEST;
        return;
    }

    //定义变量
    string strSumNum,strDeviceID,strName,strStartTime,strEndTime,strType;

    //定义item中的字段
    tinyxml2::XMLElement* pElement = pRootElement->FirstChildElement("SumNum");
    if(pElement && pElement->GetText())
        strSumNum = pElement->GetText();


    pElement = pRootElement->FirstChildElement("RecordList");
    if(pElement)
    {
        tinyxml2::XMLElement* pItem = pElement->FirstChildElement("item");
        while(pItem)
        {
            tinyxml2::XMLElement* pChild = pItem->FirstChildElement("DeviceID");
            if(pChild && pChild->GetText())
            {
                strDeviceID = pChild->GetText();
            }            

            pChild = pItem->FirstChildElement("Name");
            if(pChild && pChild->GetText())
                strName = pChild->GetText();
            
            pChild = pItem->FirstChildElement("StartTime");
            if(pChild && pChild->GetText())
                strStartTime = pChild->GetText();
            
            pChild = pItem->FirstChildElement("EndTime");
            if(pChild && pChild->GetText())
                strEndTime = pChild->GetText();

            pChild = pItem->FirstChildElement("Type");
            if(pChild && pChild->GetText())
                strType = pChild->GetText();


                //将指针 pItem 移动到当前 XML 元素节点的下一个同级（兄弟）元素节点。
            pItem = pItem->NextSiblingElement();
        }

    }
    LOG(INFO)<<"strSumNum:"<<strSumNum<<",strDeviceID:"<<strDeviceID<<",strName:"<<strName\
    <<",strStartTime:"<<strStartTime<<",strEndTime:"<<strEndTime<<",strType:"<<strType;

    return;
}