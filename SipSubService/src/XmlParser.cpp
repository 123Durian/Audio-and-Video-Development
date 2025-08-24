#include "XmlParser.h"


XmlParser::XmlParser()
{
    m_document = new tinyxml2::XMLDocument();
    m_rootElement = NULL;
    m_xmlTitle = new char[max_title];
    memset(m_xmlTitle,0,max_title);
    memcpy(m_xmlTitle,XMLTITLE,max_title);
}

XmlParser::~XmlParser()
{
    if(m_document)
    {
        delete m_document;
        m_document = NULL;
    }

    delete []m_xmlTitle;
}

tinyxml2::XMLElement* XmlParser::AddRootNode(const char* rootName)
{
    if(m_document)
    {
        //创建一个新的节点
        //把这个节点作为title的子节点
        m_rootElement = m_document->NewElement(rootName);
        m_document->LinkEndChild(m_rootElement);
    }

    return m_rootElement;
}

tinyxml2::XMLElement* XmlParser::InsertSubNode(tinyxml2::XMLElement* parentNode,const char* itemName,const char* value)
{
    if(!parentNode)
        return NULL;
        
    //创建一个新的元素
    //当成孩子节点插入
    tinyxml2::XMLElement* insertNode = m_document->NewElement(itemName);
    parentNode->LinkEndChild(insertNode);

    if(strlen(value) != 0)
    {
        //生成一个text
        //给这个新生成的节点将这个text插入
        tinyxml2::XMLText* txt = m_document->NewText(value);
        insertNode->LinkEndChild(txt);
    }
    return insertNode;
}

void XmlParser::SetNodeAttributes(tinyxml2::XMLElement* node,char* attrName,char* attrValue)
{
    if(!node)
        return;
    node->SetAttribute(attrName,attrValue);
}

void XmlParser::getXmlData(char* xmlBuf)
{
    if(!xmlBuf)
        return;
    
        //XMLPrinter 是一个用于将 tinyxml2 的 XML 文档或元素转换成字符串
        //可以被 XML 文档的 Accept() 函数调用，从而把 XML 结构以格式化的文本写入内部缓冲区。

//创建了一个xml打印器对象
    tinyxml2::XMLPrinter printer;
    if(m_document)
    {
        //将doc的内润写入缓冲区（打印器）
        m_document->Accept(&printer);

        if(strlen(m_xmlTitle) != 0)
            strncpy(xmlBuf,m_xmlTitle,max_title);
        strcat(xmlBuf,(char*)printer.CStr());
    }
}