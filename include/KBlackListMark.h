#ifndef KBLACKLISTMARK_H
#define KBLACKLISTMARK_H
#include "KWhiteList.h"
#ifdef ENABLE_BLACK_LIST
class KBlackListMark : public KMark
{
public:
	KBlackListMark() {

	}
	virtual ~KBlackListMark() {
	}
	bool mark(KHttpRequest* rq, KHttpObject* obj, KFetchObject** fo) override {
		KIpList* bls = NULL;
		if (isGlobal) {
			bls = conf.gvm->vhs.blackList;
		} else {
			auto svh = rq->get_virtual_host();
			if (svh) {
				bls = svh->vh->blackList;
			}
		}
		if (bls) {
			bls->AddDynamic(rq->getClientIp());
		}
		return true;
	}
	std::string getDisplay() override {
		return "";
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		/*
if (1== atoi(attribute["enable"].c_str())) {
	enable = true;
} else {
	enable = false;
}
*/
	}
	std::string getHtml(KModel* model) override {
		/*
		KBlackListMark *m_chain = (KBlackListMark *) model;
		std::stringstream s;
		s << "<input type=checkbox name='enable' value='1' ";
		if (m_chain && m_chain->enable) {
			s << "checked";
		}
		s << ">enable" ;
		return s.str();
		*/
		return "";
	}
	KMark* new_instance() override {
		return new KBlackListMark();
	}
	const char* getName() override {
		return "black_list";
	}
private:
	//bool enable;
};
class KCheckBlackListMark : public KMark
{
public:
	KCheckBlackListMark() {
		time_out = 1800;
	}
	virtual ~KCheckBlackListMark() {

	}
	bool mark(KHttpRequest* rq, KHttpObject* obj, KFetchObject** fo) override {
		KIpList* bls = NULL;
		if (isGlobal) {
			bls = conf.gvm->vhs.blackList;
		} else {
			auto svh = rq->get_virtual_host();
			if (svh) {
				bls = svh->vh->blackList;
			}
		}
		if (bls) {
			if (bls->find(rq->getClientIp(), time_out, bls != conf.gvm->vhs.blackList)) {
				//rq->buffer << "HTTP/1.1 403 FORBIDIN\r\nConnection: close\r\n\r\nip is block";
				//jump_type = JUMP_DROP;
				return true;
			}
		}
		return false;
	}
	std::string getDisplay() override {
		if (isGlobal) {
			return "";
		}
		std::stringstream s;
		s << "time:" << time_out;
		return s.str();
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		time_out = atoi(attribute["time_out"].c_str());
	}
	std::string getHtml(KModel* model)override {
		KCheckBlackListMark* m_chain = (KCheckBlackListMark*)model;
		std::stringstream s;
		if (!isGlobal) {
			s << "block time(seconds):<input type=text name='time_out' value='";
			if (m_chain) {
				s << m_chain->time_out;
			} else {
				s << 1800;
			}
			s << "'>";
		}
		return s.str();
	}
	KMark* new_instance() override {
		return new KCheckBlackListMark();
	}
	const char* getName()override {
		return "check_black_list";
	}
private:
	int time_out;
};
#endif
//}}
#endif
