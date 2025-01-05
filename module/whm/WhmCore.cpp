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
int WhmCore::call_add_table(const char* call_name, const char* event_type, WhmContext* ctx) {
	KStringBuf name;
	KStringBuf path;
	KStringBuf file;
	auto uv = ctx->getUrlValue();
	auto vh = ctx->getVh();
	auto vh_name = uv->getx("vh");
	auto table_name = uv->get("name");
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
int WhmCore::call_get_chain(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto access = whm_get_access(ctx);
	if (!access) {
		return WHM_CALL_FAILED;
	}
	return access->get_chain(ctx, ctx->getUrlValue()->get("table"));
}
int WhmCore::call_edit_chain(const char* call_name, const char* event_type, WhmContext* ctx) {
	KStringBuf path;
	auto access = whm_get_access(ctx);
	if (!access) {
		return WHM_CALL_FAILED;
	}
	auto&& uv = ctx->getUrlValue();
	auto file = ctx->get_vh_config_file();
	ctx->build_config_base_path(path, file);
	path << access->get_qname() << "/table@" << uv->get("table") << "/chain"_CS;
	auto id = uv->attribute.get_int("id");
	auto add = uv->attribute.get_int("add");
	if (add) {
		if (file.empty()) {
			return config_result(kconfig::update(path.str().str(), id, KChain::to_xml(*uv).get(), kconfig::EvNew), ctx);
		}
		return config_result(kconfig::update(file.str(), path.str().str(), id, KChain::to_xml(*uv).get(), kconfig::EvNew),ctx);
	}
	return config_result(kconfig::update(file.str(), path.str().str(), id, KChain::to_xml(*uv).get(), kconfig::EvUpdate), ctx);
}
int WhmCore::call_del_chain(const char* call_name, const char* event_type, WhmContext* ctx) {
	KStringBuf path;
	auto uv = ctx->getUrlValue();
	auto file = ctx->get_vh_config_file();
	ctx->build_config_base_path(path, file);
	path << uv->get("access") << "/table@"_CS << uv->get("table") << "/chain";
	return config_result(kconfig::remove(file.str(), path.str().str(), uv->attribute.get_int("id")), ctx);
}
int WhmCore::call_del_table(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto uv = ctx->getUrlValue();
	auto access = whm_get_access(ctx);
	if (!access) {
		return WHM_CALL_NOT_FOUND;
	}
	if (access->is_table_used(uv->get("name"))) {
		ctx->setStatus("table is used");
		return WHM_CALL_FAILED;
	}
	auto vh = ctx->getVh();
	const char* vh_name = uv->getx("vh");
	KStringBuf path;
	auto file = ctx->get_vh_config_file();
	ctx->build_config_base_path(path, file.str());
	path << access->get_qname() << "/table@"_CS << uv->get("name");
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
	return maccess->dump_chain(ctx, ctx->getUrlValue()->get("table"));
}
int WhmCore::call_list_table(const char* call_name, const char* event_type, WhmContext* ctx) {
	KSafeAccess maccess = whm_get_access(ctx);
	if (!maccess) {
		return WHM_CALL_FAILED;
	}
	maccess->listTable(ctx);
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
int WhmCore::call_info(const char* call_name, const char* event_type, WhmContext* ctx) {

	INT64 total_mem_size = 0, total_disk_size = 0;
	int mem_count = 0, disk_count = 0;
	cache.getSize(total_mem_size, total_disk_size, mem_count, disk_count);
	ctx->add("server", PROGRAM_NAME);
	ctx->add("version", VERSION);
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
int WhmCore::call_list_named_module(const char* call_name, const char* event_type, WhmContext* ctx) {
	auto access = whm_get_access(ctx);
	if (!access) {
		return WHM_CALL_FAILED;
	}
	return access->dump_named_module(ctx, !!(ctx->getUrlValue()->attribute.get_int("detail")));
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
	auto acl = ctx->data()->add_string_array("acl");
	if (acl) {
		for (auto it = KAccess::acl_factorys[access_type].begin(); it != KAccess::acl_factorys[access_type].end(); ++it) {
			acl->push_back((*it).first);
		}
	}
	auto mark = ctx->data()->add_string_array("mark");
	if (mark) {
		for (auto it = KAccess::mark_factorys[access_type].begin(); it != KAccess::mark_factorys[access_type].end(); ++it) {
			mark->push_back((*it).first);
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