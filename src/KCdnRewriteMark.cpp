#include "KCdnRewriteMark.h"
#include "KCdnContainer.h"
#include "KRewriteMarkEx.h"
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
bool KHostRewriteMark::mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,
			int &jumpType)
{

	KRegSubString *ss = regHost.matchSubString(rq->url->host,strlen(rq->url->host),0);
	if (ss==NULL) {
		return false;
	}
	KStringBuf *cdn_host = KRewriteMarkEx::getString(NULL,host.c_str(),rq,NULL,ss);
	if (cdn_host==NULL) {
		delete ss;
		return false;
	}
	if (rewrite) {
		free(rq->url->host);
		rq->url->host = cdn_host->stealString();
		if (port>0) {
			rq->url->port = port;
		}
		SET(rq->raw_url.flags,KGL_URL_REWRITED);
	}
	if (proxy) {	
		const char *ssl = NULL;
		if (TEST(rq->url->flags, KGL_URL_SSL)) {
			ssl = "s";
		}
		KFetchObject *fo = server_container->get(NULL,(rewrite?rq->url->host:cdn_host->getString()),(port>0?port:rq->url->port),ssl,life_time);
		if (fo) {
			rq->AppendFetchObject(fo);
		}
		jumpType = JUMP_ALLOW;
	}
	delete ss;
	delete cdn_host;
	return true;
}
KMark *KHostRewriteMark::newInstance()
{
	return new KHostRewriteMark;
}
const char *KHostRewriteMark::getName()
{
	return "host_rewrite";
}
std::string KHostRewriteMark::getHtml(KModel *model)
{
	KHostRewriteMark *mark = static_cast<KHostRewriteMark *>(model);
	stringstream s;
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
	return s.str();
}
std::string KHostRewriteMark::getDisplay()
{
	stringstream s;
	s << regHost.getModel() << "=>" << host << ":" << port;
	s << "[";
	if (proxy) {
		s << "P";
	}
	if (rewrite) {
		s << "R";
	}
	s << "]";
	return s.str();
}
void KHostRewriteMark::editHtml(std::map<std::string, std::string> &attribute,bool html)
			
{
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
void KHostRewriteMark::buildXML(std::stringstream &s)
{
	s << " reg_host='" << regHost.getModel() << "' host='" << host << "' port='" << port << "' ";
	if (proxy) {
		s << "proxy='1' ";
	}
	if (rewrite) {
		s << "rewrite='1' ";
	}
	s << "life_time='" << life_time << "'>";
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
bool KHostMark::mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType, int &jumpType)
{
	if (rewrite) {
		free(rq->url->host);
		rq->url->host = strdup(host.c_str());
		if (port>0) {
			rq->url->port = port;
		}
		SET(rq->raw_url.flags,KGL_URL_REWRITED);
	}
	if (proxy) {		
		KFetchObject * fo = server_container->get(NULL,(rewrite?rq->url->host:host.c_str()),(port>0?port:rq->url->port),(ssl?"s":NULL),life_time);
		if (fo) {
			rq->AppendFetchObject(fo);
		}
		jumpType = JUMP_ALLOW;
	}
	return true;
}
KMark *KHostMark::newInstance()
{
	return new KHostMark;
}
const char *KHostMark::getName()
{
	return "host";
}
std::string KHostMark::getHtml(KModel *model)
{
	KHostMark *mark = static_cast<KHostMark *>(model);
	stringstream s;
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
	return s.str();
}
std::string KHostMark::getDisplay()
{
	stringstream s;
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
	return s.str();
}
void KHostMark::editHtml(std::map<std::string, std::string> &attribute,bool html)
			
{
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
void KHostMark::buildXML(std::stringstream &s)
{
	s << " host='" << host << "' port='" << port;
	if (ssl) {
		s << "s";
	}
	s << "' ";
	if (proxy) {
		s << "proxy='1' ";
	}
	if (rewrite) {
		s << "rewrite='1' ";
	}
	s << "life_time='" << life_time << "'>";
}


