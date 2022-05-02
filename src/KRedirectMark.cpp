/*
 * KRewriteMark.cpp
 *
 *  Created on: 2010-4-27
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
#include <sstream>
#include <vector>
#include <stdio.h>
#include "KRedirectMark.h"
#include "KStringBuf.h"
#include "http.h"
#include "kmalloc.h"

using namespace std;
KRedirectMark::KRedirectMark() {
	internalRedirect = true;
	dst = NULL;
}

KRedirectMark::~KRedirectMark() {
	if (dst) {
		xfree(dst);
	}
}
KMark* KRedirectMark::newInstance() {
	return new KRedirectMark;
}
bool KRedirectMark::mark(KHttpRequest* rq, KHttpObject* obj,
	const int chainJumpType, int& jumpType) {
	if (dst == NULL) {
		return true;
	}
	assert(rq->sink->data.url->path);
	//std::stringstream ss;
	if (internalRedirect) {
		rq->rewriteUrl(dst);
	} else {
		int status_code = code;
		push_redirect_header(rq, dst, (int)strlen(dst), status_code);
		jumpType = JUMP_DENY;
	}
	return true;
}
const char* KRedirectMark::getName() {
	return "redirect";
}

std::string KRedirectMark::getHtml(KModel* model) {
	KRedirectMark* mark = (KRedirectMark*)model;
	stringstream s;
	s << "redirect url:<input name='dst' value='";
	if (mark && mark->dst) {
		s << mark->dst;
	}
	s << "'>\n";
	s << "code:<input name='code' size='4' value='";
	if (mark && mark->dst) {
		s << mark->code;
	}
	s << "'>\n";
	s << "<input type=checkbox name='internal' value='1' ";
	if (mark == NULL || mark->internalRedirect) {
		s << "checked";
	}
	s << ">internal";
	return s.str();
}
std::string KRedirectMark::getDisplay() {
	std::stringstream s;
	if (dst) {
		s << dst;
	}
	if (internalRedirect) {
		s << " P";
	}
	if (code > 0) {
		s << "R=" << code;
	}
	return s.str();
}
void KRedirectMark::editHtml(std::map<std::string, std::string>& attribute, bool html) {
	if (dst) {
		xfree(dst);
	}
	dst = xstrdup(attribute["dst"].c_str());
	if (attribute["internal"] == "1") {
		internalRedirect = true;
	} else {
		internalRedirect = false;
	}
	code = atoi(attribute["code"].c_str());
}
bool KRedirectMark::startCharacter(KXmlContext* context, char* character, int len) {
	return true;
}
void KRedirectMark::buildXML(std::stringstream& s) {
	s << " dst='" << (dst ? dst : "") << "'";
	if (internalRedirect) {
		s << " internal='1'";
	}
	if (code > 0) {
		s << " code='" << code << "'";
	}
	s << ">";
}
