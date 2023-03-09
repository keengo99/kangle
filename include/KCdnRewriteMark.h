#ifndef KCDNREWRITEMARK_H
#define KCDNREWRITEMARK_H
#include "KMark.h"
#include "KReg.h"

class KHostRewriteMark : public KMark
{
public:
	KHostRewriteMark();
	virtual ~KHostRewriteMark();
	bool mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject **fo) override;
	KMark * new_instance() override;
	const char *getName() override;
	std::string getHtml(KModel *model) override;
	std::string getDisplay() override;
	void parse_config(const khttpd::KXmlNodeBody* xml) override;

private:
	KReg regHost;
	std::string host;
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
	bool mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo) override;
	KMark * new_instance() override;
	const char *getName() override;
	std::string getHtml(KModel *model) override;
	std::string getDisplay() override;
	void parse_config(const khttpd::KXmlNodeBody* xml) override;
private:
	std::string host;
	int port;
	int life_time;
	bool proxy;
	bool rewrite;
	bool ssl;
};
#endif

