#ifndef KREMOVEHEADERMARK_H
#define KREMOVEHEADERMARK_H
#include "KMark.h"
class KRemoveHeaderMark: public KMark
{
public:
	KRemoveHeaderMark()
	{
		attr = NULL;
		revers = false;
		val = NULL;
	}
	~KRemoveHeaderMark()
	{
		if(attr){
			free(attr);
		}
		if (val) {
			delete val;
		}
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
				const int chainJumpType, int &jumpType)
	{	
		bool result = false;
		if (attr) {
			KHttpHeader *h;
			KHttpObjectBody *data = NULL;
			if (obj) {
				if (obj->in_cache) {
					//如果已经在缓存中，则不重复操作
					return true;
				}
				data = obj->data;
				h = data->headers;
			} else {
				h = rq->GetHeader();
			}
			KHttpHeader *last = NULL;
			while (h) {
				KHttpHeader *next = h->next;
				if (is_attr(h,attr) && revers != (val==NULL || val->match(h->val,h->val_len,0)>0) ) {
					if (last) {
						last->next = next;
					} else {
						if (obj) {
							data->headers = next;
						} else {
							rq->header = next;
						}
					}
					if (!obj) {
						if (strcasecmp(attr,"Range")==0) {
							CLR(rq->flags,RQ_HAVE_RANGE);
						} else if (strcasecmp(attr, "Accept-Encoding") == 0) {
							rq->raw_url.encoding = 0;
							rq->url->encoding = 0;
						}
					}
					free(h->attr);
					free(h->val);
					free(h);
					h = next;
					result = true;
					continue;
				}
				last = h;
				h = next;
			}
		}
		return result;
	}
	KMark *newInstance()
	{
		return new KRemoveHeaderMark;
	}
	const char *getName()
	{
		return "remove_header";
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "attr:<input name='attr' value='";
		KRemoveHeaderMark *mark = (KRemoveHeaderMark *) (model);
		if (mark && mark->attr) {
			s << mark->attr;
		}
		s << "'>";
		s << "val(regex):<input name='val' value='";
		if (mark) {
			if (mark->revers) {
				s << "!";
			}
			if (mark->val) {
				s << mark->val->getModel();
			}
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay()
	{
		std::stringstream s;
		if(attr){
			s << attr << ":";
		}
		if (revers) {
			s << "!";
		}
		if (val) {
			s << val->getModel();
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html) 
	{
		if(attr){
			free(attr);
			attr = NULL;
		}
		if(attribute["attr"].size()>0){
			attr = strdup(attribute["attr"].c_str());
		}
		if (val) {
			delete val;
			val = NULL;
		}
		const char *v = attribute["val"].c_str();
		if (*v=='!') {
			revers = true;
			v++;
		} else {
			revers = false;
		}
		if (*v) {
			val = new KReg;
			val->setModel(v,PCRE_CASELESS);
		}
	}
	void buildXML(std::stringstream &s)
	{
		s << " attr='" << (attr?attr:"") << "' val='" ;
		if (revers) {
			s << "!";
		}
		if (val) {
			s << KXml::param(val->getModel());
		}
		s << "' >";
	}
private:
	char *attr;
	KReg *val;
	bool revers;
};
#endif
