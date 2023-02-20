#ifndef KREPLACEHEADERMARK_H
#define KREPLACEHEADERMARK_H
#include "KMark.h"
#include "KReg.h"
#define MAX_OVECTOR 300
class KReplaceHeaderMark : public KMark
{
public:
	KReplaceHeaderMark() {

	}
	~KReplaceHeaderMark() {

	}
	bool mark(KHttpRequest* rq, KHttpObject* obj, KFetchObject** fo) override {
		bool result = false;
		KHttpHeader* header;
		if (attr.empty()) {
			return false;
		}
		if (obj) {
			if (obj->in_cache) {
				//如果已在缓存中，则不重复操作
				return false;
			}
			header = obj->data->headers;
		} else {
			header = rq->sink->data.get_header();
		}
		int ovector[MAX_OVECTOR];
		while (header) {
			if (!kgl_is_attr(header, attr.c_str(), attr.size())) {
				header = header->next;
				continue;
			}
			int ret = val.match(header->buf + header->val_offset, header->val_len, 0, ovector, MAX_OVECTOR);
			if (ret > 0) {
				KRegSubString* subString = KReg::makeSubString(header->buf + header->val_offset, ovector, MAX_OVECTOR, ret);
				KStringBuf* replaced = KRewriteMarkEx::getString(NULL, replace.c_str(), rq, NULL, subString);
				delete subString;
				if (replaced) {
					int new_buf_len = header->val_offset + replaced->getSize();
					char* new_buf = (char*)malloc(new_buf_len + 1);
					new_buf[new_buf_len] = '\0';
					if (header->val_offset > 0) {
						//copy attr
						memcpy(new_buf, header->buf, header->val_offset);
					}
					memcpy(new_buf + header->val_offset, replaced->getBuf(), replaced->getSize());
					header->val_len = replaced->getSize();
					free(header->buf);
					header->buf = new_buf;
					delete replaced;
				}
				result = true;
			}
			break;
		}
		return result;
	}
	KMark* new_instance() override {
		return new KReplaceHeaderMark;
	}
	const char* getName() override {
		return "replace_header";
	}
	std::string getHtml(KModel* model) override {
		std::stringstream s;
		s << "attr:<input name='attr' value='";
		KReplaceHeaderMark* mark = (KReplaceHeaderMark*)(model);
		if (mark) {
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
	std::string getDisplay() override {
		std::stringstream s;
		s << attr;
		s << ": ";
		s << val.getModel();
		s << "==>" << replace;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string>& attribute, bool html) override {
		attr = attribute["attr"];
		val.setModel(attribute["val"].c_str(), 0);
		replace = attribute["replace"];
	}
	void buildXML(std::stringstream& s) override {
		s << " attr='" << attr << "' val='" << KXml::param(val.getModel()) << "' replace='" << KXml::param(replace.c_str()) << "'>";
	}
private:
	KReg val;
	std::string replace;
	std::string attr;
};
#endif
