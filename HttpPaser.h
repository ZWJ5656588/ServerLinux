#pragma once
#include "http_parser.h"
#include "Socket.h"
#include <map>

// 解析http协议的接口
class CHttpPaser
{
private:
	// 成员属性
	http_parser m_parser;
	http_parser_settings m_settings;
	std::map<Buffer, Buffer> m_HeadersValues;
	Buffer m_status;
	Buffer m_url;
	Buffer m_body;
	bool m_complate;

public:
	CHttpPaser();
	~CHttpPaser();
	CHttpPaser(const CHttpPaser& http);
	CHttpPaser& operator= (const CHttpPaser& http);
public:
	size_t Paser(const Buffer& data);

	// GET POST ... 参考http_paser.h HTTP_METHOD_MAP宏
	unsigned Method() const { return m_parser.method; }
	const std::map<Buffer, Buffer>& Headers() { return m_HeadersValues; } // 请求头
	const Buffer& Status() const { return m_status; }
	const Buffer& Url() const { return m_url; }
	const Buffer& Body() const { return m_body; }
	unsigned Errno() const { return m_parser.http_errno; }
protected:




};

