#include "ECSocket.h"
#include "ECEventPoll.h"

using namespace EC;

int ECSocket::createConnByPassive(int* lsockfd,int localPort,int* timeout)
{
    LOG(INFO)<<"start tcpserver... ";
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(-1 == sockfd)
    {
        LOG(ERROR)<<"socket create error";
        return -1;
    }

    setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(const char*)&sockfd,sizeof(sockfd));
    struct sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(localPort);
    if( bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
    {
        LOG(ERROR) << "socket bind error..please set ulimit..";
        return -1;
    }
    
    if( listen(sockfd, 20) == -1)
    {
        LOG(ERROR) << "socket listen error..please set ulimit..";
        close(sockfd);
        return -1;
    }
    sockaddr_in clientAddr;  
    socklen_t addrLen = sizeof(clientAddr);  
    

    if(timeout == NULL)
    {   
        int connfd = accept(sockfd, (struct sockaddr *)&clientAddr, &addrLen);
        if(connfd != -1)
        {
            //将监听fd传出
            *lsockfd = sockfd;
            return connfd;
        }
        else
        {
            close(sockfd);
            return -1;
        }
    }
	
    EventPoll eventPoll;
    eventPoll.init(1);
    eventPoll.addEvent(sockfd,EC_POLLIN);

    int ret = -1;
    vector<PollEventType> events;
    while(true)
    {
        ret = eventPoll.poll(events,timeout);
		//LOG(INFO)<<"ret:"<<ret;
		//LOG(INFO)<<"events.size():"<<events.size();
        if(ret > 0)
        {
            for(int i = 0;i<events.size();i++)
            {
                if(events[i].sockfd == sockfd) 
                {
                    int connfd = accept(sockfd, (struct sockaddr *)&clientAddr, &addrLen);
                    if(connfd != -1)
                    {
                        *lsockfd = sockfd;
                        return connfd;
                    }
                    else
                    {
                        close(sockfd);
                        return -1;
                    }
                }
                else
                {
                    continue;
                }

            }
        }
        else if(ret == 0)
        {
            LOG(INFO)<<"TIME OUT";
            break;
        }
        else if(ret == -1)
        {
            LOG(INFO)<<"poll ERROR";
            break;
        }
    }
    eventPoll.removeEvent(sockfd);
    close(sockfd);
    return ret;

}

int ECSocket::createConnByActive(string dstip,int dstport,int localPort,int* timeout)
{
    LOG(INFO)<<"tcpclient connect...";

    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(-1 == sockfd)
    {
        LOG(ERROR)<<"socket create error";
        return -1;
    }
    //设置socket复用，允许立即重用，忽略返回值
    setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(const char*)&sockfd,sizeof(sockfd));

    struct sockaddr_in client_addr;
    memset(&client_addr,0,sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr.sin_port = htons(localPort);
    if( bind(sockfd, (struct sockaddr*)&client_addr, sizeof(client_addr)) == -1)
    {
        LOG(ERROR) << "socket bind error..please set ulimit..";
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(dstip.c_str());
    server_addr.sin_port = htons(dstport);

    int status;
    if (timeout == NULL)  
    {  
        //发送SYN
        status = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));  
        if (status < 0)
        {
            LOG(ERROR) << "connect error,errno:" << errno;
            close(sockfd);
            return status;
        }
        return sockfd;  
    }  

    status = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));  
    if (status < 0)
    {
        LOG(ERROR) << "connect error,errno:" << errno;
        close(sockfd);
        return status;
    }

    EventPoll eventPoll;
    eventPoll.init(1);
    eventPoll.addEvent(sockfd,EC_POLLOUT);
    int ret = -1;
	vector<PollEventType> events;
    while(true)
    {
        ret = eventPoll.poll(events,timeout);
        if(ret > 0)
        {
            for(int i = 0;i<events.size();i++)
            {
                if(events[i].sockfd == sockfd) 
                {
                    return sockfd;
                }
                else
                {
                    continue;
                }

            }
        }
        else if(ret == 0)
        {
            LOG(INFO)<<"TIME OUT";
            break;
        }
        else if(ret == -1)
        {
            LOG(INFO)<<"poll ERROR";
            break;
        }
    }
    eventPoll.removeEvent(sockfd);
    close(sockfd);
    return ret;
}