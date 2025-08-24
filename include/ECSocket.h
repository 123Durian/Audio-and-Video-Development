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

namespace EC
{
    class ECSocket
    {
        public:
            static int createConnByPassive(int* lsockfd,int localPort,int* timeout);
            static int createConnByActive(string dstip,int dstport,int localPort,int* timeout);
        
        private:
            ECSocket(){}
            ~ECSocket(){}
    };
}
#endif