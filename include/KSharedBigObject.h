#ifndef KSHAREDBIGOBJECT_H
#define KSHAREDBIGOBJECT_H
#include "global.h"
#include <list>
#include "krbtree.h"
#include "KHttpRequest.h"
#include "KBigObject.h"
#include "kfile.h"
#include "kfiber.h"
#include "kfiber_sync.h"


#ifdef ENABLE_BIG_OBJECT_206

struct BigObjectReadQueue
{
	kfiber* rq;
	char* buf;
	int64_t from;
	int64_t length;
};
struct KFileBlock {
	int64_t from;
	int64_t to;
};
//大物件数据块
class KBigObjectBlock
{
public:
	//block data = [from,to)
	//so length = to - from
	KFileBlock file_block;
	//最大读取点
	int64_t read_point;
	std::list<BigObjectReadQueue*> wait_queue;
	kfiber* net_fiber;
};
enum kgl_satisfy_status
{
	kgl_satisfy_none,
	kgl_satisfy_part,
	kgl_satisfy_all
};
//部分内容大物件共享部分
class KSharedBigObject
{
public:
	KSharedBigObject()
	{
		blocks.rb_node = NULL;		
		body_complete = false;
		read_refs = 0;
		write_refs = 0;
		fp = NULL;
		lock = kfiber_mutex_init();
	}
	~KSharedBigObject();
	kgl_satisfy_status can_satisfy(kgl_request_range *range, KHttpObject *obj);
	void save_last_verified(KHttpObject *obj);

	int read(KHttpRequest* rq, KHttpObject* obj,int64_t offset, char* buf, int length, bool *net_fiber);
	KGL_RESULT write(KHttpObject* obj,int64_t offset, const char* buf, int length);

	bool create(KHttpObject* obj);
	void open_read(KHttpObject* obj);
	int64_t open_write(KHttpObject* obj,int64_t from);
	void close_write(KHttpObject* obj, int64_t from);
	void close_read(KHttpObject* obj);


	friend class KBigObjectContext;
	bool restore(char *buf,int length);
	void print();
private:
	/**
	* 保存和恢复进度数据
	*/
	bool save_progress(KHttpObject* obj);
	void close(KHttpObject* obj);
	kgl_satisfy_status create_if_range(kgl_request_range*range,krb_node *node);
	kgl_satisfy_status fix_if_range_to(kgl_request_range *range,krb_node *node);
	krb_node *find_block_node(int64_t from);
	krb_node *find_next_block_node(int64_t from);
	krb_node *insert(int64_t from,bool &new_obj);
	bool open_file_handle(KHttpObject* obj);
	bool body_complete;
	//数据块
	int read_refs;
	int write_refs;
	struct krb_root blocks;
	kfiber_file* fp;
	kfiber_mutex* lock;
};

#endif
#endif

