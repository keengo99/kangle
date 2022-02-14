#ifndef KMULTIPARTINPUTFILTER_H
#define KMULTIPARTINPUTFILTER_H
#include "KInputFilter.h"
#include "KHttpHeaderManager.h"
#include "KHttpParser.h"

#ifdef ENABLE_INPUT_FILTER
class KFileContentFilterHelper
{
public:
	KFileContentFilterHelper(KFileFilterHook *hook,int action)
	{
		this->hook = hook;
		hook->addRefHook();
		next = NULL;
		this->action = action;
	}
	~KFileContentFilterHelper()
	{
		hook->releaseHook();
	}
	int matchContent(KInputFilterContext *rq,const char *str,int len)
	{
		//todo:deal with partial match
		if (hook->matchContent(rq,str,len)) {
			return action;
		}
		return JUMP_ALLOW;
	}
	int matchFilter(KInputFilterContext *rq,const char *name,int name_len,const char *filename,int filename_len,KHttpHeader *header)
	{
		if (hook->matchFilter(rq,name,name_len,filename,filename_len,header)) {
			return action;
		}
		return JUMP_ALLOW;
	}
	KFileContentFilterHelper *next;
private:
	KFileFilterHook *hook;
	int action;
};
class KMultiPartInputFilter : public KInputFilter
{
public:
	KMultiPartInputFilter()
	{
		//isFileContent = false;
		memset(&hm, 0, sizeof(hm));
		param = NULL;
		filename = NULL;
		file_head = file_last = NULL;
		file_content_head = file_content_last = NULL;
	}
	~KMultiPartInputFilter()
	{
		if (hm.header) {
			free_header(hm.header);
		}
		if (param) {
			free(param);
		}
		if (filename) {
			free(filename);
		}
		cleanFileContentHook();
		cleanFileFilterHook();
	}
	int check(KInputFilterContext *rq,const char *str,int len,bool isLast);
	bool registerFileFilter(KFileFilterHook *filter,int action)
	{
		KInputFilterHelper<KFileFilterHook> *helper = new KInputFilterHelper<KFileFilterHook>(filter,action);
		if (file_last) {
			file_last->next = helper;
		} else {
			file_head = helper;
		}
		file_last = helper;
		return true;
	}
	bool registerFileContentFilter(KFileFilterHook *filter,int action)
	{
		KFileContentFilterHelper *helper = new KFileContentFilterHelper(filter,action);
		if (file_content_last) {
			file_content_last->next = helper;
		} else {
			file_content_head = helper;
		}
		file_content_last = helper;
		return true;
	}
private:
	int InternalCheck(KInputFilterContext *rq, const char *str, int len, bool isLast);
	int handleBody(KInputFilterContext *fc,bool &success);
	char *parseBody(KInputFilterContext *fc,int *len,bool &all);
	kgl_parse_result parseHeader(KInputFilterContext *rq);
	int hookFileContent(KInputFilterContext *fc,const char *buf,int len);
	void cleanFileContentHook()
	{
		while (file_content_head) {
			file_content_last = file_content_head->next;
			delete file_content_head;
			file_content_head = file_content_last;
		}
		file_content_last = NULL;
	}
	void cleanFileFilterHook()
	{
		while (file_head) {
			file_last = file_head->next;
			delete file_head;
			file_head = file_last;
		}
		file_last = NULL;
	}
	KHttpHeaderManager hm;
	//bool isFileContent;
	char *param;
	int param_len;
	char *filename;
	int filename_len;
	KInputFilterHelper<KFileFilterHook> *file_head;
	KInputFilterHelper<KFileFilterHook> *file_last;
	KFileContentFilterHelper *file_content_head;
	KFileContentFilterHelper *file_content_last;
};
#endif
#endif
