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
#ifndef KREG_H_sldjf9sdf9sdf
#define KREG_H_sldjf9sdf9sdf
#include<string>
#include "kmalloc.h"
#include "global.h"
#include "KStringBuf.h"
#ifdef ENABLE_PCRE2
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#define KGL_PCRE_CASELESS PCRE2_CASELESS
#define kgl_pcre_match_data pcre2_match_data
#define KGL_OVECTOR_SIZE PCRE2_SIZE
#else
#include <pcre.h>
#define KGL_PCRE_CASELESS PCRE_CASELESS
#define KGL_OVECTOR_SIZE int
struct kgl_pcre_match_data {
	KGL_OVECTOR_SIZE* ovector;
	int ovector_size
};
#define MAX_REG_MATCH            100
#define OVECTOR_SIZE		         30
#ifndef PCRE_PARTIAL
#define PCRE_PARTIAL              0x00008000
#define PCRE_DFA_RESTART          0x00020000
#define PCRE_ERROR_PARTIAL        (-12)
#define PCRE_ERROR_BADPARTIAL     (-13)
#define PCRE_ERROR_DFA_WSSIZE     (-19)
#endif
#endif
inline KString kgl_pcre_version() {
#ifdef ENABLE_PCRE2
	KStringBuf s;
	s << PCRE2_MAJOR << "." << PCRE2_MINOR;
	return s.str();
#else
	return pcre_version();
#endif
}
class KRegSubString
{
public:
	KRegSubString(int count) :count{ count }
	{
		subStrings = (char**)malloc(sizeof(char*) * count);
		memset(subStrings, 0, sizeof(char*) * count);
	}
	~KRegSubString();
	char *getString(int index)
	{
		if(index<0 || index>=count){
			return NULL;
		}
		return subStrings[index];
	}
	char **subStrings;
	int count;
};
class KReg final
{
public:
	KReg();
	~KReg();
	bool setModel(const char *model_str,int flag);
	const char *getModel();
	static KRegSubString *makeSubString(const char *str,kgl_pcre_match_data *match_data,int matched)
	{
		KRegSubString *subString = new KRegSubString(matched);
		
#ifdef ENABLE_PCRE2
		KGL_OVECTOR_SIZE* ovector = (PCRE2_SIZE*)pcre2_get_ovector_pointer(match_data);
		int ovector_count = (int)pcre2_get_ovector_count(match_data);
#else
		KGL_OVECTOR_SIZE* ovector = match_data->ovector;
		int ovector_count = match_data->ovector_size/2;
#endif
		for (int i = 0; i < matched; i++) {
			subString->subStrings[i] = getSubString(str, ovector, ovector_count, i);
		}
		return subString;
	}
	static char *getSubString(const char *str, KGL_OVECTOR_SIZE*ovector,int ovector_count,int index)
	{
		if (index> ovector_count) {
			return NULL;
		}
		int start = index*2;
		KGL_OVECTOR_SIZE len = ovector[start+1] - ovector[start];
		if (len<=0 || len>10485760) {
			return NULL;
		}
		char *buf = (char *)malloc(len+1);
		kgl_memcpy(buf,str+ovector[start],len);
		buf[len]='\0';
		return buf;
	}
#ifndef ENABLE_PCRE2
	static void freeStudy(pcre_extra *pe)
	{
#ifdef	PCRE_STUDY_JIT_COMPILE
		pcre_free_study(pe);
#else
		pcre_free(pe);
#endif
	}
#endif
	/**
	* 成功返回匹配的子串个数，最少为1.不成功返回<0
	*/
	int match(const char *str,int str_len,int flag, kgl_pcre_match_data *match_data);
	int match(const char *str,int str_len,int flag);
	KRegSubString *matchSubString(const char *str,int str_len,int flag);
	KRegSubString *matchSubString(const char *str,int str_len,int flag, kgl_pcre_match_data *match_data);
	int matchPartial(const char *str,int str_len,int flag,int *ovector,int ovector_size,int *workspace,int wscount);
	int matchNext(const char *str,int str_len,int flag,int *ovector,int ovector_size,int *workspace,int wscount);
	bool isPartialModel();
	int matchPartial(const char *str,int str_len,int flag,int *workspace,int wscount);
	int matchNext(const char *str,int str_len,int flag,int *workspace,int wscount);
	bool isok()
	{
		return c_model!=NULL;
	}
private:	
	char *model;
#ifdef ENABLE_PCRE2
	pcre2_code* c_model;
#else
	pcre* c_model;
	pcre_extra *pe;
#endif
};
#endif
