#include "SipTaskBase.h"


//从一个SIP消息msg中提取from头字段，并截取from字段中SIP URL的用户
//部分作为ID
string SipTaskBase::parseFromId(pjsip_msg* msg)
{
    pjsip_from_hdr* from = (pjsip_from_hdr*)pjsip_msg_find_hdr(msg,PJSIP_H_FROM,NULL);
    if(NULL == from)
    {
        return "";
    }
    char buf[1024] = {0};
    string fromId = "";
    //将from头打印成字符串写进buf
    int size = from->vptr->print_on(from,buf,1024);
    if(size > 0)
    {
        fromId = buf;
        fromId = fromId.substr(11,20);
    }
    return fromId;
}

tinyxml2::XMLElement* SipTaskBase::parseXmlData(pjsip_msg* msg,string& rootType,const string xmlkey,string& xmlvalue)
{
    tinyxml2::XMLDocument* pxmlDoc = NULL;
    pxmlDoc = new tinyxml2::XMLDocument();
    if(pxmlDoc)
    {
        pxmlDoc->Parse((char*)msg->body->data);
    }
    //指针
    tinyxml2::XMLElement* pRootElement = pxmlDoc->RootElement();
    //指针指向的元素
    rootType = pRootElement->Value();


    //指针
    tinyxml2::XMLElement* cmdElement = pRootElement->FirstChildElement((char*)xmlkey.c_str());
    //该节点不是空的，该节点的文本内容不是空的
    if(cmdElement && cmdElement->GetText())
    {
        xmlvalue = cmdElement->GetText();
    }

    return pRootElement;
}



/*


pRootElement 是个指针，指向一个节点对象，这个对象包含：
struct XMLElement {
    const char* tagName;       // "Notify"
    std::vector<XMLElement*> children; // 指向 CmdType、SN、DeviceID、Status 节点
    const char* textContent;   // 这个节点本身没文本，文本都在子节点里
};
*/