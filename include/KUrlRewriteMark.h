#ifndef KURLREWRITEMARK_H
#define KURLREWRITEMARK_H
#include "KBufferFetchObject.h"
class KUrlRewriteMark : public KMark
{
public:
	KUrlRewriteMark()
	{
		code = 0;
		icase = true;
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo)override
	{
		KStringBuf u;
		rq->sink->data.url->GetUrl(u);
		/*
		int len = url_decode(u.getBuf(),u.getSize());
		KRegSubString *subString = url.matchSubString(u.getBuf(),len,0);
		*/
		KRegSubString *subString = url.matchSubString(u.buf(),u.size(),0);
		if (subString==NULL) {
			return false;
		}
		KStringBuf *nu = KRewriteMarkEx::getString(
			NULL,
			dst.c_str(),
			rq,
			NULL, 
			subString
		);
		delete subString;
		if (nu) {
			if (code>0) {
				if (push_redirect_header(rq, nu->c_str(), nu->size(), code)) {
					*fo = new KBufferFetchObject(nullptr, 0);
					//jump_type = JUMP_DROP;
				}
			} else {
				rq->rewrite_url(nu->c_str(),0,NULL);
				delete nu;
			}
			return true;
		}
		return false;
	}
	KMark * new_instance()override
	{
		return new KUrlRewriteMark;
	}
	const char *getName()override
	{
		return "url_rewrite";
	}
	std::string getHtml(KModel *model)override
	{
		std::stringstream s;
		KUrlRewriteMark *m = (KUrlRewriteMark *)model;
		s << "url:<input name='url' size=32 value='";
		if (m) {
			s << m->url.getModel();
		}
		s << "'>,dst:<input name='dst' size=32 value='";
		if (m) {
			s << m->dst;
		}
		s << "'>";
		s << "code<input name='code' size=4 value='";
		if (m) {
			s << m->code;
		} else {
			s << 0;
		}
		s << "'>";
		s << "<input type=checkbox name='nc' value='1' ";
		if (m && m->icase) {
			s << "checked";
		}
		s << ">nc";
		return s.str();
	}
	std::string getDisplay()override
	{
		std::stringstream s;
		s << url.getModel() << "=>" << dst;
		if (code>0) {
			s << " " << code;
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)override
	{
		icase = (attribute["icase"]=="1");
		if (attribute["nc"].size()>0) {
			icase = (attribute["nc"]=="1");
		}
		url.setModel(attribute["url"].c_str(),(icase?PCRE_CASELESS : 0));
		dst = attribute["dst"];
		code = atoi(attribute["code"].c_str());
	}
	void buildXML(std::stringstream &s)override
	{
		s << "url='" << KXml::param(url.getModel()) << "' dst='" << KXml::param(dst.c_str()) << "'";
		if (icase) {
			s << " nc='1'";
		}
		if (code>0) {
			s << " code='" << code << "'";
		}
		s << ">";
	}
private:
	KReg url;
	std::string dst;
	bool icase;
	int code;
};
#endif
