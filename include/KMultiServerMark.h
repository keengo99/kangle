#ifndef KMULTISERVERMARK_H
#define KMULTISERVERMARK_H
#include "KMultiAcserver.h"
#include "KMark.h"
class KMultiServerMark : public KMark
{
public:
	KMultiServerMark() {
		brd = NULL;
	}
	virtual ~KMultiServerMark() {
		if (brd) {
			brd->release();
		}
	}
	bool process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		if (brd) {
			fo.reset(brd->rd->makeFetchObject(rq, rq->file));
		}
		return true;
	}
	void get_display(KWStream& s) override {
		if (brd) {
			KMultiAcserver* as = static_cast<KMultiAcserver*>(brd->rd);
			if (as) {
				s << "<pre>";
				as->buildAttribute(s);
				s << "\nnodes info:\n";
				as->getNodeInfo(s);
				s << "</pre>";
			}
		}
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		KMultiAcserver* as = NULL;
		if (brd == NULL) {
			as = new KMultiAcserver("");
			brd = new KBaseRedirect(as, KConfirmFile::Never);
		} else {
			as = static_cast<KMultiAcserver*>(brd->rd);
		}
		nodes = attribute["nodes"];
		as->parseNode(nodes.c_str());
		as->parse(attribute);
	}
	void get_html(KModel* model, KWStream& s) override {
		KMultiServerMark* m = static_cast<KMultiServerMark*>(model);
		KMultiAcserver::baseHtml((m && m->brd) ? static_cast<KMultiAcserver*>(m->brd->rd) : NULL, s);
		s << "nodes(host:port:lifeTime:weight,...):<br>";
		s << "<input name='nodes' size=40 value='" << (m ? m->nodes : "") << "'/>";
	}
	KMark* new_instance() override {
		return new KMultiServerMark();
	}
	const char* getName() override {
		return "multi_server";
	}
private:
	KBaseRedirect* brd;
	KString nodes;
};
#endif
