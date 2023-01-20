#ifndef KFIXHEADERMARK_H
#define KFIXHEADERMARK_H
#include "KMark.h"
#include "KReplaceContentFilter.h"
#include "KHttpRequest.h"
#include "KFilterContext.h"
#include "utils.h"
#include "KUrlParser.h"
#if 0
class KFixHeaderFilter : public KHttpStream
{
public:
	KFixHeaderFilter(const char *header,int len)
	{
		this->header = (char *)malloc(len);
		this->len = len;
		kgl_memcpy(this->header,header,len);		
	}
	~KFixHeaderFilter() {
		if (header) {
			free(header);
		}
	}
	bool support_sendfile(void* rq) override {
		return forward_support_sendfile(rq);
	}
	KGL_RESULT sendfile(void* rq, kfiber_file* fp, int64_t* len) override {
		write_header(rq);
		return KHttpStream::sendfile(rq, fp, len);
	}
	StreamState write_all(void *rq, const char *buf, int len) override
	{
		write_header(rq);
		return KHttpStream::write_all(rq, buf,len);
	}
	StreamState write_end(void *rq, KGL_RESULT result) override
	{
		write_header(rq);
		return KHttpStream::write_end(rq, result);
	}
private:
	void write_header(void*rq) {
		if (header) {
			if (st) {
				st->write_all(rq, header,len);
			}
			free(header);
			header = NULL;
		}
	}
	char *header;
	int len;
};
class KFixHeaderMark : public KMark
{
public:
	KFixHeaderMark()
	{
		buf = NULL;
	}
	~KFixHeaderMark() {
		if (buf) {
			free(buf);
		}
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,
			int &jumpType) 
	{
		if (!rq->sink->data.range) {
			return true;
		}		
		KFixHeaderFilter *filter = new KFixHeaderFilter(buf,len);
		rq->getOutputFilterContext()->registerFilterStream(rq, filter,true);		
		return true;
	}
	std::string getDisplay() {
		std::stringstream s;
		if (buf) {
			s << url_encode(buf,len);
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html){
			if (buf) {
				free(buf);
				buf = NULL;
			}
			buf = strdup(attribute["header"].c_str());
			len = strlen(buf);
			len = url_decode(buf,len);
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		KFixHeaderMark *m = (KFixHeaderMark *)model;
		s << "header:<textarea name='header' rows=1>";
		if (m && m->buf) {
			s << url_encode(m->buf,len);
		}
		s << "</textarea>";		
		return s.str();
	}
	KMark *newInstance() {
		return new KFixHeaderMark;
	}
	const char *getName() {
		return "fix_header";
	}
	void buildXML(std::stringstream &s) {
		if (buf) {
			s << " header='" << url_encode(buf,len) << "'";
		}
		s << ">";
	}
	friend class KReplaceContentFilter;
private:
	char *buf;
	size_t len;
};
#endif

#endif
