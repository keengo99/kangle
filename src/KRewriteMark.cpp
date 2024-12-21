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
#include "KRewriteMark.h"
#include "KStringBuf.h"
#include "http.h"
#include "kmalloc.h"
using namespace std;
KRewriteMark::KRewriteMark() {
}

KRewriteMark::~KRewriteMark() {

}
KMark *KRewriteMark::new_instance() {
	return new KRewriteMark;
}
uint32_t KRewriteMark::process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo){
	return rule.mark(rq, obj,NULL, prefix,NULL , fo);
}
const char *KRewriteMark::getName() {
	return "rewrite";
}
void KRewriteMark::get_html(KWStream &s) {
	KRewriteMark *mark = (KRewriteMark *) this;
	s << "prefix:<input name='prefix' value='";
	if(mark){
		s << mark->prefix;
	}
	s << "'><br>";
	s << "path:<input name='path' value='";
	if (mark) {
		s << mark->rule.reg.getModel();
	}
	s << "'><br>rewrite to:<input name='dst' value='";
	if (mark && mark->rule.dst) {
		s << mark->rule.dst;
	}
	s << "'><br>";
	s << "code:<input name='code' value='";
	if (mark) {
		s << mark->rule.code;
	}
	s << "'><br>";
	s << "<input type=checkbox name='internal' value='1' ";
	if (mark == NULL || mark->rule.internal) {
		s << "checked";
	}
	s << ">internal";
	s << "<input type=checkbox name='nc' value='1' ";
	if (mark == NULL || mark->rule.nc) {
		s << "checked";
	}
	s << ">no case";
	
	s << "<input type=checkbox name='qsa' value='1' ";
	if (mark  && mark->rule.qsa) {
		s << "checked";
	}
	s << ">qsappend";
}
void KRewriteMark::get_display(KWStream& s) {
	if(prefix.size()>0){
		s << prefix << " ";
	}
	s << rule.reg.getModel() << " ";
	if (rule.dst) {
		s << rule.dst;
	}
	if (rule.internal) {
		s << " P";
	}
	if(rule.nc){
		s << " nc";
	}
}
void KRewriteMark::parse_config(const khttpd::KXmlNodeBody* xml) {
	auto attribute = xml->attr();
	prefix = attribute["prefix"];
	rule.parse(attribute);
}
