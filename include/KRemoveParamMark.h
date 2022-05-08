#ifndef KREMOVEPARAMMARK_H
#define KREMOVEPARAMMARK_H
/**
* 删除url的参数，用于提高缓存命中率
*/
class KRemoveParamMark : public KMark
{
public:
	KRemoveParamMark()
	{
		raw = false;
		revert = false;
	}
	~KRemoveParamMark()
	{
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,
			int &jumpType) {
		KUrl *url = (raw?rq->sink->data.raw_url:rq->sink->data.url);
		char *param = url->param;
		if (param==NULL) {
			return false;
		}
		char *hot = param;
		bool matched = false;
		KStringBuf np;
		for (;;) {
			char *p = strchr(hot,'&');
			if (p) {
				*p = '\0';
			}
			char *value = strchr(hot,'=');
			if (value) {
				*value = '\0';
				value ++;
			}
			if (revert == matchParam(hot)) {
				if (np.getSize()>0) {
					np.write_all("&",1);
				}
				np << hot;
				if (value) {
					np.write_all("=",1);
					np << value;
				}
			} else {
				matched = true;
			}
			if (p==NULL) {
				break;
			}
			hot = p+1;
		}
		free(url->param);
		if (np.getSize()>0) {
			url->param = np.stealString();
		} else {
			url->param = NULL;
		}
		return matched;
	}
	std::string getDisplay() {
		std::stringstream s;
		if (raw) {
			s << "[raw]";
		}
		if (revert) {
			s << "!";
		}
		s << params.getModel();
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html){
		raw = (attribute["raw"]=="1");
		nc = (attribute["nc"]=="1");
		const char *param = attribute["params"].c_str();
		if (*param=='!') {
			revert = true;
			param ++;
		} else {
			revert = false;
		}
		params.setModel(param,(nc?PCRE_CASELESS:0));
	}
	std::string getHtml(KModel *model)
	{
		KRemoveParamMark *m = (KRemoveParamMark *)model;
		std::stringstream s;
		s << "param name(regex):<input name='params' value='";
		if (m) {
			if (m->revert) {
				s << "!";
			}
			s << params.getModel();
		}
		s << "'>";
		s << "<input type=checkbox name='raw' value='1' ";
		if (m && m->raw) {
			s << "checked";
		}
		s << ">raw";
		s << "<input type=checkbox name='nc' value='1' ";
		if (m && m->nc) {
			s << "checked";
		}
		s << ">nc";
		return s.str();		
	}
	KMark *newInstance() {
		return new KRemoveParamMark();
	}
	const char *getName() {
		return "remove_param";
	}
public:
	void buildXML(std::stringstream &s) {
		s << "params='";
		if (revert) {
			s << "!";
		}
		s << params.getModel();
		s << "' raw='" << (raw?1:0) << "'";
		s << " nc='" << (nc?1:0) << "'";
		s << ">";
	}
private:
	bool matchParam(const char *name)
	{
		return params.match(name,strlen(name),0)>0;
	}
	KReg params;
	bool raw;
	bool revert;
	bool nc;
};
#endif
