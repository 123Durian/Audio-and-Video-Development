#ifndef _ECEVENTPOLL_H
#define _ECEVENTPOLL_H
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <vector>
#include "Common.h"


namespace EC
{
    enum EventType
    {
        EC_POLLIN = 0,  
        EC_POLLOUT,
        EC_POLLERR
    };


    //可以管理select和epoll集合的共通的结构体
    struct PollEventType
    {
        //管理指定的fd
        int sockfd;
        //fd需要监听的事件
        int inEvents;
        //返回的就绪的事件
        int outEvents;
    };

    class PollSet
    {
        public:
            PollSet(){}
            ~PollSet(){}
            virtual int initSet() = 0;
            virtual int clearSet() = 0;
            virtual int addFd(int sockfd,EventType type) = 0;
            virtual int deleteFd(int sockfd) = 0;
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
            fd_set _rset;
            fd_set _wset;
            fd_set _eset;

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


    //声明了一个类，该类由用户来实例化这个类，这个类对封装的上面两个类的接口进行调用
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
        
        //定义了一个结构体变量，管理上面两个的基类
            vector<PollEventType> _events;
            //这个基类指向哪个接口由用户指定
            PollSet* _pollset;
    };
}

#endif