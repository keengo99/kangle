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
#include "KReg.h"
#include "log.h"
#include "kmalloc.h"
#define DEFAULT_OVECTOR	30
KRegSubString::~KRegSubString()
{
	if(subStrings){
		for (int i = 0; i < count; i++) {
			if (subStrings[i]) {
				xfree(subStrings[i]);
			}
		}
		xfree(subStrings);
	}
}
KReg::KReg() {
	model=NULL;
	c_model=NULL;
	pe=NULL;
//	int match_limit=1;
//	pcre_config(PCRE_CONFIG_MATCH_LIMIT,&match_limit);
}
KReg::~KReg() {
	if (model!=NULL) {
		xfree(model);
	}
	if (c_model) {
		pcre_free(c_model);
	}
	if (pe) {
		freeStudy(pe);
	}
}
KRegSubString *KReg::matchSubString(const char *str,int str_len,int flag,int *ovector,int ovector_size)
{
	int matched = match(str, str_len, 0, ovector, ovector_size);
	if (matched < 1) {
		return NULL;
	}
	return makeSubString(str,ovector,ovector_size,matched);
}
KRegSubString *KReg::matchSubString(const char *str,int str_len,int flag)
{
	int ovector[OVECTOR_SIZE];
	int matched = match(str, str_len, 0, ovector, OVECTOR_SIZE);
	if (matched < 1) {
		return NULL;
	}
	return makeSubString(str,ovector,OVECTOR_SIZE,matched);
}
bool KReg::isPartialModel() {
#ifdef PCRE_INFO_OKPARTIAL
	int okPartial=0;
	if (pcre_fullinfo(c_model, pe, PCRE_INFO_OKPARTIAL, &okPartial)==0) {
		if (okPartial==1) {
			return true;
		}
	}
	return false;
#else
	return true;
#endif

}

bool KReg::setModel(const char *model_str, int flag,int match_limit) {
	if (model_str==NULL||*model_str=='\0'){
		return false;
	}
	if (model!=NULL) {
		xfree(model);
		model=NULL;
	}
	if (c_model) {
		pcre_free(c_model);
		c_model=NULL;
	}
	model=xstrdup(model_str);
	const char *error = NULL;
	int erroffset;
	c_model=pcre_compile(model_str, flag, &error, &erroffset, NULL);
	if (c_model) {
		const char * error;
		if (pe) {
			freeStudy(pe);
		}
		pe = pcre_study(c_model, /* result of pcre_compile() */
#ifdef PCRE_STUDY_JIT_COMPILE
			PCRE_STUDY_JIT_COMPILE
#else
			0
#endif
			,
		&error); /* set to NULL or points to a message */
		if(pe && match_limit>0){
			pe->flags|=PCRE_EXTRA_MATCH_LIMIT;
			pe->match_limit = match_limit;
		}
		return true;
	}
	klog(KLOG_ERR,"cann't compile regex [%s] pos=%d,error=[%s]\n",model,erroffset,(error?error:""));
	return false;
}
const char *KReg::getModel() {
	if (model)
		return model;
	return "";
}
int KReg::matchPartial(const char *str, int str_len, int flag, int *workspace,
		int wscount) {
	int ovector[DEFAULT_OVECTOR];
	return matchPartial(str, str_len, flag, ovector, DEFAULT_OVECTOR,
			workspace, wscount);
}
int KReg::matchNext(const char *str, int str_len, int flag, int *workspace,
		int wscount) {
	int ovector[DEFAULT_OVECTOR];
	return matchNext(str, str_len, flag, ovector, DEFAULT_OVECTOR, workspace,
			wscount);
}
int KReg::matchPartial(const char *str, int str_len, int flag, int *ovector,
		int ovector_size, int *workspace, int wscount) {
	//	return -1;
	#ifdef PCRE_DFA_SHORTEST
	return pcre_dfa_exec(c_model, pe, str, str_len, 0, 
	PCRE_PARTIAL|flag, ovector, ovector_size, workspace, wscount);
	#else
	return -1;
	#endif
}
int KReg::matchNext(const char *str, int str_len, int flag, int *ovector,
		int ovector_size, int *workspace, int wscount) {
	        #ifdef PCRE_DFA_SHORTEST

	return pcre_dfa_exec(c_model, pe, str, str_len, 0, 
	PCRE_DFA_RESTART|flag, ovector, ovector_size, workspace, wscount);
	#else
	return -1;
	#endif
}

int KReg::match(const char *str, int str_len, int flag) {
	int ovector[DEFAULT_OVECTOR];
	return match(str, str_len, flag, ovector, DEFAULT_OVECTOR);
}

int KReg::match(const char *str, int str_len, int flag, int *ovector,
		int ovector_size) {
	if (str_len==-1) {
		str_len=strlen(str);
	}
	return pcre_exec(c_model, /* result of pcre_compile() */
	pe, /* we didn't study the pattern */
	str, /* the subject string */
	str_len, /* the length of the subject string */
	0, /* start at offset 0 in the subject */
	flag, /* default options */
	ovector, /* vector of integers for substring information */
	ovector_size); /* number of elements in the vector (NOT size in bytes) */
}

