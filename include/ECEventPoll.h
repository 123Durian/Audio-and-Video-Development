#ifndef _ECEVENTPOLL_H
#define _ECEVENTPOLL_H
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <vector>
#include "Common.h"

//封装epoll和select

//所有封装都是EC这个命名空间下
namespace EC
{
    //文件描述符的类型，可读，可写，有错误
    enum EventType
    {
        EC_POLLIN = 0,  
        EC_POLLOUT,
        EC_POLLERR
    };

    struct PollEventType
    {
        int sockfd;// 文件描述符
        int inEvents;// 进入poll前，关注的事件（如读、写、错误）
        int outEvents;// poll之后，实际发生的事件（如是否可读、可写）
    };

    //声明一个基类
    //这个是一个虚基类，只定义接口不实现
    class PollSet
    {
        public:
            PollSet(){}
            ~PollSet(){}
            //初始化集合
            virtual int initSet() = 0;
            //清空集合
            virtual int clearSet() = 0;
            //添加文件描述符
            virtual int addFd(int sockfd,EventType type) = 0;
            //删除文件描述符
            virtual int deleteFd(int sockfd) = 0;
            //返回一个就绪的集合
            virtual int doSetPoll(vector<PollEventType>& inEvents,vector<PollEventType>& outEvents,int* timeout) = 0;
    };

    class SelectSet : public PollSet
    {
        public:
            SelectSet(){}
            ~SelectSet(){}
            virtual int initSet();
            virtual int clearSet();
            virtual int addFd(int sockfd,EventType type);
            virtual int deleteFd(int sockfd);
            virtual int doSetPoll(vector<PollEventType>& inEvents,vector<PollEventType>& outEvents,int* timeout);
        private:
        //读集合
            fd_set _rset;
            //写集合
            fd_set _wset;
            //错误集合
            fd_set _eset;

            //最大的文件描述符个数
            int _maxfd;
    };

    class EpollSet : public PollSet
    {
        public:
            EpollSet(){}
            ~EpollSet(){}
            virtual int initSet();
            virtual int clearSet();
            virtual int addFd(int sockfd,EventType type);
            virtual int deleteFd(int sockfd);
            virtual int doSetPoll(vector<PollEventType>& inEvents,vector<PollEventType>& outEvents,int* timeout);
        private:
            int _epollFd;
    };

    class EventPoll
    {
        public:
            EventPoll();
            ~EventPoll();
            int init(int method);
            int destory();
            int addEvent(const int& sockfd,EventType type);
            int removeEvent(const int& sockfd);
            int poll(vector<PollEventType>& outEvents,int* timeout);
        
        private:
            vector<PollEventType> _events;
            PollSet* _pollset;
    };
}

#endif