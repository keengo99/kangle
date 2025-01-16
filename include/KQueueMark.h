#ifndef KQUEUEMARK_H
#define KQUEUEMARK_H
#include "KMark.h"
#include "KReg.h"
#include "KHttpRequest.h"
#include "utils.h"
#ifdef ENABLE_REQUEST_QUEUE
class KRequestQueue;
class per_queue_matcher
{
public:
	per_queue_matcher() {
		header = NULL;
		next = NULL;
	}
	~per_queue_matcher() {
		if (header) {
			free(header);
		}
	}
	char* header;
	KReg reg;
	per_queue_matcher* next;
};
class KQueueMark : public KMark
{
public:
	KQueueMark() {
		queue = NULL;
	}
	~KQueueMark();
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override;
	KMark* new_instance()override {
		return new KQueueMark;
	}
	const char *get_module() const override {
		return "queue";
	}
	void get_html(KWStream& s) override;
	void get_display(KWStream& s) override;
	void parse_config(const khttpd::KXmlNodeBody* xml) override;
private:
	KRequestQueue* queue;
};

class KPerQueueMark : public KMark
{
public:
	KPerQueueMark() {
		max_worker = 0;
		max_queue = 0;
		matcher = NULL;
	}
	~KPerQueueMark();
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override;
	KMark* new_instance() override {
		return new KPerQueueMark;
	}
	const char *get_module() const  override {
		return "per_queue";
	}
	void get_display(KWStream& s) override;
	void get_html(KWStream& s) override;
	void parse_config(const khttpd::KXmlNodeBody* xml) override;
private:
	void build_matcher(KWStream& s);
	per_queue_matcher* matcher;
	unsigned max_worker;
	unsigned max_queue;
};
#endif
#endif

