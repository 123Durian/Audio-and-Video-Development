#include "ECEventPoll.h"

using namespace EC;

//对select所需要传入的参数进行初始化
int SelectSet::initSet()
{
    FD_ZERO(&_rset);
    FD_ZERO(&_wset);
    FD_ZERO(&_eset);
    _maxfd = 0;
    return 0;
}

int SelectSet::clearSet()
{
    initSet();
}

//传入文件描述符和事件类型
//以什么样的类型将这个事件添加进来
int SelectSet::addFd(int sockfd,EventType type)
{
    
    if(sockfd == -1)
        return -1;
    
    if(type == EventType::EC_POLLIN)
    {
        FD_SET(sockfd,&_rset);
    }
    else if (type == EventType::EC_POLLOUT)
    {
        FD_SET(sockfd,&_wset);
    }
    else if (type == EventType::EC_POLLERR)
    {
        FD_SET(sockfd,&_eset);
    }

    _maxfd = sockfd;
}

//从外面传入一个需要删除的fd
int SelectSet::deleteFd(int sockfd)
{
    FD_CLR(sockfd, &_rset);
    FD_CLR(sockfd, &_wset);
    FD_CLR(sockfd, &_eset);

    return 0;
}


//利用 select 机制检测一组文件描述符的读写和异常事件，并将发生事件的结果写入 outEvents
int SelectSet::doSetPoll(vector<PollEventType>& inEvents,vector<PollEventType>& outEvents,int* timeout)
{

    //将以毫秒为单位的超时时间 timeout 转换为 select() 所需要的 struct timeval 格式（秒 + 微秒）
    struct timeval* ptv = NULL;
    if(timeout != NULL && *timeout >= 0)
    {
        //超时时间是客户端传过来的
        struct timeval tv;
        tv.tv_sec = *timeout / 1000;
        tv.tv_usec = (*timeout % 1000) * 1000;
        ptv = &tv;
    }

    //定义三个临时的集合
    //给三个临时变量赋值
    fd_set readset,writeset,exceptset;
    memcpy(&readset,&_rset,sizeof(fd_set));
    memcpy(&writeset,&_wset,sizeof(fd_set));
    memcpy(&exceptset,&_eset,sizeof(fd_set));

    //最后一个参数是超时参数
    int ret = select(_maxfd+1,&readset,&writeset,&exceptset,ptv);
    if(ret <= 0)
        return ret;
    
    outEvents.clear();
    vector<PollEventType>::iterator it = inEvents.begin();
    for(;it != inEvents.end();++it)
    {
        it->outEvents = 0;
        if(FD_ISSET(it->sockfd,&readset))
            it->outEvents = EC_POLLIN;
        if(FD_ISSET(it->sockfd,&writeset))
            it->outEvents = EC_POLLOUT;
        if(FD_ISSET(it->sockfd,&exceptset))
            it->outEvents = EC_POLLERR;
        
        if(it->outEvents != 0)
            outEvents.push_back(*it);
    }

    return ret;
}

int EpollSet::initSet()
{
    _epollFd = epoll_create(1024);

    return _epollFd;
}

int EpollSet::clearSet()
{
    if(_epollFd != -1)
    {
        close(_epollFd);
        _epollFd = -1;
    }
    return 0;
}

int EpollSet::addFd(int sockfd,EventType type)
{
    if(sockfd <= 0)
        return -1;
    
    struct epoll_event event;
    event.events = 0;
    event.data.fd = sockfd;
    if(type == EventType::EC_POLLIN)
    {
        event.events = EPOLLIN;
    }
    else if (type == EventType::EC_POLLOUT)
    {
        event.events = EPOLLOUT;
    }
    else if (type == EventType::EC_POLLERR)
    {
        event.events = EPOLLERR;
    }

    if(event.events == 0)
        return -1;

    return epoll_ctl(_epollFd, EPOLL_CTL_ADD, sockfd, &event); 
}

int EpollSet::deleteFd(int sockfd)
{
    struct epoll_event event;
    return epoll_ctl(_epollFd,EPOLL_CTL_DEL,sockfd,&event);
}

int EpollSet::doSetPoll(vector<PollEventType>& inEvents,vector<PollEventType>& outEvents,int* timeout)
{
    int tv = -1;
    if(timeout != NULL && *timeout >= 0)
    {
        tv = *timeout;
    }

	int max = inEvents.size();
    struct epoll_event events[max];
    int evCount = epoll_wait(_epollFd, events, max, tv);

    outEvents.clear();
    vector<PollEventType>::iterator it = inEvents.begin();
    for(;it != inEvents.end();++it)
    {
        it->outEvents = 0;
        for(int i=0;i<evCount;++i)
        {
            if(it->sockfd == events[i].data.fd)
            {
                if(events[i].events == EPOLLIN)
                    it->outEvents = EC_POLLIN;
                else if(events[i].events == EPOLLOUT)
                    it->outEvents = EC_POLLOUT;
                else if(events[i].events == EPOLLERR)
                    it->outEvents = EC_POLLERR;
            }
        }
        
        if(it->outEvents != 0)
            outEvents.push_back(*it);
    }

    return 0;

}

EventPoll::EventPoll()
{
    _events.clear();
    _pollset = NULL;
}

EventPoll::~EventPoll()
{
    this->destory();
}

int EventPoll::init(int method)
{
    if(method == 1)
    {
        _pollset = new SelectSet();
    }
    else if(method == 2)
    {
        _pollset = new EpollSet();
    }
    else
    {
        return -1;
    }
    return 0;
}

int EventPoll::destory()
{
    if(_pollset != NULL)
    {
        _pollset->clearSet();
        delete _pollset;
        _pollset = NULL;
    }
    _events.clear();
}

int EventPoll::addEvent(const int& sockfd,EventType type)
{
    if(_pollset == NULL)
        return -1;
    
    _pollset->addFd(sockfd,type);

    PollEventType ev;
    ev.inEvents = type;
    ev.outEvents = 0;
    ev.sockfd = sockfd;
    _events.push_back(ev);
}

int EventPoll::removeEvent(const int& sockfd)
{
    if(_pollset == NULL)
        return -1;

    _pollset->deleteFd(sockfd);
    
    vector<PollEventType>::iterator it = _events.begin();
    for(;it != _events.end();)
    {
        if(it->sockfd == sockfd)
        {
            it = _events.erase(it);
        }
        else
        {
            ++it;
        }
    }
    return 0;
}

int EventPoll::poll(vector<PollEventType>& outEvents,int* timeout)
{
    if(_pollset == NULL)
        return -1;
    return _pollset->doSetPoll(_events,outEvents,timeout);
}