#include <stdlib.h>
#include <string.h>
#include <vector>
#include "KXml.h"
#include "KRewriteMarkEx.h"
#include "http.h"
#include "KSubVirtualHost.h"
#include "KExtendProgram.h"
#include "http.h"
#include "KCdnContainer.h"
#include "kmalloc.h"
#include "KBufferFetchObject.h"

bool check_path_info(const char *file,struct _stat64 *buf)
{
	bool result = false;
	char *str = strdup(file);
	char *end = str + strlen(str) - 1;
	while (end>str) {
		if(*end=='/' 
#ifdef _WIN32
			|| *end=='\\'
#endif
			){
				*end = '\0';
				result = (_stati64(str, buf) == 0);
				if (result) {
					break;
				}
		}
		end--;
	}
	free(str);
	if (result) {
		//排除目录
		result = (S_ISDIR(buf->st_mode) == 0);
	}
	return result;
}
KRewriteRule::KRewriteRule() {
	dst = NULL;
	qsa = false;
	proxy = NULL;
	internal = true;
	revert = false;
	code = 0;
}
KRewriteRule::~KRewriteRule() {
	if (dst) {
		xfree(dst);
	}
	if (proxy) {
		xfree(proxy);
	}
}
void KRewriteRule::buildXml(std::stringstream &s)
{
	s << " path='";
	if (revert) {
		s << "!";
	}
	s << reg.getModel() << "'";
	s << " dst='" << (dst?dst:"") << "'";
	s << " internal='" << (internal ? "1" : "0") << "'";
	s << " nc='" << (nc ? "1" : "0") << "'";
	if (qsa) {
		s << " qsa='1'";
	}
	if (proxy) {
		s << " proxy='" << proxy << "'";
	}
	if (!internal && code>0) {
		s << " code='" << code << "'";
	}
}
bool KRewriteRule::parse(const KXmlAttribute& attribute)
{
	if (attribute["nc"] == "1") {
		nc = true;
	} else {
		nc = false;
	}
	if (attribute["qsa"]=="1") {
		qsa = true;
	}
	if (attribute["internal"] == "1") {
		internal = true;
	} else {
		internal = false;
	}
	code = atoi(attribute["code"].c_str());
	if (proxy) {
		xfree(proxy);
		proxy = NULL;
	}
	if (!attribute["proxy"].empty()) {
		proxy = strdup(attribute["proxy"].c_str());
	}
	const char *path = attribute["path"].c_str();
	if (*path=='!') {
		//反转
		revert = true;
		path ++;
	} else {
		revert = false;
	}
	reg.setModel(path, (nc ? KGL_PCRE_CASELESS : 0));
	if(dst){
		xfree(dst);
	}
	dst = xstrdup(attribute["dst"].c_str());
	return true;
}
uint32_t KRewriteRule::mark(KHttpRequest *rq, KHttpObject *obj,
						std::list<KRewriteCond *> *conds,const KString &prefix,const char *rewriteBase, KSafeSource &fo) {
	size_t len = strlen(rq->sink->data.url->path);
	if (len < prefix.size()) {
		return KF_STATUS_REQ_FALSE;
	}
	//测试path
	KRegSubString *subString = reg.matchSubString(rq->sink->data.url->path + prefix.size(), (int)(len - prefix.size()), 0);
	bool match_result = (subString!=NULL);
	if (revert==match_result) {
		if (subString) {
			delete subString;
		}
		return KF_STATUS_REQ_FALSE;
	}
	KRegSubString *lastCond = NULL;
	bool result = true;
	if (conds) {
		//测试条件
		std::list<KRewriteCond *>::iterator it;
		for (it = conds->begin(); it != conds->end(); it++) {
			if (result && (*it)->is_or) {
				continue;
			}
			if (!result && !(*it)->is_or) {
				break;
			}
			auto str = KRewriteMarkEx::getString(NULL, (*it)->str, rq, NULL, subString);
			result = (*it)->testor->test(str->c_str(), &lastCond);
			if ((*it)->revert) {
				result = !result;
			}
			delete str;
		}
	}
	if (!result || dst == NULL || strcmp(dst, "-") == 0) {
		if (subString) {
			delete subString;
		}
		if (lastCond) {
			delete lastCond;
		}
		return result? KF_STATUS_REQ_TRUE: KF_STATUS_REQ_FALSE;
	}
	auto url = KRewriteMarkEx::getString(
		NULL,
		dst,
		rq,
		lastCond, 
		subString
		);
	if (url) {
		const char *param = rq->sink->data.url->param;
		if (param) {
			if (qsa) {
				//append the query string
				if (strchr(url->c_str(), '?')) {
					*url << "&" << param;
				} else {
					*url << "?" << param;
				}
			} else if (strchr(url->c_str(), '?')==NULL) {
				*url << "?" << param;
			}
		}
		if (proxy && *proxy=='-') {
			rq->rewrite_url(url->c_str(),0,(rewriteBase?rewriteBase:prefix.c_str()));
			const char *ssl = NULL;
			if (KBIT_TEST(rq->sink->data.url->flags, KGL_URL_SSL)) {
				ssl = "s";
			}
			fo.reset(server_container->get(NULL, rq->sink->data.url->host, rq->sink->data.url->port, ssl, 0));
		} else {
			bool internal_flag = internal;
			const char *u = url->c_str();
			if (internal_flag && proxy==NULL) {
				if (strncasecmp(u,"http://",7)==0
					|| strncasecmp(u,"https://",8)==0
					|| strncasecmp(u,"ftp://",6)==0
					|| strncasecmp(u,"mailto:",7)==0) {
					internal_flag = false;
				}
			}
			if (internal_flag) {
				rq->rewrite_url(u,0,(rewriteBase?rewriteBase:prefix.c_str()));
				if (proxy) {
					auto proxy_host = KRewriteMarkEx::getString(
						NULL,
						proxy,
						rq,
						lastCond,
						subString
						);
					if (proxy_host) {
						fo.reset(server_container->get(proxy_host->c_str()));
						delete proxy_host;
					}
				}
			} else {
				if (push_redirect_header(rq, u, (int)strlen(u), code)) {
					fo.reset(new KBufferFetchObject(nullptr, 0));
				}
			}
		}
		delete url;
	}
	if (subString) {
		delete subString;
	}
	if (lastCond) {
		delete lastCond;
	}
	return KF_STATUS_REQ_TRUE;
}
bool KFileAttributeTestor::test(const char *str, KRegSubString **lastSubString) {
	struct _stat64 buf;
	bool exsit = (_stati64(str, &buf) == 0);
	if (!exsit && conf.path_info && type!='d') {
		//检查path_info情况
		exsit = check_path_info(str,&buf);
	}
	if (!exsit) {
		return false;
	}
	switch (type) {
	case 'd':
		return S_ISDIR(buf.st_mode) > 0;
	case 'f':
		return S_ISREG(buf.st_mode) > 0;
	case 's':
		if (!S_ISREG(buf.st_mode)) {
			return false;
		}
		return buf.st_size > 0;
#ifndef _WIN32
	case 'l':
		return S_ISLNK(buf.st_mode) > 0;
	case 'x':
		return KBIT_TEST(buf.st_mode,S_IXOTH);
#endif
	}
	return false;
}

KRewriteMarkEx::KRewriteMarkEx(void) {
}
KRewriteMarkEx::~KRewriteMarkEx(void) {
	for (auto it = conds.begin(); it != conds.end(); it++) {
		delete (*it);
	}
	for (auto it = rules.begin(); it != rules.end(); it++) {
		delete (*it);
	}
}
uint32_t KRewriteMarkEx::process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) {
	uint32_t result = KF_STATUS_REQ_TRUE;
	for (auto it2 = rules.begin(); it2 != rules.end(); it2++) {
		std::list<KRewriteCond *> *conds = NULL;
		if (it2==rules.begin()) {
			conds = &this->conds;
		}
		result = (*it2)->mark(rq, obj, conds, prefix, (rewriteBase.size() > 0 ? rewriteBase.c_str() : NULL), fo);
		if (!result || fo) {
			break;
		}
	}
	return result;
}
KMark *KRewriteMarkEx::new_instance() {
	return new KRewriteMarkEx();
}
const char *KRewriteMarkEx::get_module() const {
	return "rewritex";
}
void KRewriteMarkEx::get_html(KWStream &s) {
	s <<  "not support in manage model";
}
void  KRewriteMarkEx::get_display(KWStream& s) {
	s <<  "not support in manage model";
}
void KRewriteMarkEx::parse_config(const khttpd::KXmlNodeBody* body) {
	prefix = body->attributes["prefix"];
	rewriteBase = body->attributes["rewrite_base"];
	return;
}
void KRewriteMarkEx::parse_child(const kconfig::KXmlChanged* changed) {
	auto xml = changed->get_xml();
	if (xml->is_tag(_KS("rule"))) {
		for (uint32_t index = 0;; ++index) {
			auto body = xml->get_body(index);
			if (!body) {
				break;
			}
			KRewriteRule* rule = new KRewriteRule;
			rule->parse(body->attributes);
			rules.push_back(rule);
		}
		return;
	}
	if (xml->is_tag(_KS("cond"))) {
		for (uint32_t index = 0;; ++index) {
			auto body = xml->get_body(index);
			if (!body) {
				break;
			}
			auto cond = new KRewriteCond;
			if (!cond->parse_config(body)) {
				delete cond;
				continue;
			}
			conds.push_back(cond);
		}
		return;
	}
}
void KRewriteMarkEx::getEnv(KHttpRequest *rq, char *env, KWStream&s) {
	if (strncasecmp(env, "LA-U:", 5) == 0 || strncasecmp(env, "LA-F:", 5) == 0) {
		env += 5;
	}
	if (strcasecmp(env, "REQUEST_FILENAME") == 0 || strcasecmp(env,
			"SCRIPT_FILENAME") == 0) {
		if (rq->file==NULL && rq->sink->data.opaque) {
			bool exsit;
			kangle::get_virtual_host(rq)->bindFile(rq,exsit,true,true);
		}
		if (rq->file) {
			s << rq->file->getName();
		}
		return;
	}
	if (strcasecmp(env, "DOCUMENT_ROOT") == 0) {
		auto svh = kangle::get_virtual_host(rq);
		if (svh) {
			s << svh->doc_root;
		}
		return;
	}
	if (strcasecmp(env, "SERVER_PORT") == 0) {
		s << rq->sink->get_self_port();
		return;
	}
	if (strcasecmp(env, "SCHEMA") == 0) {
		if (KBIT_TEST(rq->sink->data.raw_url.flags,KGL_URL_SSL)) {
			s << "https";
		} else {
			s << "http";
		}
		return;
	}
	if (strcasecmp(env, "SERVER_PROTOCOL") == 0) {
		s << "HTTP/1.1";
		return;
	}
	if (strcasecmp(env, "SERVER_SOFTWARE") == 0) {
		s << PROGRAM_NAME << "/" << VERSION;
		return;
	}
	if (strcasecmp(env,"SERVER_NAME") == 0) {
		s << rq->sink->data.url->host;
		return;
	}
	if (strcasecmp(env, "REMOTE_ADDR") == 0 || strcasecmp(env, "REMOTE_HOST") == 0) {
		s << rq->getClientIp();
		return;
	}
	if (strcasecmp(env, "REMOTE_PORT") == 0) {
		s << ksocket_addr_port(rq->sink->get_peer_addr());
		return;
	}
	if (strcasecmp(env, "REQUEST_METHOD") == 0) {
		s << rq->get_method();
		return;
	}
	if (strcasecmp(env, "PATH_INFO") == 0 || strcasecmp(env, "REQUEST_URI")
			== 0) {
		s << rq->sink->data.url->path;
		return;
	}
	if (strcasecmp(env, "QUERY_STRING") == 0) {
		const char *param = rq->sink->data.url->param;
		if (param) {
			s << param;
		}
		return;
	}
	if (strcasecmp(env, "THE_REQUEST") == 0) {
		s << rq->get_method() << " " << rq->sink->data.url->path;
		if (rq->sink->data.url->param) {
			s << "?" << rq->sink->data.url->param;
		}
		s << " HTTP/1.1";
		return;
	}
	if (strcasecmp(env, "HTTPS") == 0) {
		if (KBIT_TEST(rq->sink->data.url->flags,KGL_URL_SSL)) {
			s << "on";
		} else {
			s << "off";
		}
		return;
	}
	if (strcasecmp(env, "HTTP_HOST") == 0) {
		rq->sink->data.url->GetHost(s);
		return;
	}
	if (strncasecmp(env, "HTTP_", 5) == 0 || strncasecmp(env, "HTTP:", 5) == 0) {
		if (env[4] == '_') {
			env += 5;
			char *p = env;
			while (*p) {
				if (*p == '_') {
					*p = '-';
				}
				p++;
			}
		} else {
			env += 5;
		}
		KHttpHeader *av = rq->sink->data.get_header();
		size_t env_len = strlen(env);
		while (av) {
			if (kgl_is_attr(av,env,env_len)) {
				s.write_all(av->buf+av->val_offset, av->val_len);
				return;
			}
			av = av->next;
		}
	}
}
void KRewriteMarkEx::getString(const char *prefix, const char *str,KHttpRequest *rq, KRegSubString *s1, KRegSubString *s2,KWStream *s)
{
	KExtendProgramString ds(NULL,(rq && rq->sink->data.opaque?kangle::get_virtual_host(rq)->vh : NULL));	
	char *buf = xstrdup(str);
	char *hot = buf;
	if (prefix) {
		*s << prefix;
	}
	bool slash = false;
	for (;;) {
		const char c = *hot;
		hot++;
		if (c == '\0') {
			break;
		}
		if (slash) {
			*s << c;
			slash = false;
			continue;
		}
		if (c=='\\') {
			slash = true;
			continue;
		}
		if (c == '%') {
			if (*hot == '\0') {
				break;
			}
			if (*hot == '{') {
				//it is env
				char *p = strchr(hot, '}');
				if (p == NULL) {
					*s << c;
					continue;
				}
				*p = '\0';
				getEnv(rq, hot + 1, *s);
				hot = p + 1;
				continue;
			}
			if (isdigit(*hot) && s1) {
				char *ss = s1->getString(atoi(hot));
				if (ss) {
					*s << ss;
				}
			}
			hot++;
		} else if (c == '$') {
			if (*hot == '\0') {
				break;
			}
			if (*hot == '{') {
				char *p = strchr(hot, '}');
				if (p == NULL) {
					*s << c;
					continue;
				}
				*p = '\0';
				const char *tmp = ds.interGetValue(hot+1);
				if (tmp!=NULL) {
					*s << tmp;
				}
				hot = p + 1;
				continue;
			}
			if (isdigit(*hot) && s2) {
				char *ss = s2->getString(atoi(hot));
				if (ss) {
					*s << ss;
				}
			}
			hot++;
		} else {
			*s << c;
		}
	}
	xfree(buf);
}
KStringStream* KRewriteMarkEx::getString(const char *prefix, const char *str,
		KHttpRequest *rq, KRegSubString *s1, KRegSubString *s2) {
	KStringStream*s = new KStringStream;
	getString(prefix,str,rq,s1,s2,s);
	return s;
}
