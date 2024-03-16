#pragma once
// 封装套接字 使用一个基类 本地套接字和网络通信套接字继承基类
// 封装本地套接字用于进程之间通信

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h> // 本地套接字
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <fcntl.h>
#include "Thread.h"
#include <atomic>


// 接收或者发送时候的缓冲区
class Buffer : public std::string
{
public:
	Buffer() :std::string() {}
	Buffer(size_t size) :std::string() { resize(size); }
	Buffer(const std::string & str) :std::string(str) {}
	Buffer(const char* str) :std::string(str) {}
	Buffer(const char* str, size_t length)
		:std::string() {
		resize(length);
		memcpy((char*)c_str(), str, length);
	}
	Buffer(const char* begin, const char* end) :std::string() {
		long int len = end - begin;
		if (len > 0) {
			resize(len);
			memcpy((char*)c_str(), begin, len);
		}
	}
	operator void* () { return (char*)c_str(); }
	operator char* () { return (char*)c_str(); }
	operator unsigned char* () { return (unsigned char*)c_str(); }
	operator char* () const { return (char*)c_str(); }
	operator const char* () const { return c_str(); }
	operator const void* () const { return c_str(); }
};
 
enum SockAttr {
	SOCK_ISSERVER = 1, // 是否为服务器 1表示是服务器 0 表示客户端 0000
	SOCK_ISNONBLOCK = 2, // 是否阻塞 1表示非阻塞 0表示阻塞  0100
	SOCK_ISUDP = 4, // 是否为UDP 1表示UDP 0表示TCP          1000
	SOCK_ISIP = 8,  // 是否是ip协议的网络套接字 1表示网络套接字 1000 
	SOCK_ISREUSE = 16 // 是否重用地址
};

class CSockParam {
public:
	CSockParam() {
		bzero(&addr_in, sizeof(addr_in));
		bzero(&addr_un, sizeof(addr_un));
		port = -1;
		attr = 0; // 默认客户端 阻塞 tcp 本地
	}

	// 网络套接字通信 传入 ip port attr
	CSockParam(const Buffer& ip, short port, int attr) {
		this->ip = ip;
		this->port = port;
		this->attr = attr;
		addr_in.sin_family = AF_INET;
		addr_in.sin_port = htons(port);
		addr_in.sin_addr.s_addr = inet_addr(ip);  
	}

	CSockParam(const sockaddr_in* addrin, int attr) {
		this->ip = ip;
		this->port = htons(port);
		this->attr = attr;
		memcpy(&addr_in, addrin, sizeof(addr_in));
	}
	
	// 本地套接字通信 传入本地路径path
	CSockParam(const Buffer& path,int attr)
	{
		ip = path;
		addr_un.sun_family = AF_UNIX; // 协议族
		strcpy(addr_un.sun_path, path);
		this->attr = attr;
	}

	CSockParam(const CSockParam& param)
	{
		// 拷贝构造函数
		ip = param.ip;
		port = param.port;
		attr = param.attr;
		memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
		memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
	}

	~CSockParam() {}


public:
	CSockParam& operator=(const CSockParam& param)
	{
		if (this != &param)
		{
			// 赋值运算符重载 
			ip = param.ip;
			port = param.port;
			attr = param.attr;
			memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
			memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
		}

		return *this;
	}

	sockaddr* addrin() { return (sockaddr*)&addr_in; }
	sockaddr* addrun() { return (sockaddr*)&addr_un; }
	

public:
	// 地址
	sockaddr_in addr_in; // 网络地址
	sockaddr_un addr_un; // 本地Unix套接字地址
	// ip
	Buffer ip;
	// port
	short port;
	// 套接字属性
	int attr;
};


class CSocketBase  // 套接字通信分文两个阶段 INIT 和 LINK
{
public:
	CSocketBase()
	{
		m_socket = -1; // 未定套接字
		m_status = 0; // 未初始化状态
	}
	virtual ~CSocketBase() {
		Close();
	} 

	// 类型转换拿套接字
	virtual operator int() { return m_socket; }
	virtual operator int() const { return m_socket; }
	// m_param是保护属性 需要类型转换重载
	virtual operator sockaddr_in* () { return &m_param.addr_in; }
	virtual operator const sockaddr_in* () const { return &m_param.addr_in; }

public:
	// 初始化 套接字创建 bind listen 客户端 套接字创建
	virtual int Init(const CSockParam& param) = 0;
	// 连接 服务器 accept 客户端 connect 对于udp 这里可以忽略
	virtual int Link(CSocketBase** pClient = nullptr) = 0; // 接口类无法声明对象
	// 发送数据
	virtual int Send(const Buffer& data) = 0;
	// 接收数据 
	virtual int Recv(Buffer& data) = 0;
	// 关闭连接
	virtual int Close()
	{
		m_status = 3;
		if (m_socket != -1) { // m_socket != -1时需要析构 unlink
			if((m_param.attr & SOCK_ISSERVER) && ((SOCK_ISIP & m_param.attr) == 0)) // 非网络套接字的服务器 使用unlink断开连接
 				unlink((char*)m_param.ip);
			int fd = m_socket;
			m_socket = -1;
			close(fd);
		}
		return 0;
	}
protected:
	// 允许子类继承 不允许外部访问

	// 套接字文件描述符 默认-1
	int m_socket;

	// 标记状态
	int m_status; // 防止没有init就进行数据收发 0初始化未完成 1初始化 2连接 3关闭

	//初始化参数
	CSockParam m_param;
}; 


// 本地套接字和网络套接字在一起写 
class CSocket:public CSocketBase
{
public:
	CSocket() : CSocketBase() {}// 抽象基类是可以有构造函数的 虽然基类不能初始化 但是抽象类被继承之后 派生类可以实例化
	CSocket(int sock) : CSocketBase() { m_socket = sock; }
	virtual ~CSocket() override {
		Close();
	}
public:
	// 初始化 套接字创建 bind listen 客户端 套接字创建
	virtual int Init(const CSockParam& param) {
		if ( m_status != 0)  return -1; // 初始化未完成
		m_param = param;
		int type = (m_param.attr & SOCK_ISUDP) ? SOCK_DGRAM : SOCK_STREAM;
		if (m_socket == -1) {
			if (param.attr & SOCK_ISIP)
				m_socket = socket(PF_INET, type, 0); // 创建网络套接字
			else
				m_socket = socket(PF_LOCAL, type, 0);
		}
		else
			m_status = 2; // accept已经连接 已经link完成
		if (m_socket == -1) return -2;
		int ret = 0;
		if (m_param.attr & SOCK_ISREUSE)
		{
			int option = 1;
			ret = setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
			if (ret == -1) return -7;
		}
		if (m_param.attr & SOCK_ISSERVER) { // 判断是不是服务器
			if (param.attr & SOCK_ISIP)
			{
				// 网络套接字通过addr_in来进行连接
				ret = bind(m_socket, m_param.addrin(), sizeof(sockaddr_in));
				printf("%d is ip server socket", m_socket);
			}
			else
			{
				// 本地套接字通过路径名绑定 
				ret = bind(m_socket, m_param.addrun(), sizeof(sockaddr_un));
				printf("%d is unix server socket", m_socket);
			}
			if (ret == -1)
			{
				return -3;
			}
			ret = listen(m_socket, 1024);
			if (ret == -1) return -4;
			std::cout << " sever init " << m_socket << " success " << "pid " << getpid() << "\n";
		}

		else {
			// 是客户端 没有bind过程 继续向下走
			std::cout << " client init " << m_socket << " success " << "pid " << getpid() << "\n";
		}
		if (m_param.attr & SOCK_ISNONBLOCK) // 如果要求非阻塞 设置非阻塞
		{
			int option = fcntl(m_socket, F_GETFL);
			if (option == -1) return -5;
			option |= O_NONBLOCK;
			ret = fcntl(m_socket, F_SETFL);
			if (ret == -1) return -6;
			std::cout << "client noblock " << "pid = " << getpid() << std::endl;
		}

		// 初始化完毕
		if(m_status == 0)
			m_status = 1;
		return 0;
	}


	// 连接 服务器 accept 客户端 connect 对于udp 这里可以忽略
	virtual int Link(CSocketBase** pClient = nullptr) // 接口类无法声明对象
	{
		// 这里*pClient可以传递出来
		if (m_status <= 0 || (m_socket == -1)) return -1;
		int ret = 0;
		if (m_param.attr & SOCK_ISSERVER) // 按位相与确定属性是否为服务器
		{
			// 服务端 ，对应的pClient不能为空
			if (pClient == nullptr) return -2;
			CSockParam param;
			int fd = -1;
			socklen_t len = 0;
			if (m_param.attr & SOCK_ISIP)
			{
				param.attr |= SOCK_ISIP;
				len = sizeof(sockaddr_in);
				fd = accept(m_socket, param.addrin(), &len); // 接收客户端连接
			}
			else 
			{
				len = sizeof(sockaddr_un);
				fd = accept(m_socket, param.addrun(), &len); // 接收客户端连接
			}
			if (fd == -1) return -3;
			*pClient = new CSocket(fd); // new一个套接字对象 这个客户端套接字是new出来的 在日志处理逻辑的时候要进行delete
			if (*pClient == nullptr) return -4;
			ret = (*pClient)->Init(param); // 客户端初始化 成功返回0 已经初始化根据状态判断直接返回
			if (ret != 0)
			{
				delete (*pClient);
				*pClient = nullptr;
				return -5;

			}

			std::cout << "server accept " << m_socket << " success " << "pid = " << getpid() << "\n";
		}
		else 
		{
			if (m_param.attr & SOCK_ISIP)
				ret = connect(m_socket, m_param.addrin(), sizeof(sockaddr_in));
			else
				ret = connect(m_socket, m_param.addrun(), sizeof(sockaddr_un));
			if (ret != 0)
			{
				printf("%s(%d): <%s> pid = %d,ret = %d,erron = %d, msg:%s\n", __FILE__, __LINE__, __FUNCTION__, getpid(), ret, errno, strerror(errno));
				return -6;
			}
			std::cout << "client link " << m_socket << " success "<< "pid = " << getpid() <<  "\n";
		}
		m_status = 2; // 表示已经进行link
		return 0;
	}


	// 发送数据
	virtual int Send(const Buffer& data) 
	{ 
		if (m_status < 2 || (m_socket == -1)) return -1; // 套接没有正确初始化
		ssize_t index = 0;
		
		//阻塞状态下调用比较简单 非阻塞程序会返回
		while(index < (ssize_t)data.size())
		{
			ssize_t len = write(m_socket, (char*)data + index, data.size() - index); 
			if (len == 0) return -2; 
			if (len < 0) return -3;
			index += len;
		}

		// printf("data: %s\n", (char*)data);


		return 0;
	}


	// 接收数据 
	virtual int Recv(Buffer& data) {
		if (m_status < 2 || (m_socket == -1))return -1;
		data.resize(1024 * 1024);
		ssize_t len = read(m_socket, data, data.size());
		if (len > 0) {
			data.resize(len);
			return (int)len;//收到数据
		}
		data.clear();
		if (len < 0) {
			if (errno == EINTR || (errno == EAGAIN)) {
				data.clear();
				return 0;//没有数据收到
			}
			return -2;//发送错误
		}
		return -3;//套接字被关闭
	}



	// 关闭连接
	virtual int Close() { return CSocketBase::Close(); }

private:
	CSockParam m_param;
};




