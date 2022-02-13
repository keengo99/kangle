/*
 * KMultiHostAcl.cpp
 *
 *  Created on: 2010-5-30
 *      Author: keengo
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

#include <stdlib.h>
#include <string.h>
#include <vector>
#include "KMultiHostAcl.h"
#include "KLineFile.h"
#include "KAccess.h"
#include "do_config.h"
#include "KVirtualHost.h"
using namespace std;
KMultiHostAcl::KMultiHostAcl() {
	lastModified = 0;
	lastState = OPEN_UNKNOW;
	lastCheck = 0;
}

KMultiHostAcl::~KMultiHostAcl() {
	freeMap();
}
void KMultiHostAcl::freeMap() {
	std::map<char *, bool, lessp_icase >::iterator it;
	for (it = m.begin(); it != m.end(); it++) {
		xfree((*it).first);
	}
	m.clear();
}
KAcl *KMultiHostAcl::newInstance() {
	return new KMultiHostAcl();
}
const char *KMultiHostAcl::getName() {
	return "map_host";
}
bool KMultiHostAcl::match(KHttpRequest *rq, KHttpObject *obj) {
	loadFile(rq);
	map<char *, bool,lessp_icase >::iterator it;
	bool result = false;
	lock.Lock();
	it = m.find(rq->url->host);
	if (it != m.end()) {
		result = true;
	}
	lock.Unlock();
	return result;
}
bool KMultiHostAcl::loadFile(KHttpRequest *rq) {
	if (kgl_current_sec - lastCheck < 5) {
		return true;
	}
	lastCheck = kgl_current_sec;
	std::string path;
	if (isAbsolutePath(file.c_str())) {
		path = file;
	} else {
#ifndef HTTP_PROXY
		if (isGlobal) {
			path = conf.path;
		} else {
			assert(rq->svh);
			path = rq->svh->vh->doc_root;
		}
#else
		path = conf.path;
#endif
		path += file;
	}
	KLineFile lFile;
	lock.Lock();
	lastState = lFile.open(path.c_str(), lastModified);
	if (lastState == OPEN_NOT_MODIFIED) {
		lock.Unlock();
		return true;
	}
	freeMap();
	if (lastState == OPEN_FAILED) {
		lock.Unlock();
		return false;
	}
	for (;;) {
		char *hot = lFile.readLine();
		if (hot == NULL) {
			break;
		}
		m.insert(pair<char *, bool> (xstrdup(hot), true));
	}
	lock.Unlock();
	return true;

}
std::string KMultiHostAcl::getDisplay() {
	stringstream s;
	s << "file:" << file << " read state:";
	switch (lastState) {
	case OPEN_UNKNOW:
		s << "unknow";
		break;
	case OPEN_FAILED:
		s << "<font color='red'>failed</font>";
		break;
	case OPEN_SUCCESS:
	case OPEN_NOT_MODIFIED:
		s << "<font color='green'>success</font>";

	}
	return s.str();
}

std::string KMultiHostAcl::getHtml(KModel *model) {
	KMultiHostAcl *acl = (KMultiHostAcl *) model;
	stringstream s;
	s << "file: <input name='file' value='";
	if (acl) {
		s << acl->file;
	}
	s << "'>(one line one host)";
	return s.str();
}
void KMultiHostAcl::editHtml(std::map<std::string, std::string> &attribute,bool html)
		 {
	if (attribute["file"].size() > 0) {
		file = attribute["file"];
	}
}
void KMultiHostAcl::buildXML(std::stringstream &s) {
	s << " file='" << file << "'>";
}
