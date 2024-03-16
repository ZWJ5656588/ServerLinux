#pragma once
#include "Socket.h"
#include "http_parser.h"
#include <map>
#include <string.h>

/*ʹ��http_parser��,�ص�����������֪ͨ�û�������ڽ������ȵĲ�ͬ�¼���ͨ��������Щ�ص����������ʹ���߿����Զ���Ը���HTTP�����¼�����Ӧ��*/

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
	//GET POST ... �ο�http_parser.h HTTP_METHOD_MAP��
	unsigned Method() const { return m_parser.method; }
	const std::map<Buffer, Buffer>& Headers() { return m_HeaderValues; }
	const Buffer& Status() const { return m_status; }
	const Buffer& Url() const { return m_url; }
	const Buffer& Body() const { return m_body; }
	unsigned Errno() const { return m_parser.http_errno; }
protected:
	// ��̬���� stdcall
	static int OnMessageBegin(http_parser* parser); // stcall
	static int OnUrl(http_parser* parser, const char* at, size_t length);
	static int OnStatus(http_parser* parser, const char* at, size_t length);
	static int OnHeaderField(http_parser* parser, const char* at, size_t length);
	static int OnHeaderValue(http_parser* parser, const char* at, size_t length);
	static int OnHeadersComplete(http_parser* parser);
	static int OnBody(http_parser* parser, const char* at, size_t length);
	static int OnMessageComplete(http_parser* parser);

	/* ��Ƴɾ�̬��Ա�������г�Ա�������ϵ�ԭ��
	1. HTTP�����⣨��http_parser��ͨ��ʹ�ûص����������������¼�����URL������ɡ��յ�ͷ���ֶεȣ���
	��Щ��ͨ������C���Ա�д�ģ����������ص���������ͨ�ĺ���ָ�루����̬����������������ĳ�Ա������
	��Ա�����������ǷǾ�̬�ģ�����ͨ�����в�ͬ�ĵ���Լ������thiscall��������������ʽ�ذ���һ��ָ�����ʵ����thisָ����Ϊ������
	��ˣ�ֱ�ӽ���Ա��������Ϊ�ص�����ͨ���ǲ����еġ�

	2. ����̬�������Ա�������ʹ�ã����Ը����ع������������е�״̬����̬�������Դ�����Щ�������ڶ���״̬�Ľ�������
	����Ա�������Դ�����Щ��Ҫ���ʻ��޸Ķ���״̬������
	�ر�����C++�з�װC���Կ�ʱ�������������߽�������̵Ļص��ӿ�ת��Ϊ�������Ľӿڣ�ʹ��C++�е�����Ը���Ȼ����C���Կ�Э����
	*/

	// ��Ա���� thiscall
	int OnMessageBegin();
	int OnUrl(const char* at, size_t length);
	int OnStatus(const char* at, size_t length);
	int OnHeaderField(const char* at, size_t length);
	int OnHeaderValue(const char* at, size_t length);
	int OnHeadersComplete();
	int OnBody(const char* at, size_t length);
	int OnMessageComplete();
};

// URL������ ���������URL
class UrlParser
{
public:
	UrlParser(const Buffer& url);
	~UrlParser() {}
	int Parser();
	Buffer operator[](const Buffer& name)const;
	Buffer Protocol()const { return m_protocol; }
	Buffer Host()const { return m_host; }

	//Ĭ�Ϸ���80
	int Port()const { return m_port; }
	void SetUrl(const Buffer& url); 
	const Buffer Uri()const { return m_uri; }
private:
	Buffer m_url;
	Buffer m_protocol;
	Buffer m_host; // �����õ���������
	Buffer m_uri;
	int m_port; // �˿�
	std::map<Buffer, Buffer> m_values;
};