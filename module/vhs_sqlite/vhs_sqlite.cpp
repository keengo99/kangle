// vhs_sqlite.cpp : 定义 DLL 应用程序的导出函数。
//
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sstream>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "kforwin32.h"
#include "global.h"
#include "vhs_sqlite.h"
using namespace std;
std::string path;
std::string dbname;
#pragma comment(lib,"sqlite3.lib")
static const char *load_sql = "SELECT uid AS user,gid AS `group`,* FROM vhost";
static const char *load_black_list_sql = "SELECT ip,flags FROM black_list";
static std::string flush_sql;
static const char *load_info_sql2 = "SELECT * FROM vhost_info WHERE vhost='%s' AND type<1000 order by id";
static const char *load_info_sql = "SELECT * FROM vhost_info WHERE vhost='%s' AND type<1000";
static int sqliteBusyHandle(void *db,int count)
{
	if (count>5) {
		return 0;
	}
	sleep(1);
	return 1;
}
static void * createConnection()
{
	KVirtualHostSqliteConnection *cn = new KVirtualHostSqliteConnection;
	int ret = sqlite3_open_v2(dbname.c_str(), &cn->db, SQLITE_OPEN_READONLY, NULL);
	if(ret != SQLITE_OK){
		fprintf(stderr,"open db [%s] failed %d\n",dbname.c_str(),ret);
		delete cn;
		return NULL;
	}
	printf("open db [%s] success\n",dbname.c_str());
	sqlite3_busy_handler(cn->db,sqliteBusyHandle,NULL);
	return cn;
}
//查询操作,data作为输出参数
static int query(void *stmt,vh_data *data)
{
	KVirtualHostData *rs_data = (KVirtualHostData *)stmt;
	KVirtualHostDataSqliteResult *rs = static_cast<KVirtualHostDataSqliteResult *>(rs_data);
	if(!rs->next()){
		return 0;
	}
	for(int i=0;i<rs->getColumnCount();i++){
		const char *name = rs->getColumnName(i);
		const char *value = rs->getString(i);
		if(name && value){
			data->set(data->ctx,name,value);
		}
	}
	return 1;
}
static void freeStmt(void *stmt)
{
	KVirtualHostDataSqliteResult *rs = (KVirtualHostDataSqliteResult *)stmt;
	delete rs;
}
static void freeConnection(void *param)
{
	KVirtualHostSqliteConnection *cn = (KVirtualHostSqliteConnection *)param;
	delete cn;
}
//读取操作，返回stmt
static void * loadVirtualHost(void *cn)
{
	return ((KVirtualHostSqliteConnection *)cn)->loadVirtualHost();
}
static void * flushVirtualHost(void *cn,const char *name)
{
	return ((KVirtualHostSqliteConnection *)cn)->flushVirtualHost(name);
}
static void * loadInfo(void *cn,const char *name)
{
	return ((KVirtualHostSqliteConnection *)cn)->loadInfo(name);
}

//更新操作，成功返回1,错误返回0
static int  addVirtualHost(void *cn,vh_data *data)
{
	KVirtualHostStmt *st = ((KVirtualHostSqliteConnection *)cn)->addVirtualHost();
	if (st == NULL) {
		return 0;
	}
	st->bindString(0,data->get(data->ctx,"name"));
	st->bindString(1,data->get(data->ctx,"passwd"));
	st->bindString(2,data->get(data->ctx,"doc_root"));
	st->bindString(3,data->get(data->ctx,"user"));
	st->bindString(4,data->get(data->ctx,"group"));
	st->bindString(5,data->get(data->ctx,"templete"));
	st->bindString(6,data->get(data->ctx,"subtemplete"));
	st->bindInt(7,atoi(data->get(data->ctx,"status")));
	st->bindString(8,data->get(data->ctx,"htaccess"));
	st->bindString(9,data->get(data->ctx,"access"));
	st->bindInt(10,atoi(data->get(data->ctx,"max_connect")));
	st->bindInt(11,atoi(data->get(data->ctx,"max_worker")));
	st->bindInt(12,atoi(data->get(data->ctx,"max_queue")));
	st->bindString(13,data->get(data->ctx,"log_file"));
	st->bindString(14,data->get(data->ctx,"speed_limit"));
	int ret = st->execute();
	delete st;
	return ret;
}
static int  updateVirtualHost(void *cn,vh_data *data)
{
	const char *name = data->getx(data->ctx,"name");
	if (name==NULL) {
		return 0;
	}
	KDyamicSqliteStmt *st = ((KVirtualHostSqliteConnection *)cn)->updateVirtualHost(name);
	if (st == NULL) {
		return 0;
	}
	st->bindString("subtemplete",data);
	st->bindInt("status",data);
	st->bindString("passwd",data);
	st->bindInt("ftp",data);
	st->bindString("doc_root",data);
	st->bindInt("max_worker",data);
	st->bindInt("max_connect",data);
	st->bindInt("speed_limit",data);
	bool ret = st->execute();
	delete st;
	return ret?1:0;
}
static int delVirtualHost(void *cn,vh_data *data)
{
	KVirtualHostStmt *st = ((KVirtualHostSqliteConnection *)cn)->delAllInfo();
	if (st) {
		st->bindString(0,data->get(data->ctx,"name"));
		int ret = st->execute();
		delete st;
	}	
	st = ((KVirtualHostSqliteConnection *)cn)->delVirtualHost();
	if (st == NULL) {
		return 0;
	}
	st->bindString(0,data->get(data->ctx,"name"));
	int ret = st->execute();
	delete st;
	return ret;
}
static int  delInfo(void *cn,vh_data *data)
{
	const char *value = data->getx(data->ctx,"value");
	KVirtualHostStmt *st = ((KVirtualHostSqliteConnection *)cn)->delInfo(value!=NULL);
	if (st == NULL) {
		return 0;
	}
	st->bindString(0,data->get(data->ctx,"vhost"));
	st->bindString(1,data->get(data->ctx,"name"));
	st->bindInt(2,atoi(data->get(data->ctx,"type")));
	if(value){
		st->bindString(3,value);
	}
	int ret = st->execute();
	delete st;
	return ret;
}
static int addInfo(void *cn,vh_data *data)
{
	KVirtualHostStmt *st;
	const char *multi = data->getx(data->ctx,"multi");
	if (multi==NULL || strcmp(multi,"0")==0 || *multi=='\0') {
		//del the info
		st = ((KVirtualHostSqliteConnection *)cn)->delInfo(false);
		if (st == NULL) {
			return 0;
		}
		st->bindString(0,data->get(data->ctx,"vhost"));
		st->bindString(1,data->get(data->ctx,"name"));
		st->bindInt(2,atoi(data->get(data->ctx,"type")));
		st->execute();
		delete st;
	}
	st = ((KVirtualHostSqliteConnection *)cn)->addInfo();
	if (st == NULL) {
		return 0;
	}
	st->bindString(0,data->get(data->ctx,"value"));
	st->bindString(1,data->get(data->ctx,"vhost"));
	st->bindString(2,data->get(data->ctx,"name"));
	st->bindInt(3,atoi(data->get(data->ctx,"type")));
	int ret = st->execute();
	delete st;
	return ret;
}
static int  delAllInfo(void *cn,vh_data *data)
{
	KVirtualHostStmt *st = ((KVirtualHostSqliteConnection *)cn)->delAllInfo();
	if (st == NULL) {
		return 0;
	}
	st->bindString(0,data->get(data->ctx,"vhost"));
	int ret = st->execute();
	delete st;
	return ret;
}
static kgl_vh_stmt loadBlackList(kgl_vh_connection cn)
{
	return ((KVirtualHostSqliteConnection *)cn)->loadBlackList();
}
static int setFlow(void *cn,vh_data *data)
{
	const char *flow_model = data->getx(data->ctx,"flow_model");
	KVirtualHostSqliteStmt *st;
	if (flow_model && strcmp(flow_model,"set")==0) {
		st = ((KVirtualHostSqliteConnection *)cn)->setFlow();
	} else {
		st = ((KVirtualHostSqliteConnection *)cn)->addFlow();
	}
	if (st==NULL) {
		return 0;
	}
	st->bindInt64(0,string2int(data->get(data->ctx,"flow")));
	st->bindInt64(1,string2int(data->get(data->ctx,"hcount")));
	st->bindString(2,data->get(data->ctx,"name"));
	int ret = st->execute();
	delete st;
	return ret;
}
static void *getFlow(void *cn,const char *name)
{
	return ((KVirtualHostSqliteConnection *)cn)->getFlow(name);
}
static int parseConfig(vh_data *data)
{
	const char *value = data->getx(data->ctx,"dbname");
	if (value == NULL || *value == '\0') {
		return 0;
	}
	if (value[0] == '/' 
#ifdef _WIN32
		|| value[1] == ':'
#endif
		) {
		dbname = value;
	} else {
		dbname = path;
		dbname += value;
	}
	return 1;
}

int initVirtualHostModule(vh_module *ctx)
{
	flush_sql = load_sql;
	flush_sql += " WHERE name='%s'";
	const char *value = ctx->getConfigValue(ctx->ctx,"kangle_home");
	if(value==NULL){
		return 0;
	}
	path = value;
	ctx->parseConfig = parseConfig;
	ctx->createConnection = createConnection;
	ctx->query = query;
	ctx->freeStmt = freeStmt;
	ctx->freeConnection = freeConnection;
	ctx->loadVirtualHost = loadVirtualHost;
	ctx->flushVirtualHost = flushVirtualHost;
	ctx->loadInfo = loadInfo;
	ctx->addVirtualHost = addVirtualHost;
	ctx->updateVirtualHost = updateVirtualHost;
	ctx->delVirtualHost = delVirtualHost;
	ctx->delInfo = delInfo;
	ctx->addInfo = addInfo;
	ctx->delAllInfo = delAllInfo;
	if (ctx->vhi_version>=1) {
		ctx->setFlow = setFlow;
		ctx->getFlow = getFlow;
	}
	if (ctx->vhi_version >= 2) {
		ctx->loadBlackList = loadBlackList;
	}
	return 1;
}
KVirtualHostSqliteStmt::KVirtualHostSqliteStmt()
{
	stmt = NULL;
}
KVirtualHostSqliteStmt::~KVirtualHostSqliteStmt()
{
	if(stmt){
		sqlite3_finalize(stmt);
	}
}

const char *KVirtualHostDataSqliteResult::getColumnName(int columnIndex)
{
	return sqlite3_column_name(stmt,columnIndex);
}
bool KVirtualHostSqliteStmt::bindInt64(unsigned columnIndex,sqlite_int64 value)
{
	int ret = sqlite3_bind_int64(stmt,columnIndex+1,value);
	return ret == SQLITE_OK;
}
bool KVirtualHostSqliteStmt::bindInt(unsigned columnIndex,int value)
{
	int ret = sqlite3_bind_int(stmt,columnIndex+1,value);
	return ret == SQLITE_OK;
}
bool KVirtualHostSqliteStmt::bindString(unsigned columnIndex,const char *value)
{
	int ret = sqlite3_bind_text(stmt,columnIndex+1,value,-1,NULL);
	return ret == SQLITE_OK;
}
bool KVirtualHostSqliteStmt::execute()
{
	int ret = sqlite3_step(stmt);
	return SQLITE_DONE == ret;
}
KVirtualHostDataSqliteResult::KVirtualHostDataSqliteResult()
{
	stmt = NULL;
}
KVirtualHostDataSqliteResult::~KVirtualHostDataSqliteResult()
{
	if(stmt){
		sqlite3_finalize(stmt);
	}
}
int KVirtualHostDataSqliteResult::getInt(unsigned columnIndex)
{
	return sqlite3_column_int(stmt,columnIndex);
}
int KVirtualHostDataSqliteResult::getColumnCount()
{
	return sqlite3_column_count(stmt);
}
const char *KVirtualHostDataSqliteResult::getString(unsigned columnIndex)
{
	return (const char *)sqlite3_column_text(stmt,columnIndex);
}
bool KVirtualHostDataSqliteResult::next()
{
	if(SQLITE_ROW == sqlite3_step(stmt)){
		return true;
	}
	return false;
}
KVirtualHostSqliteConnection::KVirtualHostSqliteConnection()
{
		db = NULL;
}
KVirtualHostSqliteConnection::~KVirtualHostSqliteConnection()
{
	if(db){
		sqlite3_close(db);
	}
}
void KVirtualHostSqliteConnection::executeSqls(const char *sql[])
{
	for(int i=0;;i++){
		if(sql[i]==NULL){
			break;
		}
		KVirtualHostStmt *st = createStmt(sql[i]);
		if(st){
			st->execute();
		}
		delete st;
	}
}
KVirtualHostData *KVirtualHostSqliteConnection::loadVirtualHost()
{
	return querySql(load_sql);
}
KVirtualHostData *KVirtualHostSqliteConnection::loadBlackList()
{
	return querySql(load_black_list_sql);
}
KVirtualHostData *KVirtualHostSqliteConnection::flushVirtualHost(const char *name)
{
	char sql[512];
	snprintf(sql,sizeof(sql)-1,flush_sql.c_str(),name);
	return querySql(sql);
}
KVirtualHostData *KVirtualHostSqliteConnection::loadInfo(const char *name)
{
	char sql[512];
	snprintf(sql,sizeof(sql)-1,load_info_sql2,name);
	KVirtualHostData *vd = querySql(sql);
	if (vd) {
		return vd;
	}
	snprintf(sql,sizeof(sql)-1,load_info_sql,name);
	return querySql(sql);;
}
KVirtualHostData *KVirtualHostSqliteConnection::querySql(const char *sql)
{
	KVirtualHostDataSqliteResult *rs = new KVirtualHostDataSqliteResult;
	if(SQLITE_OK == sqlite3_prepare(db,sql,-1,&rs->stmt,NULL)){
		return rs;
	}
	delete rs;
	return NULL;
}
KVirtualHostSqliteStmt *KVirtualHostSqliteConnection::createStmt(const char *sql)
{
	KVirtualHostSqliteStmt *st = new KVirtualHostSqliteStmt;
	int ret = sqlite3_prepare(db,sql,-1,&st->stmt,NULL);
	if(SQLITE_OK == ret) {
		return st;
	}
	//printf("prepare sql [%s],failed %d\n",sql,ret);
	delete st;
	return NULL;
}
