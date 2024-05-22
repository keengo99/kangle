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
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		KStringStream u;
		rq->sink->data.url->GetUrl(u);
		/*
		int len = url_decode(u.getBuf(),u.getSize());
		KRegSubString *subString = url.matchSubString(u.getBuf(),len,0);
		*/
		KRegSubString *subString = url.matchSubString(u.buf(),u.size(),0);
		if (subString==NULL) {
			return KF_STATUS_REQ_FALSE;
		}
		auto nu = KRewriteMarkEx::getString(
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
					fo.reset(new KBufferFetchObject(nullptr, 0));
				}
			} else {
				rq->rewrite_url(nu->c_str(),0,NULL);
				delete nu;
			}
			return KF_STATUS_REQ_TRUE;
		}
		return KF_STATUS_REQ_FALSE;
	}
	KMark * new_instance() override
	{
		return new KUrlRewriteMark;
	}
	const char *getName()override
	{
		return "url_rewrite";
	}
	void get_html(KModel* model, KWStream& s) override {
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
	}
	void get_display(KWStream &s)override
	{
		s << url.getModel() << "=>" << dst;
		if (code>0) {
			s << " " << code;
		}
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		icase = (attribute["icase"]=="1");
		if (attribute["nc"].size()>0) {
			icase = (attribute["nc"]=="1");
		}
		url.setModel(attribute["url"].c_str(),(icase?PCRE_CASELESS : 0));
		dst = attribute["dst"];
		code = atoi(attribute["code"].c_str());
	}
private:
	KReg url;
	KString dst;
	bool icase;
	int code;
};
#endif
