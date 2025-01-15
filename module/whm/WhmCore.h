#ifndef KGL_MODULE_WHM_CORE_H
#define KGL_MODULE_WHM_CORE_H
#include "WhmExtend.h"
class WhmCore final : public WhmExtend {
public:
	const char* getType() {
		return "core";
	}
	whm_call_ptr parse_call(const KString& call) override {
		if (call.empty()) {
			return nullptr;
		}
		const char* callName = call.c_str();
		switch (*callName) {
		case 'a':
			if (strcmp(callName, "add_table") == 0) {
				return (whm_call_ptr)&WhmCore::call_add_table;
			}
			break;
		case 'b':
			if (strcmp(callName, "black_list") == 0) {
				return nullptr;
			}
			break;
		case 'c':
			if (strcmp(callName, "change_admin_password") == 0) {
				return nullptr;
			}
			if (strcmp(callName, "check_vh_db") == 0) {
				return (whm_call_ptr)&WhmCore::call_check_vh_db;
			}
			if (strcmp(callName, "clean_cache") == 0) {
				return nullptr;
			}
			if (strcmp(callName, "check_ssl") == 0) {
				return nullptr;
			}
			if (strcmp(callName, "cache_info") == 0) {
				return nullptr;
			}
			if (strcmp(callName, "cache_prefetch") == 0) {
				return nullptr;
			}
			if (strcmp(callName, "clean_all_cache") == 0) {
				return nullptr;
			}
			if (strcmp(callName, "connection") == 0) {
				return (whm_call_ptr)&WhmCore::call_connection;
			}
			break;
		case 'd':
			if (strcmp(callName, "del_chain") == 0) {
				return (whm_call_ptr)&WhmCore::call_del_chain;
			}
			if (strcmp(callName, "del_table") == 0) {
				return (whm_call_ptr)&WhmCore::call_del_table;
			}
#ifdef ENABLE_VH_FLOW
			if (strcmp(callName, "dump_flow") == 0) {
				return nullptr;
			}
#endif
			if (strcmp(callName, "dump_load") == 0) {
				return nullptr;
			}
			if (strcmp(callName, "del_named_module") == 0) {
				return (whm_call_ptr)&WhmCore::call_del_named_module;
			}
			break;
		case 'e':
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
				return nullptr;
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
				return nullptr;
			}
			if (strcmp(callName, "info_domain") == 0) {
				return nullptr;
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
				return nullptr;
			}
			if (strcmp(callName, "list_listen") == 0) {
				return nullptr;
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
				return nullptr;
			}
#endif
			break;
		case 'q':
			if (strcmp(callName, "query_domain") == 0) {
				return nullptr;
			}
			break;
		case 'r':
			if (strcmp(callName, "reload_vh") == 0) {
				return nullptr;
			}
			if (strcmp(callName, "reload_vh_access") == 0) {
				return nullptr;
			}
			if (strcmp(callName, "reboot") == 0) {
				return (whm_call_ptr)&WhmCore::call_reboot;
			}
			if (strcmp(callName, "reload") == 0) {
				return nullptr;
			}
			if (strcmp(callName, "report_ip") == 0) {
				return nullptr;
			}
			break;
		case 's':
			if (strcmp(callName, "stat_vh") == 0) {
				return nullptr;
			}
			if (strcmp(callName, "server_info") == 0) {
				return nullptr;
			}
			break;
		case 'u':
			if (strcmp(callName, "update_vh") == 0) {
				return nullptr;
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
	int call_get_connection(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_connection(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_get_config_listen(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_get_listen(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_check_vh_db(const char* call_name, const char* event_type, WhmContext* ctx);
	int call_list_connect_per_ip(const char* call_name, const char* event_type, WhmContext* ctx);
};
#endif
