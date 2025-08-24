#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include <signal.h>


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
#include "GetCatalog.h"
#include "OpenStream.h"
#include "GetRecordList.h"
using namespace EC;





/*
注册和心跳都会更新lasttime，定时检查通过lasttime来判断该下级是否还有效
lasttime表示上次注册成功的时间点
*/
class SetGlogLevel
{
	public:
		SetGlogLevel(const int type)
		{
			//将日志重定向到指定文件中
			google::InitGoogleLogging(LOG_FILE_NAME);
			//设置输出到控制台的Log等级
			FLAGS_stderrthreshold = type;
			FLAGS_colorlogtostderr = true;
			FLAGS_logbufsecs = 0;
			FLAGS_log_dir = LOG_DIR;
			FLAGS_max_log_size = 4;
			google::SetLogDestination(google::GLOG_WARNING,"");
			google::SetLogDestination(google::GLOG_ERROR,"");
			signal(SIGPIPE,SIG_IGN);
		}
		~SetGlogLevel()
		{
			google::ShutdownGoogleLogging();
		}
};

void* func(void* argc)
{
	pthread_t id = pthread_self();
	LOG(INFO)<<"current thread id:"<<id;


	return NULL;
}
int main()
{
	srand(time(0));
	//signal(SIGINT,SIG_IGN);
	SetGlogLevel glog(0);
	SipLocalConfig* config = new SipLocalConfig();
	int ret = config->ReadConf();
	if(ret == -1)
	{
		LOG(ERROR)<<"read config error";
		return ret;
	}
	bool re = GlobalCtl::instance()->init(config);
	if(re == false)
	{
		LOG(ERROR)<<"init error";
		return -1;
	}
	LOG(INFO)<<GBOJ(gConfig)->localIp();


	EventMsgHandle* pMsgHandle = new EventMsgHandle(GBOJ(gConfig)->localIp(),GBOJ(gConfig)->localPort());
	if(pMsgHandle != NULL)
	{
		if(pMsgHandle->init() != 0)
		{
			LOG(ERROR)<<"EventMsgHandle init error";
			return -1;
		}
		delete pMsgHandle;
		pMsgHandle = NULL;
	}
	//定期检查下级
	SipRegister* regc = new SipRegister();
	regc->registerServiceStart();
	sleep(3);
	GetCatalog dirct;
	sleep(3);
	OpenStream::StreamGetProc(NULL);
	//OpenStream* gbStream = new OpenStream();
	//gbStream->StreamServiceStart();
	//GetRecordList list;
	while(true)
	{
		sleep(30);
	}
	return 0;
}