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
	bool mark(KHttpRequest* rq, KHttpObject* obj, KFetchObject** fo) override {
		if (brd && fo) {
			assert(*fo == nullptr);
			*fo = brd->rd->makeFetchObject(rq, rq->file);
		}
		return true;
	}
	std::string getDisplay() override {
		std::stringstream s;
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
		return s.str();
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
	std::string getHtml(KModel* model) override {
		KMultiServerMark* m = static_cast<KMultiServerMark*>(model);
		std::stringstream s;
		KMultiAcserver::baseHtml((m && m->brd) ? static_cast<KMultiAcserver*>(m->brd->rd) : NULL, s);
		s << "nodes(host:port:lifeTime:weight,...):<br>";
		s << "<input name='nodes' size=40 value='" << (m ? m->nodes : "") << "'/>";
		return s.str();
	}
	KMark* new_instance() override {
		return new KMultiServerMark();
	}
	const char* getName() override {
		return "multi_server";
	}
private:
	KBaseRedirect* brd;
	std::string nodes;
};
#endif
