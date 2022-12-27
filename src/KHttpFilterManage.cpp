#include "KHttpFilterManage.h"
#include "KHttpFilterDsoManage.h"
#include "KHttpFilterDso.h"
#include "KConfig.h"
#include "kselector.h"
#include "KHttpStream.h"
#include "KConfig.h"
#include "http.h"
#include "KVirtualHostManage.h"
#ifdef ENABLE_KSAPI_FILTER
#if 0
KHttpFilterManage::~KHttpFilterManage()
{
}
bool KHttpFilterManage::add(const char *name)
{
	if (conf.hfdm==NULL) {
		return false;
	}
	KHttpFilterDso *filter_dso = conf.hfdm->find(name);
	if (filter_dso==NULL) {
		return false;
	}
	std::map<const char *,KHttpFilterDso *,lessp>::iterator it;
	it = filters.find(name);
	if (it!=filters.end()) {
		return false;
	}
	kgl_filter_version *version = filter_dso->get_version();
	if (KBIT_TEST(version->flags,KF_NOTIFY_REQUEST)) {
		if (hook.request==NULL) {
			hook.request = new KHttpFilterHookCollectRequest;
		}
		hook.request->add_hook(filter_dso);
	}
	if (KBIT_TEST(version->flags,KF_NOTIFY_URL_MAP)) {
		if (hook.url_map==NULL) {
			hook.url_map = new KHttpFilterHookCollectUrlMap;
		}
		hook.url_map->add_hook(filter_dso);
	}
	if (KBIT_TEST(version->flags,KF_NOTIFY_RESPONSE)) {
		if (hook.response==NULL) {
			hook.response = new KHttpFilterHookCollectResponse;
		}
		hook.response->add_hook(filter_dso);
	}
	if (KBIT_TEST(version->flags,KF_NOTIFY_END_REQUEST)) {
		if (hook.end_request==NULL) {
			hook.end_request = new KHttpFilterHookCollect;
		}
		hook.end_request->add_hook(filter_dso);
	}
#if 0
	if (KBIT_TEST(version->flags,KF_NOTIFY_LOG)) {
		if (hook.log==NULL) {
			hook.log = new KHttpFilterHookCollectLog;
		}
		hook.log->add_hook(filter_dso);
	}
#endif
	if (KBIT_TEST(version->flags,KF_NOTIFY_END_CONNECT)) {
		if (hook.end_connection==NULL) {
			hook.end_connection = new KHttpFilterHookCollect;
		}
		hook.end_connection->add_hook(filter_dso);
	}
	if (KBIT_TEST(version->flags,KF_NOTIFY_READ_DATA)) {
		if (hook.read_raw==NULL) {
			hook.read_raw = new KHttpFilterHookCollect;
		}
		hook.read_raw->add_hook(filter_dso);
	}
	if (KBIT_TEST(version->flags,KF_NOTIFY_SEND_DATA)) {
		if (hook.send_raw==NULL) {
			hook.send_raw = new KHttpFilterHookCollect;
		}
		hook.send_raw->add_hook(filter_dso);
	}
	filters.insert(std::pair<const char *,KHttpFilterDso *>(filter_dso->get_name(),filter_dso));
	return true;
}
void KHttpFilterManage::html(std::stringstream &s)
{
	std::map<const char *,KHttpFilterDso *,lessp>::iterator it;
	s << "<table border=1><tr><td>name</td></tr>";
	for (it=filters.begin();it!=filters.end();it++) {
		s << "<tr><td>" << (*it).first << "</td></tr>";
	}
	s << "</table>";
}
bool KHttpFilterManage::check_urlmap(KHttpRequest *rq)
{
	if (hook.url_map==NULL) {
		return true;
	}
	if (hook.url_map->check_urlmap(rq)==JUMP_DENY) {
		stageEndRequest(rq);
		return false;
	}
	return true;
}
int KHttpFilterManage::check_response(KHttpRequest *rq)
{
	if (hook.response==NULL) {
		return JUMP_ALLOW;
	}
	return hook.response->check_response(rq);
}
bool KHttpFilterManage::check_request(KHttpRequest *rq)
{
	if (hook.request==NULL) {
		return true;
	}
	if (hook.request->check_request(rq)==JUMP_DENY) {
#if 0
		if (rq->send_ctx.getBufferSize()>0 || rq->buffer.getLen() > 0) {
#ifdef ENABLE_TF_EXCHANGE
			if (rq->tf) {
				//{{ent
#ifdef ENABLE_FATBOY
				if (rq->tf->post_ctx) {
					rq->tf->startPost(rq, AfterPostForSaveRequest, rq->tf->post_ctx->arg);
					return false;
				}
#endif//}}
				delete rq->tf;
				rq->tf = NULL;
			}
#endif
			rq->start_response_body(-1);
			stageWriteRequest(rq);
			return false;
		}
#endif
		stageEndRequest(rq);
		return false;
	}
	return true;
}
void KHttpFilterManage::buildReadStream(KHttpRequest *rq,KHttpStream **head,KHttpStream **end)
{
	KHttpFilterManage *hfm = conf.gvm->globalVh.hfm;
	if (hfm && hfm->hook.read_raw) {
		hfm->hook.read_raw->buildRawStream(KF_NOTIFY_READ_DATA,rq,head,end);
	}
	if (rq->svh) {
		hfm = rq->svh->vh->hfm;
		if (hfm && hfm->hook.read_raw) {
			hfm->hook.read_raw->buildRawStream(KF_NOTIFY_READ_DATA,rq,head,end);
		}
	}
	return ;
}
void KHttpFilterManage::buildSendStream(KHttpRequest *rq,KHttpStream **head,KHttpStream **end)
{
	KHttpFilterManage *hfm = conf.gvm->globalVh.hfm;
	if (hfm && hfm->hook.send_raw) {
		hfm->hook.send_raw->buildRawStream(KF_NOTIFY_SEND_DATA,rq,head,end);
	}
	if (rq->svh) {
		hfm = rq->svh->vh->hfm;
		if (hfm && hfm->hook.send_raw) {
			hfm->hook.send_raw->buildRawStream(KF_NOTIFY_SEND_DATA,rq,head,end);
		}
	}
	return;
}
#endif
#endif
