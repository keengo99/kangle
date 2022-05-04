/*
 * KWebDavService.cpp
 *
 *  Created on: 2010-8-7
 *      Author: keengo
 */
#include <sstream>
#include "KWebDavService.h"
#include "KDavLock.h"
#include "KHttpKeyValue.h"
#include "lib.h"
#include "KFSResource.h"

#define MAX_DEPTH 1
#define MAX_DOCUMENT_SIZE   1048576
static const char
		*allowed_header =
				"OPTIONS,GET,HEAD,POST,DELETE,PROPFIND,PROPPATCH,COPY,MOVE,LOCK,UNLOCK,MKCOL,PUT";
static const char *xml_head = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n";
static const char *xml_dav_ns = "xmlns:D=\"DAV:\"";
static const char
		*xml_supported_lock =
				"\n<D:supportedlock><D:lockentry><D:lockscope><D:exclusive/></D:lockscope><D:locktype><D:write/></D:locktype>"
					"</D:lockentry>" "<D:lockentry>"
					"<D:lockscope><D:shared/></D:lockscope>"
					"<D:locktype><D:write/></D:locktype>"
					"</D:lockentry></D:supportedlock>";
KResourceMaker *rsMaker = new KFSResourceMaker;
KDavLockManager lockManager;
using namespace std;
KWebDavService::KWebDavService() {
	if_token = NULL;
}
KWebDavService::~KWebDavService() {
	if (if_token) {
		xfree(if_token);
	}
}
bool KWebDavService::service(KServiceProvider *provider) {
	this->provider = provider;
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
	return true;
}
bool KWebDavService::send(int status_code) {
	return provider->sendStatus(status_code, NULL);
}
bool KWebDavService::doGet(bool head) {
	KResource *rs = rsMaker->bindResource(provider->getFileName(),	provider->getRequestUri());
	if (rs == NULL) {
		return send(STATUS_NOT_FOUND);
	}
	char *if_modified_since = provider->getHttpHeader("If-Modified-Since");
	if (if_modified_since) {
		time_t ims = parse1123time(if_modified_since);
		provider->freeHttpHeader(if_modified_since);
		if (ims == rs->getLastModified()) {
			delete rs;
			return send(STATUS_NOT_MODIFIED);
		}
	}
	provider->sendStatus(200,"OK");
	provider->sendUnknowHeader("Cache-Control", "no-cache,no-store");
	char tbuf[50];
	mk1123time(rs->getLastModified(), tbuf, sizeof(tbuf));
	provider->sendUnknowHeader("Last-Modified", tbuf);
	if (!head && rs->open(false)) {
		KWStream *out = provider->getOutputStream();
		char buf[1024];
		for (;;) {
			int len = rs->read(buf, sizeof(buf));
			if (len <= 0) {
				break;
			}
			if (!out->write_all(buf, len)) {
				break;
			}
		}

	}
	delete rs;
	return true;
}
bool KWebDavService::doCopy() {
	return true;
}
bool KWebDavService::doDelete() {
	KResource *rs = rsMaker->bindResource(provider->getFileName(),
			provider->getRequestUri());
	if (rs == NULL) {
		return send(STATUS_NOT_FOUND);
	}
	if (!rs->remove()) {
		send(STATUS_FORBIDEN);
	}
	delete rs;
	return true;
}
bool KWebDavService::doOptions() {
	send(STATUS_OK);
	provider->sendUnknowHeader("Allow", allowed_header);
	provider->sendUnknowHeader("DAV", "1,2,3");
	return true;
}
bool KWebDavService::doPut() {
	//TODO:检查锁
	KResource *rs = rsMaker->makeFile(provider->getFileName(),
			provider->getRequestUri());
	if (rs == NULL) {
		return send(STATUS_FORBIDEN);
	}
	int len = 0;
	KRStream *in = provider->getInputStream();
	int content_length = provider->getContentLength();
	content_length -= len;
	bool result = true;
	while (content_length > 0) {
		char buf[512];
		int this_read_len = MIN(content_length,(int)sizeof(buf));
		int actual_read_len = in->read(buf, this_read_len);
		if (actual_read_len <= 0) {
			result = false;
			break;
		}
		content_length -= actual_read_len;
		if (!rs->write(buf, actual_read_len)) {
			result = false;
			break;
		}
	}
	delete rs;
	if (result) {
		send(STATUS_NO_CONTENT);
	}
	return result;
}
bool KWebDavService::parseDocument(KXmlDocument &document) {
	int content_length = provider->getContentLength();
	if (content_length > MAX_DOCUMENT_SIZE) {
		return false;
	}
	if (content_length == 0) {
		return false;
	}
	int len = 0;
	char *buf = (char *) xmalloc(content_length+1);
	if (len < content_length) {
		//not all data have
		KRStream *in = provider->getInputStream();
		if (!in->read_all(buf + len, content_length - len)) {
			xfree(buf);
			return false;
		}
	}
	buf[content_length] = '\0';
	try {
		document.parse(buf);
		xfree(buf);
		return true;
	} catch (...) {
		xfree(buf);
		return false;
	}

}
const char *KWebDavService::getIfToken() {
	if (if_token) {
		return if_token;
	}
	char *if_val = provider->getHttpHeader("If");
	if (if_val == NULL) {
		return NULL;
	}
	char *p = strchr(if_val, '<');
	if (p == NULL) {
		provider->freeHttpHeader(if_val);
		return NULL;
	}
	if_token = xstrdup(p+1);
	provider->freeHttpHeader(if_val);
	p = strchr(if_token, '>');
	if (p) {
		*p = '\0';
	}
	return if_token;

}
bool KWebDavService::doLock() {
	KXmlDocument document;
	char ips[255];
	int len = sizeof(ips);
	provider->getEnv("REMOTE_ADDR",ips,&len);
	if (!parseDocument(document)) {
		if (getIfToken() != NULL) {
			//todo:refresh the lock
			KLockToken *token = lockManager.findLockToken(if_token,ips);
			if (token != NULL) {
				token->refresh();
				lockManager.releaseToken(token);
			}
			return send(STATUS_OK);
		}
		return send(STATUS_BAD_REQUEST);
	}
	KXmlNode *node = document.getRootNode();
	if (node == NULL) {
		return send(STATUS_BAD_REQUEST);
	}
	//printf("node tag=[%s]\n",node->getTag().c_str());
	if (node->getTag() != "lockinfo") {
		return send(STATUS_BAD_REQUEST);
	}
	Lock_type type = Lock_none;
	if (node->getChild("lockscope/exclusive")) {
		type = Lock_exclusive;
	} else if (node->getChild("lockscope/shared")) {
		type = Lock_share;
	}
	if (type == Lock_none) {
		return send(STATUS_BAD_REQUEST);
	}
	KLockToken *token = lockManager.newToken(ips, type,3600);
	if (token == NULL) {
		return send(STATUS_SERVER_ERROR);
	}
	//TODO:这里要真lock了。
	Lock_op_result result = Lock_op_success;//lockManager.lock(provider->getFileName(),token);
	if (result != Lock_op_success) {
		send(423);
	} else {
		std::stringstream h;
		h << "<" << token->getValue() << ">";
		provider->sendUnknowHeader("Lock-Token", h.str().c_str());
		writeXmlHeader();
		KWStream *s = provider->getOutputStream();
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
	}
	lockManager.releaseToken(token);
	return true;
}
bool KWebDavService::doUnlock() {
	return false;
}
bool KWebDavService::doMkcol() {
	KResource *rs = rsMaker->makeDirectory(provider->getFileName(),
			provider->getRequestUri());
	if (rs) {
		send(STATUS_CREATED);
		delete rs;
	} else {
		send(STATUS_CONFLICT);
	}
	return true;
}
bool KWebDavService::doProppatch() {
	KXmlDocument document;
	parseDocument(document);
	KXmlNode *node = document.getNode("propertyupdate/set/prop");
	if (node != NULL) {
		//todo for set;
	}
	node = document.getNode("propertyupdate/remove/prop");
	//todo:for remove
	KResource *rs = rsMaker->bindResource(provider->getFileName(),
			provider->getRequestUri());
	if (rs == NULL) {
		return send(STATUS_NOT_FOUND);
	}
	KWStream *s = provider->getOutputStream();
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
bool KWebDavService::listResourceProp(KResource *rs, int depth) {
	//KWStream *s = provider->getOutputStream();
	if (!writeResourceProp(rs)) {
		return false;
	}
	bool result = true;
	//	printf("depth=%d\n",depth);
	if (depth-- > 0 && rs->isDirectory()) {
		std::list<KResource *> childs;
		rs->listChilds(childs);
		std::list<KResource *>::iterator it;
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
	KXmlDocument document;
	parseDocument(document);

	char *depth_header = provider->getHttpHeader("Depth");
	//printf("depth=%s\n",depth_header);
	int depth = 0;
	if (depth_header) {
		if (strcasecmp(depth_header, "1") == 0) {
			depth = 1;
		} else if (strcasecmp(depth_header, "infinity") == 0) {
			depth = MAX_DEPTH;
		}
		provider->freeHttpHeader(depth_header);
	}
	KResource *rs = rsMaker->bindResource(provider->getFileName(),
			provider->getRequestUri());
	if (rs == NULL) {
		return send(STATUS_NOT_FOUND);
	}
	KWStream *s = provider->getOutputStream();
	provider->sendStatus(STATUS_MULTI_STATUS, NULL);
	writeXmlHeader();
	*s << "<D:multistatus " << xml_dav_ns << ">\n";
	bool result = listResourceProp(rs, depth);
	*s << "</D:multistatus>\n";
	delete rs;
	return result;
}
bool KWebDavService::writeResourceProp(KResource *rs) {
	KWStream *s = provider->getOutputStream();
	KAttribute *attribute = rs->getAttribute();
	*s << "<D:response>\n";
	*s << "<D:href>" << url_encode(rs->getPath(), 0);
	if (rs->isDirectory()) {
		*s << "/";
	}
	*s << "</D:href>\n";
	*s << "<D:propstat><D:prop>\n";
	if (attribute) {
		std::map<char *, char *, attrp>::iterator it;
		for (it = attribute->atts.begin(); it != attribute->atts.end(); it++) {
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
	char destination[512];
	int len = sizeof(destination);
	if (!provider->getEnv("DAV_DESTINATION", destination, &len) || len <= 0) {
		return send(STATUS_BAD_REQUEST);
	}

	KResource *rs = rsMaker->bindResource(provider->getFileName(),
			provider->getRequestUri());
	if (rs == NULL) {
		return send(STATUS_NOT_FOUND);
	}
	int status = STATUS_CREATED;
	//TODO:check lock
	KResource *rsd = rsMaker->bindResource(destination, "/");
	if (rsd) {
		char *overWriteHeader = provider->getHttpHeader("Overwrite");
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
		rsd->remove();
		delete rsd;
	}
	if (rs->rename(destination)) {
		send(status);
		char *dst = provider->getHttpHeader("Destination");
		provider->sendUnknowHeader("Location",dst);
		provider->freeHttpHeader(dst);

	} else {
		send(STATUS_FORBIDEN);
	}
	delete rs;
	return true;
}
