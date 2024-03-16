#pragma once
#include "http_parser.h"
#include "Socket.h"
#include <map>

// ����httpЭ��Ľӿ�
class CHttpPaser
{
private:
	// ��Ա����
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

	// GET POST ... �ο�http_paser.h HTTP_METHOD_MAP��
	unsigned Method() const { return m_parser.method; }
	const std::map<Buffer, Buffer>& Headers() { return m_HeadersValues; } // ����ͷ
	const Buffer& Status() const { return m_status; }
	const Buffer& Url() const { return m_url; }
	const Buffer& Body() const { return m_body; }
	unsigned Errno() const { return m_parser.http_errno; }
protected:




};

