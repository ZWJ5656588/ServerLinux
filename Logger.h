#pragma once
#include "Thread.h"
#include "Epoll.h"
#include "Socket.h"
#include <map>
#include <sys/timeb.h> // 拿到毫秒数
#include <sys/stat.h>
#include <stdarg.h>
#include <sstream>

/*Thread，Epoll，Socket的头文件都用于Logger模块的功能实现
	Logger模块实现*/

enum LogLevel{
	LOG_INFO,
	LOG_DEBUG,
	LOG_WARNING,
	LOG_ERROR,
	LOG_FATAL // 致命错误
};

class CLoggerServer;
class LogInfo;

// 封装LogInfo
class LogInfo {
public:
	LogInfo(
		const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level,
		const char* fmt ...);
	
	// Log << 流式日志
	LogInfo(
		const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level);
	
	// DUMP
	LogInfo(
		const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level,
		void* pData, size_t nSize);
	
	// 析构
	~LogInfo();

	operator Buffer() const;

	// 流式运算符重载
	template<typename T>
	LogInfo& operator<< (const T& data);

private:
	bool bAuto; // 默认是false 流媒体日志为true
	Buffer m_buf;
};

class CLoggerServer
{
public:
	// 初始化开启线程 传入可调用函数地址
	CLoggerServer() :m_thread(&CLoggerServer::ThreadFunc, this), m_server(nullptr)
	{
		char curpath[256] = "";
		getcwd(curpath, sizeof(curpath));
		m_path = curpath;
		m_path += "/log/" + GetTimeStr() + ".log";
	
		printf("%s(%d):[%s]path=%s\n", __FILE__, __LINE__, __FUNCTION__, (char*)m_path);
	}
	~CLoggerServer() {
		Close();

	}
public:
	CLoggerServer(const CLoggerServer&) = delete;
	CLoggerServer& operator= (const CLoggerServer&) = delete;
public:
	// 对外接口

	// 启动日志服务器 日志一个线程写即可
	int Start() {
		if (m_server != nullptr) return -1;
		if (access("log", W_OK | R_OK) != 0) { // 等于0表示已经有了log文件 且有读写权限
			// 说明log文件夹不存在 需要新建
			mkdir("log", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH); // mkdir("log", 0664); // 八进制表示创建文件的权限
		}
		
		// 创建并且打开文件 获得文件的FILE
		m_file = fopen(m_path, "w+"); // 标准C库打开文件
		if (m_file == nullptr) return -2;
		int ret = m_epoll.Create(1); // 启动epoll
		if (ret != 0) return -3;
		m_server = new CSocket(); // 创建服务端 本地套接字
		if (m_server == nullptr) {
			Close();
			return -4;
		}

		// 本地套接字
		ret = m_server->Init(CSockParam("./log/server.sock", SOCK_ISSERVER | SOCK_ISREUSE)); // 阻塞 tcp 服务器
		if (ret != 0) {
			printf("%s(%d): <%s> pid = %d,ret = %d,erron = %d, msg:%s", __FILE__, __LINE__, __FUNCTION__, getpid(), ret, errno, strerror(errno));
			Close();
			return -5;
		}

		// 客户端监听文件描述符上树
		m_epoll.Add(*m_server, EpollData((void*)m_server), EPOLLIN | EPOLLERR);
		if (ret != 0)
		{
			Close();
			return -6;
		}

		// 这里模拟使用异步proactor模式 epoll_wait置于子线程操作
		ret = m_thread.Start(); // 一步一步测试 debug 
		if (ret != 0) {
			printf("%s(%d): <%s> pid = %d,ret = %d,erron = %d, msg:%s", __FILE__, __LINE__, __FUNCTION__, getpid(), ret, errno, strerror(errno));
			Close();
			return -7;
		}

		return 0;
	}


	// 退出日志
	int Close() {
		if (m_server != nullptr) {
			CSocketBase* p = m_server;
			m_server = nullptr;
			delete p;
		}
		m_epoll.Close(); // close epolll
		m_thread.Stop(); // 线程停止
		return 0;
	}

	// 给其他非日志的进程和线程使用 为其他线程或者进程提供简单统一的方式记录日志信息
	static void Trace(const LogInfo& info) {
		// 三种日志 内存导出 标准输入输出 pirnt
		usleep(100*000); // 等待子进程的日志服务器初始化完毕 等待100毫秒
		int ret = 0;
		//printf("%s(%d): <%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
		static thread_local CSocket client; // 每一个线程都有自己的client实例

		if (client == -1) // 需要初始化
		{
			// 客户端也初始化到这个路径 对应相应的服务器！！
			ret = client.Init(CSockParam("./log/server.sock", 0)); // 默认非阻塞 客户端 tcp
			if (ret != 0) {
#ifdef  _DEBUG
				printf("%s(%d):[%s]ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
#endif // RELEASE
				return;
			}
			//printf("%s(%d):[%s]ret=%d client=%d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client);
			ret = client.Link();
		}
		// printf("%s(%d):[%s] ret=%d client=%d pid = %d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client,getpid());
		// 初始化成功 

		ret = client.Send(info); 
		// 
		// printf("%s(%d):[%s] ret=%d client=%d pid = %d\n" , __FILE__, __LINE__, __FUNCTION__, ret,(int)client, getpid());
		// send成功
	}

	// 拿到毫秒数
	static Buffer GetTimeStr() {
		Buffer result(128);
		timeb tmb;
		ftime(&tmb); // 填充当前的时间和日期信息到变量tmb中
		tm* pTm = localtime(&tmb.time); // 将一个通用的时间格式转换为一个详细的,分散的,时间格式 
		int nSize = snprintf(result, result.size(), "%04d-%02d-%02d__%02d-%02d-%03d",
			pTm->tm_year + 1900, pTm->tm_mon + 1,pTm->tm_mday, pTm->tm_min, pTm->tm_sec,
			tmb.millitm);
		result.resize(nSize);
		return result;
	}

private:
	// 后台写日志
	void WriteLog(const Buffer& data)
	{
		if (m_file != nullptr)
		{
			auto pFile = m_file;
			// 这里在后台写日志
			size_t ret = fwrite((char*)data, 1, data.size(), pFile);
			printf("%s(%d):[%s] ret = %d pid = %d\n", __FILE__, __LINE__, __FUNCTION__, ret,getpid());
			fflush(pFile);

			// 测试输出
#ifdef _DEBUG
			printf("DEBUG: %s", (char*)data);
#endif  // RELEASE
		}
;
	}
private:
	// 线程执行回调函数 线程里面进行 private权限
	int ThreadFunc()
	{
		/*printf("%s(%d):[%s] valid = %d pid = %d\n", __FILE__, __LINE__, __FUNCTION__, m_thread.isVaild(),getpid());
		printf("%s(%d):[%s] epoll = %d pid = %d\n", __FILE__, __LINE__, __FUNCTION__, (int)m_epoll, getpid());
		printf("%s(%d):[%s] server= %p pid = %d\n", __FILE__, __LINE__, __FUNCTION__, m_server, getpid());*/

		EPEvents events;
		std::map<int, CSocketBase*> mapClients{};
		while (m_thread.isVaild() && m_epoll != -1 && (m_epoll != -1) && m_server != nullptr) // epoll若等于epoll_create返回的int值(表示epoll树的个数)
		{
			// 进入子线程epoll等待
			ssize_t ret = m_epoll.WaitEvents(events, 10000);
			// 执行epoll_wait之后的处理套接字的逻辑
			// printf("%s(%d):[%s] epoll=%d  pid = %d\n", __FILE__, __LINE__, __FUNCTION__, (int)m_epoll, getpid());
			if (ret < 0) break;
			if (ret >= 0)
			{
				ssize_t i = 0; // 如果出现EPOLLERR（epoll错误) 则ret与i出现差值
				for (; i < ret; i++)
				{
					if (events[i].events & EPOLLERR)
					{
						break;
					}

					// 执行读取传入数据的操作
					else if (events[i].events & EPOLLIN) {

						// accept的文件描述符是监听文件描述符 表示有客户端连接
						if (events[i].data.ptr == m_server) {
							CSocketBase* pClient = nullptr;
								
							int r = m_server->Link(&pClient);
							if (r < 0) continue; // 连接失败 继续读取下一个文件描述符
							//printf("%s(%d):[%s] r=%d\n", __FILE__, __LINE__, __FUNCTION__, r);
							r = m_epoll.Add(*pClient, EpollData((void*)pClient), EPOLLIN | EPOLLERR); // 添加客户端文件描述符上epoll树，数据类型转换去拿socket文件描述
							//printf("%s(%d):[%s] r=%d\n", __FILE__, __LINE__, __FUNCTION__, r);
							if (r < 0)
							{
								delete pClient; // 没有连接上 释放客户端new出来的资源
								continue;
							}

							// 映射表中寻找到文件描述符 表示已经重复使用  
							auto it = mapClients.find(*pClient);
							if (it != mapClients.end())
							{
								if(it->second) delete it->second; // 删除旧的映射
							}

							//  映射表中添加映射
							mapClients[*pClient] = pClient;

						}
						// 处理客户端读写
						else
						{
							//printf("%s(%d):[%s]ptr=%p\n", __FILE__, __LINE__, __FUNCTION__, events[i].data.ptr);
							CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;
							if (pClient != nullptr)
							{
								//printf("%s(%d):[%s]ptr=%p\n", __FILE__, __LINE__, __FUNCTION__, events[i].data.ptr);
								Buffer data(1024 * 1024) ;

								// 日志服务器读取其他线程TRACE过来的数据
								int r = pClient->Recv(data);
								//printf("%s(%d):[%s] r=%d\n", __FILE__, __LINE__, __FUNCTION__, r);

								if (r <= 0) {									
									mapClients[*pClient] = nullptr; 									
									delete pClient; // 接收出现错误 置空
								}
								else
								{ 
									//printf("%s(%d):[%s] pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
									WriteLog(data);
								}
								//printf("%s(%d):[%s] r=%d\n", __FILE__, __LINE__, __FUNCTION__, r);
							}

						}
					}
				}
				if (i != ret) break; // 出现差值 则有文件描述符错误,提前退出了  则break这个线程
			}
		}

		// 防止内存泄露
		for (auto it = mapClients.begin(); it != mapClients.end(); it++)
		{
			if (it->second) {
				delete it->second;
			}

		}

		mapClients.clear();
		return 0;

	}

private:
	CThread m_thread;  // 线程
	CEpoll m_epoll; // io多路复用 处理高并发
	CSocketBase* m_server; // 日志服务器的本地套接字
	Buffer m_path;
	FILE* m_file; // 这里的记录日志文件描述符我们区别与套接字的一般描述符 使用C语言的FILE进行表示
};

// 创建一个宏

//#ifndef DISABLE_LOGGING

#ifndef TRACE
#define TRACEI(...) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_INFO,__VA_ARGS__))
#define TRACED(...) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_DEBUG,__VA_ARGS__))
#define TRACEW(...) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_WARNING,__VA_ARGS__))
#define TRACEE(...) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_ERROR,__VA_ARGS__))
#define TRACEF(...) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_FATAL,__VA_ARGS__))

// LOGI << "hello" << "how are you";
#define LOGI LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_INFO)
#define LOGD LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_DEBUG)
#define LOGW LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_WARNING)
#define LOGE LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_ERROR)
#define LOGF LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_FATAL)

// 00 01 02 03 65... ; ... a 内存导出 
# define DUMPI(data,size) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_INFO,data,size))
# define DUMPD(data,size) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_DEBUG,data,size))
# define DUMPW(data,size) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_WARNING,data,size))
# define DUMPE(data,size) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_ERROR,data,size))
# define DUMPF(data,size) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_FATAL,data,size)) // 这些日志格式知道构造函数的生成
#endif

//#else

//#define TRACEI(...)
//#define TRACED(...)
//#define TRACEW(...)
//#define TRACEE(...)
//#define TRACEF(...)
//
//#define LOGI
//#define LOGD
//#define LOGW
//#define LOGE
//#define LOGF
//
//#define DUMPI(data,size)
//#define DUMPD(data,size)
//#define DUMPW(data,size)
//#define DUMPE(data,size)
//#define DUMPF(data,size)
//
//#endif

template<typename T>
inline LogInfo& LogInfo::operator<<(const T& data)
{
	// TODO: 在此处插入 return 语句
	std::stringstream stream;
	stream << data;
	m_buf += stream.str().c_str();
	return *this;
}
