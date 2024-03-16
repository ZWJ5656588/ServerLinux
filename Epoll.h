#pragma once
#include <unistd.h>
#include <sys/epoll.h>
#include <vector> // 存储events
#include <errno.h>
#include <sys/signal.h>
#include <memory.h>

#define EVENT_SIZE  1024

class EpollData //封装epoll_data 纯数据 epoll_event中的da
{
public:
	EpollData() { m_data.u64 = 0; }
	explicit EpollData(void* ptr) { m_data.ptr = ptr; }   // 防止空指针和0进行隐式转换
	explicit EpollData(int fd) { m_data.fd = fd; } // 只允许显式构造
	EpollData(uint32_t u32) { m_data.u32 = u32; }
    EpollData(uint64_t u64) { m_data.u64 = u64; }
	EpollData(const EpollData& data) { m_data.u64 = data.m_data.u64; }

	// 转换运算符重载 隐式运算符号重载
	operator epoll_data_t() { return m_data; }
	operator epoll_data_t() const { return m_data; }
	operator epoll_data_t* () { return &m_data; }
	operator const  epoll_data_t* () const { return &m_data; }

	// 赋值重载
	EpollData& operator=(const EpollData& data)
	{
		if (this != &data)
			m_data.u64 = data.m_data.u64; // 最大的内存进行赋值 后面方便后续处理
		return *this;
	}

	// 项目常用
	EpollData& operator=(void* data)
	{
		m_data.ptr = data; // 一般将套接字的数据传进来CSocketBase*
		return *this;
	}
	
	EpollData& operator=(int data) {
		m_data.fd = data;
		return *this;
	}

	EpollData& operator=(uint32_t data)
	{
		m_data.u32 = data;
		return *this;
	}

	EpollData& operator=(uint64_t data)
	{
		m_data.u64 = data;
		return *this;
	}
private:
	epoll_data_t m_data;
};

using EPEvents = std::vector<epoll_event>; // 接收epoll_wait返回来的就序数组


class CEpoll // 封装内核的epoll 不能复制拷贝
{
public:
	CEpoll() {
		m_epoll = -1; // -1 表示epoll树没有开启
	}
	~CEpoll() {
		Close();
	}
public:
	CEpoll(const CEpoll&) = delete;
	CEpoll& operator=(const CEpoll&) = delete;
public:
	operator int() const { return m_epoll; } // 类型转换
public:
	int Create(unsigned count)// 封装epoll_create
	{
		if (m_epoll != -1) return -1;
		m_epoll = epoll_create(count); // m_epoll表示返回后的epoll文件描述符 体现了万物皆文件的思想
		if (m_epoll == -1) return -2;
		return 0;
	}

	ssize_t WaitEvents(EPEvents& events, int timeout = -1) // 封装epoll_wait 
	{
		if (m_epoll == -1) return -1;
		EPEvents evs(EVENT_SIZE);
		int ret = epoll_wait(m_epoll, evs.data(), (int)evs.size(), timeout); // evs。data()返回vector首元素指针
		if (ret == -1) {
			if (errno == EINTR || errno == (EAGAIN | EWOULDBLOCK))
			{
				return 0; // 表示没有就绪文件描述符 由于信号原因 或者因为没有读取到数据
			}

			return -2;
		}

		if (ret > (int)events.size()) {
			events.resize(ret);  /*这样做可以避免在 epoll_wait 调用期间对原始 events 向量进行修改，从而保持了原始向量的稳定性。*/
		}

		// 事件发生之后拷贝到events中 events是传出变量 
		memcpy(events.data(), evs.data(), sizeof(epoll_event) * ret);
		return ret;
	}

	int Add(int fd, const EpollData& data = EpollData((void*)0), uint32_t events = EPOLLIN) // 封装添加文件描述符
	{
		if (m_epoll == -1) return -1;
		epoll_event ev = { events,data };
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, &ev);
		if (ret == -1) return -2;
		return 0;
	}
	int Modify(int fd, uint32_t events, const EpollData& data = EpollData(nullptr))
	{
		if (m_epoll == -1) return -1;
		epoll_event ev = { events,data };
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, &ev);
		if (ret == -1) return -2;
		return 0;
	}
	int Del(int fd)
	{
		if (m_epoll == -1) return -1;
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_DEL, fd, NULL);
		if (ret == -1) return -2;
		return 0;
	}
	void Close()
	{
		if (m_epoll != -1) {
			int fd = m_epoll; // 防御性编程
			m_epoll = -1;
			close(fd);
		}
	}
private:
	int m_epoll;
};



