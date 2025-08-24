#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include <signal.h>
#include "rtpsession.h"
#include "rtpsourcedata.h"
#include "rtptcptransmitter.h"
#include "rtptcpaddress.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtplibraryversion.h"
#include "rtcpsrpacket.h"

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/thread.h>
#include "Common.h"
#include "SipLocalConfig.h"
#include "GlobalCtl.h"
#include "ECThread.h"
#include "SipRegister.h"
#include "SipHeartBeat.h"
using namespace EC;

//国标id的组成=中心编码（8位）+行业编码（两位）+类型编码（三位）+序号（七位）
//10000000+51+200+0000001
//目录树结构：
/*

1：根+摄像头
+200
   -131

2：根+业务分组（组织）+虚拟组织+摄像头

按照银行大楼举例
+200
   +215（代表某一层）
     +216（代表一层中的某一个房间）
	   -131（该房间中的某一个摄像头）
*/

//定时注册是如果没有注册会定期注册，心跳保活是注册之后更新其时间。
class SetGlogLevel
{
	public:
	//传入参数是日志级别
	//0	INFO	普通信息，用于程序运行状态	白色
    //  1	WARNING	警告信息，提示潜在问题	紫色/黄色
//2	ERROR	错误信息，但程序还能运行	红色
//3	FATAL	致命错误，程序会终止退出	红色高亮
		SetGlogLevel(const int type)
		{
			//将日志重定向到指定文件中
			//LOG_FILE_NAME是文件前缀
			//该函数是日志初始化函数，初始化之后就可以直接输出日志
			google::InitGoogleLogging(LOG_FILE_NAME);
			//设置输出到控制台的Log等级
			

			//对日志库的配置
			//都输出到文件，之后》type的也可以输出到终端
			//设置日志的等级
			FLAGS_stderrthreshold = type;
			//开启终端（stderr）日志输出的“彩色显示”功能。
			FLAGS_colorlogtostderr = true;
			//设置日志缓冲区的刷新时间，单位是秒
			FLAGS_logbufsecs = 0;
			//日志存储路径
			FLAGS_log_dir = LOG_DIR;
			//单个日志文件的最大大小，单位是 MB
			FLAGS_max_log_size = 4;

			//通过将 WARNING 和 ERROR 级别日志的文件输出路径设为空字符串，程序只会在终端输出这些级别的日志，不会写入对应日志文件。
			google::SetLogDestination(google::GLOG_WARNING,"");
			google::SetLogDestination(google::GLOG_ERROR,"");

			//忽略 SIGPIPE 信号
			//如果不忽略的话日志写失败，整个程序会奔溃，忽略的话不会影响整个程序运行
			signal(SIGPIPE,SIG_IGN);
		}
		~SetGlogLevel()
		{
			google::ShutdownGoogleLogging();
		}
};


//定义了一个标准的 POSIX 线程函数 func
void* func(void* argc)
{
	//获取当前线程id
	pthread_t id = pthread_self();
	LOG(INFO)<<"current thread id:"<<id;


	return NULL;
}
int main()
{
	//signal(SIGINT,SIG_IGN);
	//1：启动日志系统
	SetGlogLevel glog(0);
	//2：读取配置文件信息
	//读取的下级的配置文件中的东西放到该类数据成员中，读取的上级的配置文件信息放到pNodeInfoList链表中
	SipLocalConfig* config = new SipLocalConfig();
	int ret = config->ReadConf();
	if(ret == -1)
	{
		LOG(ERROR)<<"read config error";
		return ret;
	}
	//已经读取完配置文件信息，创建线程池并创建SIP服务器
	//将上级的配置信息保存到supDomainInfoList中
	//线程池：按照指定个数创建线程，并有一个信号量，该信号量绝对该线程是不是阻塞等待状态
	//初始化pjsip核心库，util库，初始化内存池，用改内存池的工厂去创建endpoint
	//用创建的endpoint对象去创建一个io队列（muduo）网络库
	//用内存池工厂，io队列去创建媒体端点
	//用endpoint对象去创建事务模块，会话模块和传输层
	bool re = GlobalCtl::instance()->init(config);
	if(re == false)
	{
		LOG(ERROR)<<"init error";
		return -1;
	}
	//记录本地服务器的ip地址到日志中
	LOG(INFO)<<GBOJ(gConfig)->localIp();

	//创建一个线程
	pthread_t pid;
	ret = EC::ECThread::createThread(func,NULL,pid);
	if(ret != 0)
	{
		ret = -1;
		LOG(ERROR)<<"create thread error";
		return ret;
	}


	LOG(INFO)<<"create thread pid:"<<pid;
	LOG(INFO)<<"main thread pid:"<<pthread_self();
	//创建一个SIP注册类对象
	SipRegister* regc = new SipRegister();
	regc->registerServiceStart();

	SipHeartBeat* heart = new SipHeartBeat();
	heart->gbHeartBeatServiceStart();
	// 防止程序运行完之后就结束
	while(true)
	{
		//避免cpu占用过高，定期让出cpu30s
		sleep(30);
	}
	return 0;
}