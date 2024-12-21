#ifndef KRANDACL_H
#define KRANDACL_H
#include "KAcl.h"
class KRandAcl : public KAcl
{
public:
	KRandAcl()
	{
		count = 0;
		total = 1;
	}
	~KRandAcl()
	{
	}
	void get_html(KWStream& s) override {
		s << "<input name='rand' value='";
		KRandAcl *urlAcl = (KRandAcl *) (this);
		if (urlAcl) {
			urlAcl->get_display(s);
		}
		s << "'>";
	}
	KAcl *new_instance() override {
		return new KRandAcl();
	}
	const char *getName() override {
		return "rand";
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		return (rand() % total) < count;
	}
	void get_display(KWStream& s) override {
		s << count << "/" << total;
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		setRand(attribute["rand"].c_str());
	}
private:
	void setRand(const char *str)
	{
		count = atoi(str);
		total = 0;
		const char *p = strchr(str,'/');
		if (p) {
			total = atoi(p+1);
		}
		if (total==0) {
			total = 1;
		}
	}
	int count;
	int total;
};
#endif
