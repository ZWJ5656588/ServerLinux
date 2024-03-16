#pragma once

#include <functional>
#include <sys/types.h> 
#include <unistd.h>

class CSocketBase;
class Buffer;

// 脱离模板的基类设计
class CFunctionBase
{
public:
	// 虚析构函数
	virtual ~CFunctionBase() {}
	virtual int operator()() { return -1; }

	// 虚函数和模板函数不能同时使用
	// 但是模板类可以有虚函数 
	virtual int operator()(CSocketBase*) { return -1; }
	virtual int operator()(CSocketBase*, const Buffer&) { return -1; }
};

// 模板类 用来封装任务函数
template<class _FUNCTION_, class... _ARGS_>
class CFunction : public CFunctionBase
{
public:
	CFunction(_FUNCTION_ func, _ARGS_... args) :m_binder(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...) // m_binder初始化
	{
		// std::forward完美转发
	}

	virtual ~CFunction() {}
	virtual int operator() ()
	{
		return m_binder();  // 将参数都打包到m_binder中 m_binder就开始调用函数
		//return f();
	}

	// STL实现 type表示根据指定参数推到最终的函数对象类型 
	// std::function<void()> f = std::bind(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)..);
	typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder; // typename告知编译器这是一个类型名字 而不是一个函数或者成员
};


