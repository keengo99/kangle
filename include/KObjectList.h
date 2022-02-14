#ifndef KOBJECTLIST_H
#define KOBJECTLIST_H
#include "global.h"
#include "KMutex.h"
#include "KHttpObject.h"
class KVirtualHost;
class KObjectList;
struct KTempHttpObject
{
	KHttpObject *obj;
	bool is_dead;
	INT64 decSize;
	KTempHttpObject *next;
};
class KObjectList
{
public:
	KObjectList();
	int clean_cache(KReg *reg,int flag);
	void add(KHttpObject *obj);
	void dead(KHttpObject *obj);
	void remove(KHttpObject *obj);
	int move(KBufferFile *bf,int64_t begin_msec,INT64 size,bool swapout);
	void dead_count(int &count);
	void dead_all_obj();
#ifdef ENABLE_DISK_CACHE
	void swap_all_obj();
	//int save_disk_index(KFile *fp);
#endif
	void dump_refs_obj(std::stringstream &s);
	unsigned char list_state;
	void getSize(INT64 &csize,INT64 &cdsiz);
	KHttpObject *getHead()
	{
		return l_head;
	}
#ifdef MALLOCDEBUG
	void free_all_cache();
#endif
private:
	void swapout(KTempHttpObject *tho, KBufferFile *bf, int gc_used_msec);
	void swapout_result(KTempHttpObject *tho,int gc_used_msec, bool result);
	void checkList()
	{
		if (l_head==NULL) {
			assert(l_end==NULL);
			return;
		}
		assert(l_end);
		assert(l_head->lprev == NULL);
		assert(l_end->lnext ==NULL);
	}
	KHttpObject *l_head;
	KHttpObject *l_end;	
	int cache_model;
};
#endif
