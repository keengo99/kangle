#ifndef KINPUTFILTER_H
#define KINPUTFILTER_H
#include <list>
#include "ksapi.h"
#include "kmalloc.h"
#include "KSharedObj.h"
#include "katom.h"
#include "KStream.h"
#include <stdlib.h>

class KInputFilterContext;
class KBuilderWriter : public KWStream {
public:
	KBuilderWriter(kgl_access_build* builder_ctx) : builder{ builder_ctx } {}
protected:
	int write(const char* buf, int len) override {
		if (0 == builder->write_string(builder->cn, buf, len, 0)) {
			return len;
		}
		return 0;
	}
	kgl_access_build* builder;
};
struct KParamPair
{
	char *name;
	int name_len;
	char *value;
	int value_len;
	KParamPair *next;
};
#if 0
template<typename T>
class KInputFilterHelper
{
public:
	KInputFilterHelper(T *t,int action)
	{
		next = NULL;
		this->t = t;
		this->action = action;
		t->addRefHook();
	}
	~KInputFilterHelper()
	{
		if (t) {
			t->releaseHook();
		}
	}
	T *t;
	int action;
	KInputFilterHelper<T> *next;
};
#endif

class KParamFilterHook
{
public:
	KParamFilterHook* add_ref() {
		katom_inc((void *)&ref);
		return this;
	}
	void release() {
		if (katom_dec((void*)&ref) == 0) {
			delete this;
		}
	}
	virtual bool check(const char *name,int name_len,const char *value,int value_len) = 0;
protected:
	virtual ~KParamFilterHook() {
	}
private:
	volatile int32_t ref = 1;
};
class KFileFilterHook 
{
public:
	KFileFilterHook *add_ref()
	{
		katom_inc((void*)&ref);
		return this;
	}
	void release() {
		if (katom_dec((void*)&ref) == 0) {
			delete this;
		}
	}
	virtual bool check_content(const char *str,int len) = 0;
	virtual bool check_name(const char *name,int name_len,const char *filename,int filename_len,KHttpHeader *header) = 0;
protected:
	virtual ~KFileFilterHook() {}
private:
	volatile int32_t ref = 1;
};
using KParamFilter = KSharedObj<KParamFilterHook>;
using KFileFilter = KSharedObj<KFileFilterHook>;
class KInputFilter
{
public:
	KInputFilter()
	{		
	}
	virtual ~KInputFilter()
	{		
	}
	virtual bool match(KInputFilterContext* rq, const char* str, int len, bool is_last) {
		return false;
	}
	virtual bool register_param(KParamFilterHook *filter)
	{
		return false;
	}
	virtual bool register_file(KFileFilterHook *filter)
	{
		return false;
	}
};
class KParamInputFilter : public KInputFilter {
public:
	KParamInputFilter() {
		last_buf = NULL;
		last_buf_len = 0;
	}
	~KParamInputFilter() {
		if (last_buf) {
			free(last_buf);
		}
	}
	bool register_param(KParamFilterHook* filter) override {
		param_list.emplace_back(filter->add_ref());
		return true;
	}
	virtual bool match(KInputFilterContext* rq, const char* str, int len, bool isLast) override;
protected:
	bool match_param(const char* name, int name_len, const char* value, int value_len);
private:	
	char* last_buf;
	int last_buf_len;
	std::list<KParamFilter> param_list;
	bool match_param_item(char* buf, int len);
};
class KInputFilterContext
{
public:
	KInputFilterContext() : body{ 0 }
	{
		filter = NULL;	
		gParamHeader = NULL;
	}
	~KInputFilterContext()
	{		
		if (filter) {
			delete filter;
		}
		while (gParamHeader) {
			KParamPair *t = gParamHeader->next;
			delete gParamHeader;
			gParamHeader = t;
		}
	}
	KInputFilter *get_filter(KREQUEST rq, kgl_access_context* ctx);
	bool match(const char *str,int len,bool isLast)
	{
		if (filter){
			return filter->match(this,str,len,isLast);
		}
		return false;

	}
	bool empty()
	{
		return filter==NULL;
	}
	void tee_body(kgl_request_body* body);
	bool check_get(KParamFilterHook *hook, KREQUEST rq, kgl_access_context* ctx);
	KParamPair *gParamHeader;
	KInputFilter *filter;
	kgl_request_body body;
private:	
};
KInputFilterContext* get_input_filter_context(KREQUEST rq, kgl_access_context* ctx);
#endif