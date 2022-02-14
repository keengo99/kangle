#ifndef KWHITELIST_H
#define KWHITELIST_H
#include "utils.h"
#include "do_config.h"
#include "KReportIp.h"
#include "klist.h"
#ifdef ENABLE_BLACK_LIST
class KWhiteListItem
{
public:
	KWhiteListItem()
	{
		ip = NULL;
	};
	~KWhiteListItem()
	{
		if(ip){
			free(ip);
		}
	}
	char *host;
	char *ip;
	time_t lastTime;
	KWhiteListItem *next;
	KWhiteListItem *prev;
};
class KWhiteListManager;
class KWhiteList
{
public:
	KWhiteList(const char *host)
	{
		this->host = strdup(host);
	}
	~KWhiteList()
	{
		if (host) {
			free(host);
		}
	}
	std::map<char *,KWhiteListItem *,lessp> items;
	bool add(const char *ip,KWhiteListManager *wlm);
	bool find(const char *ip,KWhiteListManager *wlm);
	void remove(KWhiteListItem *item);
	char *host;
};
class KWhiteListManager
{
public:
	KWhiteListManager()
	{
		klist_init(&queue);
	}
	~KWhiteListManager()
	{
	}
	void flush(time_t nowTime,int time_out)
	{
		lock.Lock();
		KWhiteListItem *item = klist_head(&queue);
		while (item !=&queue) {
			if (nowTime - item->lastTime < time_out) {
				break;
			}
			KWhiteListItem *next = item->next;
			klist_remove(item);
			std::map<char *, KWhiteList *, lessp_icase>::iterator it = wls.find(item->host);
			assert(it != wls.end());
			KWhiteList *wl = (*it).second;
			wl->remove(item);
			delete item;
			if (wl->items.empty()) {
				wls.erase(it);
				delete wl;
			}
			item = next;
		}
		lock.Unlock();
	}
	void add(const char *host,const char *vh,const char *ip,bool skip_report_ip=false)
	{
		lock.Lock();
		std::map<char *,KWhiteList *,lessp_icase>::iterator it;
		it = wls.find((char *)host);
		KWhiteList *wl;
		if (it==wls.end()) {
			wl = new KWhiteList(host);
			wls.insert(std::pair<char *,KWhiteList *>(wl->host,wl));
		} else {
			wl = (*it).second;
		}
		bool added = wl->add(ip,this);
		lock.Unlock();
#ifdef ENABLE_SIMULATE_HTTP
		if (added && !skip_report_ip) {
			report_white_list(host,vh,ip);
		}
#endif
	}
	bool find(const char *host,const char *ip,bool flush)
	{
		lock.Lock();
		std::map<char *,KWhiteList *,lessp_icase>::iterator it;
		it = wls.find((char *)host);
		if (it==wls.end()) {
			lock.Unlock();
			return false;
		}		
		bool result = (*it).second->find(ip,(flush?this:NULL));
		lock.Unlock();
		return result;
	}
	friend class KWhiteList;
private:
	void addItem(KWhiteListItem *item)
	{
		klist_append(&queue, item);
	}
	std::map<char *,KWhiteList *,lessp_icase> wls;
	KMutex lock;
	KWhiteListItem queue;
};
//°×Ãûµ¥
extern KWhiteListManager wlm;
#endif
#endif
