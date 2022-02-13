/*************************************************************
mysql½Ó¿Ú
author:king
date:2005-1-21
**************************************************************/
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "kmysql.h"
#include "kmalloc.h"
#ifdef ENABLE_MYSQL
KMysql::KMysql()
{
	mysql_init(&m_cn);

}
KMysql::~KMysql()
{
	DisConnect();
}
int KMysql::ping()
{
	int ret;
	m_lock.Lock();
	 ret=mysql_ping(&m_cn);
	 m_lock.Unlock();
	last_active_time=time(NULL);
	 return ret;
}
int KMysql::Connect(const char *host,int port,const char *user,const char *passwd,const char *db)
{
//	last_active_time=time(NULL);
	if(mysql_real_connect(&m_cn,host,user,passwd,db,port,NULL,0)==NULL)
		return 0;
	if(mysql_select_db(&m_cn, db)==0)
		return 1;
	return 0;
}
/*
 bool KMysql::SetCharset(const char *charset)
 {
 	if (!mysql_set_character_set(&m_cn, charset)) 
		return true;
	return false;
 }
 */
int KMysql::Connect(const char *host,int port,const char *user,const char *passwd)
{
//	last_active_time=time(NULL);
	 if(mysql_real_connect(&m_cn,host,user,passwd,NULL,port,NULL,0)==NULL)
           return 0;
	 return 1;
}
int KMysql::SelectDb(const char *db)
{
//	last_active_time=time(NULL);
	if(mysql_select_db(&m_cn,db)==0)
		return 1;
	return 0;
}
MYSQL_RES * KMysql::Query(const char *sql)
{
	if(mysql_query(&m_cn,sql)!=0){
		return NULL;
	}
	return mysql_store_result(&m_cn);
}
int KMysql::Query(const char *sql,KResultSet &pRet)
{

//	last_active_time=time(NULL);
	MYSQL_RES * result=NULL;
	m_lock.Lock();
	if(mysql_query(&m_cn,sql)!=0){
		goto err;
	}
	result=mysql_store_result(&m_cn);
	if(result){
		unsigned int col = mysql_field_count(&m_cn);
		unsigned int row = mysql_num_rows(result);
		if(col<=0){
			mysql_free_result(result);
			goto err;
		}
		pRet.AddCol(row);
		for(unsigned int i=0;i<row;i++){
			char **pp = mysql_fetch_row(result);
			if(pp){
				pRet.recorder[i] = new CSqlResult(col,pp);
			}
		}
		mysql_free_result(result);
	}else{
		goto err;
	}
	m_lock.Unlock();
//	printf("%s use %d usec\n",sql,get_usec()-begin_tm);
	return 1;
err:
//	pRet.FreeData();
	m_lock.Unlock();
	return 0;
}
int KMysql::Execute(const char *sql,int len)
{
	//last_active_time=time(NULL);
	m_lock.Lock();
	if(mysql_query(&m_cn,sql)==0){
		int ret=mysql_affected_rows(&m_cn);
		m_lock.Unlock();
	//	printf("%s use %d usec\n",sql,get_usec()-begin_tm);
		return ret;
	}
	m_lock.Unlock();
//	printf("%s use %d usec\n",sql,get_usec()-begin_tm);
	return 0;
}
void KMysql::DisConnect()
{
//	last_active_time=time(NULL);
	mysql_close(&m_cn);
}
CSqlResult::CSqlResult(unsigned int _col,char **pp)
{
	col = _col;
	result = (char **)malloc(_col*sizeof(char *));
	for(unsigned int i=0;i<col;i++)
	{
		if(pp[i])
		{
			int len = strlen(pp[i]);
			result[i] = new char[len+1];
			strcpy(result[i],pp[i]);
		}
		else
		{
			result[i] = new char[1];
			result[i][0] = '\0';
		}
	}
}

CSqlResult::~CSqlResult()
{
	for(unsigned int i=0;i<col;i++) delete [] result[i];
	free(result); 
};
KResultSet::KResultSet()
{
	count=0;
	point=0;
	recorder=NULL;
}
void KResultSet::AddCol(unsigned int col)
{
	close();
	count = col;
	recorder = (CSqlResult **)malloc(col*sizeof(CSqlResult*));
}
KResultSet::~KResultSet()
{
	close();
}
void KResultSet::close()
{
	for(unsigned int i=0;i<count;i++) delete recorder[i];
	if(recorder){
		free(recorder);
		recorder=NULL;
	}
	count=0;
	point=0;
}
char ** KResultSet::getData(unsigned &col)
{
     if(point>=count){
                return NULL;
	}
	CSqlResult * data = recorder[point++];
	col = data->col;
        return data->result;
}
char ** KResultSet::getData()
{
	if(point>=count)
		return NULL;
	return recorder[point++]->result;
}
#endif
