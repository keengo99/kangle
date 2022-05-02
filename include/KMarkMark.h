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
	bool mark(KHttpRequest *rq, KHttpObject *obj,
				const int chainJumpType, int &jumpType)
	{
		rq->sink->data.mark = v;
		return true;
	}
	KMark *newInstance()
	{
		return new KMarkMark;
	}
	const char *getName()
	{
		return "mark";
	}
	std::string getHtml(KModel *model)
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
	std::string getDisplay()
	{
		std::stringstream s;
		s << (int)v;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html) 
	{
		v = atoi(attribute["v"].c_str());
	}
	void buildXML(std::stringstream &s)
	{
		s << " v='" << (int)v << "'>";
	}
private:
	uint32_t v;
};
#endif
