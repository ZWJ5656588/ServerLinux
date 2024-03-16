#pragma once
#include "Socket.h"
#include "http_parser.h"
#include <map>
#include <string.h>

/*使用http_parser库,回调函数机制来通知用户代码关于解析进度的不同事件。通过定义这些回调函数，库的使用者可以自定义对各种HTTP解析事件的响应。*/

class CHttpParser
{
private:
	http_parser m_parser;
	http_parser_settings m_settings;
	std::map<Buffer, Buffer> m_HeaderValues;
	Buffer m_status;
	Buffer m_url;
	Buffer m_body;
	bool m_complete;
	Buffer m_lastField;
public:
	CHttpParser();
	~CHttpParser();
	CHttpParser(const CHttpParser& http);
	CHttpParser& operator=(const CHttpParser& http);
public:
	size_t Parser(const Buffer& data);
	//GET POST ... 参考http_parser.h HTTP_METHOD_MAP宏
	unsigned Method() const { return m_parser.method; }
	const std::map<Buffer, Buffer>& Headers() { return m_HeaderValues; }
	const Buffer& Status() const { return m_status; }
	const Buffer& Url() const { return m_url; }
	const Buffer& Body() const { return m_body; }
	unsigned Errno() const { return m_parser.http_errno; }
protected:
	// 静态方法 stdcall
	static int OnMessageBegin(http_parser* parser); // stcall
	static int OnUrl(http_parser* parser, const char* at, size_t length);
	static int OnStatus(http_parser* parser, const char* at, size_t length);
	static int OnHeaderField(http_parser* parser, const char* at, size_t length);
	static int OnHeaderValue(http_parser* parser, const char* at, size_t length);
	static int OnHeadersComplete(http_parser* parser);
	static int OnBody(http_parser* parser, const char* at, size_t length);
	static int OnMessageComplete(http_parser* parser);

	/* 设计成静态成员函数还有成员函数相结合的原因
	1. HTTP解析库（如http_parser）通常使用回调函数来处理解析事件（如URL解析完成、收到头部字段等）。
	这些库通常是用C语言编写的，它们期望回调函数是普通的函数指针（即静态函数），而不是类的成员方法。
	成员方法（尤其是非静态的）与普通函数有不同的调用约定（如thiscall），并且它们隐式地包含一个指向对象实例的this指针作为参数。
	因此，直接将成员方法设置为回调函数通常是不可行的。

	2. 将静态方法与成员方法配合使用，可以更灵活地管理解析过程中的状态。静态方法可以处理那些不依赖于对象状态的解析任务，
	而成员方法可以处理那些需要访问或修改对象状态的任务。
	特别是在C++中封装C语言库时。它允许开发者将面向过程的回调接口转换为面向对象的接口，使得C++中的类可以更自然地与C语言库协作。
	*/

	// 成员方法 thiscall
	int OnMessageBegin();
	int OnUrl(const char* at, size_t length);
	int OnStatus(const char* at, size_t length);
	int OnHeaderField(const char* at, size_t length);
	int OnHeaderValue(const char* at, size_t length);
	int OnHeadersComplete();
	int OnBody(const char* at, size_t length);
	int OnMessageComplete();
};

// URL解析类 解析传入的URL
class UrlParser
{
public:
	UrlParser(const Buffer& url);
	~UrlParser() {}
	int Parser();
	Buffer operator[](const Buffer& name)const;
	Buffer Protocol()const { return m_protocol; }
	Buffer Host()const { return m_host; }

	//默认返回80
	int Port()const { return m_port; }
	void SetUrl(const Buffer& url); 
	const Buffer Uri()const { return m_uri; }
private:
	Buffer m_url;
	Buffer m_protocol;
	Buffer m_host; // 解析得到的主机名
	Buffer m_uri;
	int m_port; // 端口
	std::map<Buffer, Buffer> m_values;
};