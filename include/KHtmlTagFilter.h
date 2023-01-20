#ifndef KHtmlTagFilter_H
#define KHtmlTagFilter_H
#include "KStream.h"
#include "KHttpRequest.h"
#include "utils.h"
#if 0
struct html_tag_t
{
	const char *name;
	int name_len;
	const char *attr;
	int attr_len;
	html_tag_t *next;
};
class KReplaceUrlMark;
typedef char * (* rewriteUrlCallBack)(void *param,html_tag_t *tag,const char *src_url,int url_len);
typedef void (* rewriteUrlEndCallBack)(void *param);
class KHtmlTagFilter : public KHttpStream
{
public:
	KHtmlTagFilter();
	~KHtmlTagFilter();
	StreamState write_all(void *rq, const char *buf, int len) override;
	StreamState write_end(void*rq, KGL_RESULT result) override;
	void setHook(rewriteUrlCallBack rewriteUrl,rewriteUrlEndCallBack rewriteUrlEnd,void *param,html_tag_t *tag_head)
	{
		this->rewriteUrl = rewriteUrl;
		this->rewriteUrlEnd = rewriteUrlEnd;
		this->param = param;
		this->tag_head = tag_head;
	}
private:
	bool dealTag(void*rq, const char *str,const char *end);
	html_tag_t *matchTag(const char *tag,int len);
	//找URL,-1=失败,成功返回charEnd('或")或1表示没有charEnd
	int findUrl(const char *str,const char *end,html_tag_t *tag,const char **url,int &url_len);
	bool getTag(const char *start,const char *end,const char **tag_start,int &tag_len);
	//-1=失败，0=tag，成功返回charEnd('或")或1表示没有charEnd
	int getTagValue(const char *start,const char *end,const char **tag_val,int &len);
	bool writeBuffer(const char *str,int len);
	//char *rewriteUrl(const char *src_url,int url_len);
	bool matchString(const char *str1,int str1_len,const char *str2,int str2_len)
	{
		if(str1_len!=str2_len){
			return false;
		}
		return strncasecmp(str1,str2,str1_len)==0;
	}
	bool dumpBuffer();
	char *prevData;
	int prevLen;
	rewriteUrlCallBack rewriteUrl;
	rewriteUrlEndCallBack rewriteUrlEnd;
	void *param;
	html_tag_t *tag_head;
};
extern html_tag_t url_tag[];
#endif
#endif
