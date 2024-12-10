/*
 * Kconf.gvm->cpp
 *
 *  Created on: 2010-4-19
 *      Author: keengo
 *
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

#include "do_config.h"
#include "KVirtualHostManage.h"
#include "KLineFile.h"
#include "kmalloc.h"
#include "lang.h"
#include "KHttpServerParser.h"
#include "KAcserverManager.h"
#include "KVirtualHostDatabase.h"
#include "directory.h"
#include "KHtAccess.h"
#include "KConfigBuilder.h"
#include "KDynamicListen.h"
#include "WhmContext.h"
#include "KDefer.h"
using namespace std;
kfiber_mutex* KVirtualHostManage::lock = kfiber_mutex_init();
KDynamicListen KVirtualHostManage::dlisten;
volatile int32_t curInstanceId = 1;
void free_ssl_certifycate(void* arg) {
	KSslCertificate* cert = (KSslCertificate*)arg;
	delete cert;
}
static KVirtualHostManage* vhm = nullptr;
KVirtualHostManage* KVirtualHostManage::get_instance() {
	if (!vhm) {
		vhm = new KVirtualHostManage();
	}
	return vhm;
}
bool KVirtualHostManage::on_config_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	auto xml = ev->get_xml();
	switch (ev->type) {
	case kconfig::EvSubDir | kconfig::EvNew:
	{
		if (xml->is_tag(_KS("vh"))) {
			assert(tree->ls == nullptr);
			KSafeVirtualHost vh(new KVirtualHost(xml->attributes()["name"]));
			vh->parse_xml(ev->get_xml()->get_first(), nullptr);
			if (tree->bind(vh.get())) {
				vh->add_ref();
			}
			if (!this->addVirtualHost(tree, vh.get())) {
				tree->unbind();
				vh->release();
			}
			return true;
		}
#ifdef ENABLE_SVH_SSL
		if (xml->is_tag(_KS("ssl"))) {
			auto& attr = xml->attributes();
			add_ssl(attr("domain"), attr("certificate"), attr("certificate_key"));
			return true;
		}
#endif
		break;
	}
	case kconfig::EvSubDir | kconfig::EvUpdate:
	{
		if (xml->is_tag(_KS("vh"))) {
			KSafeVirtualHost old_vh(static_cast<KVirtualHost*>(tree->unbind()));
			KSafeVirtualHost new_vh(new KVirtualHost(old_vh->name));
			new_vh->parse_xml(xml->get_first(), old_vh.get());
			if (tree->bind(new_vh.get())) {
				new_vh->add_ref();
			}
			this->updateVirtualHost(tree, new_vh.get(), old_vh.get());
			return true;
		}
#ifdef ENABLE_SVH_SSL
		if (xml->is_tag(_KS("ssl"))) {
			auto& attr = xml->attributes();
			update_ssl(attr("domain"), attr("certificate"), attr("certificate_key"));
			return true;
		}
#endif
		break;
	}
	case kconfig::EvSubDir | kconfig::EvRemove:
	{
		if (xml->is_tag(_KS("vh"))) {
			KSafeVirtualHost vh(static_cast<KVirtualHost*>((KBaseVirtualHost*)tree->unbind()));
			if (this->removeVirtualHost(tree, vh.get())) {
			}
			return true;
		}
#ifdef ENABLE_SVH_SSL
		if (xml->is_tag(_KS("ssl"))) {
			auto attr = xml->attributes();
			remove_ssl(attr("domain"));
			return true;
		}
#endif
		break;
	}
	}
	return true;
}
KVirtualHostManage::KVirtualHostManage() {
#ifdef ENABLE_BLACK_LIST
	vhs.blackList = new KIpList;
#endif
#ifdef ENABLE_SVH_SSL
	ssl_config = NULL;
#endif
}
KVirtualHostManage::~KVirtualHostManage() {
	for (auto it4 = avh.begin(); it4 != avh.end(); it4++) {
		InternalUnBindVirtualHost((*it4).second);
		(*it4).second->release();
	}
	avh.clear();
	kassert(this != conf.gvm || dlisten.IsEmpty());
#ifdef ENABLE_SVH_SSL
	if (ssl_config) {
		ssl_config->clear(free_ssl_certifycate);
		delete ssl_config;
	}
#endif
}
kserver* KVirtualHostManage::RefsServer(u_short port) {
	auto lock = locker();
	return dlisten.RefsServer(port);

}
void KVirtualHostManage::getAllVh(std::list<KString>& vhs, bool status, bool onlydb) {
	auto lock = locker();
	for (auto it = avh.begin(); it != avh.end(); ++it) {
		vhs.push_back((*it).first);
	}
}
void KVirtualHostManage::getAutoName(KString& name) {
	KStringBuf s;
	s << kgl_program_start_sec << "_" << katom_inc((void*)&curInstanceId);
	s.str().swap(name);
}
int KVirtualHostManage::getNextInstanceId() {
	return katom_inc((void*)&curInstanceId);
}
KVirtualHost* KVirtualHostManage::refsVirtualHostByName(const KString& name) {
	auto lock = locker();
	auto it = avh.find(name);
	if (it != avh.end()) {
		return (*it).second->add_ref();
	}
	return nullptr;
}
void KVirtualHostManage::shutdown() {
	dlisten.Close();
}
void KVirtualHostManage::getMenuHtml(KWStream& s, KVirtualHost* v, KStringBuf& url) {
	KBaseVirtualHost* vh = &vhs;
	if (v) {
		vh = v;
		url << "name=" << v->name << "&";
	}
	s << "<html><head>"
		<< "<LINK href=/main.css type='text/css' rel=stylesheet></head>";
	s << "<body><table border=0><tr><td>";
	s << "[<a href='/vhlist'>" << klang["LANG_VHS"] << "</a>]";
	if (v) {
		s << " ==> " << v->name;
	}
	s << "</td></tr></table><br>";

	s << "<table width='100%'><tr><td align=left>";
	if (v) {
		s << "[<a href='/vhlist?" << url << "&id=0'>" << klang["detail"] << "</a>] ";
	} else {
		s << "[<a href='/vhlist?id=0'>" << klang["all_vh"] << "</a>] ";// [<a href='/vhlist?t=1&id=0'>" << klang["all_tvh"];
	}
	s << "[<a href='/vhlist?" << url << "id=1'>" << klang["index"] << "</a>] ";
	s << "[<a href='/vhlist?" << url << "id=2'>" << klang["map_extend"] << "</a>] ";
	s << "[<a href='/vhlist?" << url << "id=3'>" << klang["error_page"] << "</a>] ";
	s << "[<a href='/vhlist?" << url << "id=5'>" << klang["alias"] << "</a>] ";
	s << "[<a href='/vhlist?" << url << "id=8'>" << klang["mime_type"] << "</a>] ";
#ifndef HTTP_PROXY
	if (v) {
		s << "[<a href='/vhlist?" << url << "id=9'>" << klang["vh_host"] << "</a>] ";
		s << "[<a href='/vhlist?" << url << "id=10'>" << klang["bind"] << "</a>] ";
		s << "[<a href='/vhlist?" << url << "id=6'>" << klang["lang_requestAccess"] << "</a>] ";
		s << "[<a href='/vhlist?" << url << "id=7'>" << klang["lang_responseAccess"] << "</a>] ";
	}
#endif
	s << "</td><td align=right>";
	s << "</td></tr></table>";
	s << "<hr>";
}
void KVirtualHostManage::getHtml(KWStream& s, const KString& name, int id, KUrlValue& attribute) {
	KStringBuf url;
	KBaseVirtualHost* vh = &vhs;
	KSafeVirtualHost v;
	if (!name.empty()) {
		v.reset(refsVirtualHostByName(name));
		if (v) {
			vh = v.get();
		}
	}
	getMenuHtml(s, v.get(), url);
	url << "id=" << id;
	if (id == 0 && !v) {
		auto lock = locker();
		getAllVhHtml(s);
	} else {
		auto locker = vh->get_locker();
		switch (id) {
		case 0:
			getVhDetail(s, v.get(), true);
			break;
		case 1:
			vh->getIndexHtml(url.str(), s);
			break;
		case 2:
			vh->getRedirectHtml(url.str(), s);
			break;
		case 3:
			vh->getErrorPageHtml(url.str(), s);
			break;
		case 4:
			getVhDetail(s, v.get(), false);
			break;
		case 5:
			vh->getAliasHtml(url.str(), s);
			break;
		case 6:
		case 7:
		{
			int	access_type = id == 6 ? REQUEST : RESPONSE;
			if (v) {
				if (!v->user_access.empty() && !v->access[access_type]) {
					v->access[access_type].reset(new KAccess(false, access_type));
				}
				if (v->access[access_type]) {
					s << v->access[access_type]->htmlAccess(name.c_str());
				}
			}
			break;
		}
		case 8:
			vh->getMimeTypeHtml(url.str(), s);
			break;
		case 9:
			if (v) {
				v->get_host_html(url.str(), s);
			}
			break;
		case 10:
			if (v) {
				v->get_bind_html(url.str(), s);
			}
			break;
		default:
			break;
		}
	}
	s << endTag();
	s << "</body></html>";
}
bool KVirtualHostManage::updateVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh, KVirtualHost* ov) {
	auto lock = locker();
	if (ov) {
#ifdef ENABLE_VH_RUN_AS
		ov->need_kill_process = vh->caculateNeedKillProcess(ov);
#endif
		if (ov) {
			internalRemoveVirtualHost(ct, ov);
		}
	}
	if (vh->name.empty()) {
		getAutoName(vh->name);
	}
	bool result = internalAddVirtualHost(ct, vh, ov);
	if (ov) {
		InternalUnBindVirtualHost(ov);
	}
	return result;
}
bool KVirtualHostManage::updateVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh) {

	KSafeVirtualHost ov(refsVirtualHostByName(vh->name));
	return updateVirtualHost(ct, vh, ov.get());
}
/*
 * 增加虚拟主机
 */
bool KVirtualHostManage::addVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh) {
	if (vh->name.size() == 0) {
		getAutoName(vh->name);
	}
	auto lock = locker();
	return internalAddVirtualHost(ct, vh, nullptr);
}
/*
 * 删除虚拟主机
 */
bool KVirtualHostManage::removeVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh) {
	auto lock = locker();
#ifdef ENABLE_VH_RUN_AS
	vh->need_kill_process = 1;
#endif
	bool result = internalRemoveVirtualHost(ct, vh);
	InternalUnBindVirtualHost(vh);
	return result;
}
bool KVirtualHostManage::internalAddVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh, KVirtualHost* ov) {
#ifdef ENABLE_USER_ACCESS
	vh->access_config_listen(ct, ov);
#endif
	auto it = avh.find(vh->name);
	if (it != avh.end()) {
		klog(KLOG_ERR, "Cann't add VirtualHost [%s] name duplicate.\n", vh->name.c_str());
		return false;
	}
	InternalBindVirtualHost(vh);
	avh.insert(pair<KString, KVirtualHost*>(vh->name, vh));
	vh->add_ref();
	return true;
}
void KVirtualHostManage::UnBindGlobalVirtualHost(kserver* server) {
	for (auto it = avh.begin(); it != avh.end(); it++) {
		kgl_vhc_remove_global_vh(get_vh_container(server), (*it).second);
	}
}
void KVirtualHostManage::BindGlobalVirtualHost(kserver* server) {
	for (auto it = avh.begin(); it != avh.end(); it++) {
		kgl_vhc_add_global_vh(get_vh_container(server), (*it).second);
	}
}
bool KVirtualHostManage::internalRemoveVirtualHost(kconfig::KConfigTree* ct, KVirtualHost* vh) {
	auto it = avh.find(vh->name);
	if (it == avh.end() || (*it).second != vh) {
		return false;
	}
	avh.erase(it);
	vh->release();
	return true;
}
int KVirtualHostManage::find_domain(const char* site, WhmContext* ctx) {
	int site_len = (int)strlen(site);
	const char* p = strchr(site, ':');
	int port = 0;
	if (p) {
		port = atoi(p + 1);
		site_len = (int)(p - site);
	}
	unsigned char bind_site[256];
	if (!revert_hostname(site, site_len, bind_site, sizeof(bind_site))) {
		return WHM_CALL_FAILED;
	}

	auto lock = locker();
	dlisten.QueryDomain(bind_site, port, ctx);
	return WHM_OK;
}
/*
 * 查找虚拟主机并绑定在rq上。
 */
query_vh_result KVirtualHostManage::queryVirtualHost(KVirtualHostContainer* vhc, KSubVirtualHost** rq_svh, const char* site, int site_len) {
	query_vh_result result = query_vh_host_not_found;
	if (vhc == NULL) {
		return result;
	}
	if (site_len == 0) {
		site_len = (int)strlen(site);
	}
	unsigned char bind_site[256];
	if (!revert_hostname(site, site_len, bind_site, sizeof(bind_site))) {
		return result;
	}
	auto locker = vhc->get_locker();
	KSubVirtualHost* svh = (KSubVirtualHost*)vhc->get_root(locker)->find(bind_site);
	if (svh) {
		result = query_vh_success;
		if ((*rq_svh) != svh) {
			//虚拟主机变化了?把老的释放，引用新的
			if (*rq_svh) {
				(*rq_svh)->release();
				(*rq_svh) = NULL;
			}
			svh->vh->add_ref();
			(*rq_svh) = svh;
		}
	}
	return result;
}

void KVirtualHostManage::getVhDetail(KWStream& s, KVirtualHost* vh, bool edit) {
	//	string host;
	string action = "vh_add";
	if (edit) {
		action = "vh_edit";
	}
	KString name;
	if (vh) {
		name = vh->name;
	}
	s << "<form name='frm' action='/vhbase?action=" << action;
	s << "' method='post'>";
	s << "<table border=1>";
	s << "<tr><td>" << LANG_NAME << "</td>";
	s << "<td>";
	s << "<input name='name' value='";
	if (edit) {
		if (vh) {
			s << vh->name.c_str();
		}
		s << "' readonly";
	} else {
		s << "'";
	}
	s << ">";
	s << klang["status"] << ":<input name='status' size=3 value='" << (vh ? vh->closed : 0) << "'>";
	s << "</td></tr>";
	s << "<tr><td>" << klang["doc_root"] << "</td><td><input name='doc_root' size=30 value='"
		<< (vh ? vh->GetDocumentRoot() : "") << "'>";
#ifndef _WIN32
	s << "<input name='chroot' type='checkbox' value='1'" << ((vh && vh->chroot) ? "checked" : "") << ">chroot";
#endif
	s << "<tr><td>" << klang["inherit"]
		<< "</td><td><input name='inherit' type='radio' value='1' ";
	if (vh == NULL || vh->inherit) {
		s << "checked";
	}
	s << ">" << klang["inherit"]
		<< "<input name='inherit' type='radio' value='0' ";
	if (vh && !vh->inherit) {
		s << "checked";
	}
	s << ">" << klang["no_inherit"]
		<< "<input name='inherit' type='radio' value='2'>";
	s << klang["no_inherit2"] << "</td></tr>\n";
#ifdef ENABLE_VH_RUN_AS
	s << "<tr><td>" << LANG_RUN_USER << "</td><td>" << LANG_USER
		<< ":<input name='user' value='";
	s << (vh ? vh->user : "") << "' autocomplete='off' size=10> ";
#ifdef _WIN32
	s << LANG_PASS;
#else
	s << klang["group"];
#endif
	s << ":<input name='group' "
#ifdef _WIN32
		<< "type='password' "
#endif
		<< "value='"
#ifndef _WIN32
		<< (vh ? vh->group : "")
#endif
		<< "' autocomplete='off' size=10><td></tr>\n";
#endif
#ifdef ENABLE_VH_LOG_FILE
	s << "<tr><td>" << klang["log_file"]
		<< "</td><td><input name='log_file' value='" << (vh ? vh->logFile
			: "") << "'></td></tr>\n";
	s << "<tr><td>" << klang["log_mkdir"]
		<< "</td><td><input name='log_mkdir' type='radio' value='on' ";
	bool mkdirFlag = false;
	if (vh && vh->logger && vh->logger->mkdir_flag) {
		mkdirFlag = true;
	}
	if (mkdirFlag) {
		s << "checked";
	}
	s << ">" << LANG_ON << "<input name='log_mkdir' type='radio' value='off' ";
	if (!mkdirFlag) {
		s << "checked";
	}
	s << ">" << LANG_OFF << "</td></tr>\n";

	//log_handle
	s << "<tr><td>" << klang["log_handle"]
		<< "</td><td><input name='log_handle' type='radio' value='on' ";
	bool log_handle = true;
	if (vh && vh->logger && !vh->logger->log_handle) {
		log_handle = false;
	}
	if (log_handle) {
		s << "checked";
	}
	s << ">" << LANG_ON << "<input name='log_handle' type='radio' value='off' ";
	if (!log_handle) {
		s << "checked";
	}
	s << ">" << LANG_OFF << "</td></tr>\n";


	KString rotateTime;
	if (vh && vh->logger) {
		vh->logger->getRotateTime(rotateTime);
	}
	s << "<tr><td>" << LANG_LOG_ROTATE_TIME
		<< "</td><td><input name='log_rotate_time' value='" << rotateTime
		<< "'></td></tr>\n";
	s << "<td>" << klang["log_rotate_size"]
		<< "</td><td><input name='log_rotate_size' value='" << (vh
			&& vh->logger ? vh->logger->rotate_size : 0) << "'></td></tr>\n";
	s << "<td>" << klang["logs_day"]
		<< "</td><td><input name='logs_day' value='" << (vh
			&& vh->logger ? vh->logger->logs_day : 0) << "'></td></tr>\n";
	s << "<td>" << klang["logs_size"]
		<< "</td><td><input name='logs_size' value='" << (vh
			&& vh->logger ? vh->logger->logs_size : 0) << "'></td></tr>\n";
#endif
	s << "<tr><td>" << klang["option"]
		<< "</td><td>";
	s << "<input name='browse' type='checkbox' value='on'"
		<< ((vh && vh->browse) ? "checked" : "") << ">" << klang["browse"];

#ifdef ENABLE_VH_FLOW
	s << "<input name='fflow' type='checkbox' value='1'"
		<< ((vh && vh->fflow) ? "checked" : "") << ">" << klang["flow"];
#endif
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
#ifdef ENABLE_HTTP2
	s << "<input name='http2' type='checkbox' value='1'" << ((vh && KBIT_TEST(vh->alpn, KGL_ALPN_HTTP2) > 0) ? "checked" : "") << ">http2";
#endif
#ifdef ENABLE_HTTP3
	s << "<input name='http3' type='checkbox' value='1'" << ((vh && KBIT_TEST(vh->alpn, KGL_ALPN_HTTP3) > 0) ? "checked" : "") << ">http3";
#endif
#ifdef SSL_READ_EARLY_DATA_SUCCESS
	s << "<input type='checkbox' name='early_data' value='1' ";
	if (vh && vh->early_data) {
		s << "checked";
	}
	s << ">early_data";
#endif
#endif
	s << "</td></tr>\n";
#ifdef ENABLE_USER_ACCESS
	s << "<tr><td>" << klang["access_file"]
		<< "</td><td><input name='access' value='"
		<< (vh ? vh->user_access : "") << "'></td></tr>\n";
#endif
	s << "<tr><td>" << klang["htaccess"]
		<< "</td><td><input name='htaccess' value='"
		<< (vh ? vh->htaccess : "") << "'></td></tr>\n";
	s << "<tr><td>" << klang["app_count"] << "</td><td>";
	s << "<input name='app' value='" << (vh ? vh->app : 1) << "' size='4'>";
	s << "<input type='checkbox' name='ip_hash' value='1' ";
	if (vh && vh->ip_hash) {
		s << "checked";
	}
	s << ">" << klang["ip_hash"] << "</td></tr>";
	s << "<tr><td>" << klang["app_share"] << "</td><td>";
	s << "<input type='radio' name='app_share' value='0' ";
	if (vh && vh->app_share == 0) {
		s << "checked";
	}
	s << "/>" << klang["app_share0"];
	s << "<input type='radio' name='app_share' value='1' ";
	if (vh == NULL || vh->app_share == 1) {
		s << "checked";
	}
	s << "/>" << klang["app_share1"];
#if 0
	s << "<input type='radio' name='app_share' value='2' ";
	if (vh && vh->app_share == 2) {
		s << "checked";
	}
	s << "/>" << klang["app_share2"];
	s << "<input type='radio' name='app_share' value='3' ";
	if (vh && vh->app_share == 3) {
		s << "checked";
	}
	s << "/>" << klang["app_share3"];
#endif
	s << "</td></tr>\n";
#ifdef ENABLE_VH_RS_LIMIT
	s << "<tr><td>" << klang["connect"]
		<< "</td><td><input name='max_connect' value='";
	s << (vh ? vh->max_connect : 0) << "'></td></tr>\n";
	s << "<tr><td>" << LANG_LIMIT_SPEED
		<< "</td><td><input name='speed_limit' value='";
	s << (vh ? vh->speed_limit : 0) << "'></td></tr>\n";
#endif
#ifdef ENABLE_VH_QUEUE
	s << "<tr><td>" << klang["max_worker"]
		<< "</td><td><input name='max_worker' value='";
	s << (vh && vh->queue ? vh->queue->getMaxWorker() : 0) << "'></td></tr>\n";

	s << "<tr><td>" << klang["max_queue"]
		<< "</td><td><input name='max_queue' value='";
	s << (vh && vh->queue ? vh->queue->getMaxQueue() : 0) << "'></td></tr>\n";
#endif
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	s << "<tr><td>" << klang["cert_file"] << "</td><td>";
	s << "<input name='certificate' value='";
	if (vh) {
		s << vh->cert_file;
	}
	s << "'></td></tr>\n";
	s << "<tr><td>" << klang["private_file"] << "</td><td>";
	s << "<input name='certificate_key' value='";
	if (vh) {
		s << vh->key_file;
	}
	s << "'></td></tr>\n";
	s << "<tr><td>cipher</td><td>";
	s << "<input name='cipher' value='";
	if (vh) {
		s << vh->cipher;
	}
	s << "'></td></tr>\n";
	s << "<tr><td>protocols</td><td>";
	s << "<input name='protocols' value='";
	if (vh) {
		s << vh->protocols;
	}
	s << "'></td></tr>\n";
#endif
	s << "<tr><td>envs</td><td>";
	s << "<input name='envs' size=32 value=\"";
	if (vh) {
		vh->buildEnv(s);
	}
	s << "\"></td></tr>\n";
	s << "</table>\n";
	s << "<input type=submit value='" << LANG_SUBMIT << "'>";
	s << "</form>";
}
void KVirtualHostManage::getVhIndex(KWStream& s, KVirtualHost* vh, int id) {
	vh->lock.Lock();
	s << "<tr id='tr" << id << "' style='background-color: #ffffff' onmouseover=\"setbgcolor('tr";
	s << id << "','#bbbbbb')\" onmouseout=\"setbgcolor('tr" << id << "','#ffffff')\">";
	s << "<td>";
	s << "[<a href=\"javascript:if(confirm('really delete')){ window.location='/vhbase?";
	s << "name=" << vh->name << "&action=vh_delete';}\">" << LANG_DELETE << "</a>]";
	s << "</td><td ";
	if (vh->closed) {
		s << "bgcolor='#bbbbbb'";
	}
	s << "><a href='/vhlist?id=0&name=" << vh->name << "'";
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	if (vh->ssl_ctx) {
		s << " class=\"ssl_enabled\"";
	}
#endif
	s << ">";
	s << vh->name;
	s << "</td><td>";
	list<KSubVirtualHost*>::iterator it2;
	for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
		s << "<a href=# title=\"";
		if (strcmp((*it2)->dir, "/") != 0
#ifdef ENABLE_SVH_SSL
			|| (*it2)->ssl_param != NULL
#endif
			) {
			s << (*it2)->dir;
#ifdef ENABLE_SVH_SSL
			if ((*it2)->ssl_param != NULL) {
				s << KGL_SSL_PARAM_SPLIT_CHAR << (*it2)->ssl_param;
			}
#endif
		}
		s << "\"";
#ifdef ENABLE_SVH_SSL
		if ((*it2)->ssl_ctx) {
			s << " class=\"ssl_enabled\"";
		}
#endif
		s << ">";
		if ((*it2)->wide) {
			s << "*";
		}
		s << (*it2)->host;
		s << "</a> ";
	}
	s << "</td><td ><div title='" << vh->doc_root << "'>" << vh->GetDocumentRoot() << "</div></td>";
#ifdef ENABLE_VH_RUN_AS
	s << "<td >" << (vh->user.size() > 0 ? vh->user : "&nbsp;");
#ifndef _WIN32
	if (vh->group.size() > 0) {
		s << ":" << vh->group;
	}
#endif
	s << "</td>";
#endif
	s << "<td >" << (vh->inherit ? LANG_ON : LANG_OFF) << "</td>";
#ifdef ENABLE_VH_LOG_FILE
	s << "<td >" << (!vh->logFile.empty() ? vh->logFile : "&nbsp;") << "</td>";
#endif
	s << "<td >" << (vh->browse ? LANG_ON : LANG_OFF) << "</td>";
#ifdef ENABLE_USER_ACCESS
	s << "<td >" << (!vh->user_access.empty() ? vh->user_access : "&nbsp;") << "</td>";
#endif
#ifdef ENABLE_VH_RS_LIMIT
	s << "<td >" << vh->GetConnectionCount() << "/" << vh->max_connect << "</td>";
	s << "<td >";
#ifdef ENABLE_VH_FLOW
	s << vh->get_speed(false) << "/";
#endif
	s << vh->speed_limit << "</td>";
#endif		
#ifdef ENABLE_VH_FLOW
	s << "<td>";
	if (vh->flow) {
		s << vh->flow->flow << " " << (vh->flow->flow > 0 ? (vh->flow->cache * 100) / vh->flow->flow : 0) << "%";
	}
	s << "</td>";
#endif
#ifdef ENABLE_VH_QUEUE
	s << "<td >";
	if (vh->queue) {
		s << vh->queue->getWorkerCount() << "/" << vh->queue->getMaxWorker();
	}
	s << "</td>";
	s << "<td >";
	if (vh->queue) {
		s << vh->queue->getQueueSize() << "/" << vh->queue->getMaxQueue();
	}
	s << "</td>";
#endif
	//*
	s << "<td >";
	s << "</td>";
	//*/
	s << "</tr>\n";
	vh->lock.Unlock();
}
#ifdef ENABLE_VH_FLOW
void KVirtualHostManage::dumpLoad(KVirtualHostEvent* ctx, bool revers, const char* prefix, int prefix_len) {
	KStringBuf s2;
	auto lock = locker();
	for (auto it = avh.begin(); it != avh.end(); it++) {
		KVirtualHost* vh = (*it).second;
		if (prefix && revers == (strncmp(vh->name.c_str(), prefix, prefix_len) == 0)) {
			continue;
		}
		int connect_count = vh->GetConnectionCount();
		INT64 speed = vh->get_speed(true);
		if (connect_count == 0 && speed == 0) {
			continue;
		}
		s2 << vh->name.c_str() << "\t";
		s2 << connect_count << "\t";
		s2 << speed << "\t";
#ifdef ENABLE_VH_QUEUE
		if (vh->queue) {
			s2 << vh->queue->getQueueSize() << "\t";
			s2 << vh->queue->getWorkerCount() << "\t";
		} else
#endif
			s2 << "0\t0";
		s2 << "\n";
	}
	ctx->data()->add("load", s2.str());
}
//extend=4,导出上下行，及缓存流量
void KVirtualHostManage::dumpFlow(KVirtualHostEvent* ctx, bool revers, const char* prefix, int prefix_len, int extend) {
	char buf[64];
	KStringBuf s;
	KStringBuf s2;
	auto lock = locker();
	for (auto it = avh.begin(); it != avh.end(); it++) {
		KVirtualHost* vh = (*it).second;
		if (!vh->fflow || vh->flow == NULL || vh->flow->flow == 0) {
			continue;
		}
		if (prefix && revers == (strncmp(vh->name.c_str(), prefix, prefix_len) == 0)) {
			continue;
		}
		int64_t post_flow;
		int len = vh->flow->dump(buf, sizeof(buf), post_flow);
		s << vh->name.c_str() << "\t";
		if (len > 0) {
			s.write_all(buf, len);
		}
		if (extend == 4) {
			s << "\t" << (INT64)post_flow;
		}
#ifdef ENABLE_BLACK_LIST
		if (vh->blackList && extend >= 3) {
			INT64 total_error_upstream, total_request, total_upstream;
			vh->blackList->getStat(total_request, total_error_upstream, total_upstream, true);
			s << "\t" << total_error_upstream << "\t" << total_upstream << "\t" << total_request;
		}
#endif
		s << "\n";
	}
#ifdef ENABLE_BLACK_LIST
	if (extend > 0 && extend < 3) {
		for (auto it = avh.begin(); it != avh.end(); it++) {
			KVirtualHost* vh = (*it).second;
			if (prefix && revers == (strncmp(vh->name.c_str(), prefix, prefix_len) == 0)) {
				continue;
			}
			s2 << vh->name.c_str() << "\t";
			s2 << vh->GetConnectionCount() << "\t";
			s2 << vh->get_speed(extend == 2) << "\t";
#ifdef ENABLE_VH_QUEUE
			if (vh->queue) {
				s2 << vh->queue->getQueueSize() << "\t";
				s2 << vh->queue->getWorkerCount() << "\t";
			} else
#endif
				s2 << "0\t0\t";
			if (vh->blackList) {
				INT64  total_error_upstream, total_request, total_upstream;
				vh->blackList->getStat(total_request, total_error_upstream, total_upstream, extend == 2);
				s2 << "\t" << total_error_upstream << "\t" << total_upstream << "\t" << total_request;
			}
			s2 << "\n";
		}
	}
#endif
	ctx->data()->add("flow", s.str());
	if (extend > 0 && s2.size() > 0) {
		ctx->data()->add("stat", s2.str());
	}
}
void KVirtualHostManage::dumpFlow() {
#ifdef _WIN32
	const char* formatString = "%s\t%I64d\t%I64d\n";
#else
	const char* formatString = "%s\t%lld\t%lld";
#endif
	FILE* fp = NULL;
	auto  flow_file = conf.path;
	flow_file += "etc/flow.log";
	auto lock = locker();
	for (auto it = avh.begin(); it != avh.end(); it++) {
		//todo: dump vh flow
		KVirtualHost* vh = (*it).second;
		if (!vh->fflow || vh->flow == NULL) {
			continue;
		}
		if (fp == NULL) {
			fp = fopen(flow_file.c_str(), "a+");
			if (fp) {
				fprintf(fp, "#flow auto writed\n");
			}
		}
		if (fp) {
			fprintf(fp, formatString, vh->name.c_str(), vh->flow->flow, vh->flow->cache);
		}
	}
	if (fp) {
		fclose(fp);
	}
}
#endif
bool KVirtualHostManage::vh_base_action(KUrlValue& uv, KString& err_msg) {
	auto action = uv.attribute.remove("action");
	auto name = uv["name"];
	KBaseVirtualHost* bvh = &vhs;
	KSafeVirtualHost vh;
	kconfig::KConfigResult result = kconfig::KConfigResult::ErrUnknow;
	KStringBuf path;

	if (name.size() > 0) {
		vh.reset(refsVirtualHostByName(name));
		bvh = vh.get();
		path << "vh@" << name;
	} else {
		path << "vhs";
	}
	if (action == "vh_add" || action == "vh_edit") {
		auto xml = uv.to_xml(_KS("vh"), name.c_str(), name.size());
		result = kconfig::update(path.str().str(), 0, xml.get(), kconfig::EvUpdate | kconfig::FlagCreate | kconfig::FlagCopyChilds);
	} else {
		if (name.size() > 0 && vh == nullptr) {
			err_msg = "cann't find vh";
			return false;
		}
		if (action == "indexadd") {
			path << "/index";
			auto xml = kconfig::new_xml("index"_CS);
			xml->attributes().emplace("file"_CS, uv.attribute["file"]);
			result = kconfig::add(path.str().str(), uv.attribute.get_int("index"), xml.get());
		} else if (action == "indexdelete") {
			path << "/index";
			result = kconfig::remove(path.str().str(), uv.attribute.get_int("index"));
		} else if (action == "vh_delete") {
			result = kconfig::remove(path.str().str(), 0);
		} else if (action == "redirectadd") {
			auto type = uv.attribute.remove("type"_CS);
			auto value = uv.attribute.remove("value"_CS);
			khttpd::KSafeXmlNode xml;
			if (type == "file_ext") {
				path << "/map_file";
				xml = kconfig::new_xml(_KS("map_file"), value.c_str(), value.size());
				uv.attribute.emplace("ext", value);
				xml->attributes().swap(uv.attribute);
				result = kconfig::update(path.str().str(), 0, xml.get(), kconfig::EvUpdate | kconfig::FlagCreate | kconfig::FlagCopyChilds);
			} else {
				path << "/map_path";
				xml = kconfig::new_xml("map_path"_CS);
				uv.attribute.emplace("path", value);
				xml->attributes().swap(uv.attribute);
				result = kconfig::add(path.str().str(), khttpd::last_pos, xml.get());
			}
		} else if (action == "redirectdelete") {
			auto type = uv.attribute["type"];
			uint32_t index = 0;
			if (type == "file_ext") {
				path << "/map_file";
				path << "@" << uv.attribute["value"];
			} else {
				path << "/map_path";
				index = uv.attribute.get_int("index");
			}
			result = kconfig::remove(path.str().str(), index);
		} else if (action == "mimetypeadd") {
			path << "/mime_type@"_CS << uv.attribute["ext"];
			auto xml = uv.to_xml(_KS("mime_type"), uv.attribute["ext"].c_str(), uv.attribute["ext"].size());
			result = kconfig::update(path.str().str(), 0, xml.get(), kconfig::EvUpdate | kconfig::FlagCreate | kconfig::FlagCopyChilds);
		} else if (action == "mimetypedelete") {
			path << "/mime_type@"_CS << uv.attribute["ext"];
			result = kconfig::remove(path.str().str(), 0);
		} else if (action == "errorpagedelete") {
			path << "/error@"_CS << uv.attribute["code"];
			result = kconfig::remove(path.str().str(), 0);
		} else if (action == "errorpageadd") {
			path << "/error@"_CS << uv.attribute["code"];
			auto xml = uv.to_xml(_KS("error"), uv.attribute["code"].c_str(), uv.attribute["code"].size());
			result = kconfig::update(path.str().str(), 0, xml.get(), kconfig::EvUpdate | kconfig::FlagCreate | kconfig::FlagCopyChilds);
		} else if (action == "aliasdelete") {
			path << "/alias"_CS;
			result = kconfig::remove(path.str().str(), uv.attribute.get_int("index"));
		} else if (action == "aliasadd") {
			auto index = uv.attribute.remove("index");
			path << "/alias"_CS;
			auto xml = uv.to_xml(_KS("alias"));
			result = kconfig::update(path.str().str(), atoi(index.c_str()), xml.get(), kconfig::EvNew);
		} else if (action == "host_add") {
			path << "/host"_CS;
			auto xml = kconfig::new_xml("host"_CS);
			xml->get_first()->set_text(uv.attribute["host"]);
			xml->attributes().emplace("dir", uv.attribute["dir"]);
			if (!uv.attribute["certificate"].empty()) {
				xml->attributes().emplace("certificate", uv.attribute["certificate"]);
				xml->attributes().emplace("certificate_key", uv.attribute["certificate_key"]);
			}
			result = kconfig::update(path.str().str(), khttpd::last_pos, xml.get(), kconfig::EvNew);
		} else if (action == "host_del") {
			path << "/host"_CS;
			result = kconfig::remove(path.str().str(), uv.attribute.get_int("index"));
		} else if (action == "bind_add"_CS) {
			path << "/bind"_CS;
			auto xml = kconfig::new_xml("bind"_CS);
			xml->get_first()->set_text(uv.attribute["bind"]);
			result = kconfig::update(path.str().str(), khttpd::last_pos, xml.get(), kconfig::EvNew);
		} else if (action == "bind_del"_CS) {
			path << "/bind"_CS;
			result = kconfig::remove(path.str().str(), uv.attribute.get_int("index"));
		}
	}
	switch (result) {
	case kconfig::KConfigResult::Success:
		return true;
	case kconfig::KConfigResult::ErrNotFound:
		err_msg = "not found";
		return false;
	case kconfig::KConfigResult::ErrSaveFile:
		err_msg = "config file cann't save";
		return false;
	default:
		err_msg = "unknow error";
		return false;
	}
}
void KVirtualHostManage::getAllVhHtml(KWStream& s) {
	s << "<script language='javascript'>\r\n"
		"	function setbgcolor(id,color){"
		"		document.getElementById(id).style.backgroundColor = color;"
		"	}"
		"</script>\r\n";
	s << "[<a href='/vhlist?id=4'>" << klang["new_vh"] << "</a>] ";
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>" << LANG_NAME << "</td>";
	s << "<td>" << klang["vh_host"] << "</td><td>" << klang["doc_root"] << "</td>";
#ifdef ENABLE_VH_RUN_AS
	s << "<td>" << LANG_RUN_USER << "</td>";
#endif
	s << "<td>" << klang["inherit"] << "</td>";
#ifdef ENABLE_VH_LOG_FILE
	s << "<td>" << klang["log_file"] << "</td>";
#endif
	s << "<td>" << klang["browse"] << "</td>";
#ifdef ENABLE_USER_ACCESS
	s << "<td>" << klang["access_file"] << "</td>";
#endif
#ifdef ENABLE_VH_RS_LIMIT
	s << "<td>" << klang["connect"] << "/" << klang["limit"] << "</td>";
	s << "<td>" << klang["speed"] << "/" << klang["limit"] << "</td>";
#endif
#ifdef ENABLE_VH_FLOW
	s << "<td>" << klang["flow"] << "</td>";
#endif
#ifdef ENABLE_VH_QUEUE
	s << "<td>" << klang["worker"] << "</td>";
	s << "<td>" << klang["queue"] << "</td>";
#endif
	s << "</tr>";
	int id = 0;
	//vh
	for (auto it = avh.begin(); it != avh.end(); it++, id++) {
		getVhIndex(s, (*it).second, id);
	}
	s << "</table>";
	s << "[<a href='/vhlist?id=4'>" << klang["new_vh"] << "</a>]";
}
void KVirtualHostManage::BindGlobalListen(KListenHost* listen) {
	auto lock = locker();
	dlisten.AddGlobal(listen);
}
void KVirtualHostManage::UnBindGlobalListen(KListenHost* listen) {
	auto lock = locker();
	dlisten.RemoveGlobal(listen);
}
void KVirtualHostManage::UnBindGlobalListens(std::vector<KListenHost*>& services) {
	auto lock = locker();
	for (size_t i = 0; i < services.size(); i++) {
		dlisten.RemoveGlobal(services[i]);
	}
	return;
}
void KVirtualHostManage::BindGlobalListens(std::vector<KListenHost*>& services) {
	auto lock = locker();
	for (size_t i = 0; i < services.size(); i++) {
		//防止加载时间太长，而安全进程误认为挂掉。
		setActive();
		dlisten.AddGlobal(services[i]);
	}
	return;
}
int KVirtualHostManage::getCount() {
	auto lock = locker();
	return (int)avh.size();
}
void KVirtualHostManage::GetListenWhm(WhmContext* ctx) {
	auto lock = locker();
	dlisten.GetListenWhm(ctx);
}
void KVirtualHostManage::GetListenHtml(KWStream& s) {
	auto lock = locker();
	dlisten.GetListenHtml(s);
}
void KVirtualHostManage::InternalUnBindVirtualHost(KVirtualHost* vh) {
#ifdef ENABLE_VH_RUN_AS
	if (vh->need_kill_process) {
		vh->KillAllProcess();
	}
#endif
	if (vh->binds.empty()) {
		dlisten.UnbindGlobalVirtualHost(vh);
		return;
	}
#ifdef ENABLE_BASED_PORT_VH
	for (auto&& bind : vh->binds) {
		dlisten.RemoveDynamic(bind.c_str(), vh);
	}
#endif
}
/*
void KVirtualHostManage::updateVirtualHost(KVirtualHost* vh, std::list<KSubVirtualHost*>& hosts) {
	auto vm_locker = locker();
	InternalUnBindVirtualHost(vh);
	{
		auto locker = vh->get_locker();
		vh->hosts.swap(hosts);
	}
	InternalBindVirtualHost(vh);
}

void KVirtualHostManage::updateVirtualHost(KVirtualHost* vh, std::list<KString>& binds) {
	auto vm_locker = locker();
	InternalUnBindVirtualHost(vh);
	{
		auto locker = vh->get_locker();
		vh->binds.swap(binds);
	}
	InternalBindVirtualHost(vh);
}
*/
void KVirtualHostManage::InternalBindVirtualHost(KVirtualHost* vh) {
#ifdef ENABLE_SVH_SSL
	if (ssl_config) {
		for (auto&& svh : vh->hosts) {
			if (svh->ssl_ctx != NULL) {
				continue;
			}
			KSslCertificate* cert = (KSslCertificate*)ssl_config->find(svh->bind_host, svh->wide);
			if (cert) {
				svh->set_ssl_info(cert->cert_file.c_str(), cert->key_file.c_str(), true);
			}
		}
	}
#endif
	if (vh->binds.empty()) {
		dlisten.BindGlobalVirtualHost(vh);
		return;
	}
#ifdef ENABLE_BASED_PORT_VH
	for (auto&& bind : vh->binds) {
		dlisten.AddDynamic(bind.c_str(), vh);
	}
#endif
}
#ifdef ENABLE_SVH_SSL
bool KVirtualHostManage::remove_ssl(const char* domain) {
	auto lock = locker();
	if (ssl_config == NULL) {
		ssl_config = new KDomainMap;
	}
	KSslCertificate* ssl = (KSslCertificate*)ssl_config->unbind(domain);
	if (ssl) {
		delete ssl;
		return true;
	}
	return false;
}
bool KVirtualHostManage::update_ssl(const char* domain, const char* cert_file, const char* key_file) {
	KSslCertificate* cert = new KSslCertificate;
	cert->cert_file = cert_file;
	cert->key_file = key_file;
	auto lock = locker();
	if (ssl_config == NULL) {
		ssl_config = new KDomainMap;
	}
	KSslCertificate* old_ssl = (KSslCertificate*)ssl_config->unbind(domain);
	if (old_ssl) {
		delete old_ssl;
	}
	if (!ssl_config->bind(domain, cert, kgl_bind_unique)) {
		delete cert;
		return false;
	}
	return true;
}
bool KVirtualHostManage::add_ssl(const char* domain, const char* cert_file, const char* key_file) {

	KSslCertificate* cert = new KSslCertificate;
	cert->cert_file = cert_file;
	cert->key_file = key_file;
	auto lock = locker();
	if (ssl_config == NULL) {
		ssl_config = new KDomainMap;
	}
	bool result = ssl_config->bind(domain, cert, kgl_bind_unique);
	if (!result) {
		delete cert;
	}
	return true;
}
#endif
