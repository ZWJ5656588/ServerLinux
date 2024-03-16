/*若进程被挂起 进程底下的线程也全部挂起
主进程->守护进程，主进程->网络服务器 
日志服务器->客户端处理->数据库*/


// 先准备好进程结构 再去使用线程 
#include <stdio.h>
#include "Process.h"
#include "Logger.h"
#include "ThreadPoll.h"
#include "PlayerServer.h"
#include "MysqlClient.h"
#include <thread>


 

// 日志服务器 子进程 传入CProcess 与主进程交互
int CreateLogServer(CProcess *proc)
{
	// 测试点
	//printf("%s(%d): <%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	CLoggerServer server; // 构造的时候就绑定了处理日志的函数
	int ret = server.Start();	
	if (ret != 0) {
		printf("%s(%d): <%s> pid = %d,ret = %d,erron = %d, msg:%s\n", __FILE__, __LINE__, __FUNCTION__, getpid(), ret, errno, strerror(errno));
	}
	int fd = -1;
	while(true)
	{
		ret = proc->RecvFd(fd); // 没有收到 阻塞
		//printf("%s(%d): <%s> fd = %d pid = %d\n", __FILE__, __LINE__, __FUNCTION__, fd,getpid());
		if (fd >= 0) break; // 收到大于0 结束进程 
	}
	ret = server.Close();
	printf("%s(%d): <%s> ret = %d pid = %d\n", __FILE__, __LINE__, __FUNCTION__, ret,getpid());
	return 0;
}

// 客户端处理 子进程  后期还要进行封装 做到面向对象
int CreateClientServer(CProcess* proc)
{
	//printf("%s(%d): <%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	int fd = -1;
	int ret = proc->RecvFd(fd); // 传的是引用 拿到文件描述符
	printf("%s(%d): <%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	printf("%s(%d): <%s> fd = %d\n", __FILE__, __LINE__, __FUNCTION__, fd);
	//if (ret != 0)  
	//printf("%s(%d): <%s> pid = %d,ret = %d,erron = %d, msg:%s", __FILE__, __LINE__, __FUNCTION__, getpid(), ret, errno, strerror(errno));
	sleep(1);
	char buf[10] = "";
	lseek(fd, 0, SEEK_SET); // 文件指针移动到文件开头
	read(fd, buf, sizeof(buf)); 
	printf("%s(%d): <%s> buf = %s\n", __FILE__, __LINE__, __FUNCTION__, buf);
	close(fd);
	return 0;
}

// 日志进程进行测试
int LogTest()
{
	//printf("%s(%d): <%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	char buffer[] = "BUAA Zhuwenjun! 交通学院 Zwj Love 前沿院 Wy";
	usleep(1000 * 000); // 等待子进程启动
	TRACEI("here is Log %d %c %f %g %s 交通基础设施软件研发", 2313216, 'z',2.0f,20.2415,buffer);
	printf("%s(%d): <%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	DUMPD((void*)buffer, (size_t)sizeof(buffer));
	printf("%s(%d): <%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	LOGE << 100 << " " << 'S' << " " << 0.12345f << " " << 1.2343535 << 1.2345589 << " " << buffer << " 工业软件研发";
	printf("%s(%d): <%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	return 0;
}

// 进程间传递文件描述符 以及线程池测试
int threadTest()
{
	// 设置守护进程
	// CProcess::SwitchDeamo();
	CProcess proclog, procclients;
	printf("%s(%d): <%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	proclog.SetEntryFunction(CreateLogServer, &proclog);
	int ret = proclog.CreateSubProcess(); // 启动 日志子进程
	if (ret != 0)
	{
		printf("%s(%d): <%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
		return -1;
	}
	/*LogTest();*/
	printf("%s(%d): <%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	//CThread thread(LogTest);
	//thread.Start();
	procclients.SetEntryFunction(CreateClientServer, &procclients); // 启动客户端处理子进程
	ret = procclients.CreateSubProcess();
	if (ret != 0)
	{
		printf("%s(%d): <%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
		return -2;
	}
	printf("%s(%d): <%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	usleep(100 * 000);
	// 在创建出子进程之后 再传递文件描述符
	int fd = open("./test.txt", O_RDWR | O_CREAT | O_APPEND);
	printf("%s(%d): <%s> fd = %d\n", __FILE__, __LINE__, __FUNCTION__, fd); // 测试文件描述符
	if (fd == -1) return -3;
	ret = procclients.SendFd(fd); // 发送文件描述符 主发送给子进程
	printf("%s(%d): <%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	if (ret != 0) printf("errno:%d msg:%s\n", errno, strerror(errno));
	write(fd, "BUAA", 5);
	close(fd);
	CThreadPool pool;
	ret = pool.Start(2);
	
	// 主进程起四个线程来测试
	std::vector<std::thread> threads;
	for (int i = 0; i < 4; i++)
	{
		auto func = std::bind(&CThreadPool::AddTask<int(*)()>, &pool, LogTest);
		threads.emplace_back(std::move(func));
	}

	printf("%s(%d): <%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	ret = pool.AddTask(LogTest);
	printf("%s(%d): <%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	ret = pool.AddTask(LogTest);
	printf("%s(%d): <%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	ret = pool.AddTask(LogTest);
	printf("%s(%d): <%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	ret = pool.AddTask(LogTest);
	printf("%s(%d): <%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	for (auto& th : threads)
		th.join();
	(void)getchar();
	printf("%s(%d): <%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	pool.Close(); // 清理掉epoll和本地套接字伪文件
	ret = proclog.SendFd(1); // 传递一个有效的文件描述符
	printf("%s(%d): <%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	(void)getchar();
	printf("%s(%d): <%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret); // -2;
	return 0;
}

// 主模块进行测试
int serverTest()
{
	int ret = 0;
	CProcess prolog;
	ret = prolog.SetEntryFunction(CreateLogServer, &prolog);
	ERROR_RETURN(ret, -1);
	ret = prolog.CreateSubProcess();
	ERROR_RETURN(ret, -2);
	CPlayerServer business(10); // 主进程开启两个线程 处理业务
	CServer server; // 客户端接入
	ret = server.Init(&business); // CServer开启客户端处理进程
	ERROR_RETURN(ret, -3);
	ret = server.Run();
	ERROR_RETURN(ret, -4);
	return 0;
}


// http模块测试
#include "HttpParser.h"
int http_test()
{
	const char* str = "GET / dianng / ? vfrm = pcw_home & vfrmblk = 712211_topNav & vfrmrst = 712211_channel_dianng HTTP / 2\r\n"
		"Host : www.iqi.com\r\n"
		"User - Agent : Mozilla / 5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko / 20100101 Firefox / 116.0\r\n"
		"Accept : text / html, application / xhtml + xml, application / xml; q = 0.9, image / avif, image / webp, */*;q=0.8\r\n"
		"Accept-Language: zh-CN,zh;q=0.8,zh-TW;q=0.7,zh-HK;q=0.5,en-US;q=0.3,en;q=0.2\r\n"
		"Accept-Encoding: gzip, deflate, br\r\n"
		"Referer: https://www.iqi.com/\r\n"
		"Upgrade-Insecure-Requests: 1\r\n"
		"Sec-Fetch-Dest: document\r\n"
		"Sec-Fetch-Mode: navigate\r\n"
		"Sec-Fetch-Site: same-origin\r\n"
		"Sec-Fetch-User: ?1\r\n"
		"Connection: keep-alive\r\n"
		"Cookie: __oaa_session_id__=4589b516c6cd4745b26a08adaec21cd3; QC005=20c5a79fa77843da878fec7aab3fc50b; QC173=0; QC008=1708695973.1708695973.1708695973.1; QC010=185903486; QC007=https%2525253A%2525252F%2525252Fwww.baidu.com%2525252Flink%2525253Furl%2525253Dw2yF4ue022tMd0eXNkMnjU-FZyE-YnhbCro1i3Vnjai%25252526wd%2525253D%25252526eqid%2525253D9ed0ed440000779c0000000365d8a1a2; QC175=%7B%22upd%22%3Atrue%2C%22ct%22%3A%22%22%7D; QC006=045467ae05cb37e242e8f9b9fc2be001; QP0037=0; __uuid=8cacaa3c-4764-d6c1-c908-b91752c3b3ff; QC189=5257_B%2C8004_C%2C6082_B%2C5335_B%2C5465_B%2C6843_B%2C6832_C%2C7074_C%2C7682_C%2C5924_D%2C6151_C%2C5468_B%2C7024_C%2C5592_B%2C6031_B%2C6629_B%2C5670_B%2C6050_B%2C6578_B%2C6312_B%2C6091_B%2C6237_A%2C6249_C%2C7996_B%2C6704_C%2C6752_C%2C7332_B%2C7381_B%2C7423_C%2C7581_A%2C7760_A; QC186=false; TQC030=1; QC142=9b9c2b170b5c131e; QC191=; nu=0; __dfp=a096133da1695e4926a043fa1dee251d099f70242b611e9961195461bbe56fa51a@1709991975792@1708695976792; QC218=3efa2aef62f3101f9abe16fb3e04de35; QP0034=%7B%22v%22%3A16%2C%22dp%22%3A1%2C%22m%22%3A%7B%22wm-vp9%22%3A1%2C%22wm-av1%22%3A1%7D%2C%22hvc%22%3Afalse%7D; QP0030=1; QC220=b0892a519ac80c7980244bea72c7cd0d; QC219=a0664e83e1342e4cb694c46135f6286c83872e9460bc064ca5f3f8968d5cb1c4df; QP0036=2024223%7C37.766; QC187=true; __oaa_session_referer_app__=pcw-player; __oaa_session_id__=f79407ed68ba47f9adf305ed5dc3d7bf; T00404=84ee58bbf90aa44a58f375492c825e02; IMS=IggQARj_geOuBioyCiBlODAyNTBmMzY3ZjU4YTNhY2Y5YTBmMGIwZDIxM2YzYhAAIggI0AUQAhiwCSg-MAVyJAogZTgwMjUwZjM2N2Y1OGEzYWNmOWEwZjBiMGQyMTNmM2IQAA\r\n"
		"TE: trailers\r\n"
		"\r\n";

	CHttpParser parser;
	size_t size = parser.Parser(str);
	if (parser.Errno() != 0) {
		printf("errno %d\n", parser.Errno());
		return -1;
	}
	if (size != strlen(str)) {
		printf("size error:%lld  %lld\n", size,strlen(str));
		return -2;
	}
	printf("method %d url %s\n", parser.Method(), (char*)parser.Url());
	str = "GET /favicon.ico HTTP/1.1\r\n"
		"Host: 0.0.0.0=5000\r\n"
		"User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) Gecko/2008061015 Firefox/3.0\r\n"
		"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
	size = parser.Parser(str);
	printf("errno %d size %lld\n", parser.Errno(), size);
	if (parser.Errno() != 0x7F) {
		return -3;
	}
	if (size != 0) {
		return -4;
	}
	UrlParser url1("https://www.baidu.com/s?ie=utf8&oe=utf8&wd=httplib&tn=98010089_dg&ch=3");

	int ret = url1.Parser();
	if (ret != 0) {
		printf("urlparser1 failed:%d\n", ret);
		return -5;
	}
	printf("ie = %s except:utf8\n", (char*)url1["ie"]);
	printf("oe = %s except:utf8\n", (char*)url1["oe"]);
	printf("wd = %s except:httplib\n", (char*)url1["wd"]);
	printf("tn = %s except:98010089_dg\n", (char*)url1["tn"]);
	printf("ch = %s except:3\n", (char*)url1["ch"]);
	UrlParser url2("http://127.0.0.1:19811/?time=144000&salt=9527&user=test&sign=1234567890abcdef");
	ret = url2.Parser();
	if (ret != 0) {
		printf("urlparser2 failed:%d\n", ret);
		return -6;
	}
	printf("time = %s except:144000\n", (char*)url2["time"]);
	printf("salt = %s except:9527\n", (char*)url2["salt"]);
	printf("user = %s except:test\n", (char*)url2["user"]);
	printf("sign = %s except:1234567890abcdef\n", (char*)url2["sign"]);
	printf("host:%s port:%d\n", (char*)url2.Host(), url2.Port());
	return 0;

}

//#include "Sqlite3Client.h"
//DECLARE_TABLE_CLASS(user_test, _sqlite3_table_)
//DECLARE_FIELD(TYPE_INT, user_id, NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INTEGER", "", "", "")
//DECLARE_FIELD(TYPE_VARCHAR, user_qq, NOT_NULL, "VARCHAR", "(15)", "", "")
//DECLARE_FIELD(TYPE_VARCHAR, user_phone, NOT_NULL | DEFAULT, "VARCHAR", "(12)", "18888888888", "")
//DECLARE_FIELD(TYPE_TEXT, user_name, 0, "TEXT", "", "", "")
//DECLARE_TABLE_CLASS_EDN()

/*class user_test :public _sqlite3_table_
{
public:
	virtual PTable Copy() const {
		return PTable(new user_test(*this));
	}
	user_test() :_sqlite3_table_() {
		Name = "user_test";
		{
			PField field(new _sqlite3_field_(TYPE_INT, "user_id", NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INT", "", "", ""));
			FieldDefine.push_back(field);
			Fields["user_id"] = field;
		}
		{
			PField field(new _sqlite3_field_(TYPE_VARCHAR, "user_qq", NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "VARCHAR", "(15)", "", ""));
			FieldDefine.push_back(field);
			Fields["user_qq"] = field;
		}
	}
};*/


//int sql_test()
//{
//	user_test test, value;
//	printf("create:%s\n", (char*)test.Create());
//	printf("Delete:%s\n", (char*)test.Delete(test));
//	value.Fields["user_qq"]->LoadFromStr("1817619619");
//	value.Fields["user_qq"]->Condition = SQL_INSERT;
//	printf("Insert:%s\n", (char*)test.Insert(value));
//	value.Fields["user_qq"]->LoadFromStr("123456789");
//	value.Fields["user_qq"]->Condition = SQL_MODIFY;
//	printf("Modify:%s\n", (char*)test.Modify(value));
//	printf("Query:%s\n", (char*)test.Query());
//	printf("Drop:%s\n", (char*)test.Drop());
//	getchar();
//	int ret = 0;
//	CDatabaseClient* pClient = new CSqlite3Client();
//	KeyValue args;
//	args["host"] = "test.db";
//	ret = pClient->Connect(args);
//	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
//	ret = pClient->Exec(test.Create());
//	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
//	ret = pClient->Exec(test.Delete(value));
//	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
//	value.Fields["user_qq"]->LoadFromStr("1817619619");
//	value.Fields["user_qq"]->Condition = SQL_INSERT;
//	ret = pClient->Exec(test.Insert(value));
//	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
//	value.Fields["user_qq"]->LoadFromStr("123456789");
//	value.Fields["user_qq"]->Condition = SQL_MODIFY;
//	ret = pClient->Exec(test.Modify(value));
//	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
//	Result result;
//	ret = pClient->Exec(test.Query(), result, test);
//	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
//	ret = pClient->Exec(test.Drop());
//	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
//	ret = pClient->Close();
//	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
//	//getchar();
//	return 0;
//}


#include "MysqlClient.h"
DECLARE_TABLE_CLASS(user_test_mysql, _mysql_table_)
DECLARE_MYSQL_FIELD(TYPE_INT, user_id, NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INTEGER", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_qq, NOT_NULL, "VARCHAR", "(15)", "", "")
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_phone, NOT_NULL | DEFAULT, "VARCHAR", "(12)", "18888888888", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_name, 0, "TEXT", "", "", "")
DECLARE_TABLE_CLASS_EDN()

int mysql_test()
{
	user_test_mysql test, value;
	printf("create:%s\n", (char*)test.Create());
	printf("Delete:%s\n", (char*)test.Delete(test));
	value.Fields["user_qq"]->LoadFromStr("1817619619");
	value.Fields["user_qq"]->Condition = SQL_INSERT;
	printf("Insert:%s\n", (char*)test.Insert(value));
	value.Fields["user_qq"]->LoadFromStr("123456789");
	value.Fields["user_qq"]->Condition = SQL_MODIFY;
	printf("Modify:%s\n", (char*)test.Modify(value));
	printf("Query:%s\n", (char*)test.Query());
	printf("Drop:%s\n", (char*)test.Drop());
	getchar();
	int ret = 0;
	CDatabaseClient* pClient = new CMysqlClient();
	KeyValue args;
	args["host"] = "192.168.1.103";
	args["user"] = "root";
	args["password"] = "";
	args["port"] = 3306;
	args["db"] = "YiPlayer";
	ret = pClient->Connect(args);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	ret = pClient->Exec(test.Create());
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	ret = pClient->Exec(test.Delete(value));
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	value.Fields["user_qq"]->LoadFromStr("1817619619");
	value.Fields["user_qq"]->Condition = SQL_INSERT;
	ret = pClient->Exec(test.Insert(value));
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	value.Fields["user_qq"]->LoadFromStr("123456789");
	value.Fields["user_qq"]->Condition = SQL_MODIFY;
	ret = pClient->Exec(test.Modify(value));
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	Result result;
	ret = pClient->Exec(test.Query(), result, test);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	ret = pClient->Exec(test.Drop());
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	ret = pClient->Close();
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	//getchar();
	return 0;
}

#include "Crypto.h"
int crypto_test()
{
	Buffer data = "abcdef";
	data = Crypto::MD5(data);
	printf("except E80B5017098950FC58AAD83C8C14978E %s\n", (char*)data);
	return 0;
}

int main()
{
	int ret = serverTest();
	/*int ret = threadTest();
	int ret = crypto_test();
	int ret = mysql_test();
	int ret = sql_test();*/

	printf("%s(%d): <%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);

}




