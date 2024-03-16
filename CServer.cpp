#include "CServer.h"

CServer::CServer()
{
	m_server = nullptr;
	m_business = nullptr;
}

int CServer::Init(CBusiness* business,const Buffer& ip, short port)
{
	int ret = 0;
	if (business == nullptr) return -1;
	m_business = business;	
	// 创建客户端处理进程 
	ret = m_process.SetEntryFunction(&CBusiness::BusinessProcess, m_business,&m_process);
	if (ret != 0) return -2;
	m_process.CreateSubProcess();
	if (ret != 0) return -3;
	ret = m_pool.Start(15);
	if (ret != 0) return -4;
	ret = m_epoll.Create(15);
	if (ret != 0) return -5;
	m_server = new CSocket();
	if (m_server == nullptr) return -6;
	m_server->Init(CSockParam(ip, port, SOCK_ISSERVER | SOCK_ISIP | SOCK_ISREUSE));
	LOGI << "main epoll server has been init";
	if (m_server == nullptr) return -7;
	m_epoll.Add(*m_server, EpollData((void*)m_server));
	if (m_server == nullptr) return -8;
	for (size_t i = 0; i < m_pool.Size(); i++)
	{
		ret = m_pool.AddTask(&CServer::ThreadFunc, this);
		if (ret != 0) return -9;
	}
	return 0;
}

int CServer::Run()
{
	while (m_server != nullptr)
	{
		usleep(10);
	}
	return 0;
}

int CServer::Close()
{
	if (m_server)
	{
		CSocketBase* sock = m_server;
		m_server = nullptr;
		m_epoll.Del(*sock);
		delete sock;
	}
	m_epoll.Close();
	m_process.SendFd(-1);
	m_pool.Close();
	return 0;
}

// 线程任务函数
int CServer::ThreadFunc()
{
	// 这里的epoll为了接入客户端 解决
	
	TRACEI("epoll %d server %p", (int)m_epoll, m_server);
	int ret = 0;
	EPEvents events;
	while ((m_epoll != -1) && (m_server != nullptr))
	{
		ssize_t size = m_epoll.WaitEvents(events,5);
		if (size < 0) break;
		if (size > 0)
		{
			// // TRACEI("size = %d event %08X", size, events[0].events);
			for (ssize_t i = 0; i < size; i++)
			{
				if (events[i].events & EPOLLERR)
				{
					break;
				}
				else if (events[i].events & EPOLLIN)
				{
					if (m_server)
					{
						CSocketBase* pClient = nullptr;
						ret = m_server->Link(&pClient); // 服务端Link拿到客户端的套接字和地址设置到了pClient指针变量中 
						printf("%s(%d): <%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
						if (ret != 0) continue; 
						// 拿到客户端的套接字和客户端地址 发送给客户端处理进程
						ret = m_process.SendSocket(*pClient,*pClient);
						//TRACEE("Send client socket %d success! ", (int)*pClient);
						int s = *pClient;
						delete pClient;
						if (ret != 0)
						{
							//TRACEE("Send client %d failed", s); 
							continue;
						}
					}
				}
			}
		}
	}
	//TRACEI("服务器终止");
	return 0;
}
