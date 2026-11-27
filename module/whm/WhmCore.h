#ifndef KGL_MODULE_WHM_CORE_H
#define KGL_MODULE_WHM_CORE_H
#include "WhmExtend.h"
class WhmCore final : public WhmExtend {
public:
	const char* getType() override {
		return "core";
	}
	whm_call_ptr parse_call(const KString& call) override {
		if (call.empty()) {
			return nullptr;
		}
		const char* callName = call.c_str();
		switch (*callName) {
		case 'a':
			if (strcmp(callName, "add_chain") == 0) {
				return (whm_call_ptr)&WhmCore::call_edit_chain;
			}
			if (strcmp(callName, "add_table") == 0) {
				return (whm_call_ptr)&WhmCore::call_add_table;
			}
			break;
		case 'b':
			if (strcmp(callName, "black_list") == 0) {
				return (whm_call_ptr)&WhmCore::call_black_list;
			}
			break;
		case 'c':
			if (strcmp(callName, "change_admin_password") == 0) {
				return (whm_call_ptr)&WhmCore::call_change_admin_password;
			}
			if (strcmp(callName, "check_vh_db") == 0) {
				return (whm_call_ptr)&WhmCore::call_check_vh_db;
			}
			if (strcmp(callName, "clean_cache") == 0) {
				return (whm_call_ptr)&WhmCore::call_clean_cache;
			}
			if (strcmp(callName, "check_ssl") == 0) {
				return (whm_call_ptr)&WhmCore::call_check_ssl;
			}
			if (strcmp(callName, "cache_info") == 0) {
				return (whm_call_ptr)&WhmCore::call_cache_info;
			}
			if (strcmp(callName, "cache_prefetch") == 0) {
				return (whm_call_ptr)&WhmCore::call_cache_prefetch;
			}
			if (strcmp(callName, "clean_all_cache") == 0) {
				return (whm_call_ptr)&WhmCore::call_clean_all_cache;
			}
			if (strcmp(callName, "connection") == 0) {
				return (whm_call_ptr)&WhmCore::call_connection;
			}
			if (strcmp(callName, "config") == 0) {
				return (whm_call_ptr)&WhmCore::call_config;
			}
			if (strcmp(callName, "config_submit") == 0) {
				return (whm_call_ptr)&WhmCore::call_config_submit;
			}
			break;
		case 'd':
			if (strcmp(callName, "del_vh") == 0) {
				return (whm_call_ptr)&WhmCore::call_reload_vh;
			}
			if (strcmp(callName, "del_chain") == 0) {
				return (whm_call_ptr)&WhmCore::call_del_chain;
			}
			if (strcmp(callName, "del_table") == 0) {
				return (whm_call_ptr)&WhmCore::call_del_table;
			}
			if (strcmp(callName, "dump_load") == 0) {
				return (whm_call_ptr)&WhmCore::call_dump_load;
			}
#ifdef ENABLE_VH_FLOW
			if (strcmp(callName, "dump_flow") == 0) {
				return (whm_call_ptr)&WhmCore::call_dump_flow;
			}
#endif
			if (strcmp(callName, "del_named_module") == 0) {
				return (whm_call_ptr)&WhmCore::call_del_named_module;
			}
			break;
		case 'e':
			if (strcmp(callName, "empty_table") == 0) {
				return (whm_call_ptr)&WhmCore::call_empty_table;
			}
			if (strcmp(callName, "edit_chain") == 0) {
				return (whm_call_ptr)&WhmCore::call_edit_chain;
			}
			if (strcmp(callName, "edit_named_module") == 0) {
				return (whm_call_ptr)&WhmCore::call_edit_named_module;
			}
			break;
		case 'g':
#ifdef ENABLE_VH_FLOW
			if (strcmp(callName, "get_load") == 0) {
				return (whm_call_ptr)&WhmCore::call_get_load;
			}
			if (strcmp(callName, "get_connection") == 0) {
				return (whm_call_ptr)&WhmCore::call_get_connection;
			}
#endif
			if (strcmp(callName, "get_chain") == 0) {
				return (whm_call_ptr)&WhmCore::call_get_chain;
			}
			if (strcmp(callName, "get_config_listen") == 0) {
				return (whm_call_ptr)&WhmCore::call_get_config_listen;
			}
			if (strcmp(callName, "get_listen") == 0) {
				return (whm_call_ptr)&WhmCore::call_get_listen;
			}
			if (strcmp(callName, "get_vh") == 0) {
				return (whm_call_ptr)&WhmCore::call_get_vh;
			}
			if (strcmp(callName, "get_named_module") == 0) {
				return (whm_call_ptr)&WhmCore::call_get_named_module;
			}
			if (strcmp(callName, "get_module") == 0) {
				return (whm_call_ptr)&WhmCore::call_get_module;
			}
			break;
		case 'i':
			if (strcmp(callName, "info") == 0) {
				return (whm_call_ptr)&WhmCore::call_info;
			}
			if (strcmp(callName, "info_vh") == 0) {
				return (whm_call_ptr)&WhmCore::call_info_vh;
			}
			if (strcmp(callName, "info_domain") == 0) {
				return (whm_call_ptr)&WhmCore::call_info_domain;
			}
			break;
		case 'k':
			if (strcmp(callName, "kill_process") == 0) {
				return (whm_call_ptr)&WhmCore::call_kill_process;
			}
			break;
		case 'l':
			if (strcmp(callName, "list_vh") == 0) {
				return (whm_call_ptr)&WhmCore::call_list_vh;
			}
			if (strcmp(callName, "list_table") == 0) {
				return (whm_call_ptr)&WhmCore::call_list_table;
			}
			if (strcmp(callName, "list_chain") == 0) {
				return (whm_call_ptr)&WhmCore::call_list_chain;
			}
			if (strcmp(callName, "list_module") == 0) {
				return (whm_call_ptr)&WhmCore::call_list_module;
			}
			if (strcmp(callName, "list_named_module") == 0) {
				return (whm_call_ptr)&WhmCore::call_list_named_module;
			}
			if (strcmp(callName, "list_available_named_module") == 0) {
				return (whm_call_ptr)&WhmCore::call_list_available_named_module;
			}
			if (strcmp(callName, "list_index") == 0) {
				return (whm_call_ptr)&WhmCore::call_list_index;
			}
			if (strcmp(callName, "list_listen") == 0) {
				return (whm_call_ptr)&WhmCore::call_list_listen;
			}
			if (strcmp(callName, "list_gtvh") == 0 || strcmp(callName, "list_tvh") == 0) {
				return (whm_call_ptr)&WhmCore::call_list_vh_template;
			}
			if (strcmp(callName, "list_api") == 0) {
				return (whm_call_ptr)&WhmCore::call_list_api;
			}
			if (strcmp(callName, "list_cmd") == 0) {
				return (whm_call_ptr)&WhmCore::call_list_cmd;
			}
			if (strcmp(callName, "list_dso") == 0) {
				return (whm_call_ptr)&WhmCore::call_list_dso;
			}
			if (strcmp(callName, "list_sserver") == 0) {
				return (whm_call_ptr)&WhmCore::call_list_sserver;
			}
			if (strcmp(callName, "list_mserver") == 0) {
				return (whm_call_ptr)&WhmCore::call_list_mserver;
			}
			if (strcmp(callName, "list_process") == 0) {
				return (whm_call_ptr)&WhmCore::call_list_process;
			}
			if (strcmp(callName, "list_connect_per_ip") == 0) {
				return (whm_call_ptr)&WhmCore::call_list_connect_per_ip;
			}
#ifdef ENABLE_LOG_DRILL
			if (strcmp(callName, "log_drill") == 0) {
				return (whm_call_ptr)&WhmCore::call_log_drill;
			}
#endif
			break;
		case 'q':
			if (strcmp(callName, "query_domain") == 0) {
				return (whm_call_ptr)&WhmCore::call_query_domain;
			}
			break;
		case 'p':
			if (strcmp(callName, "port_map") == 0) {
				return (whm_call_ptr)&WhmCore::call_port_map;
			}
			break;
		case 'r':
			if (strcmp(callName, "reload_all_vh") == 0) {
				return (whm_call_ptr)&WhmCore::call_reload_vh;
			}
			if (strcmp(callName, "reload_vh") == 0) {
				return (whm_call_ptr)&WhmCore::call_reload_vh;
			}
			if (strcmp(callName, "reload_vh_access") == 0) {
				return (whm_call_ptr)&WhmCore::call_reload_vh_access;
			}
			if (strcmp(callName, "reboot") == 0) {
				return (whm_call_ptr)&WhmCore::call_reboot;
			}
			if (strcmp(callName, "reload") == 0) {
				return (whm_call_ptr)&WhmCore::call_reload;
			}
			if (strcmp(callName, "report_ip") == 0) {
				return (whm_call_ptr)&WhmCore::call_report_ip;
			}
			if (strcmp(callName, "runtime_model") == 0) {
				return (whm_call_ptr)&WhmCore::call_runtime_model;
			}
			break;
		case 's':
			if (strcmp(callName, "stat_vh") == 0) {
				return (whm_call_ptr)&WhmCore::call_stat_vh;
			}
			if (strcmp(callName, "server_info") == 0) {
				return (whm_call_ptr)&WhmCore::call_server_info;
			}
			break;
		case 'u':
			if (strcmp(callName, "update_vh") == 0) {
				return (whm_call_ptr)&WhmCore::call_reload_vh;
			}
			break;
		case 'v':
			if (strcmp(callName, "vh_action") == 0) {
				return (whm_call_ptr)&WhmCore::call_vh_action;
			}
		}
		return nullptr;
	}
protected:
	int call_info(const char* call_name, const char* event_type, WhmContext* ctx);
	/* table */
	int call_list_table(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_add_table(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_empty_table(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_del_table(const char* call_name, const char* event_type, WhmContext* ctx);
	/* chain */
	int call_list_chain(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_del_chain(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_get_chain(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_edit_chain(const char* call_name, const char* event_type, WhmContext* ctx);
	/* module */
	int call_list_module(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_list_named_module(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_list_available_named_module(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_get_named_module(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_get_module(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_del_named_module(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_edit_named_module(const char* call_name, const char* event_type, WhmContext* ctx);
	/* vh */
	int call_list_vh(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_get_vh(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_vh_action(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_reload_vh(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_info_vh(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_info_domain(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_list_index(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_list_vh_template(const char* call_name, const char* event_type, WhmContext* ctx);
	/* extend */
	int call_list_sserver(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_list_mserver(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_list_api(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_list_cmd(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_list_dso(const char* call_name, const char* event_type, WhmContext* ctx);
	/* process */
	int call_list_process(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_kill_process(const char* call_name, const char* event_type, WhmContext* ctx);
	/* system */
	int call_reboot(const char* call_name, const char* event_type, WhmContext* ctx) {
		console_call_reboot();
		return WHM_OK;
	}
	int call_config(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_config_submit(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_get_connection(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_connection(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_get_config_listen(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_get_listen(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_check_vh_db(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_clean_cache(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_cache_info(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_cache_prefetch(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_clean_all_cache(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_change_admin_password(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_check_ssl(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_dump_flow(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_dump_load(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_get_load(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_list_listen(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_query_domain(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_reload(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_reload_vh_access(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_black_list(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_stat_vh(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_report_ip(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_runtime_model(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_server_info(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_port_map(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_log_drill(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_list_connect_per_ip(const char* call_name, const char* event_type, WhmContext* ctx);
};
#endif
