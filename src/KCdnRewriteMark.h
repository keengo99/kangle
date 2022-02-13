#ifndef KCDNREWRITEMARK_H
#define KCDNREWRITEMARK_H
#include "KMark.h"
#include "KReg.h"

class KHostRewriteMark : public KMark
{
public:
	KHostRewriteMark();
	virtual ~KHostRewriteMark();
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,
			int &jumpType);
	KMark *newInstance();
	const char *getName();
	std::string getHtml(KModel *model);
	std::string getDisplay();
	void editHtml(std::map<std::string, std::string> &attribute,bool html);
	void buildXML(std::stringstream &s);

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
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,
			int &jumpType);
	KMark *newInstance();
	const char *getName();
	std::string getHtml(KModel *model);
	std::string getDisplay();
	void editHtml(std::map<std::string, std::string> &attribute,bool html);
	void buildXML(std::stringstream &s);

private:
	std::string host;
	int port;
	int life_time;
	bool proxy;
	bool rewrite;
	bool ssl;
};
#endif

