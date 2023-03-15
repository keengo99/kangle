#ifndef KREMOVEHEADERMARK_H
#define KREMOVEHEADERMARK_H
#include "KMark.h"
#include "khttp.h"
class KRemoveHeaderMark : public KMark
{
public:
	KRemoveHeaderMark() {
		revers = false;
		val = NULL;
	}
	~KRemoveHeaderMark() {
		if (val) {
			delete val;
		}
	}
	bool process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		bool result = false;
		if (!attr.empty()) {
			KHttpHeader* h;
			KHttpObjectBody* data = NULL;
			if (obj) {
				if (obj->in_cache) {
					//如果已经在缓存中，则不重复操作
					return false;
				}
				data = obj->data;
				h = data->headers;
			} else {
				h = rq->sink->data.get_header();
				if (strcasecmp(attr.c_str(), "Range") == 0 && rq->sink->data.range) {
					rq->sink->data.range = nullptr;
					result = true;
				} else if (strcasecmp(attr.c_str(), "Accept-Encoding") == 0 && rq->sink->data.raw_url->encoding != 0) {
					rq->sink->data.raw_url->encoding = 0;
					rq->sink->data.url->encoding = 0;
				}
			}
			KHttpHeader* last = NULL;
			while (h) {
				KHttpHeader* next = h->next;
				if (kgl_is_attr(h, attr.c_str(), attr.size()) && revers != (val == NULL || val->match(h->buf + h->val_offset, h->val_len, 0) > 0)) {
					if (last) {
						last->next = next;
					} else {
						if (obj) {
							data->headers = next;
						} else {
							rq->sink->data.header = next;
						}
					}
					xfree_header(h);
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
	KMark* new_instance() override {
		return new KRemoveHeaderMark;
	}
	const char* getName()override {
		return "remove_header";
	}
	void get_html(KModel* model, KWStream& s) override {
		s << "attr:<input name='attr' value='";
		KRemoveHeaderMark* mark = (KRemoveHeaderMark*)(model);
		if (mark) {
			s << mark->attr.c_str();
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
	}
	void get_display(KWStream& s) override {
		if (!attr.empty()) {
			s << attr.c_str() << ":";
		}
		if (revers) {
			s << "!";
		}
		if (val) {
			s << val->getModel();
		}
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		attr = attribute["attr"];
		if (val) {
			delete val;
			val = NULL;
		}
		const char* v = attribute["val"].c_str();
		if (*v == '!') {
			revers = true;
			v++;
		} else {
			revers = false;
		}
		if (*v) {
			val = new KReg;
			val->setModel(v, PCRE_CASELESS);
		}
	}
private:
	KString attr;
	KReg* val;
	bool revers;
};
#endif
