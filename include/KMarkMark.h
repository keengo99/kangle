#ifndef KMARKMARK_H
#define KMARKMARK_H
class KMarkMark : public KMark
{
public:
	KMarkMark()
	{
		v = 0;
	};
	~KMarkMark()
	{
	}
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override
	{
		rq->sink->data.mark = v;
		return KF_STATUS_REQ_TRUE;
	}
	KMark * new_instance() override
	{
		return new KMarkMark;
	}
	const char *get_module() const override
	{
		return "mark";
	}
	void get_html(KWStream& s) override {
		s << "mark(0-255):<input name='v' value='";
		KMarkMark *m = (KMarkMark *)this;
		if (m) {
			s << (int)m->v;
		}
		s << "'>";
	}
	void get_display(KWStream& s) override {
		s << (int)v;
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		v = atoi(attribute["v"].c_str());
	}
private:
	uint32_t v;
};
#endif
