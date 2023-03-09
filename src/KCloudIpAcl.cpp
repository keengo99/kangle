#include "KCloudIpAcl.h"
#include "KSimulateRequest.h"
#include "KTimer.h"
#include "kselector_manager.h"
#ifdef ENABLE_SIMULATE_HTTP
static kev_result cloud_ip_start(KOPAQUE data, void *arg, int got) {
	KCloudIpAcl *acl = (KCloudIpAcl *)arg;
	acl->start_http();
	return kev_ok;
}
static void WINAPI cloud_ip_timer(void *arg)
{
	KCloudIpAcl *acl = (KCloudIpAcl *)arg;
	acl->start_http();
}
static int WINAPI cloud_ip_http_body_hook(void *arg, const char *data, int len)
{
	KCloudIpAcl *acl = (KCloudIpAcl *)arg;
	acl->http_body_hook(data, len);
	return 0;
}
void KCloudIpAcl::http_body_hook(const char *data, int len)
{
	if (data == NULL) {
		if (!this->started) {
			this->release();
			return;
		}
		int flush_time = this->flush_time;
		if (!this->parse_data()) {
			//如果失败，则30秒后重试
			flush_time = 30;
		}
		timer_run(cloud_ip_timer, this, flush_time * 1000);
		return;
	}
	if (len >= 0) {
		this->data.write_all(data, len);
	}
}
bool KCloudIpAcl::parse_data()
{
	char *buf = data.steal();
	//data.init(512);
	char *hot = strchr(buf, '\n');
	if (hot == NULL) {
		free(buf);
		return false;
	}
	*hot++ = '\0';
	if (this->sign && strcmp(this->sign,buf)==0) {
		//not changed
		free(buf);
		return true;
	}
	int buf_len = (int)strlen(buf);
	if (buf_len < 6) {
		klog(KLOG_ERR,"cloud_ip buf=[%s] len=[%d] is too short min len [6]\n",buf,buf_len);
		free(buf);
		return false;
	}
	char *end = strchr(hot, '\n');
	if (end == NULL) {
		klog(KLOG_ERR,"cloud_ip no end char\n");
		free(buf);
		return false;
	}
	*end = '\0';
	KIpMap *im2 = new KIpMap;
	im2->add_multi_addr(hot, '|', (void *)1);
	lock.Lock();
	if (this->im) {
		delete this->im;
	}
	this->im = im2;
	if (this->sign) {
		free(this->sign);
	}
	this->sign = buf;	
	lock.Unlock();
	return true;
}
KCloudIpAcl::KCloudIpAcl()
{
	this->flush_time = 3600;
	this->started = false;
	sign = NULL;
	im = NULL;
}
KCloudIpAcl::~KCloudIpAcl()
{
	if (this->sign) {
		free(this->sign);
	}
	if (im) {
		delete im;
	}
}
std::string KCloudIpAcl::getHtml(KModel *model)
{
	std::stringstream s;
	KCloudIpAcl *m = (KCloudIpAcl *)model;
	s << "url:<input name='url' size=50 value='";
	if (m){
		s << m->url;
	}
	s << "'>";
	s << "flush_time(second):<input name='flush_time' size=4 value='";
	if (m) {
		s << m->flush_time;
	}	
	s << "'>";
	return s.str();
}
KAcl *KCloudIpAcl::new_instance()
{
	return new KCloudIpAcl;
}
const char *KCloudIpAcl::getName()
{
	return "cloud_ip";
}
bool KCloudIpAcl::match(KHttpRequest *rq, KHttpObject *obj)
{
	const char *client_ip = rq->getClientIp();	
	bool result = false;
	lock.Lock();
	if (im) {
		result = (im->find(client_ip) != NULL);
	}
	lock.Unlock();
	return result;
}
std::string KCloudIpAcl::getDisplay()
{
	std::stringstream s;
	s << url;
	lock.Lock();
	if (this->sign!=NULL) {
		s << " " << this->sign;
	}
	lock.Unlock();
	return s.str();
}
void KCloudIpAcl::parse_config(const khttpd::KXmlNodeBody* xml)
{
	auto attribute = xml->attr();
	lock.Lock();
	url = attribute["url"];
	flush_time = atoi(attribute["flush_time"].c_str());
	lock.Unlock();
	this->start();
}
void KCloudIpAcl::start() {

	if (!this->started) {
		this->started = true;
		this->add_ref();
		selector_manager_on_ready(cloud_ip_start, this);		
	}

}
void KCloudIpAcl::start_http()
{	
	this->data.clean();	
	kgl_async_http ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.meth = "GET";
	lock.Lock();
	ctx.url = (char *)this->url.c_str();
	ctx.body = cloud_ip_http_body_hook;
	ctx.arg = this;
	if (kgl_simuate_http_request(&ctx) != 0) {
		this->started = false;
		this->release();
	}
	lock.Unlock();
}
#endif
