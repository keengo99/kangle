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
	bool ReturnWithRewriteParam(KHttpRequest *rq, KStringBuf &np, bool result)
	{
		set_url_param(np, rq->sink->data.url);
		return result;
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType)
	{
		if (rq->sink->data.url->param==NULL) {
			return false;
		}
		char *hot = rq->sink->data.url->param;
		const char *sign_value = NULL;
		const char *expire_value = NULL;
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
			if (strcmp(hot,sign.c_str())==0) {
				sign_value = value;
			} else if (strcmp(hot,expire.c_str())==0) {
				expire_value = value;
			} else {
				if (np.getSize()>0) {
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
			s << rq->sink->data.raw_url.path;		
		} else {
			const char *e = strrchr(rq->sink->data.raw_url.path,'/');
			if (e==NULL) {
				return ReturnWithRewriteParam(rq, np, false);
			}
			int len = e - rq->sink->data.raw_url.path;
			s.write_all(rq->sink->data.raw_url.path,len+1);
		}
		s << key.c_str() << expire_value;
		if (strstr(expire_value,"ip")!=NULL) {
			s << rq->getClientIp();
		}
		char expected_sign[33];
		KMD5(s.getBuf(), s.getSize(), expected_sign);
		if (strncasecmp(expected_sign,sign_value,32)!=0) {
			return ReturnWithRewriteParam(rq, np, false);
		}
		return ReturnWithRewriteParam(rq, np, true);
	}
	const char *getName() {
		return "path_sign";
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
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
		return s.str();
	}
	KMark *newInstance() {
		return new KPathSignMark();
	}
	std::string getDisplay()
	{
		std::stringstream s;
		s << "sign=" << sign << ",expire=" << expire;
		if (file) {
			s << "[F]";
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
			
	{
		sign = attribute["sign"];
		expire = attribute["expire"];
		key = attribute["key"];
		file = (attribute["file"]=="1");
	}
	void buildXML(std::stringstream &s)
	{
		s << "sign='" << sign << "' expire='" << expire << "' key='" << key << "' ";
		if (file) {
			s << "file='1'";
		}
		s << ">";
	}
private:
	void set_url_param(KStringBuf &np,KUrl *url) {
		free(url->param);
		if (np.getSize()>0) {
			url->param = np.stealString();
		} else {
			url->param = NULL;
		}
	}
	std::string sign;
	std::string expire;
	std::string key;
	bool file;
};
#endif
