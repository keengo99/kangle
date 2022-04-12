#include "KLogDrill.h"
#ifdef ENABLE_LOG_DRILL
#include "KMutex.h"
#include "KConfig.h"
#include "kfile.h"
#include "KHttpRequest.h"
#include "KVirtualHost.h"
/*
#define CHECK_MAGIC_STR "ABCD1234"
struct klog_drill {
	char magic1[8];
	char *buf;
	char magic2[8];
	int len;
	char magic3[8];
	klog_drill *next;
	char magic4[8];
	klog_drill *prev;
	char magic5[8];
};
*/
struct klog_drill {
	char *buf;
	int len;
	klog_drill *next;
};
struct klog_drill_stat {
	int total_count;
	int drill_hit;
	int cache_hit;
	int cache_saved;
	int cache_verified;
};
static KMutex drill_lock;
static int drill_count = 0;
static klog_drill_stat drill_stat;
static klog_drill *drill_list_head = NULL;
static klog_drill *drill_list_end = NULL;
void init_log_drill()
{
	//memset(&drill_list, 0, sizeof(drill_list));
	//klist_init(&drill_list);
	memset(&drill_stat, 0, sizeof(drill_stat));
}
void free_log_drill(klog_drill *ld)
{
	xfree(ld->buf);
	delete ld;
}
#ifndef NDEBUG
static void internal_check_log_drill()
{
	int check_count = 0;
/*
	klog_drill *ld = drill_list.next;
	while (ld != &drill_list) {
		assert(ld != NULL);
		assert(ld->next->prev == ld);
		assert(ld->prev->next == ld);
		assert(ld->len>0);
		assert(ld->buf!=NULL);
		int ld_len = strlen(ld->buf);
		assert(ld_len==ld->len);
		assert(memcmp(ld->magic1,CHECK_MAGIC_STR,sizeof(CHECK_MAGIC_STR)-1)==0);
		assert(memcmp(ld->magic2,CHECK_MAGIC_STR,sizeof(CHECK_MAGIC_STR)-1)==0);
		assert(memcmp(ld->magic3,CHECK_MAGIC_STR,sizeof(CHECK_MAGIC_STR)-1)==0);
		assert(memcmp(ld->magic4,CHECK_MAGIC_STR,sizeof(CHECK_MAGIC_STR)-1)==0);
		assert(memcmp(ld->magic5,CHECK_MAGIC_STR,sizeof(CHECK_MAGIC_STR)-1)==0);
		ld = ld->next;
		check_count++;
	}
	assert(check_count == drill_count);
	assert(ld != NULL);
	assert(ld->next->prev == ld);
	assert(ld->prev->next == ld);
//*/
//*
	klog_drill *ld = drill_list_head;
	while (ld) {
		if (ld->next==NULL) {
			assert(ld == drill_list_end);
		}
		ld = ld->next;
		check_count++;
	}
	assert(drill_list_end==NULL || drill_list_end->next==NULL);
	//*/
	assert(check_count == drill_count);
}
void check_log_drill()
{
	//drill_lock.Lock();
	//internal_check_log_drill();
	//drill_lock.Unlock();
}

#endif
static void remove_log_drill()
{
	/*
	klog_drill *tmp = drill_list.next->next;
	if (tmp == &drill_list) {
		return;
	}
	drill_count--;
	klist_remove(tmp);
	free_log_drill(tmp);
	//*/
	//*
	if (drill_list_head==NULL) {
		assert(drill_list_end==NULL);
#ifndef NDEBUG
		internal_check_log_drill();
#endif
		return;
	}
	klog_drill *next = drill_list_head->next;
	free_log_drill(drill_list_head);
	drill_count--;
	drill_list_head = next;
	if (drill_list_head==NULL) {
		drill_list_end = NULL;
	}
	//*/
#ifndef NDEBUG
	internal_check_log_drill();
#endif
}
void add_log_drill(KHttpRequest *rq,KStringBuf &s)
{
	drill_lock.Lock();
#ifndef NDEBUG
	internal_check_log_drill();
#endif
	drill_stat.total_count++;
	if (!KBIT_TEST(rq->filter_flags, RF_LOG_DRILL)) {
		drill_lock.Unlock();
		return;
	}
	drill_stat.drill_hit++;
	if (rq->ctx->cache_hit) {
		drill_stat.cache_hit++;
	}
	if (KBIT_TEST(rq->flags, RQ_OBJ_STORED)) {
		drill_stat.cache_saved++;
	}
	if (KBIT_TEST(rq->flags, RQ_OBJ_VERIFIED)) {
		drill_stat.cache_verified++;
	}
	if (drill_count >= conf.log_drill) {
		remove_log_drill();
	}
	s.WSTR("#[");
	if (rq->svh && rq->svh->vh) {
		s.write_all(rq->svh->vh->name.c_str(), rq->svh->vh->name.size());
	}
	s.WSTR("]\n");
	drill_count++;
	klog_drill *ld = new klog_drill;
	memset(ld,0,sizeof(klog_drill));
	ld->len = s.getSize();
	ld->buf = s.stealString();
	/*
	kgl_memcpy(ld->magic1,CHECK_MAGIC_STR,sizeof(CHECK_MAGIC_STR)-1);
	kgl_memcpy(ld->magic2,CHECK_MAGIC_STR,sizeof(CHECK_MAGIC_STR)-1);
	kgl_memcpy(ld->magic3,CHECK_MAGIC_STR,sizeof(CHECK_MAGIC_STR)-1);
	kgl_memcpy(ld->magic4,CHECK_MAGIC_STR,sizeof(CHECK_MAGIC_STR)-1);
	kgl_memcpy(ld->magic5,CHECK_MAGIC_STR,sizeof(CHECK_MAGIC_STR)-1);
	*/
	//*
	ld->next = NULL;
	if (drill_list_head==NULL) {
		drill_list_head = ld;
	}
	if (drill_list_end!=NULL) {
		drill_list_end->next = ld;
	}
	drill_list_end = ld;
	//*/
	//klist_append(&drill_list,ld);
#ifndef NDEBUG
	internal_check_log_drill();
#endif
	drill_lock.Unlock();
}
void flush_log_drill()
{
	KStringBuf drill_log_file,drill_sign;
	klog(KLOG_NOTICE, "flush_log_drill now\n");
	drill_log_file << conf.path << PATH_SPLIT_CHAR << "var" << PATH_SPLIT_CHAR << "drill.log";
	KFile fp;
	if (!fp.open(drill_log_file.getString(), fileWrite)) {
		klog(KLOG_ERR, "cann't open drill_log_file [%s] for write\n", drill_log_file.getString());
		return;
	}
	drill_lock.Lock();
#ifndef NDEBUG
	internal_check_log_drill();
#endif
	int free_count = 0;
	//*
	while (drill_list_head!=NULL) {
		fp.write(drill_list_head->buf, drill_list_head->len);
		klog_drill *next = drill_list_head->next;
		free_log_drill(drill_list_head);
		drill_list_head = next;
		free_count++;
	}
	drill_list_end = NULL;
	//*/
	/*
	klog_drill *drill_list_head = drill_list.next;
	while (drill_list_head!=&drill_list) {
		fp.write(drill_list_head->buf, drill_list_head->len);
		klog_drill *next = drill_list_head->next;
		free_log_drill(drill_list_head);
		drill_list_head = next;
		free_count++;
	}
	klist_init(&drill_list);
	//*/
	assert(free_count == drill_count);
	drill_sign << "*{\"drill_count\":" << drill_count;
	drill_sign << ",\"drill_hit\":" << drill_stat.drill_hit;
	drill_sign << ",\"cache_hit\":" << drill_stat.cache_hit;
	drill_sign << ",\"cache_saved\":" << drill_stat.cache_saved;
	drill_sign << ",\"cache_verified\":" << drill_stat.cache_verified;
	drill_sign << ",\"total_count\":" << drill_stat.total_count;
	drill_sign << "}";
	fp.write(drill_sign.getString(), drill_sign.getSize());
	drill_count = 0;
	memset(&drill_stat, 0, sizeof(drill_stat));
#ifndef NDEBUG
	internal_check_log_drill();
#endif
	drill_lock.Unlock();
}
#endif
