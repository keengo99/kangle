#ifndef KMULTISERVERMARK_H
#define KMULTISERVERMARK_H
#include "KMultiAcserver.h"
#include "KMark.h"
class KMultiServerMark: public KMark {
public:
	KMultiServerMark() {
		brd = NULL;
	}
	virtual ~KMultiServerMark() {
		if (brd) {
			brd->release();
		}
	}
	bool supportRuntime()
	{
		return true;
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,
			int &jumpType) {
		if (brd) {
			KFetchObject *fo = brd->rd->makeFetchObject(rq,rq->file);
			fo->bindBaseRedirect(brd);
			rq->AppendFetchObject(fo);
			jumpType = JUMP_ALLOW;
		}
		return true;
	}
	std::string getDisplay() {
		std::stringstream s;
		if (brd) {
			KMultiAcserver *as = static_cast<KMultiAcserver *>(brd->rd);
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
	void editHtml(std::map<std::string, std::string> &attribute,bool html) {
		KMultiAcserver *as = NULL;
		if (brd==NULL) {
			as = new KMultiAcserver;
			brd = new KBaseRedirect(as,KGL_CONFIRM_FILE_NEVER);
		} else {
			as = static_cast<KMultiAcserver *>(brd->rd);
		}
		nodes = attribute["nodes"];
		as->parseNode(nodes.c_str());
		as->parse(attribute);
	}
	std::string getHtml(KModel *model) {
		KMultiServerMark *m = static_cast<KMultiServerMark *>(model);
		std::stringstream s;
		KMultiAcserver::baseHtml((m && m->brd)?static_cast<KMultiAcserver *>(m->brd->rd):NULL,s);
		s << "nodes(host:port:lifeTime:weight,...):<br>";
		s << "<input name='nodes' size=40 value='" << (m?m->nodes:"") << "'/>";
		return s.str();
	}
	KMark *newInstance() {
		return new KMultiServerMark();
	}
	const char *getName() {
		return "multi_server";
	}
	void buildXML(std::stringstream &s) {
		if (brd) {
			static_cast<KMultiAcserver *>(brd->rd)->buildAttribute(s);
			s << " nodes='" << nodes << "'";
		}
		s << ">";
	}
private:
	KBaseRedirect *brd;
	std::string nodes;
};
#endif
