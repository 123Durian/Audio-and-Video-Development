#ifndef _SIPLOCALCONFIG_H
#define _SIPLOCALCONFIG_H
#include "ConfReader.h"
#include "Common.h"
#include <list>
#include <queue>

// 用来调用读取配置文件类来保存数据


//定义配置文件路径
// #define COMFIGFILE_PATH "/mnt/hgfs/share/conf/SipSupService.conf"
class SipLocalConfig
{
    public:
        SipLocalConfig();
        ~SipLocalConfig();

        int ReadConf();           //读取配置文件
		void initRandPort();      //初始化RTP端口队列
        int popOneRandNum();      //取出一个RTP端口（偶数端口）
        int pushOneRandNum(int num);  //回收RTP端口（成对回收）
		
        inline string localIp(){return m_localIp;}
        inline string localPort(){return m_localPort;}
        inline string sipId(){return m_sipId;}
        inline string sipIp(){return m_sipIp;}
        inline int sipPort(){return m_sipPort;}


        //上级节点信息结构体
        struct SupNodeInfo
        {
            string id;
            string ip;
            int port;
            int poto; //协议
            int expires;
            string usr;
            string pwd;
            int auth; //是否认证
            string realm;
        };
        list<SupNodeInfo> upNodeInfoList; //存储多个上级节点信息
    private:
        ConfReader m_conf;   //配置文件读取器
        string m_localIp;   //本地IP
        int m_localPort;//本地端口
        string m_sipId;//SIP标识
        string m_sipIp;//SIP ip
        int m_sipPort;//sip 端口

        //备用上级节点信息
        string m_supNodeIp;
        int m_supNodePort;
        int m_supNodePoto;
        int m_supNodeAuth;



        //从配置字段中读取出来这两个字段

		int m_rtpPortBegin;//RTP端口起始号
        int m_rtpPortEnd;//RTP端口结束号

        //将上面两个字段存储到这个队列中
        std::queue<int> m_RandNum;//RTP端口队列

        //队列是共享的，对这个队列加上一把锁
        pthread_mutex_t m_rtpPortLock;
};
#endif