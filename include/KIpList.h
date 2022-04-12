#ifndef KIPLIST_H
#define KIPLIST_H
#include "utils.h"
#include "KReportIp.h"
#include "kasync_worker.h"
//{{ent
#ifdef ENABLE_BLACK_LIST
extern kasync_worker *run_fw_worker;
class WhmContext;
void init_run_fw_cmd();
void run_fw_cmd(const char *cmd,const char *ip);
class KIpListItem
{
public:
	KIpListItem(const char *ip)
	{
		this->ip = strdup(ip);
		last_time = kgl_current_sec;
		next = NULL;
	};
	KIpListItem(const char *ip, bool white) {
		this->ip = strdup(ip);
		next = this;
		SetWhite(white);
	}
	~KIpListItem()
	{
		if (ip) {
			free(ip);
		}
	}
	void SetWhite(bool white) {
		if (white) {
			flags = 1;
		} else {
			flags = 0;
		}
	}
	void ConvertToStatic() {
		last_time = 0;
	}
	bool IsConvertToStatic() {
		return last_time == 0;
	}
	bool IsStatic() {
		return next == this;
	}
	bool IsWhite() {
		return IsStatic() && flags == 1;
	}
	char *ip;
	union {
		time_t last_time;
		uint32_t flags;
	};
	KIpListItem *next;
};
class KIpList
{
public:
	KIpList()
	{
		total_request = 0;
		total_error_upstream = 0;
		total_upstream = 0;
		refs = 1;
		head = end = NULL;
		run_fw_cmd = false;
		report_ip = false;
	}
	bool Remove(const char *ip) {
		lock.Lock();
		std::map<char *, KIpListItem *, lessp>::iterator it;
		it = wls.find((char *)ip);
		if (it == wls.end()) {
			lock.Unlock();
			return false;
		}
		KIpListItem *wl = (*it).second;
		if (!wl->IsStatic()) {
			//动态转静态
			wl->ConvertToStatic();
		}
		wls.erase(it);
		lock.Unlock();
		return true;
	}
	void AddStatic(const char *ip, bool white) {
		lock.Lock();
		std::map<char *, KIpListItem *, lessp>::iterator it;
		it = wls.find((char *)ip);
		if (it != wls.end()) {
			KIpListItem *wl = (*it).second;
			if (wl->IsStatic()) {
				//本身是静态的，只修改黑白名单
				wl->SetWhite(white);
				lock.Unlock();
				return;
			}			
			//动态转静态
			wl->ConvertToStatic();
			wls.erase(it);
		}
		KIpListItem *wl = new KIpListItem(ip,white);
		wls.insert(std::pair<char *, KIpListItem *>(wl->ip, wl));
		lock.Unlock();
	}
	void AddDynamic(const char *ip,bool skip_report_ip=false)
	{
		bool added = false;
		lock.Lock();
		std::map<char *,KIpListItem *,lessp>::iterator it;
		it = wls.find((char *)ip);		
		if (it==wls.end()) {
			KIpListItem *wl = new KIpListItem(ip);
			wls.insert(std::pair<char *,KIpListItem *>(wl->ip,wl));
			addItem(wl);
			added = true;
		}
		lock.Unlock();
		if (added) {
#ifdef ENABLE_SIMULATE_HTTP
			if (!skip_report_ip && report_ip) {
				::report_black_list(ip);
			}
#endif
			if (run_fw_cmd && run_fw_worker->queue < 32) {
				assert(*conf.block_ip_cmd);
				::run_fw_cmd(conf.block_ip_cmd,ip);
			}
		}
	}
	void flush(int time_out)
	{
		KIpListItem *del_list = NULL;
		KIpListItem *next;
		lock.Lock();
		while (head && kgl_current_sec - head->last_time>time_out) {
			assert(!head->IsStatic());
			if (!head->IsConvertToStatic()) {
				//动态的，已经转静态了。
				wls.erase((char *)head->ip);
			}
			next = head->next;
			head->next = del_list;
			del_list = head;
			head = next;
		}
		if (head==NULL) {
			end = NULL;
		}
		lock.Unlock();		
		while (del_list) {
			if (run_fw_cmd && *conf.unblock_ip_cmd) {
				::run_fw_cmd(conf.unblock_ip_cmd,del_list->ip);
			}
			next = del_list->next;
			delete del_list;
			del_list = next;
		}
	}
	bool find(const char *ip,int time_out,bool flush)
	{
		lock.Lock();
		if (flush) {
			while (head && kgl_current_sec - head->last_time > time_out) {
				KIpListItem *next = head->next;
				assert(!head->IsStatic());
				if (!head->IsConvertToStatic()) {
					wls.erase((char *)head->ip);
				}
				delete head;
				head = next;
			}
			if (head == NULL) {
				end = NULL;
			}
		}
		std::map<char *,KIpListItem *,lessp>::iterator it;
		it = wls.find((char *)ip);
		if (it==wls.end()) {
			lock.Unlock();
			return false;
		}
		bool is_white = (*it).second->IsWhite();
		lock.Unlock();
		return !is_white;
	}
	void addRef()
	{
		lock.Lock();
		refs++;
		lock.Unlock();
	}
	void release()
	{
		lock.Lock();
		refs--;
		if(refs<=0){
			lock.Unlock();
			delete this;
			return;
		}
		lock.Unlock();
	}
	void setRunFwCmd(bool run_fw_cmd)
	{
		this->run_fw_cmd = run_fw_cmd;
	}
	void setReportIp(bool report_ip)
	{
		this->report_ip = report_ip;
	}
	void addStat(int flags,int filter_flags)
	{
		statLock.Lock();
		total_request++;
		if (KBIT_TEST(flags, RQ_UPSTREAM)) {
			total_upstream++;
			if (KBIT_TEST(flags, RQ_UPSTREAM_ERROR)) {
				total_error_upstream++;
			}
		}
		statLock.Unlock();
	}	
	void getStat(INT64 &total_request,INT64 &total_error_upstream, INT64 &total_upstream,bool reset)
	{
		statLock.Lock();
		total_request = this->total_request;
		total_error_upstream = this->total_error_upstream;
		total_upstream = this->total_upstream;
		if (reset) {
			this->total_request = 0;
			this->total_error_upstream = 0;
			this->total_upstream = 0;
		}
		statLock.Unlock();
	}
	void getBlackList(WhmContext *ctx);
	void clearBlackList();
	//volatile int64_t upstream_request_count;
private:
	~KIpList()
	{
		while (head) {
			end = head;
			delete head;
			head = end->next;
		}
	}
	void addItem(KIpListItem *item)
	{
		assert(item->next == NULL);
		if (end) {
			end->next = item;
		} else {
			head = item;
		}
		end = item;
	}
	bool run_fw_cmd;
	bool report_ip;
	std::map<char *,KIpListItem *,lessp> wls;
	KIpListItem *head;
	KIpListItem *end;
	KMutex lock;
	int refs;
	INT64 total_request;
	INT64 total_error_upstream;
	INT64 total_upstream;
	KMutex statLock;
};
#endif
//}}
#endif
