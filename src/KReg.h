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
#ifdef _WIN32
#include "pcre/pcre.h"
#else
#include <pcre.h>
#endif
#define MAX_REG_MATCH 100
#define OVECTOR_SIZE		30
#ifndef PCRE_PARTIAL
#define PCRE_PARTIAL 0x00008000
#define PCRE_DFA_RESTART        0x00020000
#define PCRE_ERROR_PARTIAL        (-12)
#define PCRE_ERROR_BADPARTIAL     (-13)
#define PCRE_ERROR_DFA_WSSIZE     (-19)

#endif
class KRegSubString
{
public:
	KRegSubString()
	{
		subStrings = NULL;
		count = 0;
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
class KReg
{
public:
	KReg();
	~KReg();
	bool setModel(const char *model_str,int flag,int match_limit=0);
	const char *getModel();
	static KRegSubString *makeSubString(const char *str,int *ovector,int ovector_size,int matched)
	{
		KRegSubString *subString = new KRegSubString;
		subString->count = matched;
		subString->subStrings = (char **) malloc(sizeof(char *)*matched);
		memset(subString->subStrings, 0, sizeof(char *) * matched);
		for (int i = 0; i < matched; i++) {
			subString->subStrings[i] = getSubString(str, ovector, OVECTOR_SIZE, i);
		}
		return subString;
	}
	static char *getSubString(const char *str,int *ovector,int ovector_size,int index)
	{
		if (index>ovector_size/3) {
			return NULL;
		}
		int start = index*2;
		int len = ovector[start+1] - ovector[start];
		if (len<=0 || len>10485760) {
			return NULL;
		}
		char *buf = (char *)malloc(len+1);
		kgl_memcpy(buf,str+ovector[start],len);
		buf[len]='\0';
		return buf;
	}
	static void freeStudy(pcre_extra *pe)
	{
#ifdef	PCRE_STUDY_JIT_COMPILE
		pcre_free_study(pe);
#else
		pcre_free(pe);
#endif
	}
	/**
	* 成功返回匹配的子串个数，最少为1.不成功返回<0
	*/
	int match(const char *str,int str_len,int flag,int *ovector,int ovector_size);
	int match(const char *str,int str_len,int flag);
	KRegSubString *matchSubString(const char *str,int str_len,int flag);
	KRegSubString *matchSubString(const char *str,int str_len,int flag,int *ovector,int ovector_size);
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
	pcre * c_model;
	pcre_extra *pe;
};
#endif
