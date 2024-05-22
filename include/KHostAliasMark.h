#ifndef KHOSTALIASMARK_H
#define KHOSTALIASMARK_H
struct less_host_port {
	bool operator()(KUrl * __x, KUrl * __y) const {
		int ret = __x->port - __y->port;
		if (ret>0) {
			return false;
		} else if(ret<0) {
			return true;
		}
		return strcasecmp(__x->host,__y->host)<0;
	}
};
class KHostAliasMark : public KMark
{
public:
	KHostAliasMark()
	{
	}
	~KHostAliasMark()
	{
		while (m.size()>0) {
			auto it = m.begin();
			KUrl *u = (*it).first;
			m.erase(it);
			u->relase();
		}
	}
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		KUrl *u = rq->sink->data.url;
		auto it = m.find(u);
		if (it!=m.end()) {
			free(u->host);
			u->host = strdup((*it).second.c_str());
			return KF_STATUS_REQ_TRUE;
		}
		return KF_STATUS_REQ_FALSE;
	}
	void get_display(KWStream &s) override {
		s << "total:" << m.size();
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		char *map = strdup(attribute["map"].c_str());
		char *hot = map;
		for (;;) {
			char *p = strchr(hot,'|');
			if (p) {
				*p = '\0';
			}
			char *alias = strchr(hot,'=');
			if (alias) {
				*alias = '\0';
				alias++;
				char *sport = strchr(hot,':');
				int port = 80;
				if (sport) {
					*sport = '\0';
					sport++;
					port = atoi(sport);
				}
				KUrl *u = new KUrl;
				u->host = strdup(hot);
				u->port = port;
				std::map<KUrl *,std::string,less_host_port>::iterator it;
				it = m.find(u);
				if (it!=m.end()) {
					u->relase();
				} else {
					m.insert(std::pair<KUrl *,std::string>(u,alias));
				}
			}
			if (p==NULL) {
				break;
			}
			hot = p+1;
		}
		free(map);
	}
	void get_html(KModel* model, KWStream& s) override {
		s << "map(host:port=alias|...)<input size=40 name='map' value='";
		KHostAliasMark *m = (KHostAliasMark *)model;
		if (m) {
			m->getMap(s);
		}
		s << "'>";
	}
	KMark * new_instance()override {
		return new KHostAliasMark();
	}
	const char *getName() override {
		return "host_alias";
	}
private:
	void getMap(KWStream& s)
	{
		for (auto it=m.begin();it!=m.end();it++) {
			KUrl *u = (*it).first;
			if (it!=m.begin()) {
				s << "|";
			}
			s << u->host ;
			if (u->port!=80) {
				s << ":" << u->port;
			}
			s << "=" << (*it).second;
		}
	}
	std::map<KUrl *,std::string,less_host_port> m;
};
#endif
