#ifndef VHSSQLITE_H
#define VHSSQLITE_H
#include "KVirtualHostDataInterface.h"
#include "sqlite3.h"
#include <sstream>
#include <list>
inline INT64 string2int(const char *buf) {
#ifdef _WIN32
	return _atoi64(buf);
#else
	return atoll(buf);
#endif
}
class KVirtualHostSqliteStmt : public KVirtualHostStmt
{
public:	
	KVirtualHostSqliteStmt();
	~KVirtualHostSqliteStmt();
	bool bindInt(unsigned columnIndex,int value);
	bool bindInt64(unsigned columnIndex,sqlite_int64 value);
	bool bindString(unsigned columnIndex,const char *value);
	bool execute();
	friend class KVirtualHostSqliteConnection;
private:
	sqlite3_stmt *stmt;
};
class KDyamicItem
{
public:
	KDyamicItem(const char *name,int value)
	{
		this->name = strdup(name);
		nValue = value;
		sValue = NULL;
	}
	KDyamicItem(const char *name,const char *value)
	{
		this->name = strdup(name);
		sValue = strdup(value);
	}
	~KDyamicItem()
	{
		free(name);
		if (sValue) {
			free(sValue);
		}
	}
	char *name;
	int nValue;
	char *sValue;
};
class KDyamicSqliteStmt
{
public:
	KDyamicSqliteStmt(sqlite3 *db,const char *sql,bool update)
	{
		this->sql = strdup(sql);
		this->update = update;
		this->db = db;
	}
	~KDyamicSqliteStmt()
	{
		std::list<KDyamicItem *>::iterator it;
		for (it=items.begin();it!=items.end();it++) {
			delete (*it);
		}
		free(sql);
	}
	void bind(const char *name,int value)
	{
		items.push_back(new KDyamicItem(name,value));
	}
	void bind(const char *name,const char *value)
	{
		items.push_back(new KDyamicItem(name,value));
	}
	void bindInt(const char *name,vh_data *data,const char *dataName=NULL)
	{
		const char *v = data->getx(data->ctx,dataName?dataName:name);
		if (v) {
			bind(name,atoi(v));
		}
	}
	void bindString(const char *name,vh_data *data,const char *dataName=NULL)
	{
		const char *v = data->getx(data->ctx,dataName?dataName:name);
		if (v) {
			bind(name,v);
		}
	}
	bool execute()
	{
		char s[2048];
		std::stringstream msql;
		buildSql(msql);
		memset(s,0,sizeof(s));
		snprintf(s,sizeof(s)-1,sql,msql.str().c_str());
		sqlite3_stmt *stmt = NULL;
		int ret = sqlite3_prepare(db,s,-1,&stmt,NULL);
		if (SQLITE_OK != ret) {
			return false;
		}
		std::list<KDyamicItem *>::iterator it;
		int columnIndex=1;
		for (it=items.begin();it!=items.end();it++) {
			if ((*it)->sValue) {
				ret = sqlite3_bind_text(stmt,columnIndex++,(*it)->sValue,-1,NULL);
			} else {
				ret = sqlite3_bind_int(stmt,columnIndex++,(*it)->nValue);
			}
		}
		ret = sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		return ret==SQLITE_DONE;
	}
private:
	void buildSql(std::stringstream &s)
	{
		std::list<KDyamicItem *>::iterator it;
		if (update) {
			for (it=items.begin();it!=items.end();it++) {
				if (s.str().size()>0) {
					s << ",";
				}
				s << (*it)->name << "=?";
			}
		} else {
			std::stringstream v;
			s << "(";
			for (it=items.begin();it!=items.end();it++) {
				if (v.str().size()>0) {
					s << ",";
					v << ",";
				}
				s << (*it)->name;
				v << "?";
			}
			s << ") VALUES (" << v.str() << ")";
		}
	}
	std::list<KDyamicItem *> items;
	char *sql;
	bool update;
	sqlite3 *db;
};
class KVirtualHostDataSqliteResult : public KVirtualHostData
{
public:
	KVirtualHostDataSqliteResult();
	~KVirtualHostDataSqliteResult();
	const char *getString(unsigned columnIndex);
	const char *getColumnName(int columnIndex);
	int getInt(unsigned columnIndex);
	int getColumnCount();
	bool next();
	friend class KVirtualHostSqliteConnection;	
	sqlite3_stmt *stmt;
private:

};
class KVirtualHostSqliteConnection : public KVirtualHostConnection
{
public:
	KVirtualHostSqliteConnection();
	~KVirtualHostSqliteConnection();
	KVirtualHostData *loadBlackList();
	KVirtualHostData *loadVirtualHost();
	KVirtualHostData *flushVirtualHost(const char *name);
	KVirtualHostData *loadInfo(const char *name);
	KVirtualHostStmt *addVirtualHost(){
		return createStmt("REPLACE INTO vhost (name,passwd,doc_root,uid,gid,templete,subtemplete,status,htaccess,access,max_connect,max_worker,max_queue,log_file,speed_limit) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
	}
	KDyamicSqliteStmt *updateVirtualHost(const char *name)
	{
		std::stringstream s;
		s << "UPDATE vhost SET %s WHERE name='" << name << "'";
		return createDyamicStmt(s.str().c_str(),true);
	}
	KVirtualHostStmt *delVirtualHost()
	{
		return createStmt("DELETE FROM vhost WHERE name=?");
	}
	KVirtualHostStmt *addInfo()
	{
		return createStmt("INSERT INTO vhost_info (value,vhost,name,type) VALUES (?,?,?,?)");
	}
	KVirtualHostStmt *editInfo()
	{
		return createStmt("UPDATE vhost_info SET value=? WHERE vhost=? AND name=? AND type=?");
	}
	KVirtualHostData *getFlow(const char *name)
	{
		std::stringstream s;
		s << "SELECT flow,hcount FROM vhost WHERE name='" << name << "'";
		return querySql(s.str().c_str());
	}
	KVirtualHostSqliteStmt *setFlow()
	{
		return createStmt("UPDATE vhost SET flow=?,hcount=? WHERE name=?");
	}
	KVirtualHostSqliteStmt *addFlow()
	{
		return createStmt("UPDATE vhost SET flow=flow+?,hcount=hcount+? WHERE name=?");
	}
	KVirtualHostStmt *delAllInfo()
	{
		return createStmt("DELETE FROM vhost_info WHERE vhost=?");
	}
	KVirtualHostStmt *delInfo(bool bindValue)
	{
		if(bindValue){
			return createStmt("DELETE FROM vhost_info WHERE vhost=? and name=? and type=? and value=?");
		}
		return createStmt("DELETE FROM vhost_info WHERE vhost=? and name=? and type=?");
	}
	KDyamicSqliteStmt *createDyamicStmt(const char *sql,bool update)
	{
		return new KDyamicSqliteStmt(db,sql,update);
	}
	int getVersion();
	friend class KVirtualHostDataSqlite;
	sqlite3 *db;
	void executeSqls(const char *sql[]);
	KVirtualHostData *querySql(const char *sql);
	KVirtualHostSqliteStmt *createStmt(const char *sql);
private:
};
#endif
