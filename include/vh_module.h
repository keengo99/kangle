#ifndef VH_MODULE_H
#define VH_MODULE_H
#ifdef __cplusplus
extern "C" {
#endif
	typedef void * kgl_vh_connection;
	typedef void * kgl_vh_stmt;
	struct vh_data
	{
		void *ctx;
		void (* set)(void *ctx,const char *name,const char *value);
		/*
		该函数不会返回NULL
		*/
		const char * (* get)(void *ctx,const char *name);
		/*
		该函数会返回NULL
		*/
		const char * (* getx)(void *ctx,const char *name);
	};
	struct vh_module
	{
		int cbsize;
		void *ctx;
		int vhi_version;
		const char *(* getConfigValue)(void *ctx,const char *name);
		//以下是返回
		//解析配置文件
		int (* parseConfig)(vh_data *data);
		//建立连接
		kgl_vh_connection (* createConnection)();
		//查询操作,data作为输出参数
		int (*query)(kgl_vh_stmt stmt,vh_data *data);
		void (*freeStmt)(kgl_vh_stmt stmt);
		void (*freeConnection)(kgl_vh_connection cn);
		//读取操作，返回stmt
		kgl_vh_stmt (* loadVirtualHost)(kgl_vh_connection cn);
		kgl_vh_stmt (* flushVirtualHost)(kgl_vh_connection cn,const char *name);
		kgl_vh_stmt (* loadInfo)(kgl_vh_connection cn,const char *name);
		//更新操作，成功返回1,错误返回0
		int (* addVirtualHost)(kgl_vh_connection cn,vh_data *data);
		int (* updateVirtualHost)(kgl_vh_connection cn,vh_data *data);
		int (* delVirtualHost)(kgl_vh_connection cn,vh_data *data);
		int (* delInfo)(kgl_vh_connection cn,vh_data *data);
		int (* addInfo)(kgl_vh_connection cn,vh_data *data);
		int (* delAllInfo)(kgl_vh_connection cn,vh_data *data);
		//流量操作
		void *(*getFlow)(kgl_vh_connection cn,const char *name);
		int (* setFlow)(kgl_vh_connection cn,vh_data *data);
		//黑白名单
		kgl_vh_stmt(*loadBlackList)(kgl_vh_connection cn);
	};
	int initVirtualHostModule(vh_module *ctx);
	typedef int (*initVirtualHostModulef)(vh_module *ctx);
#ifdef __cplusplus
}
#endif
#endif
