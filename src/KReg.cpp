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
	if (subStrings) {
		for (int i = 0; i < count; i++) {
			if (subStrings[i]) {
				xfree(subStrings[i]);
			}
		}
		xfree(subStrings);
	}
}
KReg::KReg() {
	model = NULL;
	c_model = NULL;
#ifndef ENABLE_PCRE2
	pe = NULL;
#endif
	//	int match_limit=1;
	//	pcre_config(PCRE_CONFIG_MATCH_LIMIT,&match_limit);
}
KReg::~KReg() {
	if (model != NULL) {
		xfree(model);
	}
#ifdef ENABLE_PCRE2
	if (c_model) {
		pcre2_code_free(c_model);
	}
#else
	if (c_model) {
		pcre_free(c_model);
	}
	if (pe) {
		freeStudy(pe);
	}
#endif
}
KRegSubString* KReg::matchSubString(const char* str, int str_len, int flag, kgl_pcre_match_data *match_data)
{
	int matched = match(str, str_len, 0, match_data);
	if (matched < 1) {
		return NULL;
	}
	return makeSubString(str, match_data, matched);
	
}
KRegSubString* KReg::matchSubString(const char* str, int str_len, int flag)
{
#ifdef ENABLE_PCRE2
	kgl_pcre_match_data* match_data = pcre2_match_data_create_from_pattern(c_model, nullptr);
	int matched = match(str, str_len, flag, match_data);
	if (matched < 1) {
		pcre2_match_data_free(match_data);
		return NULL;
	}
	KRegSubString *result = makeSubString(str, match_data, matched);
	pcre2_match_data_free(match_data);
	return result;
#else
	KGL_OVECTOR_SIZE ovector[OVECTOR_SIZE];
	kgl_pcre_match_data match_data{ ovector,OVECTOR_SIZE };
	int matched = match(str, str_len, 0, &match_data);
	if (matched < 1) {
		return NULL;
	}
	return makeSubString(str, &match_data, matched);
#endif
}
bool KReg::isPartialModel() {
#ifdef PCRE_INFO_OKPARTIAL
	int okPartial = 0;
	if (pcre_fullinfo(c_model, pe, PCRE_INFO_OKPARTIAL, &okPartial) == 0) {
		if (okPartial == 1) {
			return true;
		}
	}
	return false;
#else
	return true;
#endif

}

bool KReg::setModel(const char* model_str, int flag) {
	if (model_str == NULL || *model_str == '\0') {
		return false;
	}
	if (model != NULL) {
		xfree(model);
		model = NULL;
	}
	if (c_model) {
#ifdef ENABLE_PCRE2
		pcre2_code_free(c_model);
#else
		pcre_free(c_model);
#endif
		c_model = NULL;
	}
	model = xstrdup(model_str);	
	const char* error = NULL;
#ifdef ENABLE_PCRE2
	int errorcode;
	size_t erroffset;
	c_model = pcre2_compile((PCRE2_SPTR)model_str, strlen(model_str), flag, &errorcode, (PCRE2_SIZE *)&erroffset, NULL);
	if (c_model) {
		pcre2_jit_compile(c_model, PCRE2_JIT_COMPLETE);
		return true;
	}
#else
	int erroffset;
	c_model = pcre_compile(model_str, flag, &error, &erroffset, NULL);
	if (c_model) {
		const char* error;
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
#if 0
		if (pe && match_limit > 0) {
			pe->flags |= PCRE_EXTRA_MATCH_LIMIT;
			pe->match_limit = match_limit;
		}
#endif
		return true;
	}
#endif
	klog(KLOG_ERR, "cann't compile regex [%s] pos=%d,error=[%s]\n", model, (int)erroffset, (error ? error : ""));
	return false;
}
const char* KReg::getModel() {
	if (model)
		return model;
	return "";
}
int KReg::matchPartial(const char* str, int str_len, int flag, int* workspace,
	int wscount) {
	int ovector[DEFAULT_OVECTOR];
	return matchPartial(str, str_len, flag, ovector, DEFAULT_OVECTOR,
		workspace, wscount);
}
int KReg::matchNext(const char* str, int str_len, int flag, int* workspace,
	int wscount) {
	int ovector[DEFAULT_OVECTOR];
	return matchNext(str, str_len, flag, ovector, DEFAULT_OVECTOR, workspace,
		wscount);
}
int KReg::matchPartial(const char* str, int str_len, int flag, int* ovector,
	int ovector_size, int* workspace, int wscount) {
	//	return -1;
#ifdef PCRE_DFA_SHORTEST
	return pcre_dfa_exec(c_model, pe, str, str_len, 0,
		PCRE_PARTIAL | flag, ovector, ovector_size, workspace, wscount);
#else
	return -1;
#endif
}
int KReg::matchNext(const char* str, int str_len, int flag, int* ovector,
	int ovector_size, int* workspace, int wscount) {
#ifdef PCRE_DFA_SHORTEST
	return pcre_dfa_exec(c_model, pe, str, str_len, 0,
		PCRE_DFA_RESTART | flag, ovector, ovector_size, workspace, wscount);
#else
	return -1;
#endif
}

int KReg::match(const char* str, int str_len, int flag) {
#ifdef ENABLE_PCRE2
	kgl_pcre_match_data* match_data = pcre2_match_data_create_from_pattern(c_model, nullptr);
	int result = match(str, str_len, flag, match_data);
	pcre2_match_data_free(match_data);
	return result;
#else
	KGL_OVECTOR_SIZE ovector[DEFAULT_OVECTOR];
	kgl_pcre_match_data match_data{ovector,DEFAULT_OVECTOR };
	return match(str, str_len, flag, &match_data);
#endif
	
}

int KReg::match(const char* str, int str_len, int flag, kgl_pcre_match_data *match_data) {
	if (str_len == -1) {
		str_len = (int)strlen(str);
	}
#ifdef ENABLE_PCRE2
	return pcre2_match(c_model, (PCRE2_SPTR)str, (PCRE2_SIZE)str_len, 0, flag, match_data, nullptr);
#else
	return pcre_exec(c_model, /* result of pcre_compile() */
		pe, /* we didn't study the pattern */
		str, /* the subject string */
		str_len, /* the length of the subject string */
		0, /* start at offset 0 in the subject */
		flag, /* default options */
		match_data->ovector, /* vector of integers for substring information */
		match_data->ovector_size); /* number of elements in the vector (NOT size in bytes) */
#endif
}

