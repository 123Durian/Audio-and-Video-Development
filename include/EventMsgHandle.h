#ifndef __EVENTMSGHANDLE_H_
#define __EVENTMSGHANDLE_H_
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/thread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "Common.h"

class EventMsgHandle
{
    public:
    //在构造的时候指定一个这个TCPserver的ip和端口
        EventMsgHandle(const std::string& servIp,const int& servPort);
        ~EventMsgHandle();

        //声明接口进行TCPserver的初始化
        int init();
    private:
        std::string m_serIp;
        int m_serPort;
};

#endif