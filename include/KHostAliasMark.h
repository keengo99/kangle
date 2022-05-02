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
		std::map<KUrl *,std::string,less_host_port>::iterator it;
		while (m.size()>0) {
			it = m.begin();
			KUrl *u = (*it).first;
			m.erase(it);
			u->destroy();
			delete u;
		}
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,
			int &jumpType) {
		std::map<KUrl *,std::string,less_host_port>::iterator it;
		KUrl *u = rq->sink->data.url;
		it = m.find(u);
		if (it!=m.end()) {
			free(u->host);
			u->host = strdup((*it).second.c_str());
			u->port = 80;
			return true;
		}
		return false;
	}
	std::string getDisplay() {
		std::stringstream s;
		s << "total:" << m.size();
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html){
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
					u->destroy();
					delete u;
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
	std::string getHtml(KModel *model) {
		std::stringstream s;
		s << "map(host:port=alias|...)<input size=40 name='map' value='";
		KHostAliasMark *m = (KHostAliasMark *)model;
		if (m) {
			m->getMap(s);
		}
		s << "'>";
		return s.str();
	}
	KMark *newInstance() {
		return new KHostAliasMark();
	}
	const char *getName() {
		return "host_alias";
	}
public:
	void buildXML(std::stringstream &s) {
		s << "map='";
		getMap(s);
		s << "'";
		s << ">";
	}
private:
	void getMap(std::stringstream &s)
	{
		std::map<KUrl *,std::string,less_host_port>::iterator it;
		for (it=m.begin();it!=m.end();it++) {
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
