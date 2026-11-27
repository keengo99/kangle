#include "WhmCore.h"
#include "WhmContext.h"
#include "whm.h"
#include "global.h"
#include "KReportIp.h"
#include "KVirtualHostManage.h"
#include "KVirtualHostDatabase.h"
#include "KHttpManage.h"
#include "extern.h"
#include "kserver.h"
#include "cache.h"
#include "KHttpLib.h"
#include "KConfigBuilder.h"
#include "KAcserverManager.h"
#include "KHttpServerParser.h"
#include "kselector_manager.h"
#include "KCdnContainer.h"
#include "KDsoExtendManage.h"
#include "kaddr.h"
#include "KLogDrill.h"
#include "extern.h"
#include "KConfigTree.h"
#include "KChain.h"
#include "ssl_utils.h"
#include "KProcessManage.h"
#include "KReg.h"
#include "KIpList.h"
#include "KMultiAcserver.h"
#include "KHttpAuth.h"
#include "kmd5.h"

static int config_result(kconfig::KConfigResult rs, WhmContext* ctx) {
	switch (rs) {
	case kconfig::KConfigResult::Success:
		return WHM_OK;
	case kconfig::KConfigResult::ErrNotFound:
		ctx->setStatus("not found");
		return WHM_PARAM_ERROR;
	case kconfig::KConfigResult::ErrSaveFile:
		ctx->setStatus("save file error");
		return WHM_CALL_FAILED;
	default:
		ctx->setStatus("unknow");
		return WHM_CALL_FAILED;
	}
}
static KSafeAccess whm_get_access(WhmContext* ctx) {
	auto uv = ctx->getUrlValue();
	KVirtualHost* vh = ctx->getVh();
	const char* vh_str = uv->getx("vh");
	if (vh == NULL && vh_str && *vh_str) {
		ctx->setStatus("cann't find such vh");
		return nullptr;
	}
	const char* access = uv->getx("access");
	if (access == NULL) {
		ctx->setStatus("access must be set");
		return nullptr;
	}
	int access_type = REQUEST;
	if (strcasecmp(access, "response") == 0) {
		access_type = RESPONSE;
	} else if (strcasecmp(access, "request") == 0) {
		access_type = REQUEST;
	} else {
		ctx->setStatus("access must be response or request");
		return nullptr;
	}
#ifndef HTTP_PROXY	
	if (vh) {
		return vh->get_access(!!access_type);
	}
#endif
	return KSafeAccess(kaccess[access_type]->add_ref());
}
static KString whm_get_table_name(const KUrlValue* uv) {
	if (!uv->get("table_name").empty()) {
		return uv->get("table_name");
	}
	if (!uv->get("table").empty()) {
		return uv->get("table");
	}
	return uv->get("name");
}
static bool whm_is_json(const KUrlValue* uv) {
	return strcasecmp(uv->get("format").c_str(), "json") == 0;
}
int WhmCore::call_clean_cache(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto uv = ctx->getUrlValue();
	std::string urls = uv->get("url").c_str();
	if (urls.empty()) {
		ctx->setStatus("url is missing");
		return WHM_CALL_FAILED;
	}

	int count = 0;
	size_t start = 0;
	for (;;) {
		size_t end = urls.find(", ", start);
		std::string item = urls.substr(start, end == std::string::npos ? std::string::npos : end - start);
		if (!item.empty()) {
			const char* value = item.c_str();
			switch (*value) {
			case '1': {
				KReg reg;
				if (reg.setModel(value + 1, 0)) {
					count += clean_cache(&reg, 0);
				}
				break;
			}
			case '2': {
				KReg reg;
				if (reg.setModel(value + 1, KGL_PCRE_CASELESS)) {
					count += clean_cache(&reg, 0);
				}
				break;
			}
			case '3':
				count += clean_cache(value + 1, true);
				break;
			case '0':
				++value;
				// fall through
			default:
				count += clean_cache(value, false);
				break;
			}
		}
		if (end == std::string::npos) {
			break;
		}
		start = end + 2;
	}
	ctx->add("count", count);
	return WHM_OK;
}
int WhmCore::call_cache_info(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto urls = std::string(ctx->getUrlValue()->get("url").c_str());
	if (urls.empty()) {
		ctx->setStatus("url is missing");
		return WHM_PARAM_ERROR;
	}
	KCacheInfo info{};
	int count = 0;
	size_t start = 0;
	for (;;) {
		auto end = urls.find(", ", start);
		auto item = urls.substr(start, end == std::string::npos ? std::string::npos : end - start);
		if (!item.empty()) {
			const char* url = item.c_str();
			bool wide = false;
			if (*url == '0') {
				++url;
			} else if (*url == '3') {
				++url;
				wide = true;
			}
			count += get_cache_info(url, wide, &info);
		}
		if (end == std::string::npos) {
			break;
		}
		start = end + 2;
	}
	ctx->add("mem_size", info.mem_size);
	ctx->add("disk_size", info.disk_size);
	ctx->add("count", count);
	return WHM_OK;
}
int WhmCore::call_cache_prefetch(const char* call_name, const char* event_type, WhmContext* ctx) {
#ifdef ENABLE_SIMULATE_HTTP
	auto urls = std::string(ctx->getUrlValue()->get("url").c_str());
	if (urls.empty()) {
		ctx->setStatus("url is missing");
		return WHM_PARAM_ERROR;
	}
	int count = 0;
	size_t start = 0;
	for (;;) {
		auto end = urls.find(", ", start);
		auto item = urls.substr(start, end == std::string::npos ? std::string::npos : end - start);
		if (!item.empty() && cache_prefetch(item.c_str())) {
			++count;
		}
		if (end == std::string::npos) {
			break;
		}
		start = end + 2;
	}
	ctx->add("count", count);
	return WHM_OK;
#else
	ctx->setStatus("cache prefetch not supported");
	return WHM_CALL_NOT_FOUND;
#endif
}
int WhmCore::call_clean_all_cache(const char* call_name, const char* event_type, WhmContext* ctx) {
	dead_all_obj();
	return WHM_OK;
}
int WhmCore::call_add_table(const char* call_name, const char* event_type, WhmContext* ctx) {
	KStringBuf name;
	KStringBuf path;
	KStringBuf file;
	auto uv = ctx->getUrlValue();
	auto vh = ctx->getVh();
	auto vh_name = uv->getx("vh");
	auto table_name = whm_get_table_name(uv);
	if (table_name.empty()) {
		ctx->setStatus("table name is empty");
		return WHM_CALL_FAILED;
	}
	auto access = whm_get_access(ctx);
	if (!access) {
		return WHM_CALL_FAILED;
	}
	auto flag = kconfig::EvUpdate | kconfig::FlagCreate | kconfig::FlagCopyChilds | kconfig::FlagCreateParent;
	if (vh && vh->has_user_access()) {
		vh->get_access_file(file);
	}
	if (vh_name && strncmp(file.c_str(), _KS("@vh|")) != 0) {
		path << "vh@" << vh_name << "/";
	}
	name << "table@"_CS << table_name;
	path << access->get_qname() << "/"_CS << name;
	auto xml = kconfig::new_xml(name.c_str(), name.size());
	if (!file.empty()) {
		return config_result(kconfig::update(file.str().str(), path.str().str(), 0, xml.get(), flag), ctx);
	} else {
		return config_result(kconfig::update(path.str().str(), 0, xml.get(), flag), ctx);
	}
}
int WhmCore::call_empty_table(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto uv = ctx->getUrlValue();
	auto access = whm_get_access(ctx);
	if (!access) {
		return WHM_CALL_FAILED;
	}
	auto table_name = whm_get_table_name(uv);
	if (table_name.empty()) {
		ctx->setStatus("table name is empty");
		return WHM_PARAM_ERROR;
	}
	for (;;) {
		KChainLocation location;
		if (!access->find_chain_location(table_name, nullptr, location)) {
			return WHM_OK;
		}
		KStringBuf path;
		ctx->build_config_base_path(path, location.file);
		path << access->get_qname() << "/table@" << table_name << "/chain"_CS;
		auto result = location.file.empty()
			? kconfig::remove(path.str().str(), location.id)
			: kconfig::remove(location.file.str(), path.str().str(), location.id);
		if (result != kconfig::KConfigResult::Success) {
			return config_result(result, ctx);
		}
	}
}
int WhmCore::call_get_chain(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto access = whm_get_access(ctx);
	if (!access) {
		return WHM_CALL_FAILED;
	}
	return access->get_chain(ctx, whm_get_table_name(ctx->getUrlValue()));
}
int WhmCore::call_edit_chain(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto access = whm_get_access(ctx);
	if (!access) {
		return WHM_CALL_FAILED;
	}
	auto&& uv = ctx->getUrlValue();
	auto file = ctx->get_vh_config_file();
	auto table_name = whm_get_table_name(uv);
	if (table_name.empty()) {
		ctx->setStatus("table name is empty");
		return WHM_PARAM_ERROR;
	}
	auto id = uv->attribute.get_int("id");
	auto add = strcmp(call_name, "add_chain") == 0 || uv->attribute.get_int("add");
	if (!add && uv->getx("id") == nullptr) {
		KChainLocation location;
		auto name = uv->get("name");
		if (name.empty()) {
			ctx->setStatus("chain name is empty");
			return WHM_PARAM_ERROR;
		}
		if (access->find_chain_location(table_name, &name, location)) {
			file = location.file;
			id = location.id;
		} else {
			// The legacy WHM edit_chain call is an upsert when a chain name is
			// supplied.  EasyPanel relies on this while rebuilding access.xml
			// from access.xml.db after first emptying the destination table.
			add = true;
			id = 0;
		}
	}
	KStringBuf path;
	ctx->build_config_base_path(path, file);
	path << access->get_qname() << "/table@" << table_name << "/chain"_CS;
	auto xml = KChain::to_xml(*uv);
	if (add) {
		if (file.empty()) {
			return config_result(kconfig::update(path.str().str(), id, xml.get(), kconfig::EvNew), ctx);
		}
		return config_result(kconfig::update(file.str(), path.str().str(), id, xml.get(), kconfig::EvNew),ctx);
	}
	if (file.empty()) {
		return config_result(kconfig::update(path.str().str(), id, xml.get(), kconfig::EvUpdate), ctx);
	}
	return config_result(kconfig::update(file.str(), path.str().str(), id, xml.get(), kconfig::EvUpdate), ctx);
}
int WhmCore::call_del_chain(const char* call_name, const char* event_type, WhmContext* ctx) {
	KStringBuf path;
	auto uv = ctx->getUrlValue();
	auto access = whm_get_access(ctx);
	if (!access) {
		return WHM_CALL_FAILED;
	}
	auto table_name = whm_get_table_name(uv);
	if (table_name.empty()) {
		ctx->setStatus("table name is empty");
		return WHM_PARAM_ERROR;
	}
	auto file = ctx->get_vh_config_file();
	auto id = uv->attribute.get_int("id");
	if (uv->getx("id") == nullptr) {
		KChainLocation location;
		auto name = uv->get("name");
		if (name.empty() || !access->find_chain_location(table_name, &name, location)) {
			ctx->setStatus("chain not found");
			return WHM_PARAM_ERROR;
		}
		file = location.file;
		id = location.id;
	}
	ctx->build_config_base_path(path, file);
	path << access->get_qname() << "/table@"_CS << table_name << "/chain";
	if (file.empty()) {
		return config_result(kconfig::remove(path.str().str(), id), ctx);
	}
	return config_result(kconfig::remove(file.str(), path.str().str(), id), ctx);
}
int WhmCore::call_del_table(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto uv = ctx->getUrlValue();
	auto access = whm_get_access(ctx);
	if (!access) {
		return WHM_CALL_NOT_FOUND;
	}
	auto table_name = whm_get_table_name(uv);
	if (table_name.empty()) {
		ctx->setStatus("table name is empty");
		return WHM_PARAM_ERROR;
	}
	if (access->is_table_used(table_name)) {
		ctx->setStatus("table is used");
		return WHM_CALL_FAILED;
	}
	auto vh = ctx->getVh();
	const char* vh_name = uv->getx("vh");
	KStringBuf path;
	auto file = ctx->get_vh_config_file();
	ctx->build_config_base_path(path, file.str());
	path << access->get_qname() << "/table@"_CS << table_name;
	if (!file.empty()) {
		return config_result(kconfig::remove(file.str(), path.str().str(), 0), ctx);
	}
	return config_result(kconfig::remove(path.str().str(), 0), ctx);
}
int WhmCore::call_list_chain(const char* call_name, const char* event_type, WhmContext* ctx) {
	KSafeAccess maccess = whm_get_access(ctx);
	if (!maccess) {
		return WHM_CALL_FAILED;
	}
	auto uv = ctx->getUrlValue();
	auto table_name = whm_get_table_name(uv);
	if (!whm_is_json(uv) && !uv->get("table_name").empty()) {
		KStringBuf xml;
		auto name = uv->get("name");
		const KString* name_filter = name.empty() ? nullptr : &name;
		if (!maccess->build_legacy_chain(table_name, name_filter, uv->get("detail") != "0", xml)) {
			ctx->setStatus(name_filter ? "chain not found" : "table not found");
			return WHM_PARAM_ERROR;
		}
		ctx->add_raw_xml("table_info", xml.str());
		return WHM_OK;
	}
	return maccess->dump_chain(ctx, table_name);
}
int WhmCore::call_list_table(const char* call_name, const char* event_type, WhmContext* ctx) {
	KSafeAccess maccess = whm_get_access(ctx);
	if (!maccess) {
		return WHM_CALL_FAILED;
	}
	maccess->listTable(ctx, whm_is_json(ctx->getUrlValue()));
	return WHM_OK;
}
int WhmCore::call_vh_action(const char* call_name, const char* event_type, WhmContext* ctx) {
	KString err_msg;
	if (conf.gvm->vh_base_action(*ctx->getUrlValue(), err_msg)) {
		return WHM_OK;
	}
	ctx->setStatus(err_msg.c_str());
	return WHM_CALL_FAILED;
}
int WhmCore::call_get_vh(const char* call_name, const char* event_type, WhmContext* ctx) {
	conf.gvm->dump_vh(ctx->data());
	return WHM_OK;
}
int WhmCore::call_list_vh(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto uv = ctx->getUrlValue();
	std::list<KString> vhs;
	conf.gvm->getAllVh(
		vhs,
		uv->get("status") == "1",
		uv->get("onlydb") == "1"
	);
	auto names = ctx->data()->add_string_array("name");
	for (auto it = vhs.begin(); it != vhs.end(); it++) {
		names->push_back((*it));
	}
	return WHM_OK;
}
int WhmCore::call_info_domain(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto name = ctx->getUrlValue()->get("name");
	KSafeVirtualHost vh(conf.gvm->refsVirtualHostByName(name));
	if (!vh) {
		ctx->setStatus("vh cann't find");
		return WHM_PARAM_ERROR;
	}
	{
		auto locker = vh->get_locker();
		for (auto sub_vh : vh->hosts) {
			ctx->add("domain", sub_vh->host);
		}
	}
	vh->getParsedFileExt(ctx);
	return WHM_OK;
}
int WhmCore::call_info_vh(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto name = ctx->getUrlValue()->get("name");
	KSafeVirtualHost vh(conf.gvm->refsVirtualHostByName(name));
	if (!vh) {
		ctx->setStatus("vh cann't find");
		return WHM_PARAM_ERROR;
	}
	ctx->add("name", name);
	KStringBuf values;
	{
		auto locker = vh->get_locker();
#ifdef ENABLE_BASED_PORT_VH
		for (auto&& bind : vh->binds) {
			values << bind << "\n";
		}
		ctx->add("bind", values.str());
		values.clear();
#endif
		for (auto sub_vh : vh->hosts) {
			values << sub_vh->host;
			if (strcmp(sub_vh->dir, "/") != 0) {
				values << "|" << sub_vh->dir;
			}
			values << "\n";
		}
		ctx->add("host", values.str());
		ctx->add("doc_root", vh->GetDocumentRoot());
		ctx->add("inherit", vh->inherit ? "1" : "0");
#ifdef ENABLE_VH_RUN_AS
		ctx->add("user", vh->user);
#ifndef _WIN32
		ctx->add("group", vh->group);
#endif
#endif
#ifdef ENABLE_VH_LOG_FILE
		ctx->add("log_file", vh->logFile);
		if (vh->logger) {
			KString rotate_time;
			vh->logger->getRotateTime(rotate_time);
			ctx->add("log_rotate_time", rotate_time);
			ctx->add("log_rotate_size", vh->logger->rotate_size);
		}
#endif
		ctx->add("browse", vh->browse ? "1" : "0");
#ifdef ENABLE_USER_ACCESS
		ctx->add("access_file", vh->user_access);
#endif
#ifdef ENABLE_VH_RS_LIMIT
		ctx->add("connect", vh->max_connect);
		ctx->add("speed_limit", vh->speed_limit);
#endif
	}
	return WHM_OK;
}
int WhmCore::call_list_index(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto vh = ctx->getVh();
	if (!vh) {
		ctx->setStatus("no such vh");
		return WHM_PARAM_ERROR;
	}
	vh->listIndex(ctx);
	return WHM_OK;
}
int WhmCore::call_reload_vh(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto uv = ctx->getUrlValue();
	KString name = uv->get("name");
	if (strcmp(call_name, "reload_all_vh") == 0 || name.empty()) {
		kconfig::reload();
		return WHM_OK;
	}

	const bool deleting = strcmp(call_name, "del_vh") == 0;
	const bool destroy = deleting &&
		(uv->get("destroy") == "1" || strcasecmp(uv->get("destroy").c_str(), "true") == 0);
	KVirtualHost* old_vh = destroy ? conf.gvm->refsVirtualHostByName(name) : nullptr;

	KStringBuf config_name;
	config_name << "@vhd|"_CS << name;
	auto config_ref = kstring_from2(config_name.c_str(), config_name.size());
	bool result = kconfig::reload_config(config_ref, true);
	kstring_release(config_ref);
	if (!result) {
		// A newly-created database virtual host has no config-file entry yet;
		// non-database virtual hosts also use a different source name.  A full
		// scan handles both cases while preserving the legacy WHM contract.
		kconfig::reload();
	}

	if (deleting) {
		if (destroy && old_vh) {
			ctx->buildVh(old_vh);
			old_vh->destroyEvent(ctx);
		}
		return WHM_OK;
	}

	KVirtualHost* vh = conf.gvm->refsVirtualHostByName(name);
	if (!vh) {
		ctx->setStatus("cann't find such vh after reload");
		return WHM_CALL_FAILED;
	}
	ctx->buildVh(vh);
	KString init = uv->get("init");
	if (init == "1" || strcasecmp(init.c_str(), "true") == 0) {
		vh->initEvent(ctx);
#ifndef HTTP_PROXY
		conf.gam->killAllProcess(vh);
#endif
	} else {
		vh->updateEvent(ctx);
	}
	return WHM_OK;
}
int WhmCore::call_reload(const char* call_name, const char* event_type, WhmContext* ctx) {
	kconfig::reload();
	return WHM_OK;
}
int WhmCore::call_reload_vh_access(const char* call_name, const char* event_type, WhmContext* ctx) {
	if (!ctx->getVh()) {
		ctx->setStatus("cann't find vh");
		return WHM_PARAM_ERROR;
	}
	// Access files are part of the configuration tree.  A complete reload is
	// required here because their source name may be relative to the vhost.
	kconfig::reload();
	return WHM_OK;
}
int WhmCore::call_info(const char* call_name, const char* event_type, WhmContext* ctx) {

	INT64 total_mem_size = 0, total_disk_size = 0;
	int mem_count = 0, disk_count = 0;
	cache.getSize(total_mem_size, total_disk_size, mem_count, disk_count);
	ctx->add("server", PROGRAM_NAME);
	ctx->add("version", VERSION);
	// Report build capability independently of listener/vhost configuration.
#ifdef ENABLE_HTTP3
	ctx->add("support_http3", static_cast<INT64>(1));
#else
	ctx->add("support_http3", static_cast<INT64>(0));
#endif
	ctx->add("type", getServerType());
	ctx->add("os", getOsType());
	int total_run_time = (int)(kgl_current_sec - kgl_program_start_sec);
	ctx->add("total_run", total_run_time);
	ctx->add("connect", total_connect);
	ctx->add("fiber_count", kfiber_get_count());
	ctx->add("fiber_driver", kfiber_powered_by());
	int worker_count, free_count;
	kthread_get_count(&worker_count, &free_count);
	ctx->add("thread_worker", worker_count);
	ctx->add("thread_free", free_count);
	ctx->add("event_name", selector_manager_event_name());
	ctx->add("event_count", get_selector_count());
#ifdef ENABLE_STAT_STUB
	ctx->add("request", katom_get64((void*)&kgl_total_requests));
	ctx->add("accept", katom_get64((void*)&kgl_total_accepts));
#endif
	ctx->add("cache_count", cache.getCount());
	ctx->add("cache_mem", total_mem_size);
	ctx->add("cache_mem_count", mem_count);
	ctx->add("cache_disk_count", disk_count);
#ifdef ENABLE_DISK_CACHE
	INT64 total_size, free_size;
	ctx->add("cache_disk", total_disk_size);
	KStringBuf s;
	if (dci) {
		get_disk_base_dir(s);
		if (get_disk_size(total_size, free_size)) {
			ctx->add("disk_total", total_size);
			ctx->add("disk_free", free_size);
		}
	}
	ctx->add("disk_cache_dir", s.c_str());
#endif
	int vh_count = conf.gvm->getCount();
	ctx->add("vh", vh_count);
	ctx->add("kangle_home", conf.path.c_str());
#ifdef UPDATE_CODE
	ctx->add("update_code", UPDATE_CODE);
#endif
	ctx->add("open_file_limit", open_file_limit);
	ctx->add("addr_cache", kgl_get_addr_cache_count());
	ctx->add("disk_cache_shutdown", (int)cache.is_disk_shutdown());
	return WHM_OK;
}
int WhmCore::call_change_admin_password(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto uv = ctx->getUrlValue();
	KString user = uv->get("admin_user");
	if (user.empty()) {
		user = uv->get("user");
	}
	KString password = uv->get("admin_passwd");
	if (password.empty()) {
		password = uv->get("password");
	}
	if (user.empty() || password.empty()) {
		ctx->setStatus("admin user and password must be set");
		return WHM_PARAM_ERROR;
	}
	KString auth_name = uv->get("auth_type");
	if (auth_name.empty()) {
		auth_name = KHttpAuth::buildType(conf.auth_type);
	}
	int auth_type = KHttpAuth::parseType(auth_name.c_str());
	KStringBuf digest_source;
	if (auth_type == AUTH_DIGEST) {
		digest_source << user << ":" << PROGRAM_NAME << ":" << password;
	} else {
		digest_source << password;
	}
	char digest[33];
	KMD5(digest_source.c_str(), (int)digest_source.size(), digest);
	KXmlAttribute attributes;
	attributes.emplace("user"_CS, user);
	attributes.emplace("password"_CS, digest);
	attributes.emplace("crypt"_CS, "md5"_CS);
	attributes.emplace("auth_type"_CS, KHttpAuth::buildType(auth_type));
	auto admin_ips = uv->get("admin_ips");
	if (admin_ips.empty()) {
		KStringBuf ips;
		for (auto&& ip : conf.admin_ips) {
			if (!ips.empty()) {
				ips << "|";
			}
			ips << ip;
		}
		admin_ips = ips.str();
	}
	attributes.emplace("admin_ips"_CS, admin_ips);
	return config_result(kconfig::update("admin"_CS, 0, nullptr, &attributes,
		kconfig::EvUpdate | kconfig::FlagCreate), ctx);
}
int WhmCore::call_check_ssl(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto vh = ctx->getVh();
	if (!vh) {
		ctx->setStatus("cann't find vh");
		return WHM_PARAM_ERROR;
	}
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	SSL_CTX* ssl_ctx = kgl_get_ssl_ctx(vh->ssl_ctx);
#ifdef ENABLE_SVH_SSL
	auto domain = ctx->getUrlValue()->getx("domain");
	if (domain && *domain) {
		bool found = false;
		auto locker = vh->get_locker();
		for (auto sub_vh : vh->hosts) {
			if (sub_vh->match_host(domain)) {
				found = true;
				ssl_ctx = kgl_get_ssl_ctx(sub_vh->ssl_ctx ? sub_vh->ssl_ctx : vh->ssl_ctx);
				break;
			}
		}
		if (!found) {
			ctx->setStatus("domain not found");
			return WHM_PARAM_ERROR;
		}
	}
#endif
	ctx->add("ssl", ssl_ctx ? 1 : 0);
	if (ssl_ctx) {
		auto not_before = ssl_ctx_var_lookup(ssl_ctx, "NOTBEFORE");
		if (not_before) {
			ctx->add("not_before", not_before.get());
		}
		auto not_after = ssl_ctx_var_lookup(ssl_ctx, "NOTAFTER");
		if (not_after) {
			ctx->add("not_after", not_after.get());
		}
		auto subject = ssl_ctx_var_lookup(ssl_ctx, "SUBJECT");
		if (subject) {
			ctx->add("subject", subject.get());
		}
	}
	return WHM_OK;
#else
	ctx->setStatus("ssl sni not support");
	return WHM_CALL_NOT_FOUND;
#endif
}
int WhmCore::call_dump_flow(const char* call_name, const char* event_type, WhmContext* ctx) {
#ifdef ENABLE_VH_FLOW
	auto uv = ctx->getUrlValue();
	auto prefix = uv->getx("prefix");
	conf.gvm->dumpFlow(ctx, uv->get("revers") == "1", prefix, prefix ? (int)strlen(prefix) : 0,
		uv->attribute.get_int("extend"));
	return WHM_OK;
#else
	return WHM_CALL_NOT_FOUND;
#endif
}
int WhmCore::call_dump_load(const char* call_name, const char* event_type, WhmContext* ctx) {
#ifdef ENABLE_VH_FLOW
	auto uv = ctx->getUrlValue();
	auto prefix = uv->getx("prefix");
	conf.gvm->dumpLoad(ctx, uv->get("revers") == "1", prefix, prefix ? (int)strlen(prefix) : 0);
	return WHM_OK;
#else
	return WHM_CALL_NOT_FOUND;
#endif
}
int WhmCore::call_get_load(const char* call_name, const char* event_type, WhmContext* ctx) {
#ifdef ENABLE_VH_FLOW
	auto vh = ctx->getVh();
	if (!vh) {
		ctx->setStatus("cann't find vh");
		return WHM_PARAM_ERROR;
	}
	ctx->add("speed", vh->get_speed(ctx->getUrlValue()->get("reset") == "1"));
#ifdef ENABLE_VH_RS_LIMIT
	ctx->add("connect", vh->GetConnectionCount());
#endif
	return WHM_OK;
#else
	return WHM_CALL_NOT_FOUND;
#endif
}
int WhmCore::call_get_connection(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto uv = ctx->getUrlValue();
	KConnectionInfoContext cn_ctx;
	cn_ctx.total_count = 0;
	cn_ctx.debug = 0;
	cn_ctx.vh = uv->getx("vh");
	cn_ctx.translate = false;
	kgl_iterator_sink(kgl_connection_iterator, (void*)&cn_ctx);
	ctx->add("count", cn_ctx.total_count);
	ctx->add("connection", cn_ctx.s.str().c_str(), true);
	return WHM_OK;
}
int WhmCore::call_connection(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto uv = ctx->getUrlValue();
	KConnectionInfoContext cn_ctx;
	cn_ctx.total_count = 0;
	cn_ctx.debug = 0;
	cn_ctx.vh = uv->getx("vh");
	cn_ctx.sl = ctx->data();
	cn_ctx.translate = false;
	kgl_iterator_sink(kgl_connection_iterator2, (void*)&cn_ctx);
	return WHM_OK;
}
int WhmCore::call_get_config_listen(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto locker = kconfig::lock();
	for (auto it = conf.services.begin(); it != conf.services.end(); ++it) {
		int index = 0;
		for (auto&& lh : (*it).second) {
			if (lh) {
				auto sl = ctx->data()->add_obj_array("listen");
				sl->add("file", (*it).first);
				sl->add("id", index);
				sl->add("ip", lh->ip);
				sl->add("port", lh->port);
				sl->add("type", getWorkModelName(lh->model));
#ifdef KSOCKET_SSL
				lh->dump(sl);
#endif
			}
			++index;
		}
	}
	return WHM_OK;
}
int WhmCore::call_get_listen(const char* call_name, const char* event_type, WhmContext* ctx) {
	conf.gvm->dump_listen(ctx->data());
	return WHM_OK;
}
int WhmCore::call_check_vh_db(const char* call_name, const char* event_type, WhmContext* ctx) {
	if (vhd.check()) {
		ctx->add("status", "1");
	} else {
		ctx->add("status", "0");
	}
	return WHM_OK;
}
int WhmCore::call_list_listen(const char* call_name, const char* event_type, WhmContext* ctx) {
	conf.gvm->GetListenWhm(ctx);
	return WHM_OK;
}
int WhmCore::call_list_vh_template(const char* call_name, const char* event_type, WhmContext* ctx) {
	std::list<KString> templates;
	if (strcmp(call_name, "list_tvh") == 0) {
		conf.gvm->getAllTempleteVh(ctx->getUrlValue()->getx("name"), templates);
	} else {
		conf.gvm->getAllGroupTemplete(templates);
	}
	for (const auto& name : templates) {
		ctx->add("name", name);
	}
	return WHM_OK;
}
int WhmCore::call_query_domain(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto domain = ctx->getUrlValue()->getx("domain");
	if (!domain || !*domain) {
		ctx->setStatus("missing domain");
		return WHM_PARAM_ERROR;
	}
	return conf.gvm->find_domain(domain, ctx);
}
int WhmCore::call_black_list(const char* call_name, const char* event_type, WhmContext* ctx) {
#ifdef ENABLE_BLACK_LIST
	auto uv = ctx->getUrlValue();
	KIpList* ip_list = conf.gvm->vhs.blackList;
	if (uv->getx("vh")) {
		auto vh = ctx->getVh();
		if (!vh) {
			ctx->setStatus("cann't find vh");
			return WHM_PARAM_ERROR;
		}
		ip_list = vh->blackList;
	}
	if (!ip_list) {
		ctx->setStatus("black list not support");
		return WHM_CALL_NOT_FOUND;
	}
	auto action = uv->getx("a");
	if (!action || !*action) {
		ip_list->getBlackList(ctx);
		return WHM_OK;
	}
	if (strcmp(action, "check") == 0) {
		auto ip = uv->getx("ip");
		if (!ip || !*ip) {
			ctx->setStatus("ip param is missing");
			return WHM_PARAM_ERROR;
		}
		ctx->add("hit", ip_list->find(ip, 0, false) ? 1 : 0);
		return WHM_OK;
	}
	if (strcmp(action, "clear") == 0) {
		ip_list->clearBlackList();
	}
	return WHM_OK;
#else
	return WHM_CALL_NOT_FOUND;
#endif
}
int WhmCore::call_stat_vh(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto vh = ctx->getVh();
	if (!vh) {
		ctx->setStatus("vh cann't find");
		return WHM_PARAM_ERROR;
	}
	ctx->add("name", vh->name);
#ifdef ENABLE_VH_RS_LIMIT
	ctx->add("connect", vh->GetConnectionCount());
#ifdef ENABLE_VH_FLOW
	ctx->add("speed", vh->get_speed(ctx->getUrlValue()->get("reset") == "1"));
#endif
#ifdef ENABLE_VH_QUEUE
	if (vh->queue) {
		ctx->add("queue", vh->queue->getQueueSize());
		ctx->add("worker", vh->queue->getWorkerCount());
	}
#endif
#ifdef ENABLE_BLACK_LIST
	if (vh->blackList) {
		INT64 total_error_upstream, total_request, total_upstream;
		vh->blackList->getStat(total_request, total_error_upstream, total_upstream,
			ctx->getUrlValue()->get("reset") == "1");
		ctx->add("total_error_upstream", total_error_upstream);
		ctx->add("total_upstream", total_upstream);
		ctx->add("total_request", total_request);
	}
#endif
#endif
	return WHM_OK;
}
int WhmCore::call_report_ip(const char* call_name, const char* event_type, WhmContext* ctx) {
#if defined(ENABLE_BLACK_LIST) && defined(ENABLE_SIMULATE_HTTP)
	auto ips = ctx->getUrlValue()->getx("ips");
	if (!ips || !*ips) {
		ctx->setStatus("ips param is missing");
		return WHM_PARAM_ERROR;
	}
	add_report_ip(ips);
	return WHM_OK;
#else
	return WHM_CALL_NOT_FOUND;
#endif
}
int WhmCore::call_runtime_model(const char* call_name, const char* event_type, WhmContext* ctx) {
	// Runtime model WHM callbacks were removed together with the runtime-model
	// registry.  Keep a concrete response instead of silently failing dispatch.
	ctx->setStatus("runtime model callbacks are no longer supported");
	return WHM_CALL_NOT_FOUND;
}
int WhmCore::call_server_info(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto name = ctx->getUrlValue()->getx("name");
	if (!name || !*name) {
		ctx->setStatus("name param is missing");
		return WHM_PARAM_ERROR;
	}
	KMultiAcserver* server = server_container->refsMultiServer(name);
	if (server) {
		KStringBuf node;
		server->getNodeInfo(node);
		ctx->add("node", node.str());
		server->release();
	}
	return WHM_OK;
}
int WhmCore::call_port_map(const char* call_name, const char* event_type, WhmContext* ctx) {
#ifdef ENABLE_VH_RUN_AS
	auto vh = ctx->getVh();
	if (!vh) {
		ctx->setStatus("cann't find vh");
		return WHM_PARAM_ERROR;
	}
	auto uv = ctx->getUrlValue();
	ctx->add("port", conf.gam->getCmdPortMap(vh, uv->get("cmd"), uv->get("name"),
		uv->attribute.get_int("app")));
	return WHM_OK;
#else
	return WHM_CALL_NOT_FOUND;
#endif
}
int WhmCore::call_log_drill(const char* call_name, const char* event_type, WhmContext* ctx) {
#ifdef ENABLE_LOG_DRILL
	flush_log_drill();
	return WHM_OK;
#else
	return WHM_CALL_NOT_FOUND;
#endif
}
int WhmCore::call_list_available_named_module(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto access = whm_get_access(ctx);
	if (!access) {
		return WHM_CALL_FAILED;
	}
	return access->dump_available_named_module(ctx, ctx->getUrlValue()->attribute.get_int("type"), false);
}
int WhmCore::call_list_named_module(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto access = whm_get_access(ctx);
	if (!access) {
		return WHM_CALL_FAILED;
	}
	return access->dump_named_module(ctx, !!(ctx->getUrlValue()->attribute.get_int("detail")));
}
int WhmCore::call_get_module(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto uv = ctx->getUrlValue();
	auto access = whm_get_access(ctx);
	if (!access) {
		return WHM_CALL_FAILED;
	}
	bool is_mark = !!uv->attribute.get_int("type");
	return access->get_module(ctx, uv->attribute["module"], is_mark);
}
int WhmCore::call_get_named_module(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto uv = ctx->getUrlValue();
	auto access = whm_get_access(ctx);
	if (!access) {
		return WHM_CALL_FAILED;
	}
	bool is_mark = !!uv->attribute.get_int("type");
	return access->get_named_module(ctx, uv->attribute["name"], is_mark);
}
int WhmCore::call_list_module(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto uv = ctx->getUrlValue();
	const char* access = uv->getx("access");
	if (access == NULL) {
		ctx->setStatus("access must be set");
		return WHM_CALL_FAILED;
	}
	int access_type = 0;
	if (strcasecmp(access, "response") == 0) {
		access_type = RESPONSE;
	} else if (strcasecmp(access, "request") == 0) {
		access_type = REQUEST;
	} else {
		ctx->setStatus("access must be response or request");
		return WHM_CALL_FAILED;
	}
	if (uv->attribute.get_int("type") == 0) {
		auto acl = ctx->data()->add_string_array("module");
		if (acl) {
			for (auto it = KAccess::acl_factorys[access_type].begin(); it != KAccess::acl_factorys[access_type].end(); ++it) {				
				acl->push_back((*it).first);
			}
		}
	} else {
		auto mark = ctx->data()->add_string_array("module");
		if (mark) {
			for (auto it = KAccess::mark_factorys[access_type].begin(); it != KAccess::mark_factorys[access_type].end(); ++it) {
				mark->push_back((*it).first);
			}
		}
	}
	return WHM_OK;
}
int  WhmCore::call_list_sserver(const char* call_name, const char* event_type, WhmContext* ctx) {
	return conf.gam->dump_sserver(ctx);
}
int  WhmCore::call_list_mserver(const char* call_name, const char* event_type, WhmContext* ctx) {
	return conf.gam->dump_mserver(ctx);
}
int  WhmCore::call_list_api(const char* call_name, const char* event_type, WhmContext* ctx) {
	return conf.gam->dump_api(ctx);
}
int  WhmCore::call_list_cmd(const char* call_name, const char* event_type, WhmContext* ctx) {
#ifdef ENABLE_VH_RUN_AS
	return conf.gam->dump_cmd(ctx);
#else
	return WHM_CALL_NOT_FOUND;
#endif
}
int  WhmCore::call_list_dso(const char* call_name, const char* event_type, WhmContext* ctx) {
	return conf.dem->dump(ctx);
}
int WhmCore::call_list_process(const char* call_name, const char* event_type, WhmContext* ctx) {
	spProcessManage.dump(ctx->data());
#ifdef ENABLE_VH_RUN_AS
	conf.gam->dump_process(ctx->data());
#endif
	return WHM_OK;
}
int WhmCore::call_kill_process(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto uv = ctx->getUrlValue();
	if (uv->getx("name")) {
		if (!killProcess(uv->get("name"), uv->get("app"), uv->attribute.get_int("pid"))) {
			return WHM_CALL_FAILED;
		}
		return WHM_OK;
	}
	if (!killProcess(ctx->getVh())) {
		return WHM_CALL_FAILED;
	}
	return WHM_OK;
}
int WhmCore::call_list_connect_per_ip(const char* call_name, const char* event_type, WhmContext* ctx) {
	kangle::dump_connect_per_ip(ctx->data());
	return WHM_OK;
}
int WhmCore::call_del_named_module(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto access = whm_get_access(ctx);
	if (!access) {
		return WHM_CALL_NOT_FOUND;
	}
	auto uv = ctx->getUrlValue();
	auto type = uv->attribute.get_int("type");
	auto name = uv->attribute["name"];
	if (!access->named_module_can_remove(name, type)) {
		ctx->setStatus("named module is used");
		return WHM_CALL_FAILED;
	}
	KStringBuf path;
	auto file = ctx->get_vh_config_file();
	ctx->build_config_base_path(path, file);
	path << access->get_qname() << (type == 0 ? "/named_acl@"_CS : "/named_mark@"_CS) << name;
	if (file.empty()) {
		return config_result(kconfig::remove(path.str().str(), 0), ctx);
	}
	return config_result(kconfig::remove(file.str(), path.str().str(), 0), ctx);
}
int WhmCore::call_edit_named_module(const char* call_name, const char* event_type, WhmContext* ctx) {
	KStringBuf path;
	auto access = whm_get_access(ctx);
	if (!access) {
		return WHM_CALL_FAILED;
	}
	auto uv = ctx->getUrlValue();
	auto type = uv->attribute.get_int("type");
	auto name = uv->attribute["name"];
	auto file = ctx->get_vh_config_file();
	ctx->build_config_base_path(path, file);
	path << access->get_qname() << (type == 0 ? "/named_acl@"_CS : "/named_mark@"_CS) << name;
	auto add = uv->attribute.get_int("add");
	auto it = uv->subs.begin();
	if (it == uv->subs.end()) {
		return WHM_CALL_FAILED;
	}
	khttpd::KSafeXmlNode xml;
	if (strncmp((*it).first.c_str(), _KS("acl_")) == 0) {
		xml = (*it).second->to_xml(_KS("named_acl"),name.c_str(),name.size());
		xml->attributes().emplace("module"_CS, (*it).first.substr(4));
	} else if (strncmp((*it).first.c_str(), _KS("mark_")) == 0) {
		xml = (*it).second->to_xml(_KS("named_mark"), name.c_str(), name.size());
		xml->attributes().emplace("module"_CS, (*it).first.substr(5));
	} else {
		return WHM_CALL_FAILED;
	}
	if (add) {
		if (file.empty()) {
			return config_result(kconfig::update(path.str().str(), 0, xml.get(), kconfig::EvNew), ctx);
		}
		return config_result(kconfig::update(file.str(), path.str().str(), 0, xml.get(), kconfig::EvNew), ctx);
	}
	if (file.empty()) {
		return config_result(kconfig::update(path.str().str(), 0, xml.get(), kconfig::EvUpdate), ctx);
	}
	return config_result(kconfig::update(file.str(), path.str().str(), 0, xml.get(), kconfig::EvUpdate), ctx);
}

int WhmCore::call_config(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto uv = ctx->getUrlValue();
	size_t item = atoi(uv->remove("item").c_str());
	auto sl = ctx->data();
	conf.admin_lock.Lock();
	if (item == 0) {
		sl->add("connect_time_out", conf.connect_time_out);
		sl->add("time_out", conf.time_out);
		sl->add("worker_thread", conf.select_count);
	} else if (item == 1) {
		sl->add("default", conf.default_cache);
		sl->add("memory", get_size(conf.mem_cache));

#ifdef ENABLE_DISK_CACHE

		sl->add("disk", get_size(conf.disk_cache) + (conf.disk_cache_is_radio ? "%" : ""));

		sl->add("disk_dir", conf.disk_cache_dir2);
		sl->add("disk_work_time", conf.disk_work_time);
#endif
		sl->add("max_cache_size", get_size(conf.max_cache_size));
#ifdef ENABLE_DISK_CACHE
		sl->add("max_bigobj_size", get_size(conf.max_bigobj_size));
#ifdef ENABLE_BIG_OBJECT_206
		sl->add("cache_part", conf.cache_part);
#endif
#endif
		sl->add("refresh_time", conf.refresh_time);

	} else if (item == 2) {
		sl->add("access_log", conf.access_log);
		sl->add("rotate_time", conf.log_rotate);
		sl->add("rotate_size", get_size(conf.log_rotate_size));
		sl->add("error_rotate_size", get_size(conf.error_rotate_size));
		sl->add("level", conf.log_level);
		sl->add("logs_day", conf.logs_day);
		sl->add("logs_size", get_size(conf.logs_size));
		sl->add("radio", conf.log_radio);
		sl->add("log_handle", conf.log_handle);
		sl->add("access_log_handle", conf.logHandle);
		sl->add("log_handle_concurrent", conf.maxLogHandle);
	} else if (item == 3) {
		sl->add("max", conf.max);

		sl->add("max_per_ip", conf.max_per_ip);
		sl->add("max_keep_alive", conf.keep_alive_count);
#ifdef ENABLE_BLACK_LIST
		sl->add("per_ip_deny", conf.per_ip_deny);
#endif
		sl->add("min_free_thread", conf.min_free_thread);

#ifdef ENABLE_ADPP
		sl->add("process_cpu_usage", conf.process_cpu_usage);
#endif
		sl->add("worker_dns", conf.worker_dns);
		sl->add("fiber_stack_size", get_size(http_config.fiber_stack_size));
	} else if (item == 4) {
		//data exchange
#ifdef ENABLE_TF_EXCHANGE	
		sl->add("max_post_size", get_size(conf.max_post_size));
#endif
		sl->add("worker_io", conf.worker_io);
		sl->add("max_io", conf.max_io);
		sl->add("io_buffer",get_size(conf.io_buffer));
		sl->add("upstream_sign", conf.upstream_sign);
	} else if (item == 5) {
		sl->add("only_compress_cache", conf.only_compress_cache);

		sl->add("min_compress_length", conf.min_compress_length);
		sl->add("gzip_level", conf.gzip_level);
#ifdef ENABLE_BROTLI
		sl->add("br_level", conf.br_level);
#endif
#ifdef ENABLE_ZSTD
		sl->add("zstd_level", conf.zstd_level);
#endif
#ifdef KANGLE_ENT
		sl->add("server_software", conf.server_software);
#endif
		sl->add("hostname", conf.hostname);
		sl->add("path_info", conf.path_info);
#ifdef KSOCKET_UNIX	
		sl->add("unix_socket", conf.unix_socket);
#endif
#ifdef MALLOCDEBUG
		sl->add("mallocdebug", conf.mallocdebug);
#endif
#ifdef ENABLE_BLACK_LIST
		sl->add("bl_time", conf.bl_time);
		sl->add("wl_time", conf.wl_time);
#endif
#ifdef ENABLE_BLACK_LIST
		/*
		s << "<pre>";
		if (*conf.block_ip_cmd) {
			s << "block cmd:\t" << conf.block_ip_cmd << "\n";
			if (*conf.unblock_ip_cmd) {
				s << "unblock cmd:\t" << conf.unblock_ip_cmd << "\n";
			}
			if (*conf.flush_ip_cmd) {
				s << "flush cmd:\t" << conf.flush_ip_cmd << "\n";
			}
		}
		if (*conf.report_url) {
			s << "report_url:\t" << conf.report_url << "\n";
		}
		s << "</pre>";
		*/
#endif
	} else if (item == 6) {
		sl->add("user", conf.admin_user);
		sl->add("password", "");
		KStringBuf s;
		for (size_t i = 0; i < conf.admin_ips.size(); i++) {
			s << conf.admin_ips[i] << "|";
		}
		sl->add("admin_ips", s.str());
	}
	conf.admin_lock.Unlock();
	return WHM_OK;
}
int WhmCore::call_config_submit(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto uv = ctx->getUrlValue();
	size_t item = atoi(uv->remove("item").c_str());
	KString err_msg;
	if (!console_config_submit(item, *uv, err_msg)) {
		ctx->setStatus(err_msg.c_str());
		return WHM_CALL_FAILED;
	}
	return WHM_OK;
}
