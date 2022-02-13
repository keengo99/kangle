#ifndef KREPLACEURLMARK_H
#define KREPLACEURLMARK_H
#include "KMark.h"
#include "KHtmlTagFilter.h"
#include "KReg.h"
#include "KHttpRequest.h"
#include "KFilterContext.h"
#include "utils.h"

char *replaceUrlCallBack(void *param,html_tag_t *tag,const char *src_url,int url_len);
void replaceUrlEndCallBack(void *param);
class KReplaceUrlMark;

struct KReplaceUrlParam
{
	KReplaceUrlMark *mark;
	KHttpRequest *rq;
};
class KReplaceUrlMark : public KMark
{
public:
	KReplaceUrlMark()
	{
		dst = NULL;
		location = true;
		nc = true;
	}
	~KReplaceUrlMark() {
		if (dst) {
			free(dst);
		}
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,int &jumpType) ;
	std::string getDisplay() {
		std::stringstream s;
		s << src.getModel();
		s << "==>";
		if (dst) {
			s << dst;
		}
		if (location) {
			s << " [location]";
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html){
		nc = attribute["nc"]=="1";
		src.setModel(attribute["src"].c_str(),(nc ? PCRE_CASELESS : 0));
		if (dst) {
			free(dst);
			dst = NULL;
		}
		dst = strdup(attribute["dst"].c_str());
	}
	std::string getHtml(KModel *model);
	KMark *newInstance() {
		return new KReplaceUrlMark;
	}
	const char *getName() {
		return "replace_url";
	}
	char *replaceUrl(html_tag_t *hit_tag,const char *src_url,int url_len);
	void buildXML(std::stringstream &s) {
		s << "src='" << KXml::encode(src.getModel()) << "' dst='";
		if (dst) {
			s << KXml::encode(dst);
		}
		s << "'";
		if (nc) {
			s << " nc='1'";
		}
		s << ">";
	}
	friend class KHtmlTagFilter;
	KReg src;
private:
	bool nc;
	char *dst;
	bool location;
};
#endif
