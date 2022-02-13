#include "KHtmlTagFilter.h"
#include "KReplaceUrlMark.h"
#include "utils.h"
html_tag_t url_tag[] = {
	{"a",1,"href",4},
	{"form",4,"action",6},
	{"area",4,"href",4},
	{"frame",5,"src",3},
	{"input",5,"src",3},
	{"img",3,"src",3},
	{"javascript",10,"src",3},
	{"base",4,"href",4},
	{"script",6,"src",3},
	{"link",4,"href",4},
	{"iframe",6,"src",3},
	{NULL,0,NULL}
};
//static const char *css_url = "[\\s:]url\\s*(\\()";
/*
������,��ǿ�(spaceָ��)
*/
static const char *skipSpace(const char *start,const char *end,bool space=true)
{
	while(start<end){
		if((isspace((unsigned char)*start)!=0) != space){
			return start;
		}
		start++;
	}
	return NULL;
}
KHtmlTagFilter::KHtmlTagFilter()
{
	prevData = NULL;
	prevLen = 0;
}
KHtmlTagFilter::~KHtmlTagFilter()
{
	if (rewriteUrlEnd) {
		rewriteUrlEnd(param);
	}
	if (prevData) {
		xfree(prevData);
	}
}
StreamState KHtmlTagFilter::write_all(KHttpRequest *rq, const char *str, int len)
{
	char *buf = NULL;
	const char *start = NULL;
	if(prevData){
		//����ǰ��δ���������ݡ�
		assert(prevLen>0);
		buf = (char *)xmalloc(prevLen+len);
		kgl_memcpy(buf,prevData,prevLen);
		kgl_memcpy(buf+prevLen,str,len);
		xfree(prevData);
		prevData = NULL;
		str = buf;
		len+=prevLen;
		start = str;
	} else {
		start = (const char *)memchr(str,'<',len);
	}
	StreamState result = STREAM_WRITE_FAILED;
	for(;;){
		if(start==NULL){
			//assert(len==strlen(str));
			if(len>0){
				result = KHttpStream::write_all(rq, str,len);
			}else{
				result = STREAM_WRITE_SUCCESS;
			}
			break;
		}
		int send_len = start - str;
		if(send_len>0){
			//����<֮ǰ������
			if(KHttpStream::write_all(rq, str,send_len)==STREAM_WRITE_FAILED){
				break;
			}
			str+=send_len;
			len-=send_len;
		}
		const char *end = (const char *)memchr(str,'>',len);
		if(end==NULL){
			prevData = (char *)xmalloc(len);
			kgl_memcpy(prevData,str,len);
			prevLen = len;
			result = STREAM_WRITE_SUCCESS;
			break;
		}
		if(!dealTag(rq, str,end)){
			break;
		}
		if(KHttpStream::write_all(rq, ">",1)==STREAM_WRITE_FAILED){
			break;
		}
		str = end+1;
		len -= (end-start+1);
		assert(len>=0);
		if(len>0){
			start = (const char *)memchr(str,'<',len);
		}else{
			start = NULL;
		}
	}
	if(buf){
		xfree(buf);
	}
	return result;
}
bool KHtmlTagFilter::dealTag(KHttpRequest *rq, const char *str,const char *end)
{
	const char *tagName;
	int tag_len;
	if(!getTag(str+1,end,&tagName,tag_len)){
			return KHttpStream::write_all(rq, str,end-str) == STREAM_WRITE_SUCCESS;
	}
	html_tag_t *tag = matchTag(tagName,tag_len);
	if(tag==NULL){
		//tag��ƥ��
		return KHttpStream::write_all(rq, str,end-str) == STREAM_WRITE_SUCCESS;
	}
	const char *src_url = NULL;
	int url_len;
	int result = findUrl(tagName+tag_len,end,tag,&src_url,url_len);
	if(result<=0){
		return KHttpStream::write_all(rq, str,end-str) == STREAM_WRITE_SUCCESS;
	}
	//����URL�Ƿ��б�Ҫ��д
	char *url = rewriteUrl(param,tag,src_url,url_len);
	if(url==NULL){
			return KHttpStream::write_all(rq, str,end-str) == STREAM_WRITE_SUCCESS;
	}
	if(KHttpStream::write_all(rq, str,src_url-str)!=STREAM_WRITE_SUCCESS){
		xfree(url);
		return false;
	}
	KHttpStream::write_all(rq, url,strlen(url));
	str = src_url+url_len;
	xfree(url);
	if(result>1){
		KHttpStream::write_all(rq, (char *)&result,1);
		str++;
	}
	int endLen = end-str;
	if(endLen>0){
		return KHttpStream::write_all(rq, str,endLen) == STREAM_WRITE_SUCCESS;
	}
	return true;
}
bool KHtmlTagFilter::getTag(const char *start,const char *end,const char **tag,int &tag_len)
{
	const char *p = start;
	*tag = skipSpace(start,end);
	if(*tag==NULL){
		return false;
	}
	p = *tag;
	const char *tag_end = NULL;
	while(p<end){
		if(isspace((unsigned char)*p) || *p=='='){
			tag_end = p;
			break;
		}
		p++;
	}
	if(tag_end==NULL){
		return false;
	}
	tag_len = tag_end - (*tag);
	return true;
}
//-1=ʧ�ܣ�0=tag���ɹ�����charEnd('��")��1��ʾû��charEnd
int KHtmlTagFilter::getTagValue(const char *start,const char *end,const char **tag,int &len)
{
	const char *p = skipSpace(start,end);
	if(p==NULL){
		return -1;
	}
	if(*p!='='){
		return 0;
	}
	const char *attr = skipSpace(p+1,end);
	if(attr==NULL){
		return -1;
	}
	int endChar = 1;
	const char *attrEnd;
	if(*attr=='\'' || *attr=='"'){
		endChar = *attr;
		attr++;
		attrEnd = (const char *)memchr(attr,endChar,end-attr);
	}else{
		//ֵû������,�ҵ�һ���ո�
		const char *p = skipSpace(attr,end);
		if(p==NULL){
			return -1;
		}
		attrEnd = skipSpace(attr,end,false);

	}	
	if(attrEnd==NULL){
		attrEnd = end;
	}
	len = attrEnd - attr;
	*tag = attr;
	return endChar;
}
html_tag_t *KHtmlTagFilter::matchTag(const char *tag,int len)
{
	html_tag_t *t = tag_head;
	while (t) {
		if(matchString(tag,len,t->name,t->name_len)){
			return t;
		}
		t = t->next;
	}
	return NULL;
}
int KHtmlTagFilter::findUrl(const char *str,const char *end,html_tag_t *tag,const char **url,int &url_len)
{		
	const char *tagName;
	bool urlFinded = false;
	for(;;){
		if(!getTag(str,end,&tagName,url_len)){
			break;
		}
		if(matchString(tagName,url_len,tag->attr,tag->attr_len)){
			urlFinded = true;
		}
		str = tagName + url_len;
		int result = getTagValue(str,end,url,url_len);
		if(result>0){
			if(urlFinded){
				return result;
			}
			str = *url + url_len;
			if(result>1){
				str++;
			}
		}
		if(result<0){
			break;
		}
	}
	return -1;
}
StreamState KHtmlTagFilter::write_end(KHttpRequest *rq, KGL_RESULT result)
{
	if (prevData) {
		KGL_RESULT result2 = KHttpStream::write_all(rq, prevData, prevLen);
		if (result2!= STREAM_WRITE_SUCCESS){
			return KHttpStream::write_end(rq, STREAM_WRITE_FAILED);
		}
	}
	return KHttpStream::write_end(rq, result);
}
/*
StreamState KHtmlTagFilter::KWUpStream::write_all(const char *buf,int len)
{
	const char *hot = buf;
	//todo:bufferҪɾ��
	char *buffer = NULL;
	StreamState result = STREAM_WRITE_SUCCESS;
	if (prevCssData) {
		int new_len = len + prevCssLen;
		buffer = (char *)malloc(new_len + 1);
		kgl_memcpy(buffer,prevCssData,prevCssLen);
		kgl_memcpy(buffer + prevCssLen,buf,len);
		free(prevCssData);
		prevCssData = NULL;
		hot = buffer;
		len = new_len;
	}
	if (read_css_url_model) {
		if (!read_css_url(&hot,len)) {
			prevCssLen = len;
			prevCssData = (char *)malloc(prevCssLen);
			kgl_memcpy(prevCssData,hot,prevCssLen);
			len = 0;
		}
	}
	int ovector[6];
	while (len>0) {
		int ret = cssReg.match(hot,len,PCRE_PARTIAL,ovector,6);
		if (ret>0) {
			if (KWUpStream::write_all(hot,ovector[3])!=STREAM_WRITE_SUCCESS) {
				result = STREAM_WRITE_FAILED;
				break;
			}
			int start = ovector[3];
			hot += start;
			len -= start;
			read_css_url_model = true;
			if (read_css_url(&hot,len)) {
				continue;
			}
			ret = PCRE_ERROR_PARTIAL;
		}
		if (ret==PCRE_ERROR_PARTIAL) {
			prevCssLen = len;
			prevCssData = (char *)malloc(prevCssLen);
			kgl_memcpy(prevCssData,hot,prevCssLen);
			break;
		}
		if (!KWUpStream::write_all(hot,len)) {
			result = STREAM_WRITE_FAILED;
		}
		break;
	}
	if (buffer) {
		free(buffer);
	}
	return result;
}
StreamState KHtmlTagFilter::KWUpStream::write_all_end()
{
	if (prevCssData) {
		if(KWUpStream::write_all(prevCssData,prevCssLen)!=STREAM_WRITE_SUCCESS){
			return STREAM_WRITE_FAILED;
		}
	}
	return KWUpStream::write_end();
}
bool KHtmlTagFilter::read_css_url(const char **buf,int &len)
{
	const char *orign_buf = *buf;
	int orign_len = len;
	while (len>0 && isspace((unsigned char)**buf)) {
		(*buf)++;
		len--;
	}
	if (len<=0) {
		*buf = orign_buf;
		len = orign_len;
		return false;
	}
	const char *end = (const char *)memchr(*buf,')',len);
	if (end==NULL) {
		*buf = orign_buf;
		len = orign_len;
		return false;
	}
	char end_char=0;
	if (**buf=='"' || **buf=='\'') {
		end_char=(**buf);
		(*buf)++;
		len--;
	}
	assert(len>=0);
	while (end>*buf) {
		if (isspace((unsigned char)*end)) {
			end--;
			continue;
		}
		if (end_char==0 || end_char==*end) {
			break;
		}
		end--;
	}
	const char *src_url = *buf;
	int url_len = end - *buf;
	int space_len = *buf - orign_buf;
	if (space_len>0) {
		if (KWUpStream::write_all(orign_buf,space_len)!=STREAM_WRITE_SUCCESS) {
			return true;
		}
	}
	read_css_url_model = false;
	char *url = rewriteUrl(param,"url",src_url,url_len);
	if (url==NULL) {
		return KWUpStream::write_all(src_url,url_len) == STREAM_WRITE_SUCCESS;
	}
	KWUpStream::write_all(url,strlen(url));
	*buf += url_len;
	len -= url_len;
	return true;	
}
*/