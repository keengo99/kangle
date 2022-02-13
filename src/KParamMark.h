#ifndef KPARMMARK_H
#define KPARMMARK_H
#include "KInputFilter.h"
#include "utils.h"
#ifdef ENABLE_INPUT_FILTER
class KParamMark : public KParamFilterHook,public KMark
{
public:
	KParamMark()
	{
		icase = 0;
		name = NULL;
		value = NULL;
		get = true;
		post = true;
		hit = 0;
	}
	~KParamMark()
	{
		if(name){
			free(name);
		}
		if(value){
			free(value);
		}
	}
	void addRefHook()
	{
		addRef();
	}
	void releaseHook()
	{
		release();
	}
	 bool mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType)
	{
		KInputFilterContext *if_ctx = rq->getInputFilterContext();
		if (if_ctx) {
			if (get){
				if (if_ctx->checkGetParam(this)) {
					return true;
				}
			}
			if (post) {
				KInputFilter *filter = if_ctx->getFilter();
				if (filter) {
					filter->registerParamFilter(this,chainJumpType);
				}
			}
		}
		return false;
	}
	KMark *newInstance()
	{
		return new KParamMark;
	}
	const char *getName()
	{
		return "param";
	}
	std::string getDisplay()
	{
		std::stringstream s;
		if (icase) {
			s << "[I]";
		}
		if(name){
			s << "name:" << name << " ";
		}
		if(value){
			s << "value:" << value;
		}
		s << " hit:" << hit;
		return s.str();
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "param name:(regex)<textarea name='name' rows='1'>";
		KParamMark *urlAcl = (KParamMark *) (model);
		if (urlAcl && urlAcl->name) {
			s << urlAcl->name;
		}
		s << "</textarea><br>";
		s << "param value:(regex)<textarea name='value' rows='1'>";
		if (urlAcl && urlAcl->value) {
			s << urlAcl->value;
		}
		s << "</textarea><br>";
		s << "charset:<input name='charset' value='";
		if (urlAcl) {
			s << urlAcl->charset;
		}
		s << "'><br>";
		s << "<input type='checkbox' value='1' name='icase' ";
		if (urlAcl && urlAcl->icase) {
			s << "checked";
		}
		s << ">ignore case";
		s << "<input type='checkbox' value='1' name='get' ";
		if (urlAcl==NULL ||  urlAcl->get) {
			s << "checked";
		}
		s << ">get";
		s << "<input type='checkbox' value='1' name='post' ";
		if (urlAcl==NULL ||  urlAcl->post) {
			s << "checked";
		}
		s << ">post";
		return s.str();
	}
	bool matchFilter(KInputFilterContext *rq,const char *name,int name_len,const char *value,int value_len)
	{
		if (this->name) {
			if (name==NULL) {
				return false;
			}
			if(this->reg_name.match(name,name_len,0)<=0){
				return false;
			}
			if (this->value==NULL) {
				hit++;
				return true;
			}
		}
		if (this->value) {
			if (value==NULL) {
				return false;
			}
			if(this->reg_value.match(value,value_len,0)>0){
				hit++;
				return true;
			}
		}
		return false;
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
	{	
		icase = atoi(attribute["icase"].c_str());
		charset = attribute["charset"];
		get = attribute["get"]=="1";
		post = attribute["post"]=="1";
		setValue(attribute["name"].c_str(),attribute["value"].c_str());
	}
	void buildXML(std::stringstream &s)
	{
		
		if(name){
			s << " name='" << KXml::param(name) << "'";
		}
		if(value){
			s << " value='" << KXml::param(value) << "'";
		}
		s << " get='" << (get?1:0) << "' ";
		s << " post='" << (post?1:0) << "' ";
		s << " icase='" << icase << "' ";
		s << " charset='" << charset << "'>";
	}
private:
	std::string charset;
	char *name;
	char *value;
	KReg reg_name;
	KReg reg_value;
	bool get;
	bool post;
	int icase;
	int hit;
	bool setValue(const char *name,const char *value) {
		int flag = (icase?PCRE_CASELESS:0);
		bool result = false;
		if(this->name){
			free(this->name);
			this->name = NULL;
		}
		if(this->value){
			free(this->value);
			this->value = NULL;
		}
		if(value && *value){
			this->value = strdup(value);
		}
		if(name && *name){
			this->name = strdup(name);
		}
		if (charset.size()>0 && strcasecmp(charset.c_str(),"utf-8")!=0) {
			
			char *str = NULL;
			if(this->value){
				str = utf82charset(this->value, strlen(this->value), charset.c_str());
				if (str) {
					result = this->reg_value.setModel(str,flag);
					free(str);
				}
			}
			if(this->name){
				str = utf82charset(this->name, strlen(this->name), charset.c_str());
				if (str) {
					result = this->reg_name.setModel(str,flag);
					free(str);
				}
			}
		} else {
#ifdef PCRE_UTF8
			flag|=PCRE_UTF8;
#endif
			if(this->value){
				result = this->reg_value.setModel(this->value,flag);
			}
			if(this->name){
				this->reg_name.setModel(this->name,flag);
			}
		}
		return result;
	}
};
class KParamCountContext : public KParamFilterHook
{
public:
	KParamCountContext(int max_count)
	{
		this->max_count = max_count;
		count = 0;
	}
	void releaseHook()
	{
		delete this;
	}
	bool matchFilter(KInputFilterContext *rq,const char *name,int name_len,const char *value,int value_len)
	{
		count ++;
		if (count > max_count) {
			return true;
		}
		return false;
	}
private:
	int count;
	int max_count;
};
class KParamCountMark : public KMark
{
public:
	KParamCountMark()
	{
		get = true;
		post = true;
		max_count = 1000;
	}
	~KParamCountMark()
	{

	}
	 bool mark(KHttpRequest *rq, KHttpObject *obj,
			const int chainJumpType, int &jumpType)
	{
		KInputFilterContext *if_ctx = rq->getInputFilterContext();
		if (if_ctx) {
			KParamCountContext *ctx = new KParamCountContext(max_count);
			if (get){
				if (if_ctx->checkGetParam(ctx)) {
					delete ctx;
					return true;
				}
			}
			if (post) {
				KInputFilter *filter = if_ctx->getFilter();
				if (filter) {					
					if (filter->registerParamFilter(ctx,chainJumpType)) {
						ctx = NULL;
					}
				}
			}
			if (ctx) {
				delete ctx;
			}
		}
		return false;
	}
	KMark *newInstance()
	{
		return new KParamCountMark;
	}
	const char *getName()
	{
		return "param_count";
	}
	std::string getDisplay()
	{
		std::stringstream s;
		s << max_count;
		return s.str();
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		KParamCountMark *urlAcl = (KParamCountMark *) (model);
		s << "max count:<input name='max_count' value='";
		if (urlAcl) {
			s << urlAcl->max_count;
		}
		s << "'><br>";
		s << "<input type='checkbox' value='1' name='get' ";
		if (urlAcl==NULL ||  urlAcl->get) {
			s << "checked";
		}
		s << ">get";
		s << "<input type='checkbox' value='1' name='post' ";
		if (urlAcl==NULL ||  urlAcl->post) {
			s << "checked";
		}
		s << ">post";
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
	{	
		max_count = atoi(attribute["max_count"].c_str());
		get = attribute["get"]=="1";
		post = attribute["post"]=="1";
	}
	void buildXML(std::stringstream &s)
	{		
		s << " get='" << (get?1:0) << "' ";
		s << " post='" << (post?1:0) << "' ";
		s << " max_count='" << max_count << "' ";
		s << ">";
	}
private:
	int max_count;
	bool get;
	bool post;
};
#endif
#endif
