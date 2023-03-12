#include "KCdnRewriteMark.h"
#include "KCdnContainer.h"
#include "KRewriteMarkEx.h"
#include "KHttpRequest.h"
using namespace std;
KHostRewriteMark::KHostRewriteMark()
{
		life_time = 0;
		port = 0;
		proxy = false;
}
KHostRewriteMark::~KHostRewriteMark()
{
}
bool KHostRewriteMark::mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo)
{

	KRegSubString* ss = regHost.matchSubString(rq->sink->data.url->host, (int)strlen(rq->sink->data.url->host), 0);
	if (ss==NULL) {
		return false;
	}
	auto cdn_host = KRewriteMarkEx::getString(NULL,host.c_str(),rq,NULL,ss);
	if (cdn_host==NULL) {
		delete ss;
		return false;
	}
	if (rewrite) {
		free(rq->sink->data.url->host);
		rq->sink->data.url->host = cdn_host->steal().release();
		if (port>0) {
			rq->sink->data.url->port = port;
		}
		KBIT_SET(rq->sink->data.raw_url->flags,KGL_URL_REWRITED);
	}
	if (proxy) {	
		const char *ssl = NULL;
		if (KBIT_TEST(rq->sink->data.url->flags, KGL_URL_SSL)) {
			ssl = "s";
		}
		*fo = server_container->get(NULL,(rewrite?rq->sink->data.url->host:cdn_host->c_str()),(port>0?port:rq->sink->data.url->port),ssl,life_time);
		//jump_type = JUMP_ALLOW;
	}
	delete ss;
	delete cdn_host;
	return true;
}
KMark *KHostRewriteMark::new_instance()
{
	return new KHostRewriteMark;
}
const char *KHostRewriteMark::getName()
{
	return "host_rewrite";
}
void KHostRewriteMark::get_html(KModel *model,KWStream &s)
{
	KHostRewriteMark *mark = static_cast<KHostRewriteMark *>(model);
	s << "reg_host: <input name='reg_host' size=30 value='" << (mark?mark->regHost.getModel():"") << "'><br>";
	s << "host: <input name='host' value='" << (mark?mark->host.c_str():"") << "'>";
	s << "port: <input name='port' value='" << (mark?mark->port:80) << "' size=6><br>";
	s << "life_time: <input name='life_time' size=5 value='" << (mark?mark->life_time:10) << "'>";
	s << "<input type=checkbox name='proxy' value='1' ";
	if (mark  && mark->proxy) {
		s << "checked";
	}
	s << ">proxy";
	s << "<input type=checkbox name='rewrite' value='1' ";
	if (mark  && mark->rewrite) {
		s << "checked";
	}
	s << ">rewrite";
}
void KHostRewriteMark::get_display(KWStream &s)
{
	s << regHost.getModel() << "=>" << host << ":" << port;
	s << "[";
	if (proxy) {
		s << "P";
	}
	if (rewrite) {
		s << "R";
	}
	s << "]";
}
void KHostRewriteMark::parse_config(const khttpd::KXmlNodeBody* xml) {
	auto attribute = xml->attr();
	regHost.setModel(attribute["reg_host"].c_str(),PCRE_CASELESS);
	host = attribute["host"];
	port = atoi(attribute["port"].c_str());
	life_time = atoi(attribute["life_time"].c_str());
	if (attribute["proxy"] == "1" || attribute["proxy"]=="on" ) {
		proxy = true;
	} else {
		proxy = false;
	}
	if (attribute["rewrite"] == "1" || attribute["rewrite"]=="on") {
		rewrite = true;
	} else {
		rewrite = false;
	}
}

KHostMark::KHostMark()
{
		life_time = 0;
		port = 0;
		proxy = false;
		ssl = false;
}
KHostMark::~KHostMark()
{
}
bool KHostMark::mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject **fo)
{
	if (rewrite) {
		free(rq->sink->data.url->host);
		rq->sink->data.url->host = strdup(host.c_str());
		if (port>0) {
			rq->sink->data.url->port = port;
		}
		KBIT_SET(rq->sink->data.raw_url->flags,KGL_URL_REWRITED);
	}
	if (proxy) {		
		*fo = server_container->get(NULL,(rewrite?rq->sink->data.url->host:host.c_str()),(port>0?port:rq->sink->data.url->port),(ssl?"s":NULL),life_time);
	}
	return true;
}
KMark *KHostMark::new_instance()
{
	return new KHostMark;
}
const char *KHostMark::getName()
{
	return "host";
}
void KHostMark::get_html(KModel *model,KWStream &s)
{
	KHostMark *mark = static_cast<KHostMark *>(model);
	s << "host: <input name='host' value='" << (mark?mark->host.c_str():"") << "'>";
	s << "port: <input name='port' value='" << (mark?mark->port:80);
	if (ssl) {
		s << "s";
	}
	s << "' size=6><br>";
	s << "life_time: <input name='life_time' size=5 value='" << (mark?mark->life_time:10) << "'>";
	s << "<input type=checkbox name='proxy' value='1' ";
	if (mark  && mark->proxy) {
		s << "checked";
	}
	s << ">proxy";
	s << "<input type=checkbox name='rewrite' value='1' ";
	if (mark  && mark->rewrite) {
		s << "checked";
	}
	s << ">rewrite";
}
void KHostMark::get_display(KWStream &s)
{
	s << host << ":" << port;
	if (ssl) {
		s << "s";
	}
	s << "[";
	if (proxy) {
		s << "P";
	}
	if (rewrite) {
		s << "R";
	}
	s << "]";
}
void KHostMark::parse_config(const khttpd::KXmlNodeBody* xml) {
	auto attribute = xml->attr();
	host = attribute["host"];
	port = atoi(attribute["port"].c_str());
	if (strchr(attribute["port"].c_str(),'s')) {
		ssl = true;
	} else {
		ssl = false;
	}
	life_time = atoi(attribute["life_time"].c_str());
	if (attribute["proxy"] == "1" || attribute["proxy"]=="on" ) {
		proxy = true;
	} else {
		proxy = false;
	}
	if (attribute["rewrite"] == "1" || attribute["rewrite"]=="on") {
		rewrite = true;
	} else {
		rewrite = false;
	}
}


