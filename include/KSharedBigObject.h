#ifndef KSHAREDBIGOBJECT_H
#define KSHAREDBIGOBJECT_H
#include "global.h"
#include <list>
#include "krbtree.h"
#include "KHttpRequest.h"
#include "KBigObject.h"
#include "kfile.h"
#include "KSboFile.h"
#include "kfiber.h"
#include "kfiber_sync.h"

//{{ent
#ifdef ENABLE_BIG_OBJECT_206
struct BigObjectReadQueue
{
	kfiber* rq;
	kselector* selector;
	char* buf;
	INT64 from;
	INT64 length;
};
struct KFileBlock {
	INT64 from;
	INT64 to;
};
//大物件数据块
class KBigObjectBlock
{
public:
	//block data = [from,to)
	//so length = to - from
	KFileBlock file_block;
	//最大读取点
	INT64 read_point;
	std::list<BigObjectReadQueue*> wait_queue;
	kfiber* net_fiber;
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
	bool CanSatisfy(KHttpRequest *rq, KHttpObject *obj);
	void saveLastVerified(KHttpObject *obj);
	int Read(KHttpRequest* rq, KHttpObject *obj, char *buf, INT64 from, int length);
	bool Open(KHttpRequest* rq, KHttpObject* obj, bool create_flag);
	bool OpenRead(KHttpObject* obj);
	INT64 OpenWrite(INT64 from);
	void CloseWrite(KHttpObject* obj, INT64 from);
	void CloseRead(KHttpObject* obj);
	KGL_RESULT Write(KHttpObject *obj, INT64 offset, const char* buf, int length);
	friend class KBigObjectContext;
	//bool loadProgress(KHttpObject *obj);
	bool restore(char *buf,int length);
	void print();
private:
	/**
	* 保存和恢复进度数据
	*/
	bool SaveProgress(KHttpObject* obj);
	void Close(KHttpObject* obj);
	void create_if_range(KHttpRequest *rq,krb_node *node);
	void fix_if_range_to(KHttpRequest *rq,krb_node *node);	
	bool body_complete ;
	krb_node *find_block_node(INT64 from);
	krb_node *find_next_block_node(INT64 from);
	krb_node *insert(INT64 from,bool &new_object);
	//数据块
	int read_refs;
	int write_refs;
	struct krb_root blocks;
	kfiber_file* fp;
	kfiber_mutex* lock;
};

#endif
//}}
#endif

