#include "KMapRedirectMark.h"
#include "KHttpRequest.h"
#include "KRewriteMarkEx.h"
#include "http.h"
#include "KBufferFetchObject.h"

void map_redirect_iterator(void *arg, const char *domain, void *vh)
{
	std::stringstream *s = (std::stringstream *)arg;
	if (!s->str().empty()) {
		*s << "\n";
	}
	KMapRedirectItem *item = (KMapRedirectItem *)vh;
	*s << domain << "|" << item->code << " " << item->rewrite;
}
void map_redirect_free_iterator(void *arg, const char *domain, void *vh)
{
	KMapRedirectItem *item = (KMapRedirectItem *)vh;
	delete item;
}
KMapRedirectMark::KMapRedirectMark()
{

}
KMapRedirectMark::~KMapRedirectMark()
{
	vhc.iterator(map_redirect_free_iterator,NULL);
}
bool KMapRedirectMark::mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo)
{
	KMapRedirectItem *item = (KMapRedirectItem *)vhc.find(rq->sink->data.url->host);
	if (item == NULL) {
		return false;
	}
	KStringBuf s;
	int defaultPort = 80;
	if (strstr(item->rewrite, "://") == NULL) {
		if (KBIT_TEST(rq->sink->data.url->flags, KGL_URL_SSL)) {
			s << "https://";
			defaultPort = 443;
		} else {
			s << "http://";
		}
		s << item->rewrite;
		if (rq->sink->data.url->port != defaultPort) {
			s << ":" << rq->sink->data.url->port;
		}
	} else {
		s << item->rewrite;
	}
	rq->sink->data.url->GetPath(s);
	if (push_redirect_header(rq, s.buf(), s.size(), item->code)) {
		*fo = new KBufferFetchObject(nullptr, 0);
		//jump_type = JUMP_DROP;
	}
	return true;
}
KMark *KMapRedirectMark::new_instance()
{
	return new KMapRedirectMark;
}
const char *KMapRedirectMark::getName()
{
	return "map_redirect";
}
std::string KMapRedirectMark::getHtml(KModel *model)
{
	std::stringstream s;
	s << "<textarea name='v' placeHolder='domain|code redirect'>";
	KMapRedirectMark *mark = (KMapRedirectMark *)(model);
	if (mark) {
		s << mark->getValList();
	}
	s << "</textarea>";
	return s.str();
}
std::string KMapRedirectMark::getDisplay()
{
	return this->getValList();
}
void KMapRedirectMark::	parse_config(const khttpd::KXmlNodeBody* xml) {
	auto attribute = xml->attr();
	vhc.clear();
	char *buf = strdup(attribute["v"].c_str());
	char *hot = buf;
	for (;;) {
		char *p = strchr(hot, '\n');
		if (p != NULL) {
			*p++ = '\0';
		}
		char *code_str = strchr(hot, '|');
		if (code_str != NULL) {
			*code_str = '\0';
			code_str++;
			char *rewrite = strchr(code_str, ' ');
			if (rewrite != NULL) {
				while (*rewrite && IS_SPACE(*rewrite)) {
					rewrite++;
				}
				int code = atoi(code_str);
				if (code == 301 || code == 302 || code == 307) {
					KMapRedirectItem *item = new KMapRedirectItem;
					item->code = code;					
					item->rewrite = strdup(rewrite);
					rewrite = item->rewrite;
					while (*rewrite && !IS_SPACE(*rewrite)) {
						rewrite++;
					}
					*rewrite = '\0';
					if (!vhc.bind(hot, item, kgl_bind_unique)) {
						delete item;
					}
				}
			}
		}
		if (p == NULL) {
			break;
		}
		hot = p;
	}
	free(buf);
}
std::string KMapRedirectMark::getValList() {
	std::stringstream s;
	vhc.iterator(map_redirect_iterator, &s);
	return s.str();
}