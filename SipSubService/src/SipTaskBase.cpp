#include "SipTaskBase.h"

string SipTaskBase::parseFromId(pjsip_msg* msg)
{
    pjsip_from_hdr* from = (pjsip_from_hdr*)pjsip_msg_find_hdr(msg,PJSIP_H_FROM,NULL);
    if(NULL == from)
    {
        return "";
    }
    char buf[1024] = {0};
    string fromId = "";
    int size = from->vptr->print_on(from,buf,1024);
    if(size > 0)
    {
        fromId = buf;
        fromId = fromId.substr(11,20);
    }
    return fromId;
}

string SipTaskBase::parseToId(pjsip_msg *msg)
{
	pjsip_to_hdr* to = (pjsip_to_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_TO, NULL);
	if(NULL == to)
    {
        return "";
    }
	char buf[1024] = { 0 };
	string toId = "";
	int size = to->vptr->print_on(to, buf, 1024);
	printf("to:%s\n",buf);
	if (size > 0)
	{
		toId = buf;
		toId = toId.substr(9, 20);
	}
	return toId;
}

tinyxml2::XMLElement* SipTaskBase::parseXmlData(pjsip_msg* msg,string& rootType,const string xmlkey,string& xmlvalue)
{
    tinyxml2::XMLDocument* pxmlDoc = NULL;
    pxmlDoc = new tinyxml2::XMLDocument();
    if(pxmlDoc)
    {
        pxmlDoc->Parse((char*)msg->body->data);
    }
    tinyxml2::XMLElement* pRootElement = pxmlDoc->RootElement();
    rootType = pRootElement->Value();

    tinyxml2::XMLElement* cmdElement = pRootElement->FirstChildElement((char*)xmlkey.c_str());
    if(cmdElement && cmdElement->GetText())
    {
        xmlvalue = cmdElement->GetText();
    }

    return pRootElement;
}