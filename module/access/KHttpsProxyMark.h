#ifndef KHTTPSPROXYMARK_H_99
#define KHTTPSPROXYMARK_H_99
#include "KMark.h"
class KHttpsProxyMark : public KMark {
	bool mark(KHttpRequest* rq, KHttpObject* obj, const int chainJumpType, int& jumpType) 
	{
		return true;
	}
	std::string getDisplay() {
		return "";
	}
	KMark* newInstance() {
		return new KHttpsProxyMark();
	}
	const char* getName() {
		return "https_proxy";
	}
	std::string getHtml(KModel* model) {
		return "";
	}
	void editHtml(std::map<std::string, std::string>& attribute, bool html) 
	{
	}
};
#endif
