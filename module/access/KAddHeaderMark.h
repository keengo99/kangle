#ifndef KADDHEADERMARK_H
#define KADDHEADERMARK_H
#include "KMark.h"
#include "KRewriteMarkEx.h"
class KAddHeaderMark: public KMark
{
public:
	KAddHeaderMark()
	{
		attr = NULL;
		val = NULL;
	}
	~KAddHeaderMark()
	{
		if(attr){
			free(attr);
		}
		if(val){
			free(val);
		}
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
				const int chainJumpType, int &jumpType)
	{
		if (attr && val) {
			KStringBuf *s = KRewriteMarkEx::getString(NULL,val,rq,NULL,NULL);
			if (s==NULL) {
				return true;
			}
			if (obj) {
				//如果是缓存物件，则不再重复增加header
				if (!obj->in_cache && (this->force || obj->findHeader(attr,attr_len)==NULL)) {
					obj->insertHttpHeader((const char *)attr,attr_len,s->getString(),s->getSize());
				}
			} else if (this->force || rq->sink->data.FindHeader(attr, attr_len) ==NULL) {
				rq->sink->data.AddHeader((const char *)attr,attr_len,s->getString(),s->getSize());
			}
			delete s;
		}
		return true;
	}
	KMark *newInstance()
	{
		return new KAddHeaderMark;
	}
	const char *getName()
	{
		return "add_header";
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "attr:<input name='attr' value='";
		KAddHeaderMark *mark = (KAddHeaderMark *) (model);
		if (mark && mark->attr) {
			s << mark->attr;
		}
		s << "'>";
		s << "val:<input name=val value='";
		if (mark && mark->val) {
			s << mark->val;
		}
		s << "'>\n";
		return s.str();
	}
	std::string getDisplay()
	{
		std::stringstream s;
		if(attr){
			s << attr;
		}
		s << ": ";
		if(val){
			s << val;
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute, bool html)
	{
		if(attr){
			free(attr);
			attr = NULL;
		}
		if(val){
			free(val);
			val = NULL;
		}
		attr_len = attribute["attr"].size();
		if(attr_len>0){
			attr = strdup(attribute["attr"].c_str());
		}
		val_len = attribute["val"].size();
		if(val_len>0){
			val = strdup(attribute["val"].c_str());
		}
	}
	void buildXML(std::stringstream &s)
	{
		s << " attr='" << (attr?attr:"") << "' val='" << (val?val:"") << "' force='" << force << "'>";
	}
private:
	int force;
	int attr_len;
	int val_len;
	char *attr;
	char *val;
};
class KAddResponseHeaderMark : public KMark
{
public:
	KAddResponseHeaderMark()
	{
		attr = NULL;
		val = NULL;
	}
	~KAddResponseHeaderMark()
	{
		if (attr) {
			free(attr);
		}
		if (val) {
			free(val);
		}
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
		const int chainJumpType, int &jumpType)
	{
		if (attr && val) {
			KStringBuf *s = KRewriteMarkEx::getString(NULL, val, rq, NULL, NULL);
			if (s == NULL) {
				return true;
			}
			rq->responseHeader((const char *)attr, attr_len, s->getString(), s->getSize());
		}
		return true;
	}
	KMark *newInstance()
	{
		return new KAddResponseHeaderMark;
	}
	const char *getName()
	{
		return "add_response_header";
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "attr:<input name='attr' value='";
		KAddResponseHeaderMark *mark = (KAddResponseHeaderMark *)(model);
		if (mark && mark->attr) {
			s << mark->attr;
		}
		s << "'>";
		s << "val:<input name=val value='";
		if (mark && mark->val) {
			s << mark->val;
		}
		s << "'>\n";
		return s.str();
	}
	std::string getDisplay()
	{
		std::stringstream s;
		if (attr) {
			s << attr;
		}
		s << ": ";
		if (val) {
			s << val;
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
	{
		if (attr) {
			free(attr);
			attr = NULL;
		}
		if (val) {
			free(val);
			val = NULL;
		}
		attr_len = attribute["attr"].size();
		if (attr_len>0) {
			attr = strdup(attribute["attr"].c_str());
		}
		val_len = attribute["val"].size();
		if (val_len>0) {
			val = strdup(attribute["val"].c_str());
		}
	}
	void buildXML(std::stringstream &s)
	{
		s << " attr='" << (attr ? attr : "") << "' val='" << (val ? val : "") << "'>";
	}
private:
	int attr_len;
	int val_len;
	char *attr;
	char *val;
};
#endif
