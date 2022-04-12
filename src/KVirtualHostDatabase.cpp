#include "KVirtualHostDatabase.h"
#include "KVirtualHostManage.h"
#include "KHttpServerParser.h"
#include "KTempleteVirtualHost.h"
#include "log.h"
#include "utils.h"
#include "extern.h"
#include "KAcserverManager.h"
#include "kmalloc.h"
struct vhd_load_base_param {
	vh_module* vhm;
	kgl_vh_connection cn;
	kgl_vh_stmt* st;
};
struct vhd_load_vh_param : vhd_load_base_param {
	const char* name;
};
static int thread_load_vh(void* arg, int argc)
{
	vhd_load_vh_param* param = (vhd_load_vh_param*)arg;
	if (param->name) {
		*(param->st) = param->vhm->flushVirtualHost(param->cn, param->name);
	} else {
		*(param->st) = param->vhm->loadVirtualHost(param->cn);
	}
	return 0;
}
struct vhd_load_info_param : vhd_load_base_param {

	const char* name;

};
static int thread_vhd_load_info(void* arg, int argc)
{
	vhd_load_info_param* param = (vhd_load_info_param*)arg;
	*(param->st) = param->vhm->loadInfo(param->cn, param->name);
	return 0;
}
struct vhd_load_black_list_param : vhd_load_base_param {

};
static int thread_vhd_load_black_list(void* arg, int argc)
{
	vhd_load_black_list_param* param = (vhd_load_black_list_param*)arg;
	*(param->st) = param->vhm->loadBlackList(param->cn);
	return 0;
}
struct vhd_query_param {
	vh_module *vhm;
	kgl_vh_stmt st;
	vh_data* data;
};
static int thread_vhd_query(void* arg, int argc)
{
	vhd_query_param* param = (vhd_query_param*)arg;
	return param->vhm->query(param->st, param->data);
}
struct vhd_connect_param {
	vh_module *vhm;
	kgl_vh_connection* cn;
};
static int thread_vhd_connect(void* arg, int argc)
{
	vhd_connect_param* param = (vhd_connect_param*)arg;
	*(param->cn) = param->vhm->createConnection();
	return 0;
}
KVirtualHostDatabase vhd;
static void set_vh_data(void *ctx,const char *name,const char *value)
{
	std::map<std::string,std::string> *attribute = (std::map<std::string,std::string> *)ctx;
	attribute->insert(std::pair<std::string,std::string>(name,value));
	return;
}
static const char *getx_vh_data(void *ctx,const char *name)
{
	std::map<std::string,std::string> *attribute = (std::map<std::string,std::string> *)ctx;
	std::map<std::string,std::string>::iterator it;
	it = attribute->find(name);
	if(it==attribute->end()){
		return NULL;
	}
	return (*it).second.c_str();
}
static const char *get_vh_data(void *ctx,const char *name)
{
	const char *value = getx_vh_data(ctx,name);
	if(value){
		return value;
	}
	return "";
}
void init_vh_data(vh_data *vd,std::map<std::string,std::string> *attribute)
{
	vd->set = set_vh_data;
	vd->get = get_vh_data;
	vd->getx = getx_vh_data;
	vd->ctx = (void *)attribute;
}
static const char *getSystemEnv(void *param,const char *name)
{
	static std::string value;
	const char *value2 = getSystemEnv(name);
	if(value2){
		return value2;
	}
	if(!conf.gvm->globalVh.getEnvValue(name,value)){
		return NULL;
	}
	return value.c_str();
}
KVirtualHostDatabase::KVirtualHostDatabase()
{
	lock = kfiber_mutex_init();
	ext = false;
	lastStatus = false;
	memset(&vhm,0,sizeof(vhm));
	vhm.vhi_version = 2;
	vhm.cbsize = sizeof(vhm);
	vhm.getConfigValue = getSystemEnv;
}
KVirtualHostDatabase::~KVirtualHostDatabase()
{
	kfiber_mutex_destroy(lock);
}
bool KVirtualHostDatabase::check()
{
	kfiber_mutex_lock(lock);
	kgl_vh_connection cn = createConnection();
	kfiber_mutex_unlock(lock);
	bool result = false;
	if (cn) {
		result = true;
		vhm.freeConnection(cn);
	}	
	return result;
}
bool KVirtualHostDatabase::flushVirtualHost(const char *vhName,bool initEvent,KVirtualHostEvent *ctx)
{
	kfiber_mutex_lock(lock);
	kgl_vh_connection cn = createConnection();
	if(cn==NULL){
		ctx->setStatus("cann't create connection");
		kfiber_mutex_unlock(lock);
		return false;
	}
	KVirtualHost *ov = conf.gvm->refsVirtualHostByName(vhName);
	KVirtualHost *vh = NULL;
	kgl_vh_stmt rs = NULL;
	vhd_load_vh_param param;
	param.vhm = &vhm;
	param.cn = cn;
	param.st = &rs;
	param.name = vhName;
	kfiber_thread_call(thread_load_vh, &param, 1, NULL);
	kfiber_mutex_unlock(lock);
	if(rs==NULL){
		ctx->setStatus("cann't load virtualHost");
		vhm.freeConnection(cn);
		//delete cn;
		if(ov){
			ov->destroy();
		}
		return false;
	}
	vh_data vd;
	std::map<std::string,std::string> attribute;
	init_vh_data(&vd,&attribute);
	vhd_query_param query_param;
	query_param.vhm = &vhm;
	query_param.st = rs;
	query_param.data = &vd;
	int result;
	if (kfiber_thread_call(thread_vhd_query, &query_param, 1, &result) == 0 && result) {
		vh = newVirtualHost(cn,attribute,conf.gvm,ov);
	} else if(ov) {
		conf.gvm->removeVirtualHost(ov);
	}
	vhm.freeStmt(rs);
	vhm.freeConnection(cn);
	if (vh && ctx) {
		//vh的引用由ctx处理了.
		ctx->buildVh(vh);
		KTempleteVirtualHost *tvh = vh->tvh;
		if (initEvent) {
			if (tvh) {
				tvh->initEvent(ctx);
			}
#ifndef HTTP_PROXY
			conf.gam->killAllProcess(vh);
#endif
		} else {
			if (tvh) {
				tvh->updateEvent(ctx);
			}
		}
	}
	if(ov){
		ov->destroy();
	}
	return true;
}
bool KVirtualHostDatabase::isLoad()
{
	return (vhm.createConnection != NULL);
}
bool KVirtualHostDatabase::loadVirtualHost(KVirtualHostManage *vm,std::string &errMsg)
{
	kassert(kfiber_self()!=NULL);
	kfiber_mutex_lock(lock);
	lastStatus = false;
	kgl_vh_connection cn = createConnection();
	if(cn==NULL){
		errMsg = "cann't connect to database";
		kfiber_mutex_unlock(lock);
		return false;
	}
	kgl_vh_stmt rs = NULL;
	lastStatus = true;
	vhd_load_vh_param param;
	param.vhm = &vhm;
	param.cn = cn;
	param.st = &rs;
	param.name = NULL;
	kfiber_thread_call(thread_load_vh, &param, 1, NULL);
	lastStatus = (rs!=NULL);
	kfiber_mutex_unlock(lock);
	if(rs==NULL){
		errMsg = "load virtual host error";
		vhm.freeConnection(cn);
		return true;
	}
	vh_data vd;
	std::map<std::string,std::string> attribute;
	init_vh_data(&vd,&attribute);
	vhd_query_param query_param;
	query_param.vhm = &vhm;
	query_param.st = rs;
	query_param.data = &vd;
	int result;
	for(;;) {
		if (kfiber_thread_call(thread_vhd_query, &query_param, 1, &result) != 0) {
			break;
		}
		if (!result) {
			break;
		}
	//while(vhm.query(rs,&vd)){
		//防止加载时间太长，而安全进程误认为挂掉。
		setActive();
		KVirtualHost *ov = conf.gvm->refsVirtualHostByName(attribute["name"]);
		KVirtualHost *vh = newVirtualHost(cn,attribute,vm,ov);
		if (vh) {
			vh->destroy();
		}
		if (ov) {
			ov->destroy();
		}
		attribute.clear();
	}
	vhm.freeStmt(rs);
#ifdef ENABLE_BLACK_LIST
	if (vhm.loadBlackList) {
		rs = NULL;
		vhd_load_black_list_param black_list_param;
		black_list_param.vhm = &vhm;
		black_list_param.cn = cn;
		black_list_param.st = &rs;
		kfiber_thread_call(thread_vhd_load_black_list, &black_list_param, 1, NULL);
		if (rs) {
			vhd_query_param query_param;
			query_param.vhm = &vhm;
			query_param.st = rs;
			query_param.data = &vd;
			int result;
			for (;;) {
				if (kfiber_thread_call(thread_vhd_query, &query_param, 1, &result) != 0) {
					break;
				}
				if (!result) {
					break;
				}
				const char *ip = attribute["ip"].c_str();
				if (*ip) {
					int flags = atoi(attribute["flags"].c_str());
					conf.gvm->globalVh.blackList->AddStatic(ip, KBIT_TEST(flags,1));
				}
			}
			vhm.freeStmt(rs);
		}
	}
#endif
	vhm.freeConnection(cn);
	return true;
}
bool KVirtualHostDatabase::parseAttribute(std::map<std::string,std::string> &attribute)
{
	bool result = false;
	kfiber_mutex_lock(lock);
	std::string driver = attribute["driver"];
	if (vhm.createConnection == NULL) {	
		if(!isAbsolutePath(driver.c_str())){
			driver = conf.path + driver;
		}
		if(!vhm_handle.isloaded()){
			if(!vhm_handle.load(driver.c_str())){
				kfiber_mutex_unlock(lock);
				klog(KLOG_ERR,"cann't load driver [%s]\n",driver.c_str());
				return false;
			}
		}
		initVirtualHostModulef m_init_vh_module = (initVirtualHostModulef)vhm_handle.findFunction("initVirtualHostModule");
		if(m_init_vh_module==NULL){
			kfiber_mutex_unlock(lock);
			klog(KLOG_ERR,"cann't find initVirtualHostModule function in driver [%s]\n",driver.c_str());
			return false;
		}
		if(m_init_vh_module(&vhm) == 0 || vhm.createConnection==NULL){
			kfiber_mutex_unlock(lock);
			klog(KLOG_ERR,"Cann't init vh module in driver [%s]\n",driver.c_str());
			return false;
		}
		ext = cur_config_ext;
	}
	if(vhm.parseConfig){
		vh_data vd;
		init_vh_data(&vd,&attribute);
		vhm.parseConfig(&vd);
	}
	result = true;
	kfiber_mutex_unlock(lock);
	return result;
}
KVirtualHost *KVirtualHostDatabase::newVirtualHost(kgl_vh_connection cn,std::map<std::string,std::string> &attribute,KVirtualHostManage *vm,KVirtualHost *ov)
{
	KTempleteVirtualHost *tm = NULL;
	KVirtualHost *vh = NULL;
	bool result = false;
	std::string templete = attribute["templete"];
	if(templete.size()>0){
		std::string subtemplete = attribute["subtemplete"];
		if(subtemplete.size()>0){
			templete += ":";
			templete += subtemplete;
		}	
		tm = vm->refsTempleteVirtualHost(templete);		
	}
#ifndef HTTP_PROXY
	vh = KHttpServerParser::buildVirtualHost(attribute,&vm->globalVh,tm,ov);
#endif
	if (vh) {
		vh->db = true;
		vh->addRef();
		loadInfo(vh,cn);
		conf.gvm->inheritVirtualHost(vh,false);
		if (ov) {
			result = vm->updateVirtualHost(vh,ov);
		} else {
			result = vm->addVirtualHost(vh);
		}
	}
	if (tm) {
		tm->destroy();
	}
	if (!result && vh) {		
		vh->destroy();
		return NULL;
	}
	return vh;
}
bool KVirtualHostDatabase::loadInfo(KVirtualHost *vh, kgl_vh_connection cn)
{
	if(vhm.loadInfo == NULL){
		return false;
	}
	std::map<std::string,std::string> attribute;
	vh_data vd;
	init_vh_data(&vd,&attribute);
	vhd_load_info_param param;
	kgl_vh_stmt st = NULL;
	param.cn = cn;
	param.vhm = &vhm;
	param.name = vh->name.c_str();
	param.st = &st;
	int ret;
	if (kfiber_thread_call(thread_vhd_load_info, &param, 1, &ret) != 0) {
		return false;
	}
	if(st==NULL){
		return false;
	}
	vhd_query_param query_param;
	query_param.vhm = &vhm;
	query_param.st = st;
	query_param.data = &vd;
	int result;
	for (;;) {
		if (kfiber_thread_call(thread_vhd_query, &query_param, 1, &result) != 0) {
			break;
		}
		if (!result) {
			break;
		}
		const char *type = attribute["type"].c_str();
		const char *name = attribute["name"].c_str();
		const char *value = attribute["value"].c_str();
		if (attribute["skip_kangle"]=="1") {
			continue;
		}
		if(type==NULL || name==NULL){
			attribute.clear();
			continue;
		}
		int t = atoi(type);
		switch(t){
			case VH_INFO_HOST:
			case VH_INFO_HOST2:
			{
				KSubVirtualHost *svh = new KSubVirtualHost(vh);
				svh->setHost(name);
				svh->setDocRoot(vh->doc_root.c_str(),value);
				vh->hosts.push_front(svh);
				break;
			}
			case VH_INFO_ERROR_PAGE:
			{
				if(value){
					vh->addErrorPage(atoi(name),value);
				}
				break;
			}
			case VH_INFO_INDEX:
			{
				vh->addIndexFile(name,atoi(value));
				break;
			}
			case VH_INFO_ALIAS:
			{
				char *buf = strdup(value);
				char *to = buf;
				char *p = strchr(buf,',');
				if (p) {
					*p = '\0';
					p++;
					char *internal = p;
					p = strchr(internal,',');
					std::string errMsg;
					if (p) {
						*p = '\0';
						p++;
						vh->addAlias(name,to,vh->doc_root.c_str(),*internal=='1',atoi(p),errMsg);
					}
				}
				free(buf);
				break;
			}
			case VH_INFO_MAP:
			{
				//name格式       是否文件扩展名1|0,值
				//value格式      是否验证文件存在1|0,target,allowMethod
				const char *map_val = strchr(name,',');
				if (map_val) {
					map_val ++;
					bool file_ext = (*name=='1');
					char *buf = strdup(value);
					char *p = strchr(buf,',');
					if (p) {
						*p = '\0';
						p++;
						char *target = p;
						p = strchr(p,',');
						if (p) {
							*p = '\0';
							p++;
							char *allowMethod = p;
							bool confirmFile = false;
							if (*buf=='1') {
								confirmFile = true;
							}
							vh->addRedirect(file_ext,map_val,target,allowMethod,confirmFile,"");
						}
					}
					free(buf);
				}
				break;
			}
			case VH_INFO_MIME:
			{
				char *buf = strdup(value);
				char *p = strchr(buf,',');
				if(p){
					*p = '\0';
					p++;
					kgl_compress_type compress = (kgl_compress_type)atoi(p);
					p = strchr(p,',');
					if(p){
						int max_age = atoi(p+1);
						vh->addMimeType(name,buf, compress,max_age);
					}
				}
				free(buf);
				break;
			}
#ifdef ENABLE_BASED_PORT_VH
			case VH_INFO_BIND:
			{
				vh->binds.push_back(name);
				break;
			}
#endif
			default:
				vh->addEnvValue(name,value);
		}
		attribute.clear();
	}
	vhm.freeStmt(st);
	return true;
}
#if 0
void KVirtualHostDatabase::clear()
{
	if(ext){
		return;
	}	
}
#endif
void KVirtualHostDatabase::freeConnection(kgl_vh_connection cn)
{
	if (vhm.freeConnection) {
		vhm.freeConnection(cn);
	}
}
kgl_vh_connection KVirtualHostDatabase::createConnection()
{
	if (vhm.createConnection == NULL) {
		return NULL;
	}
	kgl_vh_connection cn;
	vhd_connect_param param;
	param.cn = &cn;
	param.vhm = &vhm;
	int ret;
	if (kfiber_thread_call(thread_vhd_connect, &param, 1, &ret) != 0) {
		return NULL;
	}
	return cn;
}