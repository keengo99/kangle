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
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		KUrl *url = (raw?&rq->sink->data.raw_url:rq->sink->data.url);
		char *param = url->param;
		if (param==NULL) {
			return KF_STATUS_REQ_FALSE;
		}
		char *hot = param;
		bool matched = false;
		KStringStream np;
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
				if (np.size()>0) {
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
		if (np.size()>0) {
			url->param = np.steal().release();
		} else {
			url->param = NULL;
		}
		return matched? KF_STATUS_REQ_TRUE: KF_STATUS_REQ_FALSE;
	}
	void get_display(KWStream &s) override {
		if (raw) {
			s << "[raw]";
		}
		if (revert) {
			s << "!";
		}
		s << params.getModel();
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		raw = (attribute["raw"]=="1");
		nc = (attribute["nc"]=="1");
		const char *param = attribute["params"].c_str();
		if (*param=='!') {
			revert = true;
			param ++;
		} else {
			revert = false;
		}
		params.setModel(param,(nc? KGL_PCRE_CASELESS :0));
	}
	void get_html(KModel* model, KWStream& s) override {
		KRemoveParamMark *m = (KRemoveParamMark *)model;
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
	}
	KMark * new_instance() override {
		return new KRemoveParamMark();
	}
	const char *getName()override {
		return "remove_param";
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
