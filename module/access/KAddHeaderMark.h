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
	bool process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		if (attr && val) {
			auto s = KRewriteMarkEx::getString(NULL,val,rq,NULL,NULL);
			if (s==NULL) {
				return true;
			}
			if (obj) {
				//如果是缓存物件，则不再重复增加header
				if (!obj->in_cache && (this->force || obj->find_header(attr,attr_len)==NULL)) {
					obj->insert_http_header((const char *)attr,attr_len,s->c_str(),s->size());
				}
			} else if (this->force || rq->sink->data.find(attr, attr_len) ==NULL) {
				rq->sink->data.add_header((const char *)attr,attr_len,s->c_str(),s->size());
			}
			delete s;
		}
		return true;
	}
	KMark * new_instance() override
	{
		return new KAddHeaderMark;
	}
	const char *getName() override
	{
		return "add_header";
	}
	void get_html(KModel *model,KWStream &s) override
	{
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
	}
	void get_display(KWStream& s)override {
		if(attr){
			s << attr;
		}
		s << ": ";
		if(val){
			s << val;
		}
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
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
	bool process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		if (attr && val) {
			auto s = KRewriteMarkEx::getString(NULL, val, rq, NULL, NULL);
			if (s == NULL) {
				return true;
			}
			rq->response_header((const char *)attr, attr_len, s->c_str(), s->size());
		}
		return true;
	}
	KMark *new_instance() override
	{
		return new KAddResponseHeaderMark;
	}
	const char *getName() override
	{
		return "add_response_header";
	}
	void get_html(KModel* model, KWStream& s) override {
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
	}
	void get_display(KWStream& s) override {
		if (attr) {
			s << attr;
		}
		s << ": ";
		if (val) {
			s << val;
		}
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
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
private:
	int attr_len;
	int val_len;
	char *attr;
	char *val;
};
#endif
