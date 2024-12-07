#include "KVirtualHostDatabase.h"
#include "KVirtualHostManage.h"
#include "KHttpServerParser.h"
#include "log.h"
#include "utils.h"
#include "extern.h"
#include "KAcserverManager.h"
#include "kmalloc.h"
#include "KDefer.h"
#include "KAccessDso.h"

struct vhd_load_base_param
{
	vh_module* vhm;
	kgl_vh_connection cn;
	kgl_vh_stmt* st;
};
struct vhd_load_vh_param : vhd_load_base_param
{
	const char* name;
};
static int thread_load_vh(void* arg, int argc) {
	vhd_load_vh_param* param = (vhd_load_vh_param*)arg;
	if (param->name) {
		*(param->st) = param->vhm->flushVirtualHost(param->cn, param->name);
	} else {
		*(param->st) = param->vhm->loadVirtualHost(param->cn);
	}
	return 0;
}
struct vhd_load_info_param : vhd_load_base_param
{

	const char* name;

};
static int thread_vhd_load_info(void* arg, int argc) {
	vhd_load_info_param* param = (vhd_load_info_param*)arg;
	*(param->st) = param->vhm->loadInfo(param->cn, param->name);
	return 0;
}
struct vhd_load_black_list_param : vhd_load_base_param
{

};
static int thread_vhd_load_black_list(void* arg, int argc) {
	vhd_load_black_list_param* param = (vhd_load_black_list_param*)arg;
	*(param->st) = param->vhm->loadBlackList(param->cn);
	return 0;
}
struct vhd_query_param
{
	vh_module* vhm;
	kgl_vh_stmt st;
	vh_data* data;
};
static int thread_vhd_query(void* arg, int argc) {
	vhd_query_param* param = (vhd_query_param*)arg;
	return param->vhm->query(param->st, param->data);
}
struct vhd_connect_param
{
	vh_module* vhm;
	kgl_vh_connection* cn;
};
static int thread_vhd_connect(void* arg, int argc) {
	vhd_connect_param* param = (vhd_connect_param*)arg;
	*(param->cn) = param->vhm->createConnection();
	return 0;
}
KVirtualHostDatabase vhd;
static void set_vh_data(void* ctx, const char* name, const char* value) {
	KXmlAttribute* attribute = (KXmlAttribute*)ctx;
	attribute->emplace(name, value);
	return;
}
static const char* getx_vh_data(void* ctx, const char* name) {
	KXmlAttribute* attribute = (KXmlAttribute*)ctx;
	auto it = attribute->find(name);
	if (it == attribute->end()) {
		return NULL;
	}
	return (*it).second.c_str();
}
static const char* get_vh_data(void* ctx, const char* name) {
	const char* value = getx_vh_data(ctx, name);
	if (value) {
		return value;
	}
	return "";
}
void init_vh_data(vh_data* vd, khttpd::KXmlNodeBody* body) {
	vd->body = &kgl_config_body_func;
	vd->node = &kgl_config_node_func;
	vd->ctx = (kgl_config_body*)body;
}
static const char* getSystemEnv(void* param, const char* name) {
	static KString value;
	const char* value2 = getSystemEnv(name);
	if (value2) {
		return value2;
	}
	if (!conf.gvm->vhs.getEnvValue(name, value)) {
		return NULL;
	}
	return value.c_str();
}
KVirtualHostDatabase::KVirtualHostDatabase() {
	lock = kfiber_mutex_init();
	lastStatus = false;
	memset(&vhm, 0, sizeof(vhm));
	vhm.vhi_version = 2;
	vhm.cbsize = sizeof(vhm);
	vhm.getConfigValue = getSystemEnv;
}
KVirtualHostDatabase::~KVirtualHostDatabase() {
	kfiber_mutex_destroy(lock);
}
bool KVirtualHostDatabase::check() {
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
bool KVirtualHostDatabase::isLoad() {
	return (vhm.createConnection != NULL);
}
bool KVirtualHostDatabase::parse_config(khttpd::KXmlNodeBody* xml) {
	auto attribute = xml->attr();
	bool result = false;
	kfiber_mutex_lock(lock);
	auto driver = attribute["driver"];
	if (vhm.createConnection == NULL) {

		if (!isAbsolutePath(driver.c_str())) {
			driver = conf.path + driver;
		}
		if (!vhm_handle.isloaded()) {
			if (!vhm_handle.load(driver.c_str())) {
				kfiber_mutex_unlock(lock);
				klog(KLOG_ERR, "cann't load driver [%s]\n", driver.c_str());
				return false;
			}
		}
		initVirtualHostModulef m_init_vh_module = (initVirtualHostModulef)vhm_handle.findFunction("initVirtualHostModule");
		if (m_init_vh_module == NULL) {
			kfiber_mutex_unlock(lock);
			klog(KLOG_ERR, "cann't find initVirtualHostModule function in driver [%s]\n", driver.c_str());
			return false;
		}
		if (m_init_vh_module(&vhm) == 0 || vhm.createConnection == NULL) {
			kfiber_mutex_unlock(lock);
			klog(KLOG_ERR, "Cann't init vh module in driver [%s]\n", driver.c_str());
			return false;
		}
	}
	if (vhm.parseConfig) {
		vh_data vd;
		init_vh_data(&vd, xml);
		vhm.parseConfig(&vd);
	}
	result = true;
	kfiber_mutex_unlock(lock);
	return result;
}
bool KVirtualHostDatabase::loadInfo(khttpd::KXmlNodeBody *vh, kgl_vh_connection cn) {
	if (vhm.loadInfo == NULL) {
		return false;
	}
	auto tvh = khttpd::KSafeXmlNodeBody(new khttpd::KXmlNodeBody());
	vh_data vd;
	init_vh_data(&vd, tvh.get());
	vhd_load_info_param param;
	kgl_vh_stmt st = NULL;
	param.cn = cn;
	param.vhm = &vhm;
	param.name = vh->attributes("name");
	param.st = &st;
	int ret;
	if (kfiber_thread_call(thread_vhd_load_info, &param, 1, &ret) != 0) {
		return false;
	}
	if (st == NULL) {
		return false;
	}
	defer(vhm.freeStmt(st));

	vhd_query_param query_param;
	query_param.vhm = &vhm;
	query_param.st = st;
	query_param.data = &vd;
	int result;
	auto attribute = tvh->attr();
	for (;;) {
		if (kfiber_thread_call(thread_vhd_query, &query_param, 1, &result) != 0) {
			break;
		}
		if (!result) {
			break;
		}
		const char* type = attribute["type"].c_str();
		const char* name = attribute["name"].c_str();
		const char* value = attribute["value"].c_str();
		if (attribute["skip_kangle"] == "1") {
			continue;
		}
		if (type == NULL || name == NULL) {
			tvh->clear();
			continue;
		}
		int t = atoi(type);
		switch (t) {
		case VH_INFO_HOST:
		case VH_INFO_HOST2:
		{
			auto svh = kconfig::new_child(vh, _KS("host"));
			svh->set_text(name);
			svh->attributes("dir", value);
			break;
		}
		case VH_INFO_ERROR_PAGE:
		{
			auto err = kconfig::new_child(vh, _KS("error"));
			err->attributes.emplace("code", name);
			err->attributes.emplace("file", value);
			break;
		}
		case VH_INFO_INDEX:
		{
			auto index = kconfig::new_child(vh, _KS("index"));
			index->attributes.emplace("id", value);
			index->attributes.emplace("file", name);
			break;
		}
		case VH_INFO_ALIAS:
		{
			char* buf = strdup(value);
			char* to = buf;
			char* p = strchr(buf, ',');
			if (p) {
				*p = '\0';
				p++;
				char* internal = p;
				p = strchr(internal, ',');
				std::string errMsg;
				if (p) {
					*p = '\0';
					p++;
					auto alias = kconfig::new_child(vh, _KS("alias"));
					alias->attributes.emplace("path", name);
					alias->attributes.emplace("to", to);
					alias->attributes.emplace("internal", internal);					
				}
			}
			free(buf);
			break;
		}
		case VH_INFO_MAP:
		{
			//name格式       是否文件扩展名1|0,值
			//value格式      是否验证文件存在1|0,target,allowMethod
			const char* map_val = strchr(name, ',');
			if (map_val) {
				map_val++;
				bool file_ext = (*name == '1');
				char* buf = strdup(value);
				char* p = strchr(buf, ',');
				if (p) {
					*p = '\0';
					p++;
					char* target = p;
					p = strchr(p, ',');
					if (p) {
						*p = '\0';
						p++;
						char* allowMethod = p;
						char *confirmFile = buf;				
						auto rd = kconfig::new_child(vh, _KS("map"));
						if (file_ext) {
							rd->attributes.emplace("file_ext", map_val);
						} else {
							rd->attributes.emplace("path", map_val);
						}
						rd->attributes.emplace("extend", target);
						rd->attributes.emplace("allow_method", allowMethod);
						rd->attributes.emplace("confirm_file", confirmFile);
					}
				}
				free(buf);
			}
			break;
		}
		case VH_INFO_MIME:
		{
			char* buf = strdup(value);
			char* p = strchr(buf, ',');
			if (p) {
				*p = '\0';
				p++;
				char *compress = p;
				p = strchr(p, ',');
				if (p) {
					*p = '\0';
					++p;
					auto mime_type = kconfig::new_child(vh, _KS("mime_type"));
					mime_type->attributes.emplace("ext", name);
					mime_type->attributes.emplace("type", buf);
					mime_type->attributes.emplace("compress", compress);
					mime_type->attributes.emplace("max_age", p);

				}
			}
			free(buf);
			break;
		}
#ifdef ENABLE_BASED_PORT_VH
		case VH_INFO_BIND:
		{
			auto bind = kconfig::new_child(vh, _KS("bind"));
			bind->set_text(name);
			break;
		}
#endif
		default:
			auto env = kconfig::new_child(vh, _KS("env"));
			env->attributes.emplace("name", name);
			env->attributes.emplace("value", value);
			break;
		}
		tvh->clear();
	}
	return true;
}
void KVirtualHostDatabase::scan(kconfig::KConfigFileScanInfo* info) {
	kassert(kfiber_self2() != NULL);
	auto locker = get_locker();
	lastStatus = false;
	kgl_vh_connection cn = createConnection();
	if (cn == NULL) {
		return;
	}
	defer(vhm.freeConnection(cn));
	kgl_vh_stmt rs = NULL;
	lastStatus = true;
	vhd_load_vh_param param;
	param.vhm = &vhm;
	param.cn = cn;
	param.st = &rs;
	param.name = NULL;
	kfiber_thread_call(thread_load_vh, &param, 1, NULL);
	lastStatus = (rs != NULL);
	if (rs == NULL) {
		return;
	}
	vh_data vd;

	defer(vhm.freeStmt(rs));
	khttpd::KSafeXmlNodeBody body(new khttpd::KXmlNodeBody());
	init_vh_data(&vd, body.get());
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
		setActive();
		auto attr = body->attr();
		KStringBuf name;
		name << "@vhd|" << attr("name");
		auto name2 = name.reset();
		KString vh_name(attr("name"));
		info->new_file(name2.data(), vh_name.data(), KFileModified(time(NULL),0), false);
		body->attributes.clear();
		body->childs.clear();
	}

#if 0
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
				const char* ip = attribute["ip"].c_str();
				if (*ip) {
					int flags = atoi(attribute["flags"].c_str());
					conf.gvm->vhs.blackList->AddStatic(ip, KBIT_TEST(flags, 1));
				}
			}
			vhm.freeStmt(rs);
		}
	}
#endif
#endif
	return;
}
khttpd::KSafeXmlNode KVirtualHostDatabase::load(kconfig::KConfigFile* file) {
	kgl_vh_connection cn = createConnection();
	if (cn == NULL) {		
		return nullptr;
	}
	file->set_index(kconfig::default_file_index);
	file->set_remove_flag(false);
	auto xml = kconfig::new_xml(_KS("config"));
	auto body = kconfig::new_xml(_KS("vh"), file->get_filename()->data, file->get_filename()->len);
	xml->append(body.get());
	defer(vhm.freeConnection(cn));
	{
		kgl_vh_stmt rs = NULL;
		vhd_load_vh_param param;
		param.vhm = &vhm;
		param.cn = cn;
		param.st = &rs;
		param.name = file->get_filename()->data;
		kfiber_thread_call(thread_load_vh, &param, 1, NULL);
		if (rs == NULL) {
			return nullptr;
		}
		defer(vhm.freeStmt(rs));
		vh_data vd;
		init_vh_data(&vd, body->get_first());
		vhd_query_param query_param;
		query_param.vhm = &vhm;
		query_param.st = rs;
		query_param.data = &vd;
		int result;
		if (kfiber_thread_call(thread_vhd_query, &query_param, 1, &result) != 0) {
			return nullptr;
		}
	}
	loadInfo(body->get_first(), cn);
	return xml;
}
KFileModified KVirtualHostDatabase::get_last_modified(kconfig::KConfigFile* file) {
	return KFileModified(0,0);
}
void KVirtualHostDatabase::freeConnection(kgl_vh_connection cn) {
	if (vhm.freeConnection) {
		vhm.freeConnection(cn);
	}
}
kgl_vh_connection KVirtualHostDatabase::createConnection() {
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