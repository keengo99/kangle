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
	std::string getHtml(KModel *model)override
	{
		std::stringstream s;
		s << "mark(0-255):<input name='v' value='";
		KMarkMark *m = (KMarkMark *)model;
		if (m) {
			s << (int)m->v;
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay()override
	{
		std::stringstream s;
		s << (int)v;
		return s.str();
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		v = atoi(attribute["v"].c_str());
	}
private:
	uint32_t v;
};
#endif
