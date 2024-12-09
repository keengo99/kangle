#include "KQueueMark.h"
#ifdef ENABLE_REQUEST_QUEUE
#include "KRequestQueue.h"
#include "KAccess.h"
#include "http.h"
#include "KAccessDsoSupport.h"
#define PER_MATCHER_SPLIT_CHAR ','
KQueueMark::~KQueueMark()
{
	if (queue) {
		queue->release();
		queue = NULL;
	}
}
uint32_t KQueueMark::process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo)
{
	if (queue && rq->queue == NULL) {
		rq->queue = queue;
		queue->addRef();
	}
	return KF_STATUS_REQ_TRUE;
}
void KQueueMark::parse_config(const khttpd::KXmlNodeBody* xml) {
	auto attribute = xml->attr();
	int max_worker = atoi(attribute["max_worker"].c_str());
	int max_queue = atoi(attribute["max_queue"].c_str());
	if (queue == NULL) {
		queue = new KRequestQueue;
	}
	queue->set(max_worker, max_queue);
}
void KQueueMark::get_display(KWStream &s)
{
	if (queue) {
		s << queue->getWorkerCount() << "/" << queue->getMaxWorker() << " " << queue->getQueueSize() << "/" << queue->getMaxQueue();
		s << " " << queue->getRef();
	}
}
void KQueueMark::get_html(KModel *model, KWStream& s)
{
	KQueueMark *mark = (KQueueMark *)(model);
	s << "max_worker:<input name='max_worker' value='";
	if (mark && mark->queue) {
		s << mark->queue->getMaxWorker();
	}
	s << "'>";
	s << "max_queue:<input name='max_queue' value='";
	if (mark && mark->queue) {
		s << mark->queue->getMaxQueue();
	}
	s << "'>\n";
}
KPerQueueMark::~KPerQueueMark()
{
	while (matcher) {
		per_queue_matcher *next = matcher->next;
		delete matcher;
		matcher = next;
	}
}
uint32_t KPerQueueMark::process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo)
{
	if (rq->queue) {
		return KF_STATUS_REQ_FALSE;
	}
	KStringStream*url = NULL;
	KRegSubString *ss = NULL;
	per_queue_matcher *matcher = this->matcher;
	while (matcher) {
		if (matcher->header == NULL) {
			if (url == NULL) {
				url = new KStringStream;
				rq->sink->data.url->GetUrl(*url);
			}
			ss = matcher->reg.matchSubString(url->c_str(), url->size(), 0);
		} else {
			KHttpHeader *av = rq->sink->data.get_header();
			while (av) {
				if (kgl_is_attr(av, matcher->header,strlen(matcher->header))) {
					ss = matcher->reg.matchSubString(av->buf+av->val_offset, av->val_len, 0);
					break;
				}
				av = av->next;
			}
		}
		if (ss != NULL) {
			break;
		}
		matcher = matcher->next;
	}
	if (url) {
		delete url;
	}
	if (ss == NULL) {
		return KF_STATUS_REQ_FALSE;
	}
	char *key = ss->getString(1);
	if (key == NULL) {
		delete ss;
		return KF_STATUS_REQ_FALSE;
	}
	KRequestQueue *queue = get_request_queue(rq, key, max_worker, max_queue);
	rq->queue = queue;
	delete ss;
	return KF_STATUS_REQ_TRUE;
}
void KPerQueueMark::parse_config(const khttpd::KXmlNodeBody* xml)
{
	auto attribute = xml->attr();
	max_worker = atoi(attribute["max_worker"].c_str());
	max_queue = atoi(attribute["max_queue"].c_str());
	while (this->matcher) {
		per_queue_matcher *next = this->matcher->next;
		delete this->matcher;
		this->matcher = next;
	}
	per_queue_matcher *last = NULL;
	char *url = strdup(attribute["url"].c_str());
	char *hot = url;
	while (hot) {
		per_queue_matcher *matcher = new per_queue_matcher;
		char *p = strchr(hot, PER_MATCHER_SPLIT_CHAR);
		if (p) {
			*p = '\0';
			p++;
		}
		char *val = hot;
		if (*hot == '#') {
			hot++;
			val = strchr(val, ':');
			if (val == NULL) {
				delete matcher;
				hot = p;
				continue;
			}
			*val = '\0';
			val++;
			matcher->header = strdup(hot);
		}
		matcher->reg.setModel(val, KGL_PCRE_CASELESS);
		if (last) {
			last->next = matcher;
		}
		last = matcher;
		if (this->matcher == NULL) {
			this->matcher = matcher;
		}
		hot = p;
	}
	free(url);
}
void KPerQueueMark::get_display(KWStream &s)
{
	s << max_worker << " " << max_queue << "/";
}

void KPerQueueMark::build_matcher(KWStream &s)
{
	per_queue_matcher *matcher = this->matcher;
	while (matcher) {
		if (matcher->header) {
			s << "#" << matcher->header << ":";
		}
		s << matcher->reg.getModel();
		if (matcher->next) {
			s << PER_MATCHER_SPLIT_CHAR;
		}
		matcher = matcher->next;
	}
}
void KPerQueueMark::get_html(KModel *model,KWStream &s)
{
	KPerQueueMark *mark = (KPerQueueMark *)(model);
	s << "url:<input name='url' value='";
	if (mark) {
		mark->build_matcher(s);
	}
	s << "'>";
	s << "max_worker:<input name='max_worker' value='";
	if (mark) {
		s << mark->max_worker;
	}
	s << "'>";
	s << "max_queue:<input name='max_queue' value='";
	if (mark) {
		s << mark->max_queue;
	}
	s << "'>\n";
}
#endif
