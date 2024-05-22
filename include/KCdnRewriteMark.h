#ifndef KCDNREWRITEMARK_H
#define KCDNREWRITEMARK_H
#include "KMark.h"
#include "KReg.h"

class KHostRewriteMark : public KMark
{
public:
	KHostRewriteMark();
	virtual ~KHostRewriteMark();
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override;
	KMark * new_instance() override;
	const char *getName() override;
	void get_display(KWStream& s) override;
	void get_html(KModel* model, KWStream& s) override;
	void parse_config(const khttpd::KXmlNodeBody* xml) override;

private:
	KReg regHost;
	KString host;
	int port;
	int life_time;
	bool proxy;
	bool rewrite;
};
class KHostMark : public KMark
{
public:
	KHostMark();
	virtual ~KHostMark();
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override;
	KMark * new_instance() override;
	const char *getName() override;
	void get_display(KWStream& s) override;
	void get_html(KModel* model, KWStream& s) override;
	void parse_config(const khttpd::KXmlNodeBody* xml) override;
private:
	KString host;
	int port;
	int life_time;
	bool proxy;
	bool rewrite;
	bool ssl;
};
#endif

