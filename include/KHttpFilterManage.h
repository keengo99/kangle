#ifndef KHTTPFILTERMANAGE_H
#define KHTTPFILTERMANAGE_H
#include "kforwin32.h"
#include "utils.h"
#include "KHttpFilterHook.h"
#include "KHttpFilterHookCollectRequest.h"
#include "KHttpFilterHookCollectResponse.h"
#include <map>
#ifdef ENABLE_KSAPI_FILTER
#if 0
class KHttpFilterDso;
class KHttpStream;
struct KHttpFilterHookPoint
{
	KHttpFilterHookCollectRequest *request;
	KHttpFilterHookCollectResponse *response;
	KHttpFilterHookCollectUrlMap *url_map;
	KHttpFilterHookCollectLog *log;
	KHttpFilterHookCollect *end_request;
	KHttpFilterHookCollect *end_connection;
	KHttpFilterHookCollect *read_raw;
	KHttpFilterHookCollect *send_raw;
};
class KHttpFilterManage
{
public:
	KHttpFilterManage(bool global_flag)
	{
		this->global_flag = global_flag;
		memset(&hook,0,sizeof(hook));
	}
	~KHttpFilterManage();
	//返回false,不用处理
	bool check_urlmap(KHttpRequest *rq);
	bool check_request(KHttpRequest *rq);
	//返回JUMP_DENY或JUMP_ALLOW
	int check_response(KHttpRequest *rq);
	bool add(const char *name);
	void html(std::stringstream &s);
	static void buildReadStream(KHttpRequest *rq,KHttpStream **head,KHttpStream **end);
	static void buildSendStream(KHttpRequest *rq,KHttpStream **head,KHttpStream **end);
	KHttpFilterHookPoint hook;
private:
	bool global_flag;
	std::map<const char *,KHttpFilterDso *,lessp> filters;
};
#endif
#endif
#endif
