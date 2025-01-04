/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#include <string.h>
#include <stdlib.h>
#include <vector>
#include "ksocket.h"
#include "KAcserver.h"
#include "KAcserverManager.h"
#include "KSingleAcserver.h"
#include "KAccess.h"
#include "utils.h"
#include <sstream>
#include "lang.h"
#include <stdlib.h>
#include <stdio.h>
#include "kmalloc.h"
#include "KCdnContainer.h"
#include "kselector_manager.h"
#include "KDsoExtendManage.h"
#include "KDefer.h"
#include "KProcessManage.h"

using namespace std;
void on_api_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	KAcserverManager* am = (KAcserverManager*)data;
	am->on_api_event(tree, ev->get_xml(), ev->type);
}
#ifdef ENABLE_VH_RUN_AS
void on_cmd_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	KAcserverManager* am = (KAcserverManager*)data;
	am->on_cmd_event(tree, ev->get_xml(), ev->type);
}
#endif
void on_server_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	KAcserverManager* am = (KAcserverManager*)data;
	am->on_server_event(tree, ev->get_xml(), ev->type);
}
int KAcserverManager::dump_sserver(WhmContext* ctx) {
	auto lock = get_rlocker();
	for (auto it = acservers.begin(); it != acservers.end(); ++it) {
		auto sl = ctx->data()->add_obj_array("sserver");
		if (!sl) {
			continue;
		}
		(*it).second->dump(sl);
	}
	return WHM_OK;
}
int KAcserverManager::dump_mserver(WhmContext* ctx) {
	auto lock = get_rlocker();
	for (auto it = mservers.begin(); it != mservers.end(); ++it) {
		auto sl = ctx->data()->add_obj_array("mserver");
		if (!sl) {
			continue;
		}
		(*it).second->dump(sl);
	}
	return WHM_OK;
}
int KAcserverManager::dump_api(WhmContext* ctx) {
	auto lock = get_rlocker();
	for (auto&& api : apis) {
		auto sl = ctx->data()->add_obj_array("api");
		if (!sl) {
			continue;
		}
		api.second->dump(sl);
	}
	return WHM_OK;
}
#ifdef ENABLE_VH_RUN_AS
int KAcserverManager::dump_cmd(WhmContext* ctx) {
	auto lock = get_rlocker();
	for (auto&& cmd : cmds) {
		auto sl = ctx->data()->add_obj_array("cmd");
		if (!sl) {
			continue;
		}
		cmd.second->dump(sl);
	}
	return WHM_OK;
}
void KAcserverManager::on_cmd_event(kconfig::KConfigTree* tree, const khttpd::KXmlNode* xml, kconfig::KConfigEventType ev) {
	auto& attr = xml->attributes();
	auto name = attr["name"];
	switch (ev) {
	case kconfig::EvNew | kconfig::EvSubDir: {
		auto cmd = new KCmdPoolableRedirect(name);
		try {
			cmd->parse_config(xml);
			auto locker = get_wlocker();
			auto result = cmds.insert(std::pair<KString, KCmdPoolableRedirect*>(name, cmd));
			if (!result.second) {
				throw 2;
			}
		} catch (...) {
			cmd->release();
			klog(KLOG_ERR, "cann't new cmd [%s]\n", name.c_str());
		}
		break;
	}
	case kconfig::EvUpdate | kconfig::EvSubDir:
	{
		auto locker = get_wlocker();
		auto it = cmds.find(name);
		if (it == cmds.end()) {
			klog(KLOG_ERR, "not found cmd [%s]\n", name.c_str());
			return;
		}
		if (!(*it).second->parse_config(xml)) {
			klog(KLOG_ERR, "cann't parse cmd [%s] config.\n", name.c_str());
		}
		(*it).second->shutdown();
		break;
	}
	case kconfig::EvRemove | kconfig::EvSubDir:
	{
		auto locker = get_wlocker();
		auto it = cmds.find(name);
		if (it == cmds.end()) {
			klog(KLOG_ERR, "not found cmd [%s]\n", name.c_str());
			return;
		}
		(*it).second->release();
		cmds.erase(it);
		break;
	}
	default:
		break;
	}
}
#endif
void KAcserverManager::on_api_event(kconfig::KConfigTree* tree, const khttpd::KXmlNode* xml, kconfig::KConfigEventType ev) {
	auto& attr = xml->attributes();
	auto name = attr["name"];
	switch (ev) {
	case kconfig::EvNew | kconfig::EvSubDir:
	{
		auto api = new KApiRedirect(name);
		try {
			if (!api->parse_config(xml)) {
				throw 1;
			}
			auto locker = get_wlocker();
			auto result = apis.insert(std::pair<KString, KApiRedirect*>(name, api));
			if (!result.second) {
				throw 2;
			}
		} catch (...) {
			api->release();
			klog(KLOG_ERR, "cann't new api [%s]\n", name.c_str());
		}
		break;
	}
	case kconfig::EvUpdate | kconfig::EvSubDir:
	{
		auto locker = get_wlocker();
		auto it = apis.find(name);
		if (it == apis.end()) {
			klog(KLOG_ERR, "not found api [%s]\n", name.c_str());
			return;
		}
		if (!(*it).second->parse_config(xml)) {
			klog(KLOG_ERR, "cann't parse api [%s] config.\n", name.c_str());
		}
		(*it).second->unload();
		break;
	}
	case kconfig::EvRemove | kconfig::EvSubDir:
	{
		auto locker = get_wlocker();
		auto it = apis.find(name);
		if (it == apis.end()) {
			klog(KLOG_ERR, "not found api [%s]\n", name.c_str());
			return;
		}
		(*it).second->release();
		apis.erase(it);
		break;
	}
	}
}
void KAcserverManager::on_server_event(kconfig::KConfigTree* tree, const khttpd::KXmlNode* xml, kconfig::KConfigEventType ev) {
	if (!xml->is_tag(_KS("server"))) {
		return;
	}
	auto name = xml->attributes()["name"];
	switch (ev) {
	case kconfig::EvNew | kconfig::EvSubDir:
	{
		auto host = xml->attributes().get_string("host", nullptr);
		if (!host) {
			//multi acserver
			KMultiAcserver* server = new KMultiAcserver(name);
			if (!server->parse_config(xml)) {
				klog(KLOG_ERR, "cann't parse server [%s] config\n", name.c_str());
				server->release();
				return;
			}
			{
				auto locker = get_wlocker();
				auto it = mservers.find(name);
				if (it != mservers.end()) {
					server->release();
					server = (*it).second;
				} else {
					mservers.emplace(name, server);
				}
				server->add_ref();
			}
			if (!tree->bind(server)) {
				server->release();
				assert(false);
			}
		} else {
			auto server = new KSingleAcserver(name);
			if (!server->parse_config(xml)) {
				klog(KLOG_ERR, "cann't parse server [%s] config\n", name.c_str());
				server->release();
				return;
			}
			auto locker = get_wlocker();
			auto it = acservers.find(name);
			if (it != acservers.end()) {
				server->release();
			} else {
				acservers.emplace(name, server);
			}
		}
		break;
	}
	case kconfig::EvUpdate | kconfig::EvSubDir:
		if (tree->ls) {
			//multi server
			KMultiAcserver* mserver = static_cast<KMultiAcserver*>(tree->ls);
			if (!mserver->parse_config(xml)) {
				klog(KLOG_ERR, "cann't parse server [%s] config\n", mserver->name.c_str());
				return;
			}
			return;
		} else {
			KSingleAcserver* sa = refsSingleAcserver(name);
			if (!sa) {
				klog(KLOG_ERR, "cann't find server [%s]\n", name.c_str());
				return;
			}
			if (!sa->parse_config(xml)) {
				klog(KLOG_ERR, "cann't parse server [%s] config\n", sa->name.c_str());
				sa->release();
				return;
			}
			sa->release();
		}
		break;
	case kconfig::EvRemove | kconfig::EvSubDir:
	{
		auto ls = tree->unbind();
		if (ls != nullptr) {
			KMultiAcserver* mserver = static_cast<KMultiAcserver*>(ls);
			defer(mserver->release());
			assert(mserver->name == name);
			auto locker = get_wlocker();
			auto it = mservers.find(mserver->name);
			if (it != mservers.end()) {
				mservers.erase(it);
				mserver->release();
			}
		} else {
			auto locker = get_wlocker();
			auto it = acservers.find(name);
			if (it != acservers.end()) {
				(*it).second->release();
				acservers.erase(it);
			}
		}
		break;
	}
	default:
		break;
	}
	return;
}

KAcserverManager::KAcserverManager() {}
void KAcserverManager::shutdown() {
	killAllProcess(NULL);
	lock.WLock();
	std::map<KString, KSingleAcserver*>::iterator it;
	for (it = acservers.begin(); it != acservers.end(); it++) {
		(*it).second->remove();
	}
	acservers.clear();
#ifdef ENABLE_MULTI_SERVER
	std::map<KString, KMultiAcserver*>::iterator it2;
	for (it2 = mservers.begin(); it2 != mservers.end(); it2++) {
		(*it2).second->remove();
	}
	mservers.clear();
#endif
#ifdef ENABLE_VH_RUN_AS
	std::map<KString, KCmdPoolableRedirect*>::iterator it3;
	for (it3 = cmds.begin(); it3 != cmds.end(); it3++) {
		(*it3).second->remove();
	}
	cmds.clear();
#endif
	std::map<KString, KApiRedirect*>::iterator it4;
	for (it4 = apis.begin(); it4 != apis.end(); it4++) {
		(*it4).second->release();
	}
	apis.clear();
	lock.WUnlock();
}
KAcserverManager::~KAcserverManager() {
	shutdown();
}
void KAcserverManager::killAllProcess(KVirtualHost* vh) {
	spProcessManage.killAllProcess(vh);
}
#ifdef ENABLE_VH_RUN_AS
void KAcserverManager::refreshCmd(time_t nowTime) {

	lock.RLock();
	std::map<KString, KCmdPoolableRedirect*>::iterator it;
	for (it = cmds.begin(); it != cmds.end(); it++) {
		KProcessManage* pm = (*it).second->getProcessManage();
		if (pm) {
			pm->refresh(nowTime);
		}
	}
	lock.RUnlock();

}

void KAcserverManager::killCmdProcess(USER_T user) {

	spProcessManage.killProcess2(user, 0);
	lock.RLock();
	std::map<KString, KCmdPoolableRedirect*>::iterator it;
	for (it = cmds.begin(); it != cmds.end(); it++) {
		KProcessManage* pm = (*it).second->getProcessManage();
		if (pm) {
			pm->killProcess2(user, 0);
		}
	}
	lock.RUnlock();

}
//{{ent
#ifdef ENABLE_ADPP
void KAcserverManager::flushCpuUsage(ULONG64 cpuTime) {

	lock.RLock();
	std::map<KString, KCmdPoolableRedirect*>::iterator it;
	for (it = cmds.begin(); it != cmds.end(); it++) {
		KProcessManage* pm = (*it).second->getProcessManage();
		if (pm) {
			pm->flushCpuUsage(cpuTime);
		}
	}
	lock.RUnlock();

}
#endif
#if 0
int KAcserverManager::getCmdPortMap(KVirtualHost* vh, KString cmd, KString name, int app) {
	KCmdPoolableRedirect* rd = refsCmdRedirect(cmd);
	if (rd == NULL) {
		return -1;
	}
	KProcessManage* pm = rd->getProcessManage();
	int port = -1;
	if (pm) {
		port = pm->getPortMap(vh, rd, name, app);
	}
	rd->release();
	return port;
}
#endif
//}}
void KAcserverManager::getProcessInfo(KWStream& s) {
	lock.RLock();
	std::map<KString, KCmdPoolableRedirect*>::iterator it;
	for (it = cmds.begin(); it != cmds.end(); it++) {
		KProcessManage* pm = (*it).second->getProcessManage();
		if (pm) {
			pm->getProcessInfo(s);
		}
	}
	lock.RUnlock();
}
#endif
void KAcserverManager::clearImportConfig() {

}
#ifdef ENABLE_MULTI_SERVER
void KAcserverManager::macserverList(KWStream& s, const KString& name) {
	KMultiAcserver* m_a = NULL;
	std::map<KString, KMultiAcserver*>::iterator it;
	lock.RLock();
	s << "<table border=1><tr>"
		<< "<td>" << LANG_OPERATOR << "</td>"
		<< "<td>" << LANG_NAME << "</td>"
		<< "<td>" << klang["server_type"] << "</td>"
		<< "<td>hash</td>"
		<< "<td>" << klang["cookie_stick"] << "</td>"
		<< "<td>" << klang["error_try_time"] << "</td>"
		<< "<td>" << klang["error_count"] << "</td>"
		<< "<td>" << LANG_REFS << "</td>"
		//{{ent
#ifdef ENABLE_MSERVER_ICP
		<< "<td>icp</td>"
#endif//}}
		<< "<td>" << klang["lang_host"] << "</td>"
		<< "<td>" << klang["LANG_PORT"] << "</td>"
		<< "<td>" << klang["lang_life_time"] << "</td>"
		<< "<td>" << klang["lang_sock_pool_size"] << "</td>"
		<< "<td>" << klang["weight"] << "</td>"
		<< "<td>self</td>"
		<< "<td>up/hit</td>"
		<< "<td>err/avg</td>"
		<< "<td>" << klang["status"] << "</td>"
		<< "</tr>";

	for (it = mservers.begin(); it != mservers.end(); it++) {
		(*it).second->getHtml(s);
		if ((*it).first == name) {
			m_a = (*it).second;
		}
	}
	s << "</table>";
	KMultiAcserver::form(m_a, s);
	lock.RUnlock();
}
#endif
void KAcserverManager::apiList(KWStream& s, const KString& name) {
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>";
	s << LANG_NAME << "</td><td>" << klang["file"] << "</td><td>";
	s << klang["seperate_process"] << "</td><td>";
	s << LANG_REFS << "</td><td>" << LANG_STATE << "</td><td>version</td></tr>";
	lock.RLock();
	KApiRedirect* m_a = NULL;
	for (auto it = apis.begin(); it != apis.end(); it++) {
		m_a = (*it).second;
		s << "<tr bgcolor='white'><td>";
		s << "[<a href=\"javascript:if(confirm('really delete?')){ window.location='/delapi?name=";
		s << m_a->name << "';}\">" << LANG_DELETE << "</a>]";
		s << "[<a href='/apilist?action=edit&name=" << m_a->name << "'>" << LANG_EDIT << "</a>]";

		s << "</td>";
		s << "<td>" << m_a->name << "</td>";
		s << "<td>" << m_a->apiFile << "</td>";
		s << "<td>" << (m_a->type == WORK_TYPE_SP ? LANG_ON : LANG_OFF)
			<< "</td>";
		s << "<td>" << m_a->get_ref() << "</td>";
		s << "<td>" << klang[m_a->dso.getStateString()] << "</td>";
		s << "<td>" << m_a->dso.getInfo() << "</td>";
		s << "</tr>";
	}
	if (name.size() > 0) {
		m_a = getApiRedirect(name);
	} else {
		m_a = NULL;
	}
	s << "</table><br>";
	s << "<form action='/apiform?action=" << (m_a ? "edit" : "add")
		<< "' method='post'>";
	s << LANG_NAME << ": <input name='name' value='" << (m_a ? m_a->name : "")
		<< "' " << (m_a ? "readonly" : "") << "><br>\n";
	s << klang["file"] << ": <input name='file' size='80' value='" << (m_a ? m_a->apiFile
		: "") << "'><br>\n";
	if (m_a && m_a->type == WORK_TYPE_MT) {
		s << "<input type=hidden name='mt' value='1'>";
	}
	s << "<input type='submit' value='" << LANG_SUBMIT << "'>";
	s << "</form>";
	lock.RUnlock();
}
#ifdef ENABLE_VH_RUN_AS
bool KAcserverManager::cmdForm(KXmlAttribute& attribute,
	KString& errMsg) {
	auto action = std::move(attribute.remove("action"));
	bool is_update = action == "edit";
	return new_item("cmd", is_update, attribute, errMsg);
}
#endif
bool KAcserverManager::apiForm(KXmlAttribute& attribute,
	KString& errMsg) {
	auto action = std::move(attribute.remove("action"));
	bool is_update = action == "edit";
	return new_item("api", is_update, attribute, errMsg);
}
#ifdef ENABLE_VH_RUN_AS
void KAcserverManager::cmdList(KWStream& s, const KString& name) {
	s << "<script language='javascript'>"
		"function $(id) \n"
		"{ \n"
		"if (document.getElementById) \n"
		"	return document.getElementById(id); "
		"else if (document.all)\n"
		"	return document.all(id);"
		"return document.layers[id];"
		"}\n"
		"function show_div(div_name,flag)"
		"{"
		"var el=$(div_name);"
		"if(flag)\n"
		"el.style.display='';\n"
		"else\n"
		"el.style.display='none';"
		"}"
		"function switch_mp(){ show_div('sp',false);show_div('mp',true);}"
		"function switch_sp(){ show_div('mp',false);show_div('sp',true);}"
		"</script>\n";
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>";
	s << LANG_NAME << "</td><td>" << klang["file"] << "</td><td>" << klang["type"]
		<< "</td><td>" << klang["protocol"] << "</td>"
		<< "<td>" << LANG_PORT << "</td>"
		<< "<td>" << LANG_REFS
		<< "</td><td>" << klang["env"] << "</td></tr>";
	lock.RLock();
	KCmdPoolableRedirect* m_a = NULL;

	for (auto it = cmds.begin(); it != cmds.end(); it++) {
		m_a = (*it).second;
		string de;
		string color = "white";

		s << "<tr bgcolor='" << color
			<< "'><td>";

		s << "[<a href=\"javascript:if(confirm('really delete?')){ window.location='/delcmd?name=";
		s << m_a->name << "';}\">" << LANG_DELETE
			<< "</a>]";
		s << "[<a href='/extends?item=3&action=edit&name=" << m_a->name << "'>"
			<< LANG_EDIT << "</a>]";

		s << "</td>";
		s << "<td>" << m_a->name << "</td>";
		s << "<td><div title='" << m_a->cmd << "'>" << m_a->cmd.substr(0, 60) << "</div></td>";
		s << "<td>" << (m_a->type == WORK_TYPE_MP ? klang["mp"] : klang["sp"]);
		if (m_a->type == WORK_TYPE_MP) {
			s << "*" << m_a->worker;
		}
		s << "</td>";
		s << "<td>" << m_a->buildProto(m_a->get_proto()) << "</td>";
		s << "<td>" << m_a->port << "</td>";
		s << "<td>" << m_a->get_ref() << "</td>";
		s << "<td>" << m_a->getEnv() << "</td>";
		s << "</tr>";
	}
	if (name.size() > 0) {
		m_a = getCmdRedirect(name);
	} else {
		m_a = NULL;
	}
	s << "</table><br>";
	s << "<form action='/cmdform?action=" << (m_a ? "edit" : "add")
		<< "' method='post'>";
	s << LANG_NAME << ": <input name='name' value='" << (m_a ? m_a->name : "")
		<< "' " << (m_a ? "readonly" : "") << "><br>\n";
	s << klang["file"] << ": <input name='file' value='"
		<< (m_a ? m_a->cmd.c_str() : "") << "' size='80'><br>\n";
	s << klang["env"] << ":<input name='env' value='" << (m_a ? m_a->getEnv()
		: "") << "' size='80'><br>";
	//type
	const char* mp_display = "display:";
	const char* sp_display = "display:";
	s << klang["type"] << ":<input name='type' value='mp'  onClick='switch_mp()'  type='radio' ";
	if (m_a == NULL || m_a->type == WORK_TYPE_MP) {
		s << "checked";
		sp_display = "display:none";
	}
	s << ">" << klang["mp"];
	s << "<input name='type' value='sp' onClick='switch_sp()' type='radio' ";
	if (m_a && m_a->type == WORK_TYPE_SP) {
		s << "checked";
		mp_display = "display:none";
	}
	s << ">" << klang["sp"];
	s << "<br>";
	s << "<div id='mp' style='" << mp_display << "'>" << klang["process_count"] << "<input name='worker' value='" << (m_a ? m_a->worker : 4) << "' size=4>";
	s << klang["error_count"] << "<input name='max_error_count' value='" << (m_a ? m_a->max_error_count : 0) << "' size=4></div>";
	s << "<div id='sp' style='" << sp_display << "'>" << LANG_PORT << "<input name='port' value='" << (m_a ? m_a->port : 0) << "' size='6'>";
#ifndef _WIN32
	s << klang["kill_sig"] << "<input name='sig' value='" << (m_a ? m_a->sig : SIGKILL) << "' size='4'>";
#endif
	s << "</div>";
	//proto
	s << klang["protocol"] << ":";
	s << "<input type='radio' name='proto' value='http' ";
	if (m_a && m_a->get_proto() == Proto_http) {
		s << "checked";
	}
	s << ">http";


	s << "<input type='radio' name='proto' value='fastcgi' ";
	if (m_a && m_a->get_proto() == Proto_fcgi) {
		s << "checked";
	}
	s << ">fastcgi";

	s << "<input type='radio' name='proto' value='ajp' ";
	if (m_a && m_a->get_proto() == Proto_ajp) {
		s << "checked";
	}
	s << ">ajp";
	//{{ent
#ifdef WORK_MODEL_TCP
	//tcp
	s << "<input type='radio' name='proto' value='tcp' ";
	if (m_a && m_a->get_proto() == Proto_tcp) {
		s << "checked";
	}
	s << ">tcp";
#endif//}}
	s << "<br>";
	s << klang["lang_life_time"] << "<input name='life_time' size=5 value=" << (m_a ? m_a->life_time : 0) << ">";
	s << klang["lang_life_time_notice"] << "<br>";
	s << klang["idle_time"] << "<input name='idle_time' size=5 value='" << (m_a ? m_a->idleTime : 120) << "'><br>";
	s << "<input type='checkbox' name='chuser' value='0' ";
	if (m_a && !m_a->chuser) {
		s << "checked";
	}
	s << ">" << klang["run_as_system"] << "<br>";
	s << "param:<input name='param' value='";
	if (m_a) {
		s << m_a->getProcessManage()->param;
	}
	s << "'><br>";
	//submit
	s << "<input type='submit' value='" << LANG_SUBMIT << "'>";
	s << "</form>";
	lock.RUnlock();
}
#endif
void KAcserverManager::acserverList(KWStream& s, const KString& name) {
	KSingleAcserver* m_a = NULL;
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>"
		<< LANG_NAME << "</td><td>" << klang["server_type"] << "</td><td>"
		<< LANG_REFS << "</td><td>" << LANG_IP << "</td><td>" << LANG_PORT << "</td>"
		<< "<td>" << klang["lang_life_time"] << "</td>"
		<< "<td>" << klang["lang_sock_pool_size"] << "</td>"
		<< "</tr>\n";
	lock.RLock();
	for (auto it = acservers.begin(); it != acservers.end(); it++) {
		m_a = (*it).second;
		s << "<tr><td>";
		s << "[<a href=\"javascript:if(confirm('" << LANG_CONFIRM_DELETE
			<< m_a->name;
		s << "')){ window.location='/acserverdelete?name=" << m_a->name
			<< "';}\">" << LANG_DELETE << "</a>]";
		s << "[<a href=\"/acserver?name=" << m_a->name << "\">" << LANG_EDIT
			<< "</a>]";
		s << "</td><td>" << m_a->name << "</td><td>";
		s << KPoolableRedirect::buildProto(m_a->get_proto());
		s << "</td><td>" << m_a->get_ref() << "</td><td>" << m_a->sockHelper->host << "</td>";
		s << "<td>" << m_a->sockHelper->get_port() << "</td>";
		s << "<td>" << m_a->sockHelper->getLifeTime() << "</td>";
		s << "<td>" << m_a->getPoolSize() << "</td>";

		s << "</tr>\n";
	}
	if (name.size() > 0) {
		m_a = getSingleAcserver(name);
	} else {
		m_a = NULL;
	}
	s << "</table>\n<hr>";
	s << "<form action=" << (m_a ? "/acserveredit" : "/acserveradd") << " method=post>\n";
	s << LANG_NAME << ":<input name=name value='" << (m_a ? m_a->name : "") << "'";
	if (m_a) {
		s << " readonly";
	}
	s << "><br>";
	KPoolableRedirect::build_proto_html(m_a, s);
	s << klang["lang_host"] << "<input name='host' value='";
	if (m_a) {
		s << m_a->sockHelper->host;
	}
	s << "'>" << LANG_PORT << "<input size=5 name=port value='";
	if (m_a) {
		s << m_a->sockHelper->get_port();
	}
	s << "'" << ">\n";
	if (m_a) {
		s << "<input type=hidden name=namefrom value='" << m_a->name << "'>\n";
	}
	s << "<br>" << klang["lang_life_time"] << "<input name='life_time' size=5 value=" << (m_a ? m_a->sockHelper->getLifeTime() : 0) << ">";
	s << klang["lang_life_time_notice"] << "<br>";
	kgl_refs_string* param = NULL;
	if (m_a) {
		param = m_a->sockHelper->GetParam();
	}
	s << "param:<input name='param' value='";
	if (param) {
		s.write_all(param->data, param->len);
		kstring_release(param);
	}
	s << "'><br>";
#ifdef HTTP_PROXY
	s << LANG_USER << ": <input name='auth_user' value='"
		<< (m_a ? m_a->sockHelper->auth_user.c_str() : "") << "'><br>";
	s << LANG_PASS << ": <input name='auth_passwd' value='"
		<< (m_a ? m_a->sockHelper->auth_passwd.c_str() : "") << "'><br>";
#endif
	s << "<br><input type=submit value=" << (m_a ? LANG_EDIT : LANG_SUBMIT) << "></form>\n";

	lock.RUnlock();
}
#ifdef ENABLE_MULTI_SERVER
void KAcserverManager::macserver_node_form(KWStream& s, const KString& name,
	KString action, unsigned nodeIndex) {
	if (action == "add") {
		return KMultiAcserver::nodeForm(s, name, NULL, 0);
	}
	KMultiAcserver* as = getMultiAcserver(name);
	if (as == NULL) {
		s << "name not found";
		return;
	}
	KMultiAcserver::nodeForm(s, name, as, nodeIndex);
}
bool KAcserverManager::macserver_node(KXmlAttribute& attribute, KString& err_msg) {
	auto name = attribute.remove("name");
	auto action = attribute.remove("action");
	if (name.empty()) {
		err_msg = "name cann't be zero";
		return false;
	}
	kconfig::KConfigResult result = kconfig::KConfigResult::ErrUnknow;
	if (action == "add" || action == "edit") {
		KStringBuf s;
		s << "server" << "@"_CS << name << "/node"_CS;
		auto xml = kconfig::new_xml(_KS("node"));
		int id = atoi(attribute.remove("id").c_str());
		xml->get_first()->attributes.swap(attribute);
		if (action[0] == 'e') {
			result = kconfig::update(s.str().str(), id, xml.get(), kconfig::EvUpdate);
		} else {
			result = kconfig::update(s.str().str(), 0, xml.get(), kconfig::EvNew);
		}
	} else if (action == "delete") {
		KStringBuf s;
		s << "server@" << name << "/node"_CS;
		result = kconfig::remove(s.str().str(), atoi(attribute["id"].c_str()));
	}
	switch (result) {
	case kconfig::KConfigResult::Success:
		return true;
	case kconfig::KConfigResult::ErrSaveFile:
		err_msg = "cann't save config file";
		return false;
	case kconfig::KConfigResult::ErrNotFound:
		err_msg = "not found";
		return false;
	default:
		err_msg = "unknow";
		return false;
	}
	return true;
}
#endif
void KAcserverManager::unloadAllApi() {
	std::map<KString, KApiRedirect*>::iterator it3;
	for (it3 = apis.begin(); it3 != apis.end(); it3++) {
		(*it3).second->dso.unload();
	}
}
std::vector<KString> KAcserverManager::getAllTarget() {
	std::vector<KString> targets;
	KStringBuf s;

	lock.RLock();
	for (auto it = acservers.begin(); it != acservers.end(); it++) {
		s.clear();
		s << (*it).second->getType() << ":" << (*it).first;
		targets.push_back(s.str());
	}
#ifdef ENABLE_MULTI_SERVER
	for (auto it2 = mservers.begin(); it2 != mservers.end(); it2++) {
		s.clear();
		s << "server:" << (*it2).first;
		targets.push_back(s.str());
	}
#endif
	for (auto it3 = apis.begin(); it3 != apis.end(); it3++) {
		s.clear();
		s << (*it3).second->getType() << ":" << (*it3).first;
		targets.push_back(s.str());
	}

#ifdef ENABLE_VH_RUN_AS
	for (auto it5 = cmds.begin(); it5 != cmds.end(); it5++) {
		s.clear();
		s << (*it5).second->getType() << ":" << (*it5).first;
		targets.push_back(s.str());
	}
#endif
	if (conf.dem) {
		conf.dem->ListTarget(targets);
	}
	//targets.push_back("ssi");
	lock.RUnlock();
	return targets;
}
std::vector<KString> KAcserverManager::getAcserverNames(bool onlyHttp) {
	std::vector<KString> table_names;
	lock.RLock();
	for (auto it = acservers.begin(); it != acservers.end(); it++) {
		if (!onlyHttp || (onlyHttp && (Proto_http == (*it).second->get_proto() || Proto_ajp == (*it).second->get_proto()))) {
			table_names.push_back((*it).first);
		}
	}
#ifdef ENABLE_MULTI_SERVER
	for (auto it2 = mservers.begin(); it2 != mservers.end(); it2++) {
		if (!onlyHttp || (onlyHttp && (Proto_http == (*it2).second->get_proto() || Proto_ajp == (*it2).second->get_proto()))) {
			table_names.push_back((*it2).first);
		}
	}
#endif
	lock.RUnlock();
	return table_names;
}
KSafeSingleAcserver KAcserverManager::findSingleAcserver(const KString& name) {
	auto locker = this->get_rlocker();
	auto ac = getSingleAcserver(name);
	if (ac == nullptr) {
		return KSafeSingleAcserver();
	}
	return KSafeSingleAcserver(static_cast<KSingleAcserver*>(ac->add_ref()));
}
KSingleAcserver* KAcserverManager::refsSingleAcserver(const KString& name) {
	lock.RLock();
	KSingleAcserver* ac = getSingleAcserver(name);
	if (ac) {
		ac->add_ref();
	}
	lock.RUnlock();
	return ac;
}
KPoolableRedirect* KAcserverManager::refsAcserver(const KString& name) {
	lock.RLock();
	KPoolableRedirect* ac = getAcserver(name);
	if (ac) {
		ac->add_ref();
	}
	lock.RUnlock();
	return ac;
}
#ifdef ENABLE_MULTI_SERVER
KMultiAcserver* KAcserverManager::getMultiAcserver(const KString& table_name) {
	auto it = mservers.find(table_name);
	if (it != mservers.end()) {
		return (*it).second;
	}
	return NULL;
}
#endif
KPoolableRedirect* KAcserverManager::getAcserver(const KString& table_name) {
	KPoolableRedirect* server = getSingleAcserver(table_name);
	if (server != NULL) {
		return server;
	}
#ifdef ENABLE_MULTI_SERVER
	return getMultiAcserver(table_name);
#else
	return NULL;
#endif
}
KSingleAcserver* KAcserverManager::getSingleAcserver(const KString& table_name) {
	auto it = acservers.find(table_name);
	if (it != acservers.end()) {
		return (*it).second;
	}
	return NULL;
}
bool KAcserverManager::new_server(
	bool is_update,
	KXmlAttribute& attr,
	KString& err_msg) {
	return new_item("server", is_update, attr, err_msg);
}
bool KAcserverManager::new_item(
	const KString& scope,
	bool is_update,
	KXmlAttribute& attr,
	KString& err_msg) {

	auto envs = std::move(attr.remove("env"));

	auto  name = attr["name"];
	err_msg = LANG_TABLE_NAME_ERR;
	kconfig::KConfigResult result;
	auto xml = kconfig::new_xml(scope.c_str(), scope.size(), name.c_str(), name.size());
	if (!envs.empty()) {
		char* env = strdup(envs.c_str());
		defer(free(env));
		auto env_xml = kconfig::new_xml(_KS("env"));
		buildAttribute(env, env_xml->get_last()->attributes);
		xml->append(env_xml.get());
	}
	xml->get_first()->attributes.swap(attr);
	KStringBuf s;
	s << scope << "@"_CS << name;
	result = kconfig::update(s.str().str(), 0, xml.get(), is_update ? kconfig::EvUpdate : kconfig::EvUpdate | kconfig::FlagCreate);
	switch (result) {
	case kconfig::KConfigResult::Success:
		return true;
	case kconfig::KConfigResult::ErrSaveFile:
		err_msg = "cann't save config file";
		return false;
	case kconfig::KConfigResult::ErrNotFound:
		err_msg = "not found";
		return false;
	default:
		err_msg = "unknow";
		return false;
	}
}
bool KAcserverManager::remove_item(const KString& scope, const KString& name, KString& err_msg) {
	KStringBuf s;
	s << scope << "@"_CS << name;
	switch (kconfig::remove(s.str().str(), 0)) {
	case kconfig::KConfigResult::Success:
		return true;
	case kconfig::KConfigResult::ErrSaveFile:
		err_msg = "cann't save config file";
		return false;
	case kconfig::KConfigResult::ErrNotFound:
		err_msg = "not found";
		return false;
	default:
		err_msg = "unknow";
		return false;
	}
}
#ifdef ENABLE_MULTI_SERVER
bool KAcserverManager::remove_server(const KString& name, KString& err_msg) {
	return remove_item("server", name, err_msg);
}
#endif
KRedirect* KAcserverManager::refsRedirect(const KString& target) {
	kgl_jump_type jumpType;
	KString name;
	if (strncasecmp(target.c_str(), "cdn:", 4) == 0) {
		KRedirect* rd = NULL;
		char* tmp = strdup(target.c_str() + 4);
		char* p = strchr(tmp, ':');
		if (p) {
			*p = '\0';
			char* host = p + 1;
			p = strchr(host, ':');
			if (p) {
				*p = '\0';
				int port = atoi(p + 1);
				p = strchr(p + 1, ':');
				int life_time = 0;
				if (p) {
					*p = '\0';
					life_time = atoi(p + 1);
				}
				rd = server_container->refsRedirect(NULL, host, port, NULL, life_time, KPoolableRedirect::parseProto(tmp));
			}
		}
		free(tmp);
		return rd;
	}

	if (!kaccess[REQUEST]->parseChainAction(target, jumpType, name)) {
		klog(KLOG_ERR, "cann't parse target=[%s]\n", target.c_str());
		return NULL;
	}
	switch (jumpType) {
	case JUMP_MSERVER:
		return refsMultiAcserver(name);
	case JUMP_SERVER:
		return refsAcserver(name);
	case JUMP_API:
		return refsApiRedirect(name);
#ifdef ENABLE_VH_RUN_AS
	case JUMP_CMD:
		return refsCmdRedirect(name);
#endif
	case JUMP_DSO:
		if (conf.dem) {
			return conf.dem->RefsRedirect(name);
		}
	}
	return NULL;
}
#ifdef ENABLE_VH_RUN_AS	
void KAcserverManager::loadAllApi() {
	lock.WLock();
	for (auto it = apis.begin(); it != apis.end(); it++) {
		(*it).second->load();
	}
	lock.WUnlock();
}
#endif
bool KAcserverManager::apiEnable(const KString& name, bool enable) {
	bool result = false;
	lock.WLock();
	KApiRedirect* rd = getApiRedirect(name);
	if (rd) {
		result = true;
	}
	lock.WUnlock();
	return result;
}
#ifdef ENABLE_VH_RUN_AS
bool KAcserverManager::cmdEnable(const KString& name, bool enable) {
	bool result = false;
	lock.WLock();
	KCmdPoolableRedirect* rd = getCmdRedirect(name);
	if (rd) {
		result = true;
	}
	lock.WUnlock();
	return result;
}
#endif

#ifdef ENABLE_VH_RUN_AS
KCmdPoolableRedirect* KAcserverManager::newCmdRedirect(
	std::map<KString, KString>& attribute, KString& errMsg) {
	auto name = attribute["name"];
	if (getCmdRedirect(name)) {
		errMsg = "name duplicate.";
		return NULL;
	}
	KCmdPoolableRedirect* rd = new KCmdPoolableRedirect(name);
	rd->name = name;
	cmds.insert(std::pair<KString, KCmdPoolableRedirect*>(rd->name, rd));
	return rd;
}
#endif


#ifdef ENABLE_VH_RUN_AS
KCmdPoolableRedirect* KAcserverManager::getCmdRedirect(const KString& name) {
	std::map<KString, KCmdPoolableRedirect*>::iterator it;
	it = cmds.find(name);
	if (it != cmds.end()) {
		return (*it).second;
	}
	return NULL;
}
KCmdPoolableRedirect* KAcserverManager::refsCmdRedirect(const KString& name) {
	lock.RLock();
	KCmdPoolableRedirect* ac = getCmdRedirect(name);
	if (ac) {
		ac->add_ref();
	}
	lock.RUnlock();
	return ac;
}
#endif
KApiRedirect* KAcserverManager::refsApiRedirect(const KString& name) {
	lock.RLock();
	KApiRedirect* ac = getApiRedirect(name);
	if (ac) {
		ac->add_ref();
	}
	lock.RUnlock();
	return ac;
}

KApiRedirect* KAcserverManager::getApiRedirect(const KString& name) {
	auto it = apis.find(name);
	if (it != apis.end()) {
		return (*it).second;
	}
	return NULL;
}