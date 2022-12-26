#ifndef KHTTPONLYCOOKIEMARK_H
#define KHTTPONLYCOOKIEMARK_H
#include "KMark.h"
#define HTTP_ONLY_STRING		"; HttpOnly"
#define COOKIE_SECURE_STRING     "; Secure"
class KHttpOnlyCookieMark : public KMark
{
public:
	bool mark(KHttpRequest* rq, KHttpObject* obj, const int chainJumpType, int& jumpType)
	{
		bool result = false;
		if (obj && obj->data) {
			KHttpHeader* h = obj->data->headers;
			while (h) {
				if (h->name_is_know && (h->know_header == kgl_header_set_cookie || h->know_header == kgl_header_set_cookie2)) {
					if (kgl_memstr(h->buf, h->val_len, _KS(HTTP_ONLY_STRING)) == NULL) {
						if (cookie.match(h->buf, h->val_len, 0) > 0) {
							int new_len = h->val_len + sizeof(HTTP_ONLY_STRING) - 1;
							char* buf = (char*)malloc(new_len);
							kgl_memcpy(buf, h->buf, h->val_len);
							kgl_memcpy(buf + h->val_len, _KS(HTTP_ONLY_STRING));
							free(h->buf);
							h->buf = buf;
							h->val_len += sizeof(HTTP_ONLY_STRING) - 1;
							result = true;
						}
					}
				}//if
				h = h->next;
			}//while
		}//if
		return result;
	}
	KMark* newInstance()
	{
		return new KHttpOnlyCookieMark;
	}
	const char* getName()
	{
		return "http_only";
	}
	std::string getHtml(KModel* model)
	{
		std::stringstream s;
		s << "deprecated use cookie mark.<br>Cookie regex:<input name='cookie' value='";
		KHttpOnlyCookieMark* m = (KHttpOnlyCookieMark*)model;
		if (m) {
			s << m->cookie.getModel();
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay()
	{
		return cookie.getModel();
	}
	void editHtml(std::map<std::string, std::string>& attribute, bool html)
	{
		cookie.setModel(attribute["cookie"].c_str(), 0);
	}
	void buildXML(std::stringstream& s)
	{
		s << " cookie='" << KXml::param(cookie.getModel()) << "'>";
	}
private:
	KReg cookie;
};

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
	bool mark(KHttpRequest* rq, KHttpObject* obj, const int chainJumpType, int& jumpType)
	{
		bool result = false;
		if (obj == NULL || obj->data == NULL) {
			return false;
		}
		KHttpHeader* h = obj->data->headers;
		while (h) {
			if (h->name_is_know && (h->know_header == kgl_header_set_cookie || h->know_header == kgl_header_set_cookie2) && (cookie == NULL || cookie->match(h->buf, h->val_len, 0) > 0)) {
				if (http_only && kgl_memstr(h->buf, h->val_len, kgl_expand_string(HTTP_ONLY_STRING)) == NULL) {
					int new_len = h->val_len + sizeof(HTTP_ONLY_STRING);
					char* buf = (char*)malloc(new_len);
					kgl_memcpy(buf, h->buf, h->val_len);
					kgl_memcpy(buf + h->val_len, HTTP_ONLY_STRING, sizeof(HTTP_ONLY_STRING));
					free(h->buf);
					h->buf = buf;
					h->val_len += sizeof(HTTP_ONLY_STRING) - 1;
					result = true;
				}
				if (secure && kgl_memstr(h->buf, h->val_len, kgl_expand_string(COOKIE_SECURE_STRING)) == NULL) {
					int new_len = h->val_len + sizeof(COOKIE_SECURE_STRING);
					char* buf = (char*)malloc(new_len);
					kgl_memcpy(buf, h->buf, h->val_len);
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
	KMark* newInstance()
	{
		return new KCookieMark;
	}
	const char* getName()
	{
		return "cookie";
	}
	std::string getHtml(KModel* model)
	{
		std::stringstream s;
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
		return s.str();
	}
	std::string getDisplay()
	{
		std::stringstream s;
		if (cookie) {
			s << cookie->getModel();
		}
		if (http_only) {
			s << " http_only";
		}
		if (secure) {
			s << " secure";
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string>& attribute, bool html)
	{
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
	void buildXML(std::stringstream& s)
	{
		if (cookie) {
			s << " cookie='" << KXml::param(cookie->getModel()) << "'";
		}
		if (http_only) {
			s << " http_only='1'";
		}
		if (secure) {
			s << " secure='1'";
		}
		s << ">";
	}
private:
	KReg* cookie;
	bool http_only;
	bool secure;
};
#endif
