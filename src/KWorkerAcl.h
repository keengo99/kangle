#ifndef KWORKERACL_H
#define KWORKERACL_H  1
#if 0
#include "KRequestQueue.h"
#define WORKER_TYPE_RQ    0
#define WORKER_TYPE_IO    1
#define WORKER_TYPE_DNS   2

class KWorkerAcl : public KAcl
{
public:
	KWorkerAcl() {
		max = 0;
	}
	virtual ~KWorkerAcl() {
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		return getCurrentWorker(rq) < max;
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		KWorkerAcl *m = (KWorkerAcl *)model;
		s << "type:<select name='type'>";
		for(int i=0;i<3;i++){
			s << "<option ";
			if (m && m->type==i) {
				s << "selected";
			}
			s << " value='" << getWorkerType(i) << "'>" << getWorkerType(i) << "</option>";
		}
		s << "</select>";
		s << " max:<input name='max' value='";
		if (m) {
			s << m->max;
		}
		s << "'>";
		return s.str();
	}
	const char *getName() {
		return "worker";
	}
	KAcl *newInstance()
	{
		return new KWorkerAcl;
	}
	std::string getDisplay() {
		std::stringstream s;
		s << getWorkerType(type) << "&lt;" << max;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
			 {
		max = atoi(attribute["max"].c_str());
		type = getWorkerType(attribute["type"].c_str());
		return;
	}
	void buildXML(std::stringstream &s) {
		s << " type='" << getWorkerType(type) << "'";
		s << " max='" << max << "'";
		s << ">";
	}
private:
	int getCurrentWorker(KHttpRequest *rq)
	{
		switch(type){
		case WORKER_TYPE_IO:
			return conf.ioWorker->getBusyCount();
		case WORKER_TYPE_DNS:
			return conf.ioWorker->getBusyCount();
		}
#ifdef ENABLE_REQUEST_QUEUE
		KRequestQueue *queue = &globalRequestQueue;
#ifdef ENABLE_VH_QUEUE
		if (rq->svh && rq->svh->vh->queue) {
			queue = rq->svh->vh->queue;
		}
#endif
		return (int)queue->getBusyCount();
#endif
		return 0;
	}
	const char *getWorkerType(int type)
	{
		switch(type){
		case WORKER_TYPE_IO:
			return "io";
		case WORKER_TYPE_DNS:
			return "dns";
		default:
			return "rq";
		}
	}
	int getWorkerType(const char *type)
	{
		if (strcasecmp(type,"io")==0) {
			return WORKER_TYPE_IO;
		}
		if (strcasecmp(type,"dns")==0) {
			return WORKER_TYPE_DNS;
		}
		return WORKER_TYPE_RQ;
	}
	int max;
	int type;
};
#endif
#endif
