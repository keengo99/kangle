#ifndef KINPUTFILTER_H
#define KINPUTFILTER_H
#include "global.h"
#ifdef ENABLE_INPUT_FILTER
#include "KMark.h"
#include "KHttpHeader.h"
#include <stdlib.h>
#include "KDechunkEngine.h"
class KHttpRequest;
class KInputFilterContext;
struct KParamPair
{
	char *name;
	int name_len;
	char *value;
	int value_len;
	KParamPair *next;
};
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
class KParamFilterHook
{
public:
	virtual ~KParamFilterHook()
	{
	}
	virtual void addRefHook()
	{
	}
	virtual void releaseHook()
	{
		delete this;
	}
	virtual bool matchFilter(KInputFilterContext *rq,const char *name,int name_len,const char *value,int value_len) = 0;
};
class KFileFilterHook 
{
public:
	virtual ~KFileFilterHook()
	{
	}
	virtual void addRefHook()
	{
	}
	virtual void releaseHook()
	{
		delete this;
	}
	virtual bool matchContent(KInputFilterContext *rq,const char *str,int len) = 0;
	virtual bool matchFilter(KInputFilterContext *rq,const char *name,int name_len,const char *filename,int filename_len,KHttpHeader *header) = 0;
};
class KRawInputFilter
{
public:
	KRawInputFilter()
	{
		next = NULL;
	}
	virtual ~KRawInputFilter()
	{
	};
	virtual void addRefFilter()
	{
	}
	virtual void releaseFilter()
	{
		delete this;
	}
	virtual int check(KInputFilterContext *rq,const char *str,int len,bool isLast) = 0;
	KRawInputFilter *next;
};
class KInputFilter
{
public:
	KInputFilter()
	{
		head = NULL;
		last = NULL;
		last_buf = NULL;
	}
	virtual ~KInputFilter()
	{
		while (head) {
			last = head->next;
			delete head;
			head = last;
		}
		if (last_buf) {
			free(last_buf);
		}
	}
	virtual int check(KInputFilterContext *rq,const char *str,int len,bool isLast);
	bool registerParamFilter(KParamFilterHook *filter,int action)
	{
		KInputFilterHelper<KParamFilterHook> *filter_helper = new KInputFilterHelper<KParamFilterHook>(filter,action);
		if (last==NULL) {
			assert(head==NULL);
			head = last = filter_helper;
		} else {
			last->next = filter_helper;
			last = filter_helper;
		}
		return true;
	}
	virtual bool registerFileFilter(KFileFilterHook *filter,int action)
	{
		return false;
	}
	virtual bool registerFileContentFilter(KFileFilterHook *filter,int action)
	{
		return false;
	};
	int hookParamFilter(KInputFilterContext *fc,const char *name,int name_len,const char *value,int value_len);
protected:
	int checkItem(KInputFilterContext *rq,char *buf,int len);
	char *last_buf;
	int last_buf_len;
	KInputFilterHelper<KParamFilterHook> *head;
	KInputFilterHelper<KParamFilterHook> *last;
};
#define FILLUNIT (1024 * 5)
#define MULTIPART_BOUNDARY_MODEL 0
#define MULTIPART_HEAD_MODEL     1
#define MULTIPART_BODY_MODEL     2

class multipart_buffer {
public:
	multipart_buffer()
	{
		memset(this,0,sizeof(multipart_buffer));
	}
	~multipart_buffer()
	{
		if (buffer) {
			free(buffer);
		}
		if (boundary) {
			free(boundary);
		}
		if (boundary_next) {
			free(boundary_next);
		}
	}
	void add(const char *str,int len)
	{
		if (buffer) {
			bufsize = len + bytes_in_buffer;
			char *t = (char *)malloc(bufsize+1);
			kgl_memcpy(t,buf_begin,bytes_in_buffer);
			kgl_memcpy(t+bytes_in_buffer,str,len);
			t[bufsize] = '\0';
			free(buffer);
			buffer = t;
		} else {
			bufsize = len;
			buffer = (char *)malloc(bufsize+1);
			kgl_memcpy(buffer,str,len);
			buffer[bufsize] = '\0';
		}
		bytes_in_buffer = bufsize;
		buf_begin = buffer;
	}
	/* read buffer */
	char *buffer;
	char *buf_begin;
	int  bufsize;
	int  bytes_in_buffer;

	/* boundary info */
	char *boundary;
	char *boundary_next;
	int  boundary_next_len;
	int  model;
};
class KInputFilterContext
{
public:
	KInputFilterContext(KHttpRequest *rq)
	{
		raw_head = NULL;
		raw_end = NULL;
		filter = NULL;
		this->rq = rq;		
		mb = NULL;
		gBuffer = NULL;
		gParamHeader = NULL;
		st = NULL;
#if 0
		dechunk = NULL;
#endif
	}
	~KInputFilterContext()
	{
		while (raw_head) {
			raw_end = raw_head->next;
			raw_head->releaseFilter();
			raw_head = raw_end;
		}
		if (filter) {
			delete filter;
		}
		if (mb) {
			delete mb;
		}
		if (gBuffer) {
			free(gBuffer);
		}
		while (gParamHeader) {
			KParamPair *t = gParamHeader->next;
			delete gParamHeader;
			gParamHeader = t;
		}
		if (st) {
			delete st;
		}
#if 0
		if (dechunk) {
			delete dechunk;
		}
#endif
	}
	void registerRawFilter(KRawInputFilter *rf)
	{
		rf->addRefFilter();
		rf->next = NULL;
		if (raw_end==NULL) {
			assert(raw_head==NULL);
			raw_head = rf;
		} else {
			raw_end->next = rf;
		}
		raw_end = rf;
	}
	KInputFilter *getFilter();
	int check(const char *str,int len,bool isLast)
	{
		int ret;
		if (filter){
			ret = filter->check(this,str,len,isLast);
			if(ret==JUMP_DENY){
				return ret;
			}

		}
		KRawInputFilter *raw = raw_head;
		while (raw) {
			ret = raw->check(this,str,len,isLast);
			if (ret==JUMP_DENY) {
				return ret;
			}
			raw = raw->next;
		}
		return JUMP_ALLOW;
	}
	bool isEmpty()
	{
		return raw_head==NULL && filter==NULL;
	}
	bool checkGetParam(KParamFilterHook *hook);
	KParamPair *gParamHeader;
	char *gBuffer;
	bool parse_boundary(const char *val, size_t len);
	KHttpRequest *rq;
	KRawInputFilter *raw_head;
	KRawInputFilter *raw_end;
	KInputFilter *filter;
	multipart_buffer *mb;
	/**
	* 输入内容解码,如chunked
	*/
	KWStream *st;
private:
};
#endif
#endif
