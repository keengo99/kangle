#ifndef KPATHSIGNMARK_H
#define KPATHSIGNMARK_H
class KPathSignMark : public KMark
{
public:
	KPathSignMark()
	{
		sign="_KS";
		expire="_KE";
		file = false;
	}
	~KPathSignMark()
	{

	}
	bool ReturnWithRewriteParam(KHttpRequest *rq, KStringStream&np, bool result)
	{
		set_url_param(np, rq->sink->data.url);
		return result;
	}
	bool process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override
	{
		if (rq->sink->data.url->param==NULL) {
			return false;
		}
		char *hot = rq->sink->data.url->param;
		const char *sign_value = NULL;
		const char *expire_value = NULL;
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
			if (strcmp(hot,sign.c_str())==0) {
				sign_value = value;
			} else if (strcmp(hot,expire.c_str())==0) {
				expire_value = value;
			} else {
				if (np.size()>0) {
					np.write_all("&",1);
				}
				np << hot;
				if (value) {
					np.write_all("=",1);
					np << value;
				}
			}
			if (p==NULL) {
				break;
			}
			hot = p+1;
		}
		if (sign_value==NULL) {
			return ReturnWithRewriteParam(rq, np, false);
		}
		INT64 expire_time =0;
		if (expire_value==NULL) {
			expire_value = strchr(sign_value,'-');
			if (expire_value==NULL) {
				return ReturnWithRewriteParam(rq, np, false);
			}
			expire_value++;
		}
		expire_time = string2int(expire_value);		
		if (expire_time>0 && expire_time < kgl_current_sec) {
			return ReturnWithRewriteParam(rq, np, false);
		}
		KStringBuf s;
		if (file) {
			s << rq->sink->data.raw_url->path;		
		} else {
			const char *e = strrchr(rq->sink->data.raw_url->path,'/');
			if (e==NULL) {
				return ReturnWithRewriteParam(rq, np, false);
			}
			int len = e - rq->sink->data.raw_url->path;
			s.write_all(rq->sink->data.raw_url->path,len+1);
		}
		s << key.c_str() << expire_value;
		if (strstr(expire_value,"ip")!=NULL) {
			s << rq->getClientIp();
		}
		char expected_sign[33];
		KMD5(s.buf(), s.size(), expected_sign);
		if (strncasecmp(expected_sign,sign_value,32)!=0) {
			return ReturnWithRewriteParam(rq, np, false);
		}
		return ReturnWithRewriteParam(rq, np, true);
	}
	const char *getName() override  {
		return "path_sign";
	}
	void get_html(KModel* model, KWStream& s) override {
		KPathSignMark *m = (KPathSignMark *)model;
		s << "sign:<input name='sign' value='";
		if (m) {
			s << m->sign;
		}
		s << "'>";
		s << "expire:<input name='expire' value='";
		if (m) {
			s << m->expire;
		}
		s << "'>";
		s << "key:<input name='key' value='";
		if (m) {
			s << m->key;
		}
		s << "'>";
		s << "<input name='file' value='1' type='checkbox' ";
		if (m && m->file) {
			s << "checked";
		}
		s << ">file";
	}
	KMark * new_instance() override  {
		return new KPathSignMark();
	}
	void get_display(KWStream& s) override {
		s << "sign=" << sign << ",expire=" << expire;
		if (file) {
			s << "[F]";
		}
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		sign = attribute["sign"];
		expire = attribute["expire"];
		key = attribute["key"];
		file = (attribute["file"]=="1");
	}
private:
	void set_url_param(KStringStream &np,KUrl *url) {
		free(url->param);
		if (np.size()>0) {
			url->param = np.steal().release();
		} else {
			url->param = NULL;
		}
	}
	KString sign;
	KString expire;
	KString key;
	bool file;
};
#endif
