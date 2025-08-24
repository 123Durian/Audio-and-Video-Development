#include "SipCore.h"
#include "Common.h"
#include "SipDef.h"
#include "GlobalCtl.h"
#include "SipTaskBase.h"
#include "SipRegister.h"
#include "SipHeartBeat.h"
#include "SipDirectory.h"
#include "SipGbPlay.h"
#include "ECThread.h"
#include "SipRecordList.h"

using namespace EC;
//made and point

static int pollingEvent(void* arg)
{
    LOG(INFO)<<"polling event thread start success";
    pjsip_endpoint* ept = (pjsip_endpoint*)arg;
    while(!(GlobalCtl::gStopPool))
    {
        pj_time_val timeout = {0,500};
        //在一定时间内检查这个事务的事件
        //不管有没有事件，时间到了之后立马返回
        pj_status_t status = pjsip_endpt_handle_events(ept,&timeout);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"polling events failed,code:"<<status;
            return -1;
        }
    }

    return 0;

}
//表示接收消息
//实现多线程
//线程中有两个参数，一个是base一个是rdata
pj_bool_t onRxRequest(pjsip_rx_data *rdata)
{
    LOG(INFO)<<"request msg coming....";
    
    if(NULL == rdata || NULL == rdata->msg_info.msg)
    {
        return PJ_FALSE;
    }
    threadParam* param = new threadParam();
    //将rdata拷贝到param的data中
    pjsip_rx_data_clone(rdata,0,&param->data);

    //先拿到message
    pjsip_msg* msg = rdata->msg_info.msg;
    //给结构体中赋值
    if(msg->line.req.method.id == PJSIP_REGISTER_METHOD)
    {
        param->base = new SipRegister();
    }
    else if(msg->line.req.method.id == PJSIP_OTHER_METHOD)
    {
        //不是所有的message都是心跳包，所以我们要根据body的

        //rootType: 用于保存解析出来的 XML 根节点名称，Root Element
        //cmdType: 指定要提取的 XML 字段名
        //cmdValue: 用于存储字段 CmdType 对应的 字段值

        string rootType = "",cmdType = "CmdType",cmdValue;
        //解析 SIP 消息体中的 XML 数据 的关键一步，作用是从字符串 msg 中提取出响应的数据
        tinyxml2::XMLElement* root = SipTaskBase::parseXmlData(msg,rootType,cmdType,cmdValue);
        LOG(INFO)<<"rootType:"<<rootType;
        LOG(INFO)<<"cmdValue:"<<cmdValue;
        if(rootType == SIP_NOTIFY && cmdValue == SIP_HEARTBEAT)
        {
            param->base = new SipHeartBeat();
        }
        else if(rootType == SIP_RESPONSE)
        {
            if(cmdValue == SIP_CATALOG)//设备目录
            {
                param->base = new SipDirectory(root);
            }
            else if(cmdValue == SIP_RECORDINFO)//录像消息查询
			{
				param->base = new SipRecordList(root);
			}
            
        }
    }
+
/*
tinyxml2::XMLDocument* pxmlDoc = NULL;     // 第一步：指针初始化为空
pxmlDoc = new tinyxml2::XMLDocument();     // 第二步：创建空的 XMLDocument 实例
if(pxmlDoc){
//将解析出来的数据放到pxmlDoc中
    pxmlDoc->Parse((char*)msg->body->data);  // 第三步：把 XML 字符串解析进去
}
*/



//创建线程
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


//在该模块中是上级请求一个信令，下级去回复
static pjsip_module recv_mod=
{
    NULL,NULL,//指定模块的上层和下层
    {"mod-recv",8},//模块的名字和字符串长度
    -1,//module_id
    PJSIP_MOD_PRIORITY_APPLICATION,//定义宏指定该模块为应用层模块
    NULL,//加载
    NULL,//运行
    NULL,//结束
    NULL,//卸载，是空直接返回pj_success，不需要endpoint调用
    onRxRequest,//接收请求的消息回调
    NULL,//响应消息回调，就是发送请求的时候，这个回调会返回响应码
    NULL,//在endpoint使用transport即将将消息发送前调用的，意思是让用户在消息发送的最后在检查一下此事务
    NULL,//在发送前检查以下响应消息
    NULL,//事务状态的变更
};
SipCore::SipCore()
:m_endpt(NULL)
{

}
SipCore::~SipCore()
{
    pjmedia_endpt_destroy(m_mediaEndpt);
    pjsip_endpt_destroy(m_endpt);
    pj_caching_pool_destroy(&m_cachingPool);
    pj_shutdown();
    GlobalCtl::gStopPool = true;
}

void SipCore::stopSip()
{
	pjmedia_endpt_destroy(m_mediaEndpt);
    pjsip_endpt_destroy(m_endpt);
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
    //将该线程加到pjsip中
    //pjsip的基础库是pjlib，再用到pjsip时，涉及到一个新的线程，就要进行pjlib线程的注册
    pjcall_thread_register(desc);
    //执行run函数
    param->base->run(param->data);
    delete param;
    param = NULL;
}

bool SipCore::InitSip(int sipPort)
{
    //int的别名
    pj_status_t status;
    //0-关闭  6-详细
    //pjsip的日志
    pj_log_set_level(0);

    do
    {
        //初始化pjlib/pj
        status = pj_init();
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"init pjlib faild,code:"<<status;
            break;
        }
        status = pjlib_util_init();
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"init pjlib util faild,code:"<<status;
            break;
        }
        
        //创建pjsip内存池

        //核心模块endpoint对象需要通过pjsip内存池工厂进行内存分配，内存创建

        //初始化内存池
        pj_caching_pool_init(&m_cachingPool,NULL,SIP_STACK_SIZE);

                //第一个参数是内存池工厂
        //内存池：静态的，预分配一段连续的空间，并按块进行管理
        //内存池工厂：动态的，内存池的创建和管理者，提供了对内存池分配的策略
        //创建endpoint对象
        status = pjsip_endpt_create(&m_cachingPool.factory,NULL,&m_endpt);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"create endpt faild,code:"<<status;
            break;
        }


       //media and point
        pj_ioqueue_t* ioqueue = pjsip_endpt_get_ioqueue(m_endpt);
        //&m_mediaEndpt输出参数，函数执行成功后，它将指向创建的 pjmedia_endpt对象
        //不让对象创建在这个函数当中，因为这样的话这个函数执行完这个对象就被析构了
        status = pjmedia_endpt_create(&m_cachingPool.factory,ioqueue,0,&m_mediaEndpt);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"create media endpoint faild,code:"<<status;
            break;
        }

        //事务层模块，tcp_transaction
        //创建一个事务
        status = pjsip_tsx_layer_init_module(m_endpt);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"init tsx layer faild,code:"<<status;
            break;
        }

        //dialog模块初始化
        status = pjsip_ua_init_module(m_endpt,NULL);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"init UA layer faild,code:"<<status;
            break;
        }

        //初始化传输层
        status = init_transport_layer(sipPort);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"init transport layer faild,code:"<<status;
            break;
        }

        //注册一个专门接收数据的模块
        //注册sip模块
        status = pjsip_endpt_register_module(m_endpt,&recv_mod);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"register recv module faild,code:"<<status;
            break;
        }

        status = pjsip_100rel_init_module(m_endpt);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"100rel module init faild,code:"<<status;
            break;
        }


        //创建媒体会话的回调
        //该结构体是设置invite事件回调的集合
        pjsip_inv_callback inv_cb;
        pj_bzero(&inv_cb,sizeof(inv_cb));
        inv_cb.on_state_changed = &SipGbPlay::OnStateChanged;
        inv_cb.on_new_session = &SipGbPlay::OnNewSession;//有新的请求过来先响铃或者200ok
        inv_cb.on_media_update = &SipGbPlay::OnMediaUpdate;
        inv_cb.on_send_ack = &SipGbPlay::OnSendAck;
        //绑定回调
        status = pjsip_inv_usage_init(m_endpt,&inv_cb);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"register invite module faild,code:"<<status;
            break;
        }

        //创建endpoint内存池
        //返回一个内存池对象
        m_pool = pjsip_endpt_create_pool(m_endpt,NULL,SIP_ALLOC_POOL_1M,SIP_ALLOC_POOL_1M);
        if(NULL == m_pool)
        {
            LOG(ERROR)<<"create pool faild";
            break;
        }

        //启动一个线程轮询的去处理endpoint事务
        pj_thread_t* eventThread = NULL;//输出参数
        //m_endpt表示线程函数传入的参数第一个0表示栈空间
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


//传输层，将sip消息发送到网络并接收来自网络的sip消息
//支持udp tcp tls
pj_status_t SipCore::init_transport_layer(int sipPort)
{
    pj_sockaddr_in addr;
    //清空结构体内存
    pj_bzero(&addr,sizeof(addr));
    addr.sin_family = pj_AF_INET();
    addr.sin_addr.s_addr = 0;
    addr.sin_port = pj_htons((pj_uint16_t)sipPort);

    pj_status_t status;
    do
    {
        //启动udp
        status = pjsip_udp_transport_start(m_endpt,&addr,NULL,1,NULL);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"start udp server faild,code:"<<status;
            break;
        }

        //启动tcp
        //处于listen状态
        //将TCP连接注册到m_endpt
        //当 TCP Socket 收到 SIP 消息时，自动调用 m_endpt 的消息分发逻辑。
        //当其他模块（如事务层）需要发送 SIP 消息时，通过 m_endpt 找到可用的传输层。
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