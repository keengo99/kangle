#ifndef sql_h_sldkfjlsdjfsdklfjlskdjfsdf9s7df7sdfs8df8
#define sql_h_sldkfjlsdjfsdklfjlskdjfsdf9s7df7sdfs8df8
#include <string>
#include "kforwin32.h"
#include "global.h"
#ifdef ENABLE_MYSQL
#include <mysql.h>
#include "KMutex.h"
#include "global.h"
class CSqlResult
{	
public:
	unsigned int col;	//ÁÐÊý
	char *Index(unsigned int index){ return result[index]; }
	CSqlResult(unsigned int _col,char **pp);
	~CSqlResult();
	char **result;
};
class KResultSet
{
public:
	KResultSet();
	void AddCol(unsigned int col);
	~KResultSet();
	char **getData();
	char **getData(unsigned &col);
	CSqlResult **recorder;
	void close();
private:
	unsigned int count;
	unsigned int point;
};
class KMysql
{
public:
	KMysql();
	~KMysql();
	int Query(const char *sql,KResultSet &pRet);
	MYSQL_RES *Query(const char *sql);
	int Execute(const char *sql,int len=0);
	int SelectDb(const char *db);
	int Connect(const char *host,int port,const char *user,const char *passwd);	
	int Connect(const char *host,int port,const char *user,const char *passwd,const char *db);
	//bool SetCharset(const char *charset);
	int ping();
	void DisConnect();
	int last_active_time;
//	void close();
private:
	MYSQL m_cn;
	KMutex m_lock;
};
#endif
#endif
