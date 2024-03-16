#pragma once
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h> // 权限操作 文件类型
#include <signal.h>
#include <map>
#include <iostream>
#include "Function.h" // 用到函数模板
#include <errno.h>
#include <map>



// 日志模块开启线程

class CThread
{
public:
	CThread() {
		m_function = nullptr;
		m_thread = 0;
		m_bpaused = false;
	}

	// 可调用对象 
	template<typename _FUNCTION_, typename... _ARGS_>
	CThread(_FUNCTION_ func, _ARGS_...args) : m_function(new CFunction<_FUNCTION_, _ARGS_...>(func, args...)) {
		m_thread = 0;
		m_bpaused = false;
	} // 有参构造

	~CThread() {}

public:
	CThread(const CThread&) = delete;
	CThread operator=(const CThread&) = delete;

public:
	template<typename _FUNCTION_, typename... _ARGS_>
	int SetThreadFunc(_FUNCTION_ func, _ARGS_... args)
	{
		m_function = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		return 0;
	}

	// 启动线程
	int Start() {
		int ret = 0;
		pthread_attr_t attr; // 设置线程属性
		ret = pthread_attr_init(&attr); if (ret != 0) return -1;
		ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE); if (ret != 0) return -2;// 线程分离 无需主进程join
		// ret = pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS); if (ret != 0) return -3;// 线程内部进行竞争 这里会出bug 先默认注释掉 后面研究
		// 线程调用时是全局调用的 并不依赖于某一个对象 所以考虑静态成员函数
		pthread_create(&m_thread, &attr, &CThread::ThreadEntry, this); if (ret != 0) return -4;  //启动成员函数的线程 传入this指针 运行线程
		m_mapThread[m_thread] = this; // 线程id映射到线程类对象的地址
		ret = pthread_attr_destroy(&attr); // 销毁线程
		if (ret != 0) return -5;
		return 0;
	}

	int Pause() // 暂停和恢复
	{
		if (m_thread == 0)  return -1;
		if (m_bpaused) // 表示暂停 需要恢复
		{
			m_bpaused = false;
			return 0;
		}
		m_bpaused = true; // 要暂停 先设置状态
		if (m_thread == 0) return -1;
		int ret = pthread_kill(m_thread, SIGUSR1);
		if (ret != 0)
		{
			// 信号设置失败 暂停失败 改回原来不暂停的状态
			m_bpaused = false;
			return -2;
		}
		return 0;
	}


	int Stop() // 终止线程
	{
		if (m_thread != 0)
		{
			pthread_t thread = m_thread;
			m_thread = 0; // 防御性编程 线程
			timespec ts;
			ts.tv_sec = 0;
			ts.tv_nsec = 100 * 1000000; // 100ms 
			int ret = pthread_timedjoin_np(thread, nullptr, &ts); // 等待子进程结束
			if(ret == ETIMEDOUT) // 连接超时
			{
				pthread_detach(thread); // 强制分离线程
				// 发一个信号量
				pthread_kill(thread, SIGUSR2); // 线程要处理这个信号 用户自定义信号
			}
		}
		return 0;

	}

	bool isVaild() const { return m_thread != 0; } // 判断线程是否stop // 这里一旦出现错误 则

private:
	// 线程入口函数 保证入口函数不会暴露  并且与对象无关 在不创建对象之前就可以调用  
	static void* ThreadEntry(void* arg) {
		CThread* ptr = (CThread*)arg; // this指针传入arg形参
		typedef struct sigaction sig;  // 用于处理信号
		sig act = {0};
		sigemptyset(&act.sa_mask); // 信号集合设置为空
		act.sa_flags = SA_SIGINFO; 
		act.sa_sigaction = &CThread::Sigaction;

		// 当SIGUSR!或者SIGUSR2传到这个进程的时候 将执行act绑定的信号处理函数
		sigaction(SIGUSR1, &act, nullptr);
		sigaction(SIGUSR2, &act, nullptr);
		ptr->EnterThread(); // 进入线程
		

		// 线程正常退出时候
		if (ptr->m_thread != 0) ptr->m_thread = 0; // 正常结束 置零
		auto it = m_mapThread.find(pthread_self());
		if(it != m_mapThread.end()) m_mapThread[pthread_self()] = nullptr;
		pthread_detach(pthread_self());
		pthread_exit(NULL);

	}

	static void Sigaction(int signo, siginfo_t* info, void* contex)
	{
		if (signo == SIGUSR1)
		{
			// 留给pause
			pthread_t thread = pthread_self(); // 获取当前线程id
			auto it = m_mapThread.find(thread);
			if (it != m_mapThread.end())
			{
				if (it->second) // CThread存在
				{
					// it->second->m_bpaused = true; // 设置暂停 调用时候已经设置暂停
					while (it->second->m_bpaused) {
						if (it->second->m_thread == 0) {
							// 线程无效 关闭这个线程
							pthread_exit(nullptr);
						}
						usleep(1000); //1ms 微秒级别
					}
				}
			}
		}
		else if (signo == SIGUSR2)
		{

			// 该信号处理线程退出
			pthread_exit(nullptr);
		}
	}

	void EnterThread() { // _thiscall
		if (m_function != nullptr)
		{
			printf("*****%s(%d):[%s] pid = %d tid = %ld***** \n", __FILE__, __LINE__, __FUNCTION__, (int)getpid(), (int)pthread_self());
			int ret = (*m_function)(); // 回调任务函数
			// printf("*****%s(%d):[%s] pid = %d tid = %ld ret = %d ***** \n", __FILE__, __LINE__, __FUNCTION__, (int)getpid(), (int)pthread_self(),ret);
			if (ret != 0) { // 后面封装到日志模块
				printf("%s(%d):[%s]ret = %d\n", __FILE__, __LINE__, __FUNCTION__,ret);
			}
		}
	}

	// _thiscall
private:
	CFunctionBase* m_function;
	pthread_t m_thread;
	bool m_bpaused; // true表示暂停 false表示正在运行  暂停和恢复
	static std::map<pthread_t, CThread*> m_mapThread; // 类内声明 类外初始化 线程映射表

};


