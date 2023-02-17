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
	void editHtml(std::map<std::string, std::string> &attribute,bool html)override
	{
		v = atoi(attribute["v"].c_str());
	}
	void buildXML(std::stringstream &s)override
	{
		s << " v='" << (int)v << "'>";
	}
private:
	uint32_t v;
};
#endif
