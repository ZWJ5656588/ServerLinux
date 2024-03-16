#include "Logger.h"

LogInfo::LogInfo(const char* file, int line, const char* func, pid_t pid, pthread_t tid, int level, const char* fmt ...)
{
	const char sLevel[][8] = {
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	};
	char* buf = nullptr;
	bAuto = false;

	// 日志头的显示 文件 行号 日期 时间 进程id 线程id  日志级别 函数
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s)", file, line, sLevel[level], (char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0)
	{
		m_buf = buf;
		free(buf);
	}
	else return;

	va_list ap; // 处理可变参数与列表 可变参数声明时候不确定 调用确定
	va_start(ap, fmt); // 处理可变参数列表

	// 日志消息体的显示 用户写入数据的显示！
	count = vasprintf(&buf, fmt, ap); // vasprintf和asprintf动态分配足够内存给buf 不同的是vasprintf分配可变参数
	if (count > 0)
	{
		m_buf += buf;
		free(buf);
		buf = nullptr;
	}

	m_buf += "\n";
	va_end(ap);
	// 

}

LogInfo::LogInfo(const char* file, int line, const char* func, pid_t pid, pthread_t tid, int level)
{
	// 自己主动发送的输入输出流式的日志
	bAuto = true;
	const char sLevel[][8] = {
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	};
	char* buf = nullptr;

	// 日志头的显示 文件 行号 日期 时间 进程id 线程id  日志级别 函数
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s)", file, line, sLevel[level], (char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0)
	{
		m_buf = buf;
		free(buf);
	}
}

LogInfo::LogInfo(const char* file, int line, const char* func, pid_t pid, pthread_t tid, int level, void* pData, size_t nSize)
{
	bAuto = false;
	const char sLevel[][8] = {
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	};
	char* buf = nullptr;

	// 日志头的显示 文件 行号 日期 时间 进程id 线程id  日志级别 函数
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s) \n", file, line, sLevel[level], (char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0)
	{
		m_buf = buf;
		free(buf);
	}
	else return;

	// dump内存导出模块
	Buffer out;
	size_t i = 0;
	char* Data = (char*)pData;
	for (; i < nSize; i++)
	{
		char buf[16] = "";
		snprintf(buf, sizeof(buf), "%02X ", Data[i] & 0xFF);
		m_buf += buf;

		// 每十六个数据一行
		if (0 == ((i + 1) % 16))
		{
			m_buf += "\t; ";
			char buf[17] = "";
			memcpy(buf, Data + i - 15, 16);
			for (int j = 0; j < 16; j++)
				if ((buf[j] < 32) && (buf[j] >= 0)) buf[j] = '.';
			m_buf += buf;
			
			/*for (size_t j = i - 15; j <= i; j++)
			{
				if ((Data[j] & 0xFF) > 31 && (Data[j] & 0xFF) < 0X7F) // 在32到127范围内是设备可打印字符 小于32是控制字符
				{
					m_buf += Data[i];
				}
				else
				{
					m_buf += '.'; // 不可打印
				}
			}*/

			m_buf += "\n"; // 换行
		}
	}

	// 尾部触发
	size_t k = i % 16;
	if (k != 0) {
		// 对齐操作
		for (size_t j = 0; j < 16 - k; j++) m_buf += "   "; // 这里空三个字符
		m_buf += "\t; "; // 这里错误写成 = "\t"导致打印不出来
		for (size_t j = i - k; j <= i; j++)
		{
			if ((Data[j] & 0xFF) > 31 && (Data[j] & 0xFF) < 0x7F)// 在32到127范围内是设备可打印字符 小于32是控制字符
			{
				m_buf += Data[j];
			}
			else
			{
				m_buf += "."; // 不可打印
			}
		}
		m_buf += "\n"; // 换行
	}

	printf("%s(%d):[%s] pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
}

LogInfo::~LogInfo()
{
	if (bAuto)
	{
		m_buf += "\n";
		CLoggerServer::Trace(*this);
	}
}

LogInfo::operator Buffer() const
{
	return m_buf;
}
