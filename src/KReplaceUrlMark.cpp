#include "KReplaceUrlMark.h"
#include "KRewriteMarkEx.h"
char *replaceUrlCallBack(void *param,html_tag_t *hit_tag,const char *src_url,int url_len)
{
	KReplaceUrlParam *rp = (KReplaceUrlParam *)param;
	return rp->mark->replaceUrl(hit_tag,src_url,url_len);
}

void replaceUrlEndCallBack(void *param)
{
	KReplaceUrlParam *p = (KReplaceUrlParam *)param;
	p->mark->release();
	delete p;
}
char *KReplaceUrlMark::replaceUrl(html_tag_t *hit_tag,const char *src_url,int url_len)
{
	if (dst==NULL) {
		return NULL;
	}
	KRegSubString *ss = src.matchSubString(src_url,url_len,0);
	if (ss==NULL) {
		return NULL;
	}
	KStringBuf s;
	KRewriteMarkEx::getString(NULL,dst,NULL,NULL,ss,&s);
	delete ss;
	return s.stealString();
}
std::string KReplaceUrlMark::getHtml(KModel *model) {
	std::stringstream s;
	s << "src:<input name='src' value='";
	KReplaceUrlMark *mark = static_cast<KReplaceUrlMark *>(model);
	if (mark) {
		s << mark->src.getModel();
	}
	s << "'><input type=checkbox name='nc' value='1' ";
	if (mark == NULL || mark->nc) {
		s << "checked";
	}
	s << ">no case<br>";
	s << "dst:<input name='dst' value='";
	if (mark && mark->dst) {
		s << mark->dst;
	}
	s << "'>";
	return s.str();
}
bool KReplaceUrlMark::mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,int &jumpType) 
{
	KHtmlTagFilter *filter = new KHtmlTagFilter;
	addRef();
	KReplaceUrlParam *param = new KReplaceUrlParam;
	param->mark = this;
	param->rq = rq;
	filter->setHook(replaceUrlCallBack,replaceUrlEndCallBack,param,url_tag);
	rq->getOutputFilterContext()->registerFilterStream(filter,true);
	if (location && obj->data->status_code>=300 && obj->data->status_code<400) {
		//rewrite location
		KHttpHeader *header = obj->data->headers;
		while (header) {
			if (strcasecmp(header->attr,"Location")==0) {
				char *url = replaceUrlCallBack((void *)param,NULL,header->val,(int)strlen(header->val));
				if (url) {
					free(header->val);
					header->val = url;
				}
			}
			header = header->next;
		}
	}
	return true;
}
