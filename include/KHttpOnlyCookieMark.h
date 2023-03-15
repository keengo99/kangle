#ifndef KHTTPONLYCOOKIEMARK_H
#define KHTTPONLYCOOKIEMARK_H
#include "KMark.h"
#include "KHttpObject.h"
#define HTTP_ONLY_STRING		"; HttpOnly"
#define COOKIE_SECURE_STRING     "; Secure"
class KCookieMark : public KMark
{
public:
	KCookieMark()
	{
		cookie = NULL;
		http_only = false;
		secure = false;
	}
	~KCookieMark()
	{
		if (cookie) {
			delete cookie;
		}
	}
	bool process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override
	{
		bool result = false;
		if (obj == NULL || obj->data == NULL) {
			return false;
		}
		KHttpHeader* h = obj->data->headers;
		while (h) {
			if (h->name_is_know && (h->know_header == kgl_header_set_cookie) && (cookie == NULL || cookie->match(h->buf + h->val_offset, h->val_len, 0) > 0)) {
				assert(h->val_offset == 0);
				if (http_only && kgl_memstr(h->buf + h->val_offset, h->val_len, kgl_expand_string(HTTP_ONLY_STRING)) == NULL) {
					int new_len = h->val_len + sizeof(HTTP_ONLY_STRING);
					char* buf = (char*)malloc(new_len);
					kgl_memcpy(buf, h->buf + h->val_offset, h->val_len);
					kgl_memcpy(buf + h->val_len, HTTP_ONLY_STRING, sizeof(HTTP_ONLY_STRING));
					free(h->buf);
					h->buf = buf;
					h->val_len += sizeof(HTTP_ONLY_STRING) - 1;
					result = true;
				}
				if (secure && kgl_memstr(h->buf + h->val_offset, h->val_len, kgl_expand_string(COOKIE_SECURE_STRING)) == NULL) {
					int new_len = h->val_len + sizeof(COOKIE_SECURE_STRING);
					char* buf = (char*)malloc(new_len);
					kgl_memcpy(buf, h->buf + h->val_offset, h->val_len);
					kgl_memcpy(buf + h->val_len, COOKIE_SECURE_STRING, sizeof(COOKIE_SECURE_STRING));
					free(h->buf);
					h->buf = buf;
					h->val_len += sizeof(COOKIE_SECURE_STRING) - 1;
					result = true;
				}
			}//if
			h = h->next;
		}//while
		return result;
	}
	KMark* new_instance() override
	{
		return new KCookieMark;
	}
	const char* getName() override
	{
		return "cookie";
	}
	void get_html(KModel* model, KWStream& s) override {
		s << "Cookie regex:<input name='cookie' value='";
		KCookieMark* m = (KCookieMark*)model;
		if (m && m->cookie) {
			s << m->cookie->getModel();
		}
		s << "'>";
		s << "<input name='http_only' value='1' type='checkbox' ";
		if (m && m->http_only) {
			s << "checked";
		}
		s << ">http_only";
		s << "<input name='secure' value='1' type='checkbox' ";
		if (m && m->secure) {
			s << "checked";
		}
		s << ">secure";
	}
	void get_display(KWStream& s) override {
		if (cookie) {
			s << cookie->getModel();
		}
		if (http_only) {
			s << " http_only";
		}
		if (secure) {
			s << " secure";
		}
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		if (cookie) {
			delete cookie;
			cookie = NULL;
		}
		if (!attribute["cookie"].empty()) {
			cookie = new KReg;
			cookie->setModel(attribute["cookie"].c_str(), 0);
		}
		http_only = (attribute["http_only"] == "1");
		secure = (attribute["secure"] == "1");
	}
private:
	KReg* cookie;
	bool http_only;
	bool secure;
};
#endif
