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
	bool mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo) override
	{
		rq->sink->data.mark = v;
		return true;
	}
	KMark * new_instance() override
	{
		return new KMarkMark;
	}
	const char *getName() override
	{
		return "mark";
	}
	void get_html(KModel* model, KWStream& s) override {
		s << "mark(0-255):<input name='v' value='";
		KMarkMark *m = (KMarkMark *)model;
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
