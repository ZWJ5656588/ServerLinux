#pragma once

#include "Function.h"
#include <memory.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include "Socket.h"


// 进程控制，子进程与主进程交互 参数类
class CProcess
{
public:
	CProcess()
	{
		m_func = NULL;
		memset(pipes, 0, sizeof(pipes));

	}

	// 进程入口函数 先写进程模型 再写线程模型
	// 限制到模板成员函数
	template<typename _FUNCTION_, typename... _ARGS_>
	int SetEntryFunction(_FUNCTION_ func, _ARGS_... args)
	{
		m_func = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		return 0;
	}

	// 创建子进程
	int CreateSubProcess()
	{
		if (m_func == nullptr)
		{
			return -1;
		}

		// fork之前创建分离进程之前进行通信创建 socketpair进程间通信 本地套接字 pipe只能在父子进程之间进行管道通信 
		int ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, pipes); // AF_LOCAL 表示unix本地通信协议族 0表示默认协议 pipes接收创建出来的文件描述符数组  

		if (ret == -1) return -2;

		pid_t pid = fork();  // 每次派生子进程时候需要进行测试
		if (pid == -1) return -3;
		if (pid == 0) { // 子进程 pid == 0

			//  关闭写端
			close(pipes[1]);
			pipes[1] = 0;

			//  子进程进行调用
			ret = (*m_func)(); // 函数调用运算符
			exit(0);  // 退出进程
		}

		// 主进程

		// 关闭读端  主进程写 子进程读
		close(pipes[0]);
		pipes[0] = 0;

		m_pid = pid;
		return  0;
	}

	~CProcess() {
		if (m_func != nullptr)
		{
			delete m_func;
			m_func = nullptr;
		}
	}

	// 主进程发送套接字 sendMessage
	int SendFd(int fd) // 传文件描述符
	{
		struct msghdr msg {};
		// 创建两个缓冲区
		iovec iov[2] = { 0 };
		char buf[2][10] = { "BUAA", "TRANSE" };
		iov[0].iov_base = buf[0];
		iov[0].iov_len = sizeof(buf[0]);
		iov[1].iov_base = buf[1];
		iov[1].iov_len = sizeof(buf[1]);
		msg.msg_iov = iov;
		msg.msg_iovlen = 2;


		// 主进程要发送的数据 真正需要传递的数据 
		/* cmsghdr* cmsg = new cmsghdr;
		bzero(cmsg, sizeof(cmsghdr));*/
		auto cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int))); // 原先cmsghdr结构体加上int的四个字节
		if (cmsg == nullptr) return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int)); // 加了一个文件描述符类型的大小  辅助传输的数据类型是int
		cmsg->cmsg_level = SOL_SOCKET; // 套接字级别的辅助数据
		cmsg->cmsg_type = SCM_RIGHTS; // 表明是传递文件描述符
		*(int*)CMSG_DATA(cmsg) = fd;

		msg.msg_control = cmsg; // 传送辅助数据
		msg.msg_controllen = cmsg->cmsg_len;

		// sendmsg发送套接字数据
		ssize_t ret = sendmsg(pipes[1], &msg, 0);

		free(cmsg);

		if (ret == -1)
		{
			printf("%s\n", strerror(errno)); // bad descriptor
			return -2;
		}

		return 0;
	}

	// 子进程接收套接字
	int RecvFd(int& fd)
	{
		msghdr msg;
		iovec iov[2];
		char buf[][10] = { 0 }; // 两行的二位数组 每一行是元素大小是10
		iov[0].iov_base = buf[0];
		iov[0].iov_len = sizeof(buf[0]);
		iov[1].iov_base = buf[1];
		iov[1].iov_len = sizeof(buf[1]);
		msg.msg_iov = iov;
		msg.msg_iovlen = 2; // 接缓冲区数据 接收BUAA TRANSE


		// 接收关键的附带数据
		auto cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));  // 编译时候就计算了长度
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS; // 拿到的是文件描述符

		// 消息头控制字段 用来接收传输过来的附带数据(比如文件描述符)
		msg.msg_control = cmsg;
		msg.msg_controllen = CMSG_LEN(sizeof(int));

		ssize_t ret = recvmsg(pipes[0], &msg, 0); // 读端
		// printf("%s(%d): <%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);  /*测试日志的时候出现了死循环!!!!*/
		if (ret == -1)
		{
			free(cmsg);
			return -2;
		}

		fd = *(int*)CMSG_DATA(cmsg); // 接收到文件描述符 传递文件描述符时候要进行测试  fork()之后进行测试 派生之后再传递文件描述符
		free(cmsg);
		return 0;


	}

	// 发送套接字
	int SendSocket(int fd, const sockaddr_in* addrin) // 传文件描述符
	{
		struct msghdr msg {};
		iovec iov;
		char buf[20] = "";
		bzero(&msg, sizeof(msg));
		memcpy(buf, addrin, sizeof(sockaddr_in));
		iov.iov_base = buf;
		iov.iov_len = sizeof(buf);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;


		// 主进程要发送的数据 真正需要传递的数据 
		/* cmsghdr* cmsg = new cmsghdr;
		bzero(cmsg, sizeof(cmsghdr));*/
		auto cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int))); // 原先cmsghdr结构体加上int的四个字节
		if (cmsg == nullptr) return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int)); // 加了一个文件描述符类型的大小  辅助传输的数据类型是int
		cmsg->cmsg_level = SOL_SOCKET; // 套接字级别的辅助数据
		cmsg->cmsg_type = SCM_RIGHTS; // 表明是传递文件描述符
		*(int*)CMSG_DATA(cmsg) = fd;

		msg.msg_control = cmsg; // 传送辅助数据
		msg.msg_controllen = cmsg->cmsg_len;

		// sendmsg发送套接字数据
		ssize_t ret = sendmsg(pipes[1], &msg, 0);

		free(cmsg);

		if (ret == -1)
		{
			printf("***error: %s\n", strerror(errno));
			return -2;
		}

		return 0;
	}

	// 子进程接收套接字
	int RecvSocket(int& fd,sockaddr_in* addrin)
	{
		msghdr msg;
		iovec iov;
		char buf[20] = ""; // 两行的二位数组 每一行是元素大小
		bzero(&msg, sizeof(msg));
		iov.iov_base = buf;
		iov.iov_len = sizeof(buf);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1; // 接缓冲区数据 接收BUAA TRANSE


		// 接收关键的附带数据
		auto cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));  // 编译时候就计算了长度
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS; // 拿到的是文件描述符

		// 消息头控制字段 用来接收传输过来的附带数据(比如文件描述符)
		msg.msg_control = cmsg;
		msg.msg_controllen = CMSG_LEN(sizeof(int));

		ssize_t ret = recvmsg(pipes[0], &msg, 0); // 读端
		// printf("%s(%d): <%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);  /*测试日志的时候出现了死循环!!!!*/
		if (ret == -1)
		{
			free(cmsg);
			return -2;
		}

		fd = *(int*)CMSG_DATA(cmsg); // 接收到文件描述符 传递文件描述符时候要进行测试  fork()之后进行测试 派生之后再传递文件描述符
		free(cmsg);
		return 0;


	}

	// 转换到守护进程 避免远程会话断开导致资源丢失
	static int SwitchDeamo() {
		pid_t ret = fork();
		if (ret == -1) return -1;
		if (ret > 0) exit(0); // 主进程到此结束
		// 子进程
		ret = setsid(); // 创建一个新的会话组 并且当前为进程组长
		if (ret == -1) return -2;

		ret = fork();
		if (ret == -1) return -3;
		if (ret > 0) exit(0); // 子进程到此为止 这样的话孙进程就是一个逃离ssh会话的孤儿进程

		// 孙进程如下 进入守护状态
		for (int i = 0; i < 3; i++) close(i); // 把输入输出错误输出关了 确认守护进程不会继承打开的文件描述符 后面的打印输出不会执行
		umask(0); // 守护进程有权读写文件
		signal(SIGCHLD, SIG_IGN); // 不关心子进程的退出状态 防止子进程成为僵尸进程 不等待子进程 也不让其成为僵尸进程

		return 0;

	}

private:
	// 基类模板的指针调用
	CFunctionBase* m_func;
	pid_t m_pid;

	// 管道 用于进程间通信 主要是传递主进程拿到的文件描述符 pipe(pipes
	int pipes[2];
};