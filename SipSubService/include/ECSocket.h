#ifndef _ECSOCKET_H
#define _ECSOCKET_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/stat.h>
#include "Common.h"

//这节课我们来对socket进行封装
namespace EC
{
    class ECSocket
    {
        public:
            //我们之前说过，开流负载的SDP协议中如果指定了传输层协议为tcp,
            //那么还需要通过setup的字段来指定当前服务是作为客户端主动连接对方还是作为服务端被动等待对方的连接
            //所以这里我们需要申明两个接口

            //服务器
            static int createConnByPassive(int* lsockfd,int localPort,int* timeout);
            //客户端
            static int createConnByActive(string dstip,int dstport,int localPort,int* timeout);
        
        //我们将这个类的构造和析构都私有化，禁止这个类的实例化，只对外提供static的接口，直接通过类来调用，而不是类的对象
        private:
            ECSocket(){}
            ~ECSocket(){}
    };
}
#endif