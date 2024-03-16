#pragma once
#include"Socket.h"
#include"Epoll.h"
#include"ThreadPoll.h"
#include"Process.h"
#include"Logger.h"


// CFunction特例化
template<class _FUNCTION_, class... _ARGS_>
class CConnectedFunction : public CFunctionBase
{
public:
	CConnectedFunction(_FUNCTION_ func, _ARGS_... args) :m_binder(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...) // m_binder初始化
	{
	}

	virtual ~CConnectedFunction() {}
	virtual int operator() (CSocketBase* pClient)
	{
		return m_binder(pClient);
	}

	typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;
};


template<class _FUNCTION_, class... _ARGS_>
class CRecievedFunction : public CFunctionBase
{
public:
	CRecievedFunction(_FUNCTION_ func, _ARGS_... args) :m_binder(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...)
	{
	}

	virtual ~CRecievedFunction() {}
	virtual int operator() (CSocketBase* pClient, const Buffer& data)
	{
		return m_binder(pClient, data);
	}

	typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;
};

// 处理业务
class CBusiness
{
public:
	CBusiness() { m_connectedcallback = nullptr;  m_recvcallback = nullptr; }

	virtual int BusinessProcess(CProcess* proc) = 0;

	// 连接后的回调函数
	template<typename _FUNCTION_, typename... _ARGS_>
	int setConnectedCallback(_FUNCTION_ func, _ARGS_...args)
	{
		m_connectedcallback = new CConnectedFunction< _FUNCTION_, _ARGS_...>(func, args...);
		if (m_connectedcallback == nullptr) return -1;
		return 0;
	}

	template<typename _FUNCTION_, typename... _ARGS_>
	int setRecvCallback(_FUNCTION_ func, _ARGS_...args)
	{
		m_recvcallback = new CRecievedFunction< _FUNCTION_, _ARGS_...>(func, args...);
		if (m_recvcallback == nullptr) return -1;
		return 0;
	}
protected:
	// 回调函数
	CFunctionBase* m_connectedcallback;
	CFunctionBase* m_recvcallback;
};

class CServer
{
public:
	CServer();
	CServer(const CServer&) = delete;
	CServer& operator=(const CServer&) = delete;
	~CServer() { Close(); }
public:
	// 对外接口
	int Init(CBusiness* business,const Buffer& ip = "0.0.0.0", short port = 9999);
	int Run();
	int Close();
private:
	int ThreadFunc();
private:
	CThreadPool m_pool;
	CSocketBase* m_server;
	CEpoll m_epoll; // 主模块的epoll 接入客户端
	CProcess m_process;
	CBusiness* m_business; //业务模块
};

