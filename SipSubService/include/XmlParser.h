#ifndef _XMLPARSER_H
#define _XMLPARSER_H
#include <string>
#include "tinyxml2.h"

const int max_title = 64;
#define XMLTITLE "<?xml version=\"1.0\"?>\n"
/*
1.插入根节点，指定根节点的名称
2.在指定的节点下再插入子节点并设置节点名称和相应的值
3.添加xml的申明部分并返回整个xml的char*数据
*/
class XmlParser
{
    public:
        XmlParser();
        ~XmlParser();

        //插入一个根节点
        tinyxml2::XMLElement* AddRootNode(const char* rootName);
        //指定子节点
        tinyxml2::XMLElement* InsertSubNode(tinyxml2::XMLElement* parentNode,const char* itemName,const char* value);
        void SetNodeAttributes(tinyxml2::XMLElement* node,char* attrName,char* attrValue);
        //传输xml
        void getXmlData(char* xmlBuf);
    private:
        tinyxml2::XMLDocument* m_document;
        tinyxml2::XMLElement* m_rootElement;
        char* m_xmlTitle;

};

#endif