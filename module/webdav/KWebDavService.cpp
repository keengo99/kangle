/*
 * KWebDavService.cpp
 *
 *  Created on: 2010-8-7
 *      Author: keengo
 */
#include <sstream>
#include <string>
#include <atomic>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "KWebDavService.h"
#include "KDavLock.h"
#include "KHttpKeyValue.h"
#include "KFSResource.h"
#include "KUrl.h"
#include "KHttpLib.h"
#include "KDefer.h"
#include "klog.h"

#define MAX_DEPTH 1
#define MAX_DOCUMENT_SIZE   1048576
//static const char* allowed_header ="OPTIONS,GET,HEAD,POST,DELETE,PROPFIND,PROPPATCH,COPY,MOVE,LOCK,UNLOCK,MKCOL,PUT";
static const char* allowed_header = "OPTIONS,GET,HEAD,POST,DELETE,PROPFIND,PROPPATCH,COPY,MOVE,LOCK,UNLOCK,MKCOL,PUT";
static const char* xml_head = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n";
static const char* xml_dav_ns = "xmlns:D=\"DAV:\"";
/*
static const char
* xml_supported_lock =
"\n<D:supportedlock><D:lockentry><D:lockscope><D:exclusive/></D:lockscope><D:locktype><D:write/></D:locktype>"
"</D:lockentry>" "<D:lockentry>"
"<D:lockscope><D:shared/></D:lockscope>"
"<D:locktype><D:write/></D:locktype>"
"</D:lockentry></D:supportedlock>";
*/
static const char* xml_supported_lock ="\n<D:supportedlock></D:supportedlock>";
KResourceMaker* rsMaker = new KFSResourceMaker;
KDavLockManager lockManager;
using namespace std;
static bool decode_destination_path(char* destination) {
	char* path = strstr(destination, "://");
	if (path) {
		path = strchr(path + 3, '/');
		if (path == NULL) {
			return false;
		}
	} else {
		path = destination;
	}
	char* end = strpbrk(path, "?#");
	if (end) {
		*end = '\0';
	}
	int len = (int)strlen(path);
	if (len > 0) {
		len = url_decode(path, len, NULL, false);
		if (memchr(path, '\0', len) != NULL) {
			return false;
		}
		path[len] = '\0';
	}
	return true;
}
static bool same_resource_path(const char* left, const char* right) {
	if (left == NULL || right == NULL) {
		return false;
	}
#ifdef _WIN32
	return strcasecmp(left, right) == 0;
#else
	return strcmp(left, right) == 0;
#endif
}
KWebDavService::KWebDavService() {
	if_token = NULL;
}
KWebDavService::~KWebDavService() {
	if (if_token) {
		xfree(if_token);
	}
}
bool KWebDavService::service(KISAPIServiceProvider* provider) {
	this->provider = provider;
	try {
		switch (provider->getMethod()) {
		case METH_OPTIONS:
			return doOptions();
		case METH_PUT:
			return doPut();
		case METH_LOCK:
			return doLock();
		case METH_UNLOCK:
			return doUnlock();
		case METH_PROPFIND:
			return doPropfind();
		case METH_PROPPATCH:
			return doProppatch();
		case METH_MKCOL:
			return doMkcol();
		case METH_MOVE:
			return doMove();
		case METH_COPY:
			return doCopy();
		case METH_DELETE:
			return doDelete();
		case METH_GET:
			return doGet(false);
		case METH_HEAD:
			return doGet(true);
		default:
			provider->sendUnknowHeader("Allow", allowed_header);
			return send(STATUS_METH_NOT_ALLOWED);
		}
	} catch (...) {
		return send(STATUS_SERVER_ERROR);
	}
}
uint16_t KWebDavService::check_file_locked(KLockToken *lock_token)
{
	if (lock_token == nullptr) {
		return STATUS_OK;
	}
	const char* if_token = getIfToken();
	if (if_token == nullptr) {
		return STATUS_LOCKED;
	}
	if (strcasecmp(lock_token->getValue(), if_token) != 0) {
		return STATUS_PRECONDITION;
	}
	return STATUS_OK;
}
bool KWebDavService::send(int status_code) {
	return provider->sendStatus(status_code, NULL);
}
bool KWebDavService::doGet(bool head) {
	KResource* rs = rsMaker->bindResource(provider->getFileName(), provider->getRequestUri());		
	if (rs == NULL) {
		//fprintf(stderr, "cann't bind resource filename=[%s] uri=[%s]\n", provider->getFileName(), provider->getRequestUri());
		return send(STATUS_NOT_FOUND);
	}
	delete rs;
	provider->sendUnknowHeader("Cache-Control", "no-cache,no-store");
	provider->sendUnknowHeader("X-Accel-Redirect", provider->getRequestUri());
	return send(STATUS_OK);
}
bool KWebDavService::doCopy() {
	khttpd::KXmlDocument document;
	if (!parseDocument(document)) {
	}
	char destination[1024];
	int len = sizeof(destination);
	if (!provider->getEnv("HTTP_DESTINATION", destination, &len) || len <= 0) {
		return send(STATUS_BAD_REQUEST);
	}
	std::string destination_url(destination, len);
	if (!decode_destination_path(destination)) {
		return send(STATUS_BAD_REQUEST);
	}

	DWORD destination_len = sizeof(destination);
	if (provider->pECB->ServerSupportFunction(provider->pECB->ConnID, HSE_REQ_MAP_URL_TO_PATH, destination, &destination_len, NULL) == FALSE) {
		return send(STATUS_BAD_REQUEST);
	}
	if (same_resource_path(provider->getFileName(), destination)) {
		return send(STATUS_FORBIDEN);
	}

	auto lock_token = lockManager.find_file_locked(destination);
	auto status_code = check_file_locked(lock_token);
	if (lock_token) {
		lock_token->release();
	}
	if (status_code != STATUS_OK) {
		return send(status_code);
	}

	auto rs = rsMaker->bindResource(provider->getFileName(), provider->getRequestUri());
	if (rs == NULL) {
		return send(STATUS_NOT_FOUND);
	}
	defer(delete rs);
	int status = STATUS_CREATED;
	if (!rs->open(false)) {
		return send(STATUS_FORBIDEN);
	}
	auto existing_dst = rsMaker->bindResource(destination, "/");
	if (existing_dst) {
		defer(delete existing_dst);
		char* overWriteHeader = provider->getHttpHeader("Overwrite");
		if (overWriteHeader) {
			if (strcasecmp(overWriteHeader, "T") != 0) {
				provider->freeHttpHeader(overWriteHeader);
				return send(STATUS_PRECONDITION);
			}
			provider->freeHttpHeader(overWriteHeader);
		}
		status = STATUS_NO_CONTENT;
		if (!existing_dst->remove()) {
			return send(STATUS_FORBIDEN);
		}
	}
	auto dst = rsMaker->makeFile(destination, "/");
	if (dst == nullptr) {
		return send(STATUS_FORBIDEN);
	}
	defer(delete dst);
	if (!rs->copy(dst)) {
		return send(STATUS_FORBIDEN);
	}
	send(status);
	return provider->sendUnknowHeader("Location", destination_url.c_str());
	
}
bool KWebDavService::doDelete() {
	auto lock_token = lockManager.find_file_locked(provider->getFileName());
	auto status_code = check_file_locked(lock_token);
	if (lock_token) {
		lock_token->release();
	}
	if (status_code != STATUS_OK) {
		return send(status_code);
	}
	KResource* rs = rsMaker->bindResource(provider->getFileName(), provider->getRequestUri());
	if (rs == NULL) {
		return send(STATUS_NOT_FOUND);
	}
	if (!rs->remove()) {
		delete rs;
		return send(STATUS_FORBIDEN);
	}
	delete rs;
	return send(STATUS_NO_CONTENT);
}
bool KWebDavService::doOptions() {
	send(STATUS_OK);
	provider->sendUnknowHeader("Allow", allowed_header);
	provider->sendUnknowHeader("DAV", "1,2,3");
	return true;
}
bool KWebDavService::doPut() {	
	auto lock_token = lockManager.find_file_locked(provider->getFileName());
	auto status_code = check_file_locked(lock_token);
	if (lock_token) {
		lock_token->release();
	}
	if (status_code != STATUS_OK) {
		return send(status_code);
	}
	int64_t content_length = provider->getContentLength();
	bool unknown_length = content_length < 0;
	const char* file_name = provider->getFileName();
	struct stat old_st;
	bool existed = stat(file_name, &old_st) == 0;
	KResource* rs = NULL;
	std::string tmp_name;
	if (existed && S_ISREG(old_st.st_mode)) {
		/* Write next to the file and rename over it once the body is
		 * complete, so a broken upload leaves the old content in place. */
		static std::atomic<unsigned> seq{ 0 };
		std::stringstream s;
		s << file_name << ".kangle-put-" << getpid() << "-" << seq++;
		tmp_name = s.str();
		rs = rsMaker->makeFile(tmp_name.c_str(), provider->getRequestUri());
		if (rs == NULL) {
			/* no write access to the directory: write in place as before */
			tmp_name.clear();
		}
	}
	if (rs == NULL) {
		rs = rsMaker->makeFile(file_name, provider->getRequestUri());
	}
	if (rs == NULL) {
		return send(STATUS_FORBIDEN);
	}
	KRStream* in = provider->getInputStream();
	if (in == NULL) {
		delete rs;
		return send(STATUS_SERVER_ERROR);
	}
	bool result = true;
	while (unknown_length || content_length > 0) {
		char buf[512];
		int this_read_len = unknown_length ? (int)sizeof(buf) : (int)(KGL_MIN(content_length, (int64_t)sizeof(buf)));
		int actual_read_len = in->read(buf, this_read_len);
		if (unknown_length && actual_read_len == 0) {
			break;
		}
		if (actual_read_len <= 0) {
			result = false;
			break;
		}
		if (!unknown_length) {
			content_length -= actual_read_len;
		}
		if (rs->write(buf, actual_read_len) != actual_read_len) {
			result = false;
			break;
		}
	}
	rs->close();
	if (!tmp_name.empty()) {
		if (result) {
#ifndef _WIN32
			chmod(tmp_name.c_str(), old_st.st_mode & 07777);
#endif
			result = rs->rename(file_name);
		}
		if (!result) {
			rs->remove();
		}
	} else if (!result && !existed) {
		rs->remove();
	}
	delete rs;
	if (result) {
		return send(existed ? STATUS_NO_CONTENT : STATUS_CREATED);
	}
	return send(STATUS_SERVER_ERROR);
}
bool KWebDavService::parseDocument(khttpd::KXmlDocument& document) {
	int64_t content_length = provider->getContentLength();
	if (content_length > MAX_DOCUMENT_SIZE) {
		return false;
	}
	if (content_length == 0) {
		return false;
	}
	KRStream* in = provider->getInputStream();
	char* buf;
	if (content_length < 0) {
		//长度未知(chunked),读到结束,最多MAX_DOCUMENT_SIZE
		buf = (char*)xmalloc(MAX_DOCUMENT_SIZE + 1);
		int len = 0;
		for (;;) {
			//多读1字节，以区分恰好达到上限和确实超过上限。
			int got = in->read(buf + len, MAX_DOCUMENT_SIZE + 1 - len);
			if (got == 0) {
				break;
			}
			if (got < 0) {
				xfree(buf);
				return false;
			}
			len += got;
			if (len > MAX_DOCUMENT_SIZE) {
				xfree(buf);
				return false;
			}
		}
		if (len == 0) {
			xfree(buf);
			return false;
		}
		buf[len] = '\0';
	} else {
		buf = (char*)xmalloc(content_length + 1);
		if (!in->read_all(buf, (int)content_length)) {
			xfree(buf);
			return false;
		}
		buf[content_length] = '\0';
	}
	try {
		document.parse(buf);
		xfree(buf);
		return true;
	} catch (...) {
		xfree(buf);
		return false;
	}

}
const char* KWebDavService::getIfToken() {
	if (if_token) {
		return if_token;
	}
	char* if_val = provider->getHttpHeader("If");
	if (if_val == NULL) {
		return NULL;
	}
	char* p = strstr(if_val, "(<");
	if (p == NULL) {
		provider->freeHttpHeader(if_val);
		return NULL;
	}
	if_token = xstrdup(p + 2);
	provider->freeHttpHeader(if_val);
	p = strchr(if_token, '>');
	if (p) {
		*p = '\0';
	}
	return if_token;

}
bool KWebDavService::response_lock_body(KLockToken* token)
{
	writeXmlHeader();
	KWStream* s = provider->getOutputStream();
	*s << "<D:prop " << xml_dav_ns << "><D:lockdiscovery><D:activelock>";
	*s << "<D:locktype><D:write/></D:locktype>";
	*s << "<D:lockscope><D:";
	switch (token->getType()) {
	case Lock_exclusive:
		*s << "exclusive";
		break;
	case Lock_share:
		*s << "shared";
		break;
	default:
		*s << "none";
		break;
	}
	*s << "/></D:lockscope>\n";
	*s << "<D:depth>0</D:depth>";
	*s << "<D:locktoken><D:href>" << token->getValue() << "</D:href>";
	*s << "</D:locktoken></D:activelock>\n";
	*s << "</D:lockdiscovery></D:prop>";
	return true;
}
bool KWebDavService::doLock() {
	khttpd::KXmlDocument document;
	char ips[255];
	int len = sizeof(ips);
	if (!provider->getEnv("REMOTE_ADDR", ips, &len) || len <= 0) {
		return send(STATUS_BAD_REQUEST);
	}
	if (!parseDocument(document)) {
		//flush the lock
		auto lock_token = lockManager.find_file_locked(provider->getFileName());
		if (lock_token == nullptr) {
			return send(STATUS_BAD_REQUEST);
		}
		defer(lock_token->release());
		const char* if_token = getIfToken();
		if (if_token == nullptr) {
			return send(STATUS_BAD_REQUEST);
		}
		if (strcasecmp(lock_token->getValue(), if_token) != 0) {
			return send(STATUS_PRECONDITION);
		}
		lock_token->refresh();
		send(STATUS_OK);
		return response_lock_body(lock_token);
	}
	auto node = document.getRootNode();
	if (node == NULL) {
		return send(STATUS_BAD_REQUEST);
	}
	//printf("node tag=[%s]\n",node->getTag().c_str());
	if (node->get_tag() != "lockinfo") {
		return send(STATUS_BAD_REQUEST);
	}
	Lock_type type = Lock_none;
	if (node->find_child("lockscope/exclusive")) {
		type = Lock_exclusive;
	} else if (node->find_child("lockscope/shared")) {
		type = Lock_share;
	}
	if (type == Lock_none) {
		return send(STATUS_BAD_REQUEST);
	}
	KLockToken* token = lockManager.new_token(ips, type, 3600);
	if (token == NULL) {
		return send(STATUS_SERVER_ERROR);
	}
	defer(token->release());
	//printf("lock file=[%s]\n", provider->getFileName());
	Lock_op_result result = lockManager.lock(provider->getFileName(), token);
	if (result != Lock_op_success) {
		return send(423);
	}
	KResource* rs = rsMaker->bindResource(provider->getFileName(), "/");
	if (rs == nullptr) {
		rs = rsMaker->makeFile(provider->getFileName(), "/");
		if (rs == nullptr) {
			lockManager.unlock(token);
			return send(STATUS_FORBIDEN);
		}
		send(STATUS_CREATED);
	} else {
		send(STATUS_OK);
	}
	delete rs;
	std::stringstream h;
	h << "<" << token->getValue() << ">";
	provider->sendUnknowHeader("Lock-Token", h.str().c_str());
	return response_lock_body(token);
}
bool KWebDavService::doUnlock() {
	char token_header[512];
	int len = sizeof(token_header);
	if (!provider->getEnv("HTTP_LOCK_TOKEN", token_header, &len)) {
		fprintf(stderr, "cann't found HTTP_LOCK_TOKEN\n");
		return send(STATUS_BAD_REQUEST);
	}
	char* token = strchr(token_header, '<');
	if (token == nullptr) {
		return send(STATUS_BAD_REQUEST);
	}
	token++;
	char* e = strrchr(token_header, '>');
	if (e == NULL || e <= token) {
		return send(STATUS_BAD_REQUEST);
	}
	*e = '\0';
	auto lock_token = lockManager.find_file_locked(provider->getFileName());
	if (lock_token == nullptr) {
		return send(STATUS_CONFLICT);
	}
	defer(lock_token->release());
	if (strcasecmp(lock_token->getValue(), token) != 0) {
		return send(STATUS_FORBIDEN);
	}
	lockManager.unlock(lock_token);
	return send(STATUS_NO_CONTENT);
}
bool KWebDavService::doMkcol() {
	KResource* rs = rsMaker->makeDirectory(provider->getFileName(),provider->getRequestUri());
	if (rs) {
		send(STATUS_CREATED);
		delete rs;
	} else {
		send(STATUS_CONFLICT);
	}
	return true;
}
bool KWebDavService::doProppatch() {
	khttpd::KXmlDocument document;
	if (!parseDocument(document)) {
		return send(STATUS_BAD_REQUEST);
	}
	auto root = document.getRootNode();
	if (root == NULL || root->get_tag() != "propertyupdate") {
		return send(STATUS_BAD_REQUEST);
	}
	auto node = document.getNode("propertyupdate/set/prop");
	if (node != NULL) {
		//todo for set;
	}
	node = document.getNode("propertyupdate/remove/prop");
	//todo:for remove
	KResource* rs = rsMaker->bindResource(provider->getFileName(),
		provider->getRequestUri());
	if (rs == NULL) {
		return send(STATUS_NOT_FOUND);
	}
	KWStream* s = provider->getOutputStream();
	provider->sendStatus(STATUS_MULTI_STATUS, NULL);
	writeXmlHeader();
	*s << "<D:multistatus " << xml_dav_ns << ">\n";
	bool result = writeResourceProp(rs);
	*s << "</D:multistatus>\n";
	delete rs;
	return result;
	//return send(STATUS_NOT_IMPLEMENT);
}
bool KWebDavService::writeXmlHeader() {
	provider->sendUnknowHeader("Content-Type", "text/xml; charset=utf-8");
	return provider->getOutputStream()->write_all(xml_head)
		== STREAM_WRITE_SUCCESS;

}
bool KWebDavService::listResourceProp(KResource* rs, int depth) {
	//KWStream *s = provider->getOutputStream();
	if (!writeResourceProp(rs)) {
		return false;
	}
	bool result = true;
	//	printf("depth=%d\n",depth);
	if (depth-- > 0 && rs->isDirectory()) {
		std::list<KResource*> childs;
		rs->listChilds(childs);
		std::list<KResource*>::iterator it;
		for (it = childs.begin(); it != childs.end(); it++) {
			result = listResourceProp(*it, depth);
			if (!result) {
				break;
			}
		}
		for (it = childs.begin(); it != childs.end(); it++) {
			delete (*it);
		}
	}
	return result;
}
bool KWebDavService::doPropfind() {
	khttpd::KXmlDocument document;
	parseDocument(document);

	char* depth_header = provider->getHttpHeader("Depth");
	//printf("filename=[%s]\n", provider->getFileName());
	int depth = 0;
	if (depth_header) {
		if (strcasecmp(depth_header, "1") == 0) {
			depth = 1;
		} else if (strcasecmp(depth_header, "infinity") == 0) {
			depth = MAX_DEPTH;
		}
		provider->freeHttpHeader(depth_header);
	}
	KResource* rs = rsMaker->bindResource(provider->getFileName(), provider->getRequestUri());
	if (rs == NULL) {
		return send(STATUS_NOT_FOUND);
	}
	if (rs->isDirectory()) {
		char* path = rs->getPath();
		int len = (int)strlen(path);
		if (len > 1) {
			if (path[len - 1] == '/') {
				path[len - 1] = '\0';
#if 0
			} else {
				KStringBuf s;
				s << url_encode(path, strlen(path)).c_str() << "/";
				send(STATUS_MOVED);
				provider->sendUnknowHeader("Location", s.getString());
				delete rs;
				return true;
#endif
			}
		}
	}

	KWStream* s = provider->getOutputStream();
	provider->sendStatus(STATUS_MULTI_STATUS, NULL);
	writeXmlHeader();
	*s << "<D:multistatus " << xml_dav_ns << ">\n";
	bool result = listResourceProp(rs, depth);
	*s << "</D:multistatus>\n";
	delete rs;
	return result;
}
bool KWebDavService::writeResourceProp(KResource* rs) {
	KWStream* s = provider->getOutputStream();
	KAttribute* attribute = rs->getAttribute();
	*s << "<D:response>\n";
	*s << "<D:href>" << url_encode(rs->getPath(), 0);
	if (rs->isDirectory()) {
		*s << "/";
	}
	*s << "</D:href>\n";
	*s << "<D:propstat><D:prop>\n";
	if (attribute) {		
		for (auto it = attribute->begin(); it != attribute->end(); it++) {
			*s << "<D:" << (*it).first << ">" << (*it).second << "</D:"
				<< (*it).first << ">\n";
		}
	}
	if (rs->isDirectory()) {
		*s << "<D:resourcetype><D:collection/></D:resourcetype>";
	} else {
		*s << "<D:resourcetype/>";
	}
	*s << xml_supported_lock;
	*s << "</D:prop>\n";
	*s << "<D:status>HTTP/1.1 200 OK</D:status>\n";
	*s << "</D:propstat>\n</D:response>\n";
	delete attribute;
	return true;
}
bool KWebDavService::doMove() {
	khttpd::KXmlDocument document(true);
	if (!parseDocument(document)) {
	}
	char destination[1024];
	int len = sizeof(destination);
	if (!provider->getEnv("HTTP_DESTINATION", destination, &len) || len <= 0) {
		return send(STATUS_BAD_REQUEST);
	}
	std::string destination_url(destination, len);
	if (!decode_destination_path(destination)) {
		return send(STATUS_BAD_REQUEST);
	}
	DWORD destination_len = sizeof(destination);
	if (provider->pECB->ServerSupportFunction(provider->pECB->ConnID, HSE_REQ_MAP_URL_TO_PATH, destination, &destination_len, NULL) == FALSE) {
		return send(STATUS_BAD_REQUEST);
	}
	if (same_resource_path(provider->getFileName(), destination)) {
		return send(STATUS_FORBIDEN);
	}
	auto src_token = lockManager.find_file_locked(provider->getFileName());
	auto status_code = check_file_locked(src_token);
	if (src_token) {
		src_token->release();
	}
	if (status_code != STATUS_OK) {
		return send(status_code);
	}
	auto dst_lock_token = lockManager.find_file_locked(destination);
	status_code = check_file_locked(dst_lock_token);
	if (dst_lock_token) {
		dst_lock_token->release();
	}
	if (status_code != STATUS_OK) {
		return send(status_code);
	}
	KResource* rs = rsMaker->bindResource(provider->getFileName(), provider->getRequestUri());
	if (rs == NULL) {
		return send(STATUS_NOT_FOUND);
	}
	int status = STATUS_CREATED;
	//TODO:check lock
	KResource* rsd = rsMaker->bindResource(destination, "/");
	if (rsd) {
		char* overWriteHeader = provider->getHttpHeader("Overwrite");
		if (overWriteHeader) {
			if (strcasecmp(overWriteHeader, "T") != 0) {
				delete rsd;
				delete rs;
				provider->freeHttpHeader(overWriteHeader);
				return send(STATUS_PRECONDITION);
			}
			provider->freeHttpHeader(overWriteHeader);
		}
		status = STATUS_NO_CONTENT;
		if (!rsd->remove()) {
			delete rsd;
			delete rs;
			return send(STATUS_FORBIDEN);
		}
		delete rsd;
	}
	if (rs->rename(destination)) {
		send(status);
		provider->sendUnknowHeader("Location", destination_url.c_str());
	} else {
		send(STATUS_FORBIDEN);
	}
	delete rs;
	return true;
}
