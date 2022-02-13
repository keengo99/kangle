#ifndef KBLACKLISTMARK_H
#define KBLACKLISTMARK_H
#include "KWhiteList.h"
//{{ent
#ifdef ENABLE_BLACK_LIST
class KBlackListMark: public KMark {
public:
	KBlackListMark() {
		
	}
	virtual ~KBlackListMark() {
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,
			int &jumpType) {
		KIpList *bls = NULL;
		if (isGlobal) {
			bls = conf.gvm->globalVh.blackList;
		} else {
			if (rq->svh) {
				bls = rq->svh->vh->blackList;
			}
		}
		if (bls) {
			bls->AddDynamic(rq->getClientIp());
		}
		jumpType = JUMP_DROP;
		return true;
	}
	std::string getDisplay() {
		return "";
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html){
				/*
		if (1== atoi(attribute["enable"].c_str())) {
			enable = true;
		} else {
			enable = false;
		}	
		*/
	}
	std::string getHtml(KModel *model) {
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
	KMark *newInstance() {
		return new KBlackListMark();
	}
	const char *getName() {
		return "black_list";
	}
public:
	void buildXML(std::stringstream &s) {
		//s << " enable='" << (enable?1:0) << "'>";
		s << ">";
	}
private:
	//bool enable;
};
class KCheckBlackListMark: public KMark {
public:
	KCheckBlackListMark() {
		time_out = 1800;
	}
	virtual ~KCheckBlackListMark() {

	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,
			int &jumpType) {	
		KIpList *bls = NULL;
		if (isGlobal) {
			bls = conf.gvm->globalVh.blackList;
		} else {
			if (rq->svh) {
				bls = rq->svh->vh->blackList;
			}
		}
		if (bls) {
			if (bls->find(rq->getClientIp(),time_out,bls!= conf.gvm->globalVh.blackList)) {
				//rq->buffer << "HTTP/1.1 403 FORBIDIN\r\nConnection: close\r\n\r\nip is block";
				jumpType = JUMP_DROP;
				return true;
			}
		}		
		return false;
	}
	std::string getDisplay() {
		if (isGlobal) {
			return "";
		}
		std::stringstream s;
		s << "time:" << time_out;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
			 {
		time_out = atoi(attribute["time_out"].c_str());
	}
	std::string getHtml(KModel *model) {
		KCheckBlackListMark *m_chain = (KCheckBlackListMark *) model;
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
	KMark *newInstance() {
		return new KCheckBlackListMark();
	}
	const char *getName() {
		return "check_black_list";
	}
public:
	void buildXML(std::stringstream &s) {
		if (!isGlobal) {
			s << "time_out='" << time_out << "'";
		}
		s << "> ";
	}
private:
	int time_out;
};
#endif
//}}
#endif
