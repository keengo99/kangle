#ifndef KCONTEXTECTIONCLOSEMARK_H
#define KCONTEXTECTIONCLOSEMARK_H
#include "KMark.h"
class KConnectionCloseMark : public KMark
{
public:
	KConnectionCloseMark()
	{
	}
	~KConnectionCloseMark()
	{
	}
	KMark *new_instance()override {
		return new KConnectionCloseMark();
	}
	const char *getName() override {
		return "connection_close";
	}
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override
	{
		KBIT_SET(rq->sink->data.flags,RQ_CONNECTION_CLOSE);
		return KF_STATUS_REQ_TRUE;
	}
	void get_display(KWStream& s) override {
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
	}
	void get_html(KWStream& s) override {
	}
private:
};
#endif
