#include "KReportIp.h"
#include "KMutex.h"
#include "KAsyncWorker.h"
#include "KStringBuf.h"
#include "KHttpHeader.h"
#include "KConfig.h"
#include <list>
#include "KSimulateRequest.h"
#include "KVirtualHostManage.h"
#include "KWhiteList.h"
#ifdef ENABLE_BLACK_LIST
#ifdef ENABLE_SIMULATE_HTTP
static KMutex report_lock;
static std::list<char *> report_items;
static bool report_work_started = false;
void start_report_worker(std::list<char *> &items);
class report_ip_context {
public:
	report_ip_context()
	{
		post = NULL;
		header = NULL;
	}
	~report_ip_context()
	{
		if (post) {
			free(post);
		}
		if (header) {
			free_header(header);
		}
	}
	void set_post(char *post,int post_len)
	{
		this->post = post;
		hot_post = post;
		this->left = post_len;
	}
	char *post;
	char *hot_post;
	int left;
	KHttpHeader *header;
};
static int WINAPI report_http_post_hook (void *arg,char *buf,int len)
{
	report_ip_context *ctx = (report_ip_context *)arg;
	kgl_memcpy(buf,ctx->hot_post,len);
	ctx->hot_post += len;
	ctx->left -= len;
	return len;
}
static int WINAPI report_http_body_hook(void *arg,const char *data,int len)
{
	if (data==NULL) {
		std::list<char *> items;
		report_lock.Lock();
		if (!report_items.empty()) {
			items.swap(report_items);
		} else {
			report_work_started = false;
		}
		report_lock.Unlock();
		report_ip_context *ctx = (report_ip_context *)arg;
		delete ctx;
		if (!items.empty()) {
			start_report_worker(items);
		}
	}
	return 0;
}

void start_report_worker(std::list<char *> &items) {
	kgl_async_http ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.meth = "POST";
	KStringBuf post;
	post << "ips=";
	std::list<char *>::iterator it;
	for (it=items.begin();it!=items.end();it++) {
		post << (*it);
		if (it!=items.begin()) {
			post << ",";
		}
		xfree((*it));
	}
	ctx.post_len = (int64_t)post.getSize();
	report_ip_context *arg = new report_ip_context;
	arg->set_post(post.stealString(),post.getSize());
	arg->header = new_http_header(kgl_expand_string("Content-Type"),kgl_expand_string("application/x-www-form-urlencoded"));
	ctx.arg = arg;
	ctx.post = report_http_post_hook;
	ctx.body = report_http_body_hook;
	ctx.url = conf.report_url;
	ctx.rh = arg->header;
	if (kgl_simuate_http_request(&ctx) != 0) {
		report_lock.Lock();
		report_work_started = false;
		report_lock.Unlock();
		delete arg;
		return;
	}
}
void add_report_item(char *item)
{
	report_lock.Lock();
	std::list<char *> items;
	report_items.push_back(item);
	if (!report_work_started) {
		report_work_started = true;
		items.swap(report_items);
	}
	report_lock.Unlock();
	if (!items.empty()) {
		start_report_worker(items);
	}
}
void report_black_list(const char *ip)
{
	if (!*conf.report_url) {
		return;
	}
	int len = (int)strlen(ip);
	char *item = (char *)malloc(len+2);
	if (item == NULL) {
		return;
	}
	memset(item, 0, len + 2);
	*item = '0';
	kgl_memcpy(item+1,ip,len);
	add_report_item(item);
}
void report_white_list(const char *host,const char *vh,const char *ip)
{
	if (!*conf.report_url) {
		return;
	}
	int host_len = (int)strlen(host);
	int vh_len = 0;
	if (vh) {
		vh_len = (int)strlen(vh);
	}
	int ip_len = (int)strlen(ip);
	int item_len = host_len + vh_len + ip_len + 4;
	char *item = (char *)malloc(item_len);
	if (item == NULL) {
		return;
	}
	memset(item, 0, item_len);
	char *hot = item;
	*hot++ = '1';
	kgl_memcpy(hot,host,host_len);
	hot += host_len;
	if (vh) {
		*hot++ = '@';
		kgl_memcpy(hot,vh,vh_len);
		hot += vh_len;
	}
	*hot++ = '|';
	kgl_memcpy(hot,ip,ip_len);
	add_report_item(item);
}
void add_report_ip(const char *ips)
{
	char *buf = strdup(ips);
	char *hot = buf;
	//<0ip |  1host[@vh]|ip>,...
	while (*hot) {
		char *p = strchr(hot,',');
		if (p) {
			*p++ = '\0';
		}
		if (*hot=='0') {
			hot++;
			conf.gvm->globalVh.blackList->AddDynamic(hot,true);
		} else if (*hot=='1') {
			hot++;
			char *ip = strchr(hot,'|');
			if (ip) {
				char *vh = strchr(hot,'@');
				if (vh) {
					*vh++ = '\0';
				}
				*ip++ = '\0';
				wlm.add(hot,vh,ip,true);
			}
		} else if (*hot == '2') {
			hot++;
			conf.gvm->globalVh.blackList->AddStatic(hot, false);
		} else if (*hot == '3') {
			hot++;
			conf.gvm->globalVh.blackList->AddStatic(hot, true);
		} else if (*hot == '4') {
			hot++;
			conf.gvm->globalVh.blackList->Remove(hot);
		}
		if (p==NULL) {
			break;
		}
		hot = p;
	}
	free(buf);
}
#endif
#endif
