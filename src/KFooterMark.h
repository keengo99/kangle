#ifndef KFOOTERMARK_H
#define KFOOTERMARK_H
#include "KFooterFilter.h"
#include "KFilterContext.h"
class KFooterMark : public KMark
{
public:
	KFooterMark()
	{
		head = false;
		replace = false;
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,
		int &jumpType) {
		KFooterFilter *filter = new KFooterFilter;
		filter->footer = footer;
		filter->head = head;
		filter->replace = replace;
		filter->added = false;
		rq->getOutputFilterContext()->registerFilterStream(filter,true);
		return true;
	}
	std::string getDisplay() {
		return "hidden";
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html) {
		footer = attribute["footer"];
		if(attribute["head"]=="1"){
			head = true;
		} else {
			head = false;
		}
		replace = (attribute["replace"]=="1");
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		KFooterMark *mark = (KFooterMark *) model;
		s << "<textarea name='footer'>";
		if (mark) {
			s << mark->footer;
		}
		s << "</textarea>";
		s << "<input type=checkbox name=head value='1' ";
		if(mark && mark->head){
			s << "checked";
		}
		s << ">add to head";
		s << "<input type=checkbox name=replace value='1' ";
		if(mark && mark->replace){
			s << "checked";
		}
		s << ">replace";
		return s.str();
	}
	KMark *newInstance() {
		return new KFooterMark();
	}
	const char *getName() {
		return "footer";
	}
	bool startCharacter(KXmlContext *context, char *character, int len) {
		footer = character;
		return true;
	}
	void buildXML(std::stringstream &s) {
		if (head) {
			s << "head='1'";
		}
		if (replace) {
			s << " replace='1'";
		}
		s << ">" << CDATA_START << footer << CDATA_END ;
	}
private:
	std::string footer;
	bool head;
	bool replace;
};
#endif
