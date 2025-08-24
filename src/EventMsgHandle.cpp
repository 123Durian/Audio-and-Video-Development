#include "EventMsgHandle.h"
#include "SipDef.h"
#include "ThreadPool.h"
#include "GetPlamtInfo.h"
#include "GetCatalog.h"
#include "GlobalCtl.h"
#include "OpenStream.h"

void parseReadEvent(struct bufferevent *bev, void *ctx)
{
    LOG(INFO)<<"parseReadEvent";
    char* buf[1024] = {0};

    //从 bufferevent 中读取最多 4 字节数据到 buf 缓冲区中
    int len = bufferevent_read(bev,buf,4);

    //表示的是一个任务
    ThreadTask* task = NULL;
    if(len == 4)
    {
        int command = *(int*)buf;
        LOG(INFO)<<"command:"<<command;
        switch(command)
        {
            case Command_Session_Register:
            {
                //在这里构造的时候我们需要保存一下这个bufferevent，相当于通信的fd，我们收到数据之后还需要将响应通过这个进行返回
                task = new GetPlamtInfo(bev);
                break;
            }
            case Command_Session_Catalog:
            {
                task = new GetCatalog(bev);
                break;
            }
            case Command_Session_RealPlay:
            {
                task = new OpenStream(bev,&command);
                break;
            }
            default:
                return;
        }
        //把任务放到任务队列当中
        if(task != NULL)
        //在全局类已经把线程创建好了，在这只需要把任务放到任务队列中，线程在自动取任务进行执行
            GBOJ(gThPool)->postTask(task);
            
    }

}
void event_callback(struct bufferevent *bev, short events, void *ctx)
{
    //判断event中是否设置了断开连接的标志位
    if(events & BEV_EVENT_EOF)
    {
        LOG(ERROR)<<"connection closed";
        //接下来关闭和释放这个buffereventevent资源
        bufferevent_free(bev);
    }
}

//这里参数中的地址市客户端的地址信息


//当服务端接收到客户端连接时，为该连接配置 socket 选项、设置保活参数、注册事件监听、创建缓冲事件 bufferevent 并绑定回调函数，从而实现高效、非阻塞、稳定的通信管理。
void onAccept(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen, void *arg)
{
    LOG(INFO)<<"accept client ip:"<<inet_ntoa(((struct sockaddr_in*)address)->sin_addr)<<" port:"<<((struct sockaddr_in*)address)->sin_port<<" fd:"<<fd;
    //首先将链接的文件描述符设置为非阻塞模式，这样在进行网络io操作的时候不会被阻塞，可以提高程序的效率和响应速度
    evutil_make_socket_nonblocking(fd);

    //开启TCP保活机制，用于检测长时间空闲连接是否仍然存在。
    //在连接空闲一段时间后自动发送探测包，以检测连接是否仍然存活。如果对方主机已崩溃或断网，可以尽早发现断开连接。
    int on = 1;
    setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,&on,sizeof(on));
    int keepIdle = 60;
    int keepInterval = 20;
    int keepCount = 3;

    //连接后多久开始发送保活
    setsockopt(fd,SOL_SOCKET,TCP_KEEPIDLE,(void*)&keepIdle,sizeof(keepIdle));
    //设置保活的周期
    setsockopt(fd,SOL_SOCKET,TCP_KEEPINTVL,(void*)&keepInterval,sizeof(keepInterval));
    //设置探测次数，在没有收到响应包的时候，会尝试探测
    setsockopt(fd,SOL_SOCKET,TCP_KEEPCNT,(void*)&keepCount,sizeof(keepCount));


    //用于在接收新连接之后，为每一个连接创建一个带缓冲的事件（bufferevent）对象，并设置对应的回调函数和读写行为
    //第三个参数表示：在释放 bufferevent 时自动关闭 fd和 启用线程安全

    //给每一个事件创建新的 bufferevent，并开启“读事件监听”
    event_base* base = (event_base*)arg;
    struct bufferevent* new_buff_event = bufferevent_socket_new(base,fd,BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
    //设置两个回调函数，一个是读事件，一个是关闭的。
    bufferevent_setcb(new_buff_event,parseReadEvent,NULL,event_callback,NULL);
    //启动事件监听，持续性监听可读事件
    bufferevent_enable(new_buff_event,EV_READ | EV_PERSIST);
    //设置写水位线，缓冲区中写事件触发的时机，当写缓冲区的数据为0字节时就触发写事件
    bufferevent_setwatermark(new_buff_event,EV_WRITE,0,0);    
}

EventMsgHandle::EventMsgHandle(const std::string& servIp,const int& servPort)
    :m_serIp(servIp)
    ,m_serPort(servPort)
 {

 }

EventMsgHandle::~EventMsgHandle()
{

}

//接收客户端的连接请求
int EventMsgHandle::init()
{
    //定义结构体，用来存储服务器端的地址信息
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(m_serPort);
    servaddr.sin_addr.s_addr = inet_addr(m_serIp.c_str());

    //我们为livevent开启使用多线程的机制，这个接口是线程安全的
    int ret = evthread_use_pthreads();

    //创建一个socket——fd，这个结构体就是用来进行事件处理的，这个base就相当于epoll中的树根的角色
    //同样的对于这个结构体来说，他会保存一系列的event事件，并且以轮询的方式去关注每一个事件，当事件就绪之后，就会触发对应的事件的回调进行处理。
    event_base* base = event_base_new();

    //进行bind和listen
    //用该端口创建一个监听器，监听指定值和端口，并且绑定到event_base当中

    //第一个参数是base，第二个参数是回调（接收客户端连接的回调），接下来是产地给这个回调函数的自定义指针
    //第五个参数是可接受连接数的大小
    //最后市服务端的地址

    //返回一个listen对象，用这个对象来进行事件的监听
    //监听服务器上新用户的连接
    struct evconnlistener* listener = evconnlistener_new_bind(base,onAccept,base,
    LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE | LEV_OPT_THREADSAFE,1000,(struct sockaddr*)&servaddr,sizeof(struct sockaddr_in));
    if(!listener)
    {
        LOG(ERROR)<<"evconnlistener_new bing ERROR";
        return -1;
    }


    //接下来我们进入libevent的事件循环，开始监听事件
    event_base_dispatch(base);

    return 0;


}