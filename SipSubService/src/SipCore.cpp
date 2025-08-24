#include "SipCore.h"
#include "Common.h"
#include "SipDef.h"
#include "GlobalCtl.h"
#include "SipTaskBase.h"
#include "SipDirectory.h"
#include "SipGbPlay.h"
#include "SipRecordList.h"



//pjsip_regc_create/init 创建和初始化注册客户端对象，是在 会话层管理注册会话。

//端点层 负责接收和分发消息，调用对应的模块处理。

//事务层 管理注册请求的发送和响应事务。

//传输层 负责实际的网络收发。












//当ioqueue读到原始数据后，会交给PJSIP协议栈进行协议解析，把字节流解析成SIP消息（请求或响应）。
//解析完成后，协议栈根据请求的类型（比如INVITE、REGISTER、BYE等）调用相应的业务处理模块。

//该代码执行完之后可以
//1：监听和处理 SIP 消息：SIP 协议栈初始化完毕后，程序开始监听指定端口上的 SIP 消息（请求和响应）

//2：处理呼叫建立和管理：通过 INVITE 模块，可以接收和发起电话呼叫，处理会话生命周期

//3：处理注册、订阅等 SIP 功能：用户代理模块支持注册、订阅等 SIP 基础功能

//4：消息回调处理：收到 SIP 消息后，会回调自定义模块的回调函数 onRxRequest，进而分发任务处理

//5：异步事件驱动：通过独立线程处理事件，保证协议栈的异步运行和响应




//用于在独立线程中循环处理PISIP库的事件
static int pollingEvent(void* arg)
{
    LOG(INFO)<<"polling event thread start success";
    pjsip_endpoint* ept = (pjsip_endpoint*)arg;
    while(!(GlobalCtl::gStopPool))
    {
        //设置超时时间
        pj_time_val timeout = {0,500};
        

//从传输层读取数据包，把数据包解析成 SIP 消息，并传递给事务层、会话层。
  //该函数的底层是io队列
        pj_status_t status = pjsip_endpt_handle_events(ept,&timeout);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"polling events failed,code:"<<status;
            return -1;
        }
    }

    return 0;

}

pj_bool_t onRxRequest(pjsip_rx_data *rdata)
{
    if(NULL == rdata || NULL == rdata->msg_info.msg)
    {
        return PJ_FALSE;
    }
    threadParam* param = new threadParam();
    pjsip_rx_data_clone(rdata,0,&param->data);
    pjsip_msg* msg = rdata->msg_info.msg;
    if(msg->line.req.method.id == PJSIP_OTHER_METHOD)
    {
        string rootType = "",cmdType = "CmdType",cmdValue;
        tinyxml2::XMLElement* root = SipTaskBase::parseXmlData(msg,rootType,cmdType,cmdValue);
        LOG(INFO)<<"rootType:"<<rootType;
        LOG(INFO)<<"cmdValue:"<<cmdValue;
        if(rootType == SIP_QUERY )
        {
            if(cmdValue == SIP_CATALOG)
            {
                param->base = new SipDirectory(root);
            }
            //录像查询
            else if(cmdValue == SIP_RECORDINFO)
            {
                param->base = new SipRecordList();
            }
            
        }
    }
    else if(msg->line.req.method.id == PJSIP_INVITE_METHOD
            ||msg->line.req.method.id == PJSIP_BYE_METHOD)
    {
        param->base = new SipGbPlay();
    }

    pthread_t pid;
    int ret = EC::ECThread::createThread(SipCore::dealTaskThread,param,pid);
    if(ret != 0)
    {
        LOG(ERROR)<<"create task thread error";
        if(param)
        {
            delete param;
            param = NULL;
        }
        return PJ_FALSE;
    }
    return PJ_SUCCESS;
}
//当收到sip消息时要调用哪个回调函数来处理它
//收到请求消息时候调用
static pjsip_module recv_mod=
{
    NULL,NULL,
    {"mod-recv",8},
    -1,
    PJSIP_MOD_PRIORITY_APPLICATION,//应用层模块优先级
    NULL,
    NULL,
    NULL,
    NULL,
    onRxRequest,//收到消息时，调用这个回调函数处理
    NULL,
    NULL,
    NULL,
    NULL,
};
SipCore::SipCore()
:m_endpt(NULL)
{

}
SipCore::~SipCore()
{
    pjmedia_endpt_destroy(m_mediaEndpt);
    pjsip_endpt_destroy(m_endpt);
    //释放缓存池
    pj_caching_pool_destroy(&m_cachingPool);
    pj_shutdown();
    GlobalCtl::gStopPool = true;
}

void* SipCore::dealTaskThread(void* arg)
{
    threadParam* param = (threadParam*)arg;
    if(!param || param->base == NULL)
    {
        return NULL;
    }
    pj_thread_desc desc;
    pjcall_thread_register(desc);
    param->base->run(param->data);
    delete param;
    param = NULL;
}


//用于初始化PISIP的SIP栈
bool SipCore::InitSip(int sipPort)
{
    pj_status_t status;
    //0-关闭  6-详细，设置日志打印的详细等级
    pj_log_set_level(0);

    do
    {
        //初始化PJSIP库核心模块（线程，内存管理，日志模块等基础设施）
        status = pj_init();
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"init pjlib faild,code:"<<status;
            break;
        }
        //初始化工具库（定时器，IO队列，字符串等辅助功能）
        status = pjlib_util_init();
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"init pjlib util faild,code:"<<status;
            break;
        }
        
        //初始化缓存池，用于高效管理内存分配
        //初始化后，后续所有的内存分配都会通过它，提高性能。
        //相当于一个内存管家，他有一个存储仓库，后续的分配请求，就像“向管家要东西”
        //管家会根据需求分配对应大小的内存块
        pj_caching_pool_init(&m_cachingPool,NULL,SIP_STACK_SIZE);

        //创建SIP端点
        //通过缓存池工厂创建核心SIP端点对象，该端点是整个协议栈的根对象
        //负责管理SIP会话，消息收发和协议状态机，是其它模块和传输层的基础等。
        status = pjsip_endpt_create(&m_cachingPool.factory,NULL,&m_endpt);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"create endpt faild,code:"<<status;
            break;
        }

        //获取SIP端点对应的IO队列
        //io队列，其功能相当于一个muduo网络库
        pj_ioqueue_t* ioqueue = pjsip_endpt_get_ioqueue(m_endpt);
        //创建媒体端点，用于处理音视频媒体相关功能
        status = pjmedia_endpt_create(&m_cachingPool.factory,ioqueue,0,&m_mediaEndpt);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"create media endpoint faild,code:"<<status;
            break;
        }

        //初始化事务模块，负责处理请求/响应的状态机和重传。
        //sip事务是指一次请求和它对应的所有响应的完整交互过程
        //他包括一个请求，和该请求对应的所有临时响应和最终响应
        status = pjsip_tsx_layer_init_module(m_endpt);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"init tsx layer faild,code:"<<status;
            break;
        }

        //初始化用户代理模块，用于为后续的（注册，呼叫建立，响应等）打下基础
        //两个sip实体之间的
        //事务模块之上，提供给应用层使用的接口
        //管理REGISTER请求


        //sip请求的调度器接收并解析 SIP 请求
        //根据请求类型（如 INVITE / REGISTER / BYE）转发给正确的上层处理器
        status = pjsip_ua_init_module(m_endpt,NULL);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"init UA layer faild,code:"<<status;
            break;
        }

        //初始化传输层，初始化基于UDP/TCP的传输通道，绑定监听端口为传入的sipport

        //多个transport都会注册到io队列中，每个transport使用io队列来收发数据
        status = init_transport_layer(sipPort);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"init transport layer faild,code:"<<status;
            break;
        }

        //注册一个模块，用于处理接收到的SIP消息
        //将自定义的接收模块（recv_mod）注册到 PJSIP 协议栈的端点（m_endpt）上，使其具备接收和处理 SIP 消息的能力。
        status = pjsip_endpt_register_module(m_endpt,&recv_mod);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"register recv module faild,code:"<<status;
            break;
        }

        //启用SIP的可靠临时响应机制
        status = pjsip_100rel_init_module(m_endpt);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"100rel module init faild,code:"<<status;
            break;
        }



        //舒适化和注册pjsip的invite事务模块，并为其设置几个重要的事件回调函数
        //以便在sip呼叫过程中处理对应的事件。


        //定义一个结构体变量用于存放回调函数指针



        //初始化并注册 SIP INVITE 会话处理模块
        //在会话层上加入监听
        pjsip_inv_callback inv_cb;
        pj_bzero(&inv_cb,sizeof(inv_cb));
        //当 INVITE 会话的状态发生变化时调用。
        inv_cb.on_state_changed = &SipGbPlay::OnStateChanged;
        //当收到一个新的 INVITE 请求并创建新的会话时调用。
        inv_cb.on_new_session = &SipGbPlay::OnNewSession;
        //当会话中的媒体流发生变化时调用。
        inv_cb.on_media_update = &SipGbPlay::OnMediaUpdate;
        status = pjsip_inv_usage_init(m_endpt,&inv_cb);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"register invite module faild,code:"<<status;
            break;
        }


        //创建内存池
        //在pj_caching_pool_init之后才可以使用，并且它是创建具体的内存池，供运行时分配内存。
        m_pool = pjsip_endpt_create_pool(m_endpt,NULL,SIP_ALLOC_POOL_1M,SIP_ALLOC_POOL_1M);
        if(NULL == m_pool)
        {
            LOG(ERROR)<<"create pool faild";
            break;
        }


        //创建事件处理线程
        pj_thread_t* eventThread = NULL;
        //m_pool	PJSIP 内存池，用于线程栈内存分配
        //&pollingEvent	线程入口函数指针，线程启动后执行的函数
        //m_endpt	传递给线程入口函数的参数，这里是 PJSIP endpoint 对象指针
        //&eventThread	输出参数，保存创建成功的线程句柄
        status = pj_thread_create(m_pool,NULL,&pollingEvent,m_endpt,0,0,&eventThread);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"create pjsip thread faild,code:"<<status;
            break;
        }
    } while (0);

    bool bret = true;
    if(PJ_SUCCESS != status)
    {
        bret = false;
    }
    return bret;
    
}

pj_status_t SipCore::init_transport_layer(int sipPort)//传入下级端口
{
    pj_sockaddr_in addr;
    pj_bzero(&addr,sizeof(addr));
    addr.sin_family = pj_AF_INET();
    addr.sin_addr.s_addr = 0;
    addr.sin_port = pj_htons((pj_uint16_t)sipPort);

    pj_status_t status;
    do
    {
        //注册接收回调和启动异步接收
        //使用改方式创建的 socket 会注册到这个传输管理器中，之后 PJSIP 收发消息都靠这个管理器来调度和转发。
        //把这些 socket 注册到 PJSIP 的 IO 队列（ioqueue）中
     
        status = pjsip_udp_transport_start(m_endpt,&addr,NULL,1,NULL);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"start udp server faild,code:"<<status;
            break;
        }

        status = pjsip_tcp_transport_start(m_endpt,&addr,1,NULL);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"start tcp server faild,code:"<<status;
            break;
        }

        LOG(INFO)<<"sip tcp:"<<sipPort<<" udp:"<<sipPort<<" running";
    } while (0);
    
    return status;
}