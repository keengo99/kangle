#ifndef KPOSTFILEMARK_H
#define KPOSTFILEMARK_H
#include "KMark.h"
#ifdef ENABLE_INPUT_FILTER
class KPostFileMark : public KFileFilterHook , public KMark
{
public:
	KPostFileMark()
	{
		icase = 0;
		hit = 0;
	}
	void addRefHook()
	{
		addRef();
	}
	void releaseHook()
	{
		release();
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
			const int chainJumpType, int &jumpType)
	{
		KInputFilterContext *if_ctx = rq->getInputFilterContext();
		if (if_ctx) {
			KInputFilter *filter = if_ctx->getFilter();
			if (filter) {
				filter->registerFileFilter(this,chainJumpType);
			}
		}
		return revers;
	}
	bool matchContent(KInputFilterContext *rq,const char *str,int len)
	{
		return false;
	}
	bool matchFilter(KInputFilterContext *rq,const char *name,int name_len,const char *filename,int filename_len,KHttpHeader *header)
	{
		bool result;
		if (filename && filename_len>0) {
			result = revers!=(reg_filename.match(filename,filename_len,0)>0);
		} else {
			result = revers;
		}
		if (result) {
			hit++;
		}
		return result;
	}
	KMark *newInstance()
	{
		return new KPostFileMark;
	}
	const char *getName()
	{
		return "post_file";
	}
	std::string getDisplay()
	{
		std::stringstream s;
		if(icase){
			s << "[I]";
		}
		s << reg_filename.getModel();
		s << " hit:" << hit;
		return s.str();
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "filename(regex):<textarea name='filename' rows='1'>";
		KPostFileMark *urlAcl = (KPostFileMark *) (model);
		if (urlAcl) {
			s << urlAcl->reg_filename.getModel();
		}
		s << "</textarea>";
		s << "<input type='checkbox' value='1' name='icase' ";
		if (urlAcl && urlAcl->icase) {
			s << "checked";
		}
		s << ">ignore case";
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
	{	
		icase = atoi(attribute["icase"].c_str());
		if (attribute["filename"].size() > 0) {
			setFilename(attribute["filename"].c_str());
		}
	}
	void buildXML(std::stringstream &s)
	{
		s << " filename='" << KXml::param(reg_filename.getModel()) << "' icase='" << icase << "'>";
	}
private:
	KReg reg_filename;
	int icase;
	bool setFilename(const char *value) {
		int flag = (icase?PCRE_CASELESS:0);
		return this->reg_filename.setModel(value,flag);
	}
	int hit;
};
#endif
#endif
