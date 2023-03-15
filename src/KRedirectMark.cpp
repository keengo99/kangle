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
#include "KBufferFetchObject.h"

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
KMark* KRedirectMark::new_instance() {
	return new KRedirectMark;
}
bool KRedirectMark::process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo){
	if (!dst) {
		return false;
	}
	assert(rq->sink->data.url->path);
	if (internalRedirect) {
		rq->rewrite_url(dst);
	} else {
		if (push_redirect_header(rq, dst, (int)strlen(dst), code)) {
			fo.reset(new KBufferFetchObject(nullptr, 0));
		}
	}
	return true;
}
const char* KRedirectMark::getName() {
	return "redirect";
}

void KRedirectMark::get_html(KModel* model,KWStream &s) {
	KRedirectMark* mark = (KRedirectMark*)model;
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
}
void KRedirectMark::get_display(KWStream &s) {
	if (dst) {
		s << dst;
	}
	if (internalRedirect) {
		s << " P";
	}
	if (code > 0) {
		s << "R=" << code;
	}
}
void KRedirectMark::parse_config(const khttpd::KXmlNodeBody* xml) {
	auto attribute = xml->attr();
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
