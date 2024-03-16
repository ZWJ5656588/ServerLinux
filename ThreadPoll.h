#pragma once
#include "Epoll.h"
#include "Thread.h"
#include "Socket.h"
#include "Function.h"
#include "Logger.h"

class CThreadPool
{
public:
	CThreadPool() {
		m_server = nullptr;
		timespec tp = { 0,0 }; // 秒和纳秒

		// cpu时钟 保证两个线程池对象不会重复
		clock_gettime(CLOCK_REALTIME, &tp);
		char* buf = nullptr;
		asprintf(&buf, "%d.%d.sock", tp.tv_sec % 10000, tp.tv_nsec % 100000); // 分配在堆区
		if (buf != nullptr) {
			m_path = buf;
			free(buf);
		}
		usleep(1); // 线程池启动阶段 避免多个线程池对象重复
	}
	~CThreadPool() {
		
	}

	CThreadPool(const CThreadPool&) = delete;
	CThreadPool& operator= (const CThreadPool&) = delete;

public:
	int Start(unsigned count) // 确定线程数量 epoll在start中创造
	{
		int ret = 0;
		if (m_server != nullptr) return -1; // 已经初始化
		if (m_path.size() == 0) return -2; // 构造函数失败
		m_server = new CSocket(); // 本地套接字
		if (m_server == nullptr) return -3;
		ret = m_server->Init(CSockParam(m_path, SOCK_ISSERVER)); // 启动时线程池相当于服务器
		//printf("%s(%d):[%s] pid = %d tid = %ld ret = %d m_path:=[%s]\n", __FILE__, __LINE__, __FUNCTION__, (int)getpid(), (int)pthread_self(), ret,(char*)m_path);
		if (ret != 0) return -4;
		ret = m_epoll.Create(count); 
		if (ret != 0) return -5;

		// epoll与线程绑定 后面有多线程进行wait
		m_epoll.Add(*m_server, EpollData((void*)m_server));
		if (ret != 0) return -6;

		// 绑定线程
		m_threads.resize(count);
		for (unsigned i = 0; i < count; i++) {
			m_threads[i] = new CThread(&CThreadPool::TaskDispatch, this);
			if (m_threads[i] == nullptr) return -7; // 在Close函数中进行错误处理
			ret = m_threads[i]->Start(); // 线程的启动函数
			if (ret != 0) return -8;
		}

		return 0;
	}
	void Close() {
		m_epoll.Close();
		if (m_server) {
			CSocketBase* p = m_server;
			m_server = nullptr;
			delete p;
			p = nullptr;
		}
		for (auto thread : m_threads)
		{
			if (thread)
			{
				auto tmp = thread;
				thread = nullptr;
				delete thread;
				thread = nullptr;
			}
		}
		m_threads.clear();
		// linux本地套接字连接会产生一个伪文件 大小永远是零字节 断开连接以后要进行unlink
		// init完成之后创建该套接字
		unlink(m_path);
	}

	// 添加任务 线程池抽象成一个本地服务器
	template<typename _FUNCTION_, typename..._ARGS_>
	int AddTask(_FUNCTION_ func, _ARGS_... args) {

		// 向线程池服务器发送任务函数的地址 
		static thread_local CSocket client; // 给每个线程分配一个客户端
		int ret = 0;
		if (client == -1) {
			ret = client.Init(CSockParam(m_path, 0));
			if (ret != 0) return -1;
			ret = client.Link();
			if (ret != 0) return -2;
		}
		//printf("%s(%d):[%s] pid = %d tid = %ld ret = %d m_path:[%s]\n", __FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), ret, (char*)m_path);

		// 添加任务 要把任务函数的指针发送给客户端
		CFunctionBase* base = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (base == nullptr) return -3;
		Buffer data(sizeof(base));
		memcpy(data, &base, sizeof(base)); // 将函数的地址复制到data,通过send 线程池分发到线程
		ret = client.Send(data);
		// printf("%s(%d):[%s] pid = %d tid = %ld ret = %d m_path:[%s]\n", __FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), ret, (char*)m_path);
		if (ret != 0)
		{
			delete base;
			base = nullptr;
			return -4;
		}
		return 0;
	}

	size_t Size()const { return m_threads.size(); }

private:
	// 任务调度函数 线程客户端去接任务
	int TaskDispatch() {
		while (m_epoll != -1) {
			EPEvents events;
			int ret = 0;
			// 这个epoll为了与线程池通信
			ssize_t esize = m_epoll.WaitEvents(events,5);
			if (esize > 0)
			{
				for (ssize_t i = 0; i < esize; i++)
				{
					if (events[i].events & EPOLLIN)
					{
						CSocketBase* pClient = nullptr;
						if (events[i].data.ptr == m_server)
						{
							// 服务器套接字监听到事件 表明有客户端连接 指的是创建的线程连入线程池
							ret = m_server->Link(&pClient);
							if (ret != 0) continue;
							m_epoll.Add(*pClient, EpollData((void*)pClient));
							if (ret != 0)
							{
								delete pClient;
								pClient = nullptr;
								continue;
							}
						}
						else
						{
							// 处理客户端发送的数据
							pClient = (CSocketBase*)events[i].data.ptr;
							if (pClient)
							{
								CFunctionBase* base = nullptr;
								Buffer data(sizeof(base)); 
								ret = pClient->Recv(data); // 收到客户端发来的任务函数指针 Recv中的recv方法收到的是pClient发来的任务函数指针
								if (ret <= 0)
								{
									m_epoll.Del(*pClient); // 下epoll树
									delete pClient;
									pClient = nullptr;
									continue;
								}
								memcpy(&base, (char*)data, sizeof(base));
								if (base != nullptr)
								{
									(*base)(); // 执行任务函数
									delete base;
									base = nullptr;
								}
							}
						}
					}
				}
			}
		}
		return 0;
	}
private:
	CEpoll m_epoll;  	
	std::vector<CThread*> m_threads; // 线程数组 使用指针 放到容器里面必须要有赋值构造函数 但是线程并不能被复制
	CSocketBase* m_server;  // 利用套接字发送任务函数的指针到客户端
	Buffer m_path; // 本地套接字

};