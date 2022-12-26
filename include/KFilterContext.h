#ifndef KFILTERCONTEXT_H
#define KFILTERCONTEXT_H
#include "KFilterHelper.h"
#include "KHttpObject.h"
class KOutputFilterContext
{
public:
	KOutputFilterContext()
	{
		filterPrevData = NULL;
		charset = NULL;
		head = NULL;
		last = NULL;
		st_head = NULL;
		st_last = NULL;
	}
	~KOutputFilterContext()
	{
		if (filterPrevData) {
			free(filterPrevData);
		}
		if (charset) {
			free(charset);
		}
		clearFilters();
		if (st_head && autoDelete) {
			delete st_head;
		}
	}
	void registerFilterStreamEx(KHttpStream *head,KHttpStream *end,bool autoDelete = true)
	{
		if (st_last==NULL) {
			assert(st_head==NULL);
			st_head = head;
			st_last = end;
			this->autoDelete = autoDelete;
		} else {
			st_last->connect(head,autoDelete);
			st_last = end;
		}
	}
	void registerFilterStream(KHttpStream *st,bool autoDelete=true)
	{
		return registerFilterStreamEx(st,st,autoDelete);
	}
	KHttpStream *getFilterStreamEnd()
	{
		return st_last;
	}
	KHttpStream *getFilterStreamHead()
	{
		return st_head;
	}
	void addFilter(KFilterHelper *chain)
	{
		if (head==NULL) {
			head = chain;
			last = chain;
			last->next = NULL;
			return;
		}
		assert(last);
		last->next = chain;
		last = chain;
		last->next = NULL;
	}
	void clearFilters()
	{
		while (head) {
			last = head->next;
			delete head;
			head = last;
		}
	}
	int checkFilter(KHttpRequest *rq,const char *buf, int len, char **prevData, bool &havePartial) {
		int match;
		//std::list<KFilterHelper *>::iterator it;
		int jump;
		KFilterHelper *tmp = head;
		while(tmp){
			if (tmp->lastResult == FILTER_PARTIALMATCH) {
				if (*prevData == NULL) {
					/* 
					 * 如果上一次的结果是部分匹配的话，肯定有上次尾数据
					 */
					assert(filterPrevDataLen>0);
					*prevData = (char *) xmalloc(len+filterPrevDataLen);
					assert(filterPrevData);
					kgl_memcpy(*prevData, filterPrevData, filterPrevDataLen);
					kgl_memcpy(*prevData + filterPrevDataLen, buf, len);
				}
				buf = *prevData;
				len += (int)filterPrevDataLen;
			}
			KFilterKey *keys = NULL;
			match = tmp->check(buf, len, charset, &keys);
			switch (match) {
			case FILTER_MATCH:
				jump = filterAction(rq,tmp, keys);
				if (jump != JUMP_CONTINUE) {
					clearFilters();
					return jump;
				}
				//delete (*it);
				//if matched and JUMP NOT DENY then erase it. and continue call next filter.
				//it = filters.erase(it);
				break;
			case FILTER_PARTIALMATCH:
				havePartial = true;
			default:
				break;
			}
			tmp = tmp->next;
		}
		return JUMP_CONTINUE;
	}
	int checkFilter(KHttpRequest *rq,const char *buf, int len)
	{
		//prevData是指这次数据加上上一次尾数据
		//而filterPrevData是指上一次的尾数据
		char *prevData = NULL;
		bool havePartial = false;
		int action = checkFilter(rq,buf, len, &prevData, havePartial);
		if (action == JUMP_DENY)
			goto done;
		done: if (prevData) {
			xfree(prevData);
		}
		if (action != JUMP_DENY && havePartial) {
			//alloc the filterPrevData
			filterPrevDataLen = KGL_MIN(len,PARTIALMATCH_BACKLEN);
			if (filterPrevData) {
				xfree(filterPrevData);
			}
			filterPrevData = (char *) xmalloc(filterPrevDataLen);
			if (filterPrevData == NULL) {
				//no memory to alloc.
				return JUMP_DENY;
			}
			int start = len - (int)filterPrevDataLen;
			kgl_memcpy(filterPrevData, buf + start, filterPrevDataLen);
		}
		return action;
	}
	int filterAction(KHttpRequest *rq,KFilterHelper *filter, KFilterKey *keys) {
		//TODO:deal with the action and free the keys;

		while (keys) {
			assert(keys->key);
			klog(KLOG_WARNING,
					"http://%s%s%s%s filter matched,charset=[%s] key [%s]\n",
					rq->sink->data.url->host, 
					rq->sink->data.url->path,
					(rq->sink->data.url->param?"?":""), 
					(rq->sink->data.url->param?rq->sink->data.url->param:""),
					(charset ? charset : ""),
					keys->key);
			KFilterKey *next = keys->next;
			delete keys;
			keys = next;
		}

		switch (filter->jump) {
		case JUMP_ALLOW:
		case JUMP_CONTINUE:
			return filter->jump;
		default:
			return JUMP_DENY;
		}
	}
	size_t filterPrevDataLen;
	char *filterPrevData;
	char *charset;
	KFilterHelper *head;
	KFilterHelper *last;

	KHttpStream *st_head;
	KHttpStream *st_last;
	bool autoDelete;
};
#endif
