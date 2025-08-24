#include "SipMessage.h"



//设置下面四个的目的：构造出一条完整的SIP协议消息中的重要的头部信息

//将四个字符数组清零
SipMessage::SipMessage()
{
    memset(fromHeader,0,sizeof(fromHeader));
    memset(toHeader,0,sizeof(toHeader));
    memset(requestUrl,0,sizeof(requestUrl));
    memset(contact,0,sizeof(contact));
}

SipMessage::~SipMessage()
{
    
}


//设置SIP消息头部
//谁是这条SIP消息的发送者
//在 REGISTER、注册请求
//INVITE、呼叫请求
//BYE、挂断请求
//OPTIONS，能力探测请求（探测对方是否在线，支持什么）
void SipMessage::setFrom(char* fromUsr,char* fromIp)
{
    sprintf(fromHeader,"<sip:%s@%s>",fromUsr,fromIp);
}


//设置SIP to头部
//在 INVITE 等请求中，表示你希望通信的对象是谁；
//在 REGISTER 请求中，通常 To 与 From 内容相同（都是自己），表示“我要注册我自己”。
void SipMessage::setTo(char* toUsr,char* toIp)
{
    sprintf(toHeader,"<sip:%s@%s>",toUsr,toIp);
}

//设置请求url
//表示你要将SIP消息发送到哪个地址，用来定位目标用户
void SipMessage::setUrl(char* sipId,char* url_ip,int url_port,char* url_proto)
{
    sprintf(requestUrl,"sip:%s@%s:%d;transport=%s",sipId,url_ip,url_port,url_proto);
}


//设置contact头部
//告诉对方，如果你之后要联系我，请联系这个地址

//sipId：SIP 用户标识（用户名），比如 "alice"。
//natIp：NAT 后的公网 IP 地址，代表客户端可被外界访问的地址。
//natPort：对应的端口号，客户端监听的端口。

void SipMessage::setContact(char* sipId,char* natIp,int natPort)
{   
    sprintf(contact,"sip:%s@%s:%d",sipId,natIp,natPort);
}
