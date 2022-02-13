#ifndef KREPLACEHEADERMARK_H
#define KREPLACEHEADERMARK_H
#include "KMark.h"
#include "KReg.h"
#define MAX_OVECTOR 300
class KReplaceHeaderMark: public KMark
{
public:
	KReplaceHeaderMark()
	{
		attr = NULL;
	}
	~KReplaceHeaderMark()
	{
		if(attr){
			free(attr);
		}	
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
				const int chainJumpType, int &jumpType)
	{
		bool result = false;
		KHttpHeader *header;
		if (obj) {
			if (obj->in_cache) {
				//如果已在缓存中，则不重复操作
				return true;
			}
			header = obj->data->headers;
		} else {
			header = rq->GetHeader();
		}
		int ovector[MAX_OVECTOR];
		while (header) {
			if (attr==NULL || strcasecmp(attr,header->attr)==0) {
				int ret = val.match(header->val,header->val_len,0,ovector,MAX_OVECTOR);
				if (ret>0) {
					KRegSubString *subString = KReg::makeSubString(header->val,ovector,MAX_OVECTOR,ret);
					KStringBuf *replaced = KRewriteMarkEx::getString(NULL,replace.c_str(),rq,NULL,subString);
					delete subString;
					if (replaced) {
						free(header->val);
						header->val_len = replaced->getSize();
						header->val = replaced->stealString();						
						delete replaced;
					}
					result = true;
				}
			}
			header = header->next;
		}	
		return result;
	}
	KMark *newInstance()
	{
		return new KReplaceHeaderMark;
	}
	const char *getName()
	{
		return "replace_header";
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "attr:<input name='attr' value='";
		KReplaceHeaderMark *mark = (KReplaceHeaderMark *) (model);
		if (mark && mark->attr) {
			s << mark->attr;
		}
		s << "'>";
		s << "val(regex):<textarea name='val' rows=1>";
		if (mark) {
			s << mark->val.getModel();
		}
		s << "</textarea>";
		s << "replace:<textarea name='replace' rows=1>";
		if (mark) {
			s << mark->replace;
		}
		s << "</textarea>";
		return s.str();
	}
	std::string getDisplay()
	{
		std::stringstream s;
		if (attr) {
			s << attr;
		}
		s << ": ";
		s << val.getModel();
		s << "==>" << replace;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
	{
		if (attr) {
			free(attr);
			attr = NULL;
		}
		if (attribute["attr"].size()>0) {
			attr = strdup(attribute["attr"].c_str());
		}
		val.setModel(attribute["val"].c_str(),0);
		replace = attribute["replace"];
	}
	void buildXML(std::stringstream &s)
	{
		s << " attr='" << (attr?attr:"") << "' val='" << KXml::param(val.getModel()) << "' replace='" << KXml::param(replace.c_str()) << "'>";
	}
private:
	char *attr;
	KReg val;
	std::string replace;
};
#endif
