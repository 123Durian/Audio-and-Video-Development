#ifndef _COMMON_H
#define _COMMON_H
#include <glog/logging.h>
#include "tinyxml2.h"
#include <json/json.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <semaphore.h>
#include <memory>
#include <sstream>
using namespace std;

#define LOG_DIR "/mnt/hgfs/share/log"
#define LOG_FILE_NAME "SipServer.log"
#define BODY_SIZE   1024*10

//智能锁，这个对象在栈上创建不能再堆上创建

//传入参数是一个互斥锁
//管理互斥锁的加锁和解锁
class AutoMutexLock
{
    public:
        AutoMutexLock(pthread_mutex_t* l):lock(l)
        {
            LOG(INFO)<<"getLock";
            getLock();
        };
        ~AutoMutexLock()
        {
            LOG(INFO)<<"freeLocak";
            freeLocak();
        };
    private:
        AutoMutexLock();
        AutoMutexLock(const AutoMutexLock&);
        AutoMutexLock& operator=(const AutoMutexLock&);
        //加锁
        void getLock(){pthread_mutex_lock(lock);}
        //释放锁
        void freeLocak(){pthread_mutex_unlock(lock);}
        pthread_mutex_t* lock;
};

class JsonParse
{
    public:
        JsonParse(string s)
        :m_str(s){}

        JsonParse(Json::Value j)
        :m_json(j){}

        //反序列化
        bool toJson(Json::Value& j)
        {
            //用于判断是否接续成功
            bool bret = false;
            //创建一个 JSON 解析器构造器对象 builder。
            Json::CharReaderBuilder builder;
            //将 builder 的配置 settings_ 设置为 严格模式
            Json::CharReaderBuilder::strictMode(&builder.settings_);
            //它用于控制解析器是否保留 JSON 字符串中的注释。
            builder["collectComents"] = true;

            //创建了一个新的Json::CharReader对象并用reader指向这个对象
            const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

            //这里定义了一个空字符串 errs，用于存储解析过程中产生的错误信息
            JSONCPP_STRING errs;
            //函数parse就是把string转换成json是吧
            bret = reader->parse(m_str.data(),m_str.data()+m_str.size(),&j,&errs);
            if(!bret || !errs.empty())
            {
                LOG(ERROR)<<"json parse error:"<<errs.c_str();
            }

            return bret;

        }

        //序列化/
        string toString()
        {
            Json::StreamWriterBuilder builder;
            char* indent="";
            builder["indentation"] = indent;//每一级的缩进
            return Json::writeString(builder,m_json);
        }

    private:
        string m_str;
        Json::Value m_json;

};

// struct StreamHeader
// {
    // char type; //媒体类型  1-》audio  2-》video
    // int pts;   //显示的时间戳
    // int length;  //本帧的长度，指后续负载的长度
    // int keyFrame;  //是否为I帧
// };

#endif