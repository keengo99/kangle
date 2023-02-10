#ifndef KQUEUEMARK_H
#define KQUEUEMARK_H
#include "KMark.h"
#include "KReg.h"
#include "KHttpRequest.h"
#include "utils.h"
#ifdef ENABLE_REQUEST_QUEUE
class KRequestQueue;
class per_queue_matcher {
public:
	per_queue_matcher()
	{
		header = NULL;
		next = NULL;
	}
	~per_queue_matcher()
	{
		if (header) {
			free(header);
		}
	}
	char *header;
	KReg reg;
	per_queue_matcher *next;
};
class KQueueMark : public KMark
{
public:
	KQueueMark()
	{
		queue = NULL;
	}
	~KQueueMark();
	bool mark(KHttpRequest *rq, KHttpObject *obj,
		const int chainJumpType, int &jumpType);
	bool supportRuntime() override
	{
		return true;
	}
	KMark *newInstance()
	{
		return new KQueueMark;
	}
	const char *getName()
	{
		return "queue";
	}
	std::string getHtml(KModel *model);
	std::string getDisplay();
	void editHtml(std::map<std::string, std::string> &attribute,bool html) ;
	void buildXML(std::stringstream &s);
private:
	KRequestQueue *queue;
};

class KPerQueueMark : public KMark
{
public:
	KPerQueueMark()
	{
		max_worker = 0;
		max_queue = 0;
		matcher = NULL;
	}
	~KPerQueueMark();
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType, int &jumpType);
	bool supportRuntime() override
	{
		return true;
	}
	KMark *newInstance()
	{
		return new KPerQueueMark;
	}
	const char *getName()
	{
		return "per_queue";
	}
	std::string getHtml(KModel *model);
	std::string getDisplay();
	void editHtml(std::map<std::string, std::string> &attribute,bool html) ;
	void buildXML(std::stringstream &s);
private:
	void build_matcher(std::stringstream &s);
	per_queue_matcher *matcher;
	unsigned max_worker;
	unsigned max_queue;
};
#endif
#endif

