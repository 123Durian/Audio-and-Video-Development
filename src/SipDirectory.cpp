#include "SipDirectory.h"
#include "SipDef.h"
#include "GlobalCtl.h"


Json::Value SipDirectory::m_jsonIn;
int SipDirectory::m_jsonIndex = 0;
SipDirectory::SipDirectory(tinyxml2::XMLElement* root)
    :SipTaskBase()
{
    m_pRootElement = root;
}

SipDirectory::~SipDirectory()
{

}

//收到下级的目录数响应的时候执行这个代码


pj_status_t SipDirectory::run(pjsip_rx_data *rdata)
{
    int status_code = SIP_SUCCESS;
    //解析message-body-xml数据
    //数据解析出来之后保存到json中，之后在上级客户端中进行展示
    SaveDir(status_code);
    //响应
    pj_status_t status = pjsip_endpt_respond(GBOJ(gSipServer)->GetEndPoint(),NULL,rdata,status_code,NULL,NULL,NULL,NULL);
    if(PJ_SUCCESS != status)
    {
        LOG(ERROR)<<"pjsip_endpt_respond error";
    }

    return status;
}


//上级对下级推送的设备目录包来进行解析和响应的功能
void SipDirectory::SaveDir(int& status_code)
{
    tinyxml2::XMLElement* pRootElement = m_pRootElement;
    if(!pRootElement)
    {
        status_code = SIP_BADREQUEST;
        return;
    }


    //定义字符串
    string strCenterDeviceID,strSumNum,strSn,strDeviceID,strName,strManufacturer,
    strModel,strOwner,strCivilCode,strParental,strParentID,strSafetyWay,
    strRegisterWay,strSecrecy,strStatus;

    //从根节点获取之后在赋值
    tinyxml2::XMLElement* pElement = pRootElement->FirstChildElement("DeviceID");
    if(pElement && pElement->GetText())
        strCenterDeviceID = pElement->GetText();

    if(!GlobalCtl::checkIsVaild(strCenterDeviceID))
    {
        status_code = SIP_BADREQUEST;
        return;
    }
    
    pElement = pRootElement->FirstChildElement("SumNum");
    if(pElement && pElement->GetText())
        strSumNum = pElement->GetText();

    pElement = pRootElement->FirstChildElement("SN");
    if(pElement && pElement->GetText())
        strSn = pElement->GetText();


    pElement = pRootElement->FirstChildElement("DeviceList");
    if(pElement)
    {
        tinyxml2::XMLElement* pItem = pElement->FirstChildElement("item");
        while(pItem)
        {
            //判断设备号
            tinyxml2::XMLElement* pChild = pItem->FirstChildElement("DeviceID");
            if(pChild && pChild->GetText())
            {
                strDeviceID = pChild->GetText();
                if(strDeviceID.length() == 20)
                {
                    DevTypeCode type = GlobalCtl::getSipDevInfo(strDeviceID);
                    if(type == Camera_Code || type == Ipc_Code)
                    {
                        //定义设备获取成功标识
                        GlobalCtl::gRcvIpc = true;
                        LOG(INFO)<<"get ipc device";
                    }
                }
            }
                
            


            pChild = pItem->FirstChildElement("Name");
            if(pChild && pChild->GetText())
                strName = pChild->GetText();
            
            pChild = pItem->FirstChildElement("Manufacturer");
            if(pChild && pChild->GetText())
                strManufacturer = pChild->GetText();
            
            pChild = pItem->FirstChildElement("Model");
            if(pChild && pChild->GetText())
                strModel = pChild->GetText();

            pChild = pItem->FirstChildElement("Owner");
            if(pChild && pChild->GetText())
                strOwner = pChild->GetText();

            pChild = pItem->FirstChildElement("CivilCode");
            if(pChild && pChild->GetText())
                strCivilCode = pChild->GetText();

            pChild = pItem->FirstChildElement("Parental");
            if(pChild && pChild->GetText())
                strParental = pChild->GetText();

            pChild = pItem->FirstChildElement("ParentID");
            if(pChild && pChild->GetText())
                strParentID = pChild->GetText();

            pChild = pItem->FirstChildElement("SafetyWay");
            if(pChild && pChild->GetText())
                strSafetyWay = pChild->GetText();

            pChild = pItem->FirstChildElement("RegisterWay");
            if(pChild && pChild->GetText())
                strRegisterWay = pChild->GetText();
            
            pChild = pItem->FirstChildElement("Secrecy");
            if(pChild && pChild->GetText())
                strSecrecy = pChild->GetText();
            
            pChild = pItem->FirstChildElement("Status");
            if(pChild && pChild->GetText())
                strStatus = pChild->GetText();

            pItem = pItem->NextSiblingElement();

            GlobalCtl::get_global_mutex();
            //当前节点的id
            m_jsonIn["catalog"][m_jsonIndex]["DeviceID"] = strDeviceID;
            //当前节点的name
            m_jsonIn["catalog"][m_jsonIndex]["Name"] = strName;
            //表示当前节点是否有子节点
            m_jsonIn["catalog"][m_jsonIndex]["Parental"] = strParental;
            //父节点的id
            m_jsonIn["catalog"][m_jsonIndex]["ParentID"] = strParentID;
            m_jsonIndex++;
            GlobalCtl::free_global_mutex();

            pItem = pItem->NextSiblingElement();
        }

    }
    //表示当前推送多少个资源的一个总数，来检查是不是推送完啦
    int sumNum = atoi(strSumNum.c_str());
    //推送的总数在于json的总数进行判断
    //如果对的上表示下级已经推送完毕
    if(m_jsonIn["catalog"].size() == sumNum)
    {
        GlobalCtl::get_global_mutex();
        //接收完之后我们需要将当前接收的json数据转换为字符串，然后再进行发送
        //这个string用于保存string之后的json数据
        GlobalCtl::gCatalogPayload = JsonParse(m_jsonIn).toString();
        GlobalCtl::free_global_mutex();
        //然后再重新初始化json
        m_jsonIn.clear();
        m_jsonIndex = 0;
        //设备接收好之后启动这个信号
        GBOJ(gThPool)->postInfo();
    }

}