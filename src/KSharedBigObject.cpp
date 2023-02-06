#include "KSharedBigObject.h"
#include "KBigObjectContext.h"
#include "kselector.h"
#include "KHttpObject.h"
#include "KDiskCache.h"
#include "http.h"
#include "KStringBuf.h"
#include "cache.h"
#include "kfiber.h"
#include "HttpFiber.h"
#include "KDefer.h"
#include "KPushGate.h"



#define UPSTREAM_AUTO_DELAY_BUFFER_SIZE 4194304
#ifdef ENABLE_BIG_OBJECT_206
krb_node* KSharedBigObject::find_next_block_node(int64_t from) {
	struct krb_node* last = NULL;
	struct krb_node* node = blocks.rb_node;
	while (node) {
		KBigObjectBlock* data = (KBigObjectBlock*)(node->data);
		if (from < data->file_block.from) {
			last = node;
			node = node->rb_left;
		} else if (from > data->file_block.to) {
			node = node->rb_right;
		} else {
			return last;
		}
	}
	return last;
}
krb_node* KSharedBigObject::find_block_node(int64_t from) {
	struct krb_node* node = blocks.rb_node;
	while (node) {
		KBigObjectBlock* data = (KBigObjectBlock*)(node->data);
		if (from < data->file_block.from) {
			node = node->rb_left;
		} else if (from > data->file_block.to) {
			node = node->rb_right;
		} else {
			return node;
		}
	}
	return NULL;
}
krb_node* KSharedBigObject::insert(int64_t from, bool& new_obj) {
	struct krb_node** n = &(blocks.rb_node), * parent = NULL;
	KBigObjectBlock* block = NULL;
	new_obj = true;
	/* Figure out where to put new node */
	while (*n) {
		block = (KBigObjectBlock*)((*n)->data);
		parent = *n;
		if (from < block->file_block.from) {
			n = &((*n)->rb_left);
		} else if (from > block->file_block.to) {
			n = &((*n)->rb_right);
		} else {
			new_obj = false;
			break;
		}
	}
	if (new_obj) {
		block = new KBigObjectBlock();
		block->file_block.from = from;
		block->file_block.to = from;
		krb_node* node = new krb_node;
		node->data = block;
		rb_link_node(node, parent, n);
		rb_insert_color(node, &blocks);
		return node;
	}
	return *n;
}
KSharedBigObject::~KSharedBigObject() {
	assert(read_refs == 0);
	assert(write_refs == 0);
	while (blocks.rb_node) {
		krb_node* node = blocks.rb_node;
		KBigObjectBlock* block = (KBigObjectBlock*)node->data;
		delete block;
		rb_erase(node, &blocks);
		delete node;
	}
	kfiber_mutex_destroy(lock);
	if (fp) {
		kfiber_file_close(fp);
	}
}
void KSharedBigObject::print() {
	krb_node* node = rb_first(&blocks);
	while (node) {
		//KBigObjectBlock *block = (KBigObjectBlock *)node->data;
		//printf("from=%d,to=%d\n",(int)block->from,(int)block->to);
		node = rb_next(node);
	}
}
kgl_satisfy_status KSharedBigObject::fix_if_range_to(kgl_request_range* range, krb_node* next_node) {
	KBigObjectBlock* next_block = (KBigObjectBlock*)next_node->data;
	//修正range_to
	if (range->to == -1 || next_block->file_block.from <= range->to) {
		range->to = next_block->file_block.from - 1;
		return kgl_satisfy_part;
	}
	return kgl_satisfy_none;
}

//创建If-Range头，from_node为from所在的块
kgl_satisfy_status KSharedBigObject::create_if_range(kgl_request_range* range, krb_node* from_node) {
	if (from_node) {
		//开始块在node中，修正range_from
		range->from = ((KBigObjectBlock*)from_node->data)->file_block.to;
		//rq->ctx.cache_hit_part = true;
		//找下一块
		krb_node* next_node = rb_next(from_node);
		if (next_node) {
			fix_if_range_to(range, next_node);
		}
		return kgl_satisfy_part;
	}
	//没有找到range_from块，找下一块。
	krb_node* next_node = find_next_block_node(range->from);
	if (next_node) {
		return fix_if_range_to(range, next_node);
	}
	return kgl_satisfy_none;
}
bool KSharedBigObject::open_file_handle(KHttpObject* obj) {
	if (fp) {
		return true;
	}
	char* filename = obj->get_filename();
	if (!filename) {
		return false;
	}
	if (filename) {
		fp = kfiber_file_open(filename, fileReadWrite, 0);
		free(filename);
	}
	return true;
}
void KSharedBigObject::open_read(KHttpObject* obj) {
	kfiber_mutex_lock(lock);
	read_refs++;
	open_file_handle(obj);
	kfiber_mutex_unlock(lock);
	return;
}
int64_t KSharedBigObject::open_write(KHttpObject* obj, int64_t from) {
	//printf("OpenWrite fiber=[%p] from=[" INT64_FORMAT "]\n", kfiber_self(), from);
	assert(from >= 0);
	kfiber_mutex_lock(lock);
	open_file_handle(obj);
	bool new_obj;
	//插入块,有就查找，没有就插入
	krb_node* node = insert(from, new_obj);
	assert(node);
	KBigObjectBlock* block = (KBigObjectBlock*)node->data;
	assert(block->net_fiber != kfiber_self());
	write_refs++;
	//数据块没有读请求,可以加入，否则关闭该网络请求
	block->net_fiber = kfiber_self();
	kfiber_mutex_unlock(lock);
	return from;
}
void KSharedBigObject::close(KHttpObject* obj) {
	if (read_refs == 0 && write_refs == 0 && fp) {
		kfiber_file_close(fp);
		fp = NULL;
		if (KBIT_TEST(obj->index.flags, FLAG_IN_DISK)) {
			save_progress(obj);
		}
	}
}
void KSharedBigObject::close_read(KHttpObject* obj) {
	assert(KBIT_TEST(obj->index.flags, FLAG_BIG_OBJECT_PROGRESS));
	kfiber_mutex_lock(lock);
	read_refs--;
	assert(read_refs >= 0);
	close(obj);
	kfiber_mutex_unlock(lock);
}
int KSharedBigObject::read(KHttpRequest* rq, KHttpObject* obj, int64_t offset, char* buf, int length, bool* net_fiber) {
	kfiber_mutex_lock(lock);
	if (fp == NULL) {
		kfiber_mutex_unlock(lock);
		return -1;
	}
	krb_node* node;
	if (!net_fiber) {
		node = find_block_node(offset);
	} else {
		bool newobj;
		*net_fiber = false;
		node = insert(offset, newobj);
		assert(node);
	}
	if (!node) {
		kfiber_mutex_unlock(lock);
		//need net fiber
		return -2;
	}
	KBigObjectBlock* block = (KBigObjectBlock*)node->data;
	krb_node* nextNode = rb_next(node);
	assert(block);
	assert(offset >= block->file_block.from);
	assert(offset <= block->file_block.to);
	if (block->read_point < offset) {
		//更新读取点
		block->read_point = offset;
	}
	if (offset < block->file_block.to) {
		//有数据
		INT64 block_length = block->file_block.to - offset;
		length = (int)KGL_MIN((INT64)length, block_length);
		kfiber_file_seek(fp, seekBegin, offset + obj->index.head_size);
		int result = kfiber_file_read(fp, buf, length);
		kfiber_mutex_unlock(lock);
		return result;
	}
	//没有数据,加入到等待队列中。
	/*
	printf("from=[" INT64_FORMAT "] block->to=[" INT64_FORMAT "],read length=[%d] no data join to the read queue to wait, block read_fiber=[%p]\n",
		from,
		block->file_block.to,
		length,
		block->net_fiber);
	*/


	if (block->net_fiber == NULL) {
		//如果没有读的请求，则启动一个读。
		if (!net_fiber) {
			kfiber_mutex_unlock(lock);
			return -2;
		}
		write_refs++;
		if (!rq->ctx.sub_request) {
			rq->ctx.sub_request = rq->sink->alloc<kgl_sub_request>();
		}
		if (!rq->ctx.sub_request->range) {
			rq->ctx.sub_request->range = rq->sink->alloc<kgl_request_range>();
		}
		rq->ctx.sub_request->range->from = offset;
		block->net_fiber = kfiber_self();
		*net_fiber = true;
		if (nextNode) {
			KBigObjectBlock* nextBlock = (KBigObjectBlock*)nextNode->data;
			rq->ctx.sub_request->range->to = nextBlock->file_block.from - 1;
			assert(rq->ctx.sub_request->range->to >= rq->ctx.sub_request->range->from);
		} else {
			rq->ctx.sub_request->range->to = -1;
		}
		kfiber_mutex_unlock(lock);
		//retry use net_fiber;
		return -2;
	}

	/* wait buffer */
	BigObjectReadQueue* queue = new BigObjectReadQueue;
	queue->rq = kfiber_self();
	queue->buf = buf;
	queue->selector = kgl_get_tls_selector();
	queue->from = offset;
	queue->length = length;
	block->wait_queue.push_back(queue);
	kfiber_mutex_unlock(lock);
	return kfiber_wait(buf);
}
void KSharedBigObject::close_write(KHttpObject* obj, int64_t write_from) {
	assert(write_from >= 0);
	//printf("CloseWrite fiber=[%p] from=[" INT64_FORMAT "]\n", kfiber_self(), write_from);
	std::list<BigObjectReadQueue*> notice_queues;
	assert(obj->getRefs() > 0);
	bool change_to_big_object = false;
	kfiber_mutex_lock(lock);
	//查找块
	krb_node* node = find_block_node(write_from);
	assert(node);
	KBigObjectBlock* block = (KBigObjectBlock*)node->data;
	assert(block);
	//printf("close write block infomation fiber=%p from=" INT64_FORMAT ",block=%p\n", block->net_fiber, block->file_block.from, block);

	if (block->net_fiber == kfiber_self()) {
		//如果该块的读请求是自已，则清空,并通知等待队列
		block->net_fiber = NULL;
		//复原读取点
		block->read_point = block->file_block.from;
		notice_queues.swap(block->wait_queue);
	}
	if (block->file_block.from == 0 && block->file_block.to >= obj->index.content_length) {
		//整个物件已经完成
		if (!body_complete) {
			//如果没有设置完成标识,则设置变身完成物件标识
			change_to_big_object = true;
		}
		body_complete = true;
	}
	assert(write_refs > 0);
	write_refs--;
	if (change_to_big_object && obj->in_cache) {
		//printf("变身完成物件\n");
		//assert(KBIT_TEST(obj->index.flags,FLAG_IN_MEM|FLAG_IN_DISK)==FLAG_IN_DISK);
		//变身完成物件
		KBIT_CLR(obj->index.flags, FLAG_IN_DISK);
		KHttpObject* nobj = new KHttpObject();
		kgl_memcpy(&nobj->dk, &obj->dk, sizeof(nobj->dk));
		kgl_memcpy(&nobj->index, &obj->index, sizeof(nobj->index));
		//assert(KBIT_TEST(nobj->index.flags, FLAG_BIG_OBJECT));
		nobj->uk.url = obj->uk.url->refs();
		if (obj->uk.vary != NULL) {
			KMutex* lock = obj->getLock();
			lock->Lock();
			nobj->uk.vary = obj->uk.vary->Clone();
			lock->Unlock();
		}
		nobj->h = obj->h;
		KBIT_CLR(nobj->index.flags, FLAG_BIG_OBJECT_PROGRESS);
		assert(nobj->data == NULL);
		nobj->data = new KHttpObjectBody(obj->data);
		nobj->data->i.type = BIG_OBJECT;
		nobj->dc_index_update = 1;
		bool cache_result = cache.add(nobj, LIST_IN_MEM);
		char* aio_buffer = NULL;
		int aio_buffer_size = 0;
		if (cache_result) {
			//把文件传给新物件
			KBIT_SET(nobj->index.flags, FLAG_IN_DISK);
			dci->start(ci_update, nobj);
			aio_buffer = nobj->build_aio_header(aio_buffer_size, nullptr, 0);
		} else {
			KBIT_SET(obj->index.flags, FLAG_IN_DISK);
		}
		nobj->release();
		KBIT_SET(obj->index.flags, FLAG_DEAD);
		//写文件头代码
		if (cache_result && fp) {
			kfiber_file_seek(fp, seekBegin, 0);
			kfiber_file_write_full(fp, aio_buffer, &aio_buffer_size);
			aio_free_buffer(aio_buffer);
		}
	}
	close(obj);
	kfiber_mutex_unlock(lock);
	//通知等待队列失败
	std::list<BigObjectReadQueue*>::iterator it;
	for (it = notice_queues.begin(); it != notice_queues.end(); it++) {
		kfiber_wakeup2((*it)->selector, (*it)->rq, (*it)->buf, -2);
		delete (*it);
	}
}
bool KSharedBigObject::create(KHttpObject* obj) {
	assert(obj->data->i.type == BIG_OBJECT_PROGRESS);
	char* filename = obj->get_filename();
	if (filename == NULL) {
		return false;
	}
	kfiber_mutex_lock(lock);
	assert(fp == NULL);
	fp = kfiber_file_open(filename, fileWriteRead, 0);
	xfree(filename);
	if (fp == NULL) {
		kfiber_mutex_unlock(lock);
		return false;
	}
	if (KBIT_TEST(obj->index.flags, ANSW_HAS_CONTENT_RANGE)) {
		KBIT_CLR(obj->index.flags, ANSW_HAS_CONTENT_RANGE);
		obj->remove_http_header(_KS("Content-Range"));
		obj->index.content_length = obj->index.content_range_length;
	}
	obj->index.last_verified = kgl_current_sec;
	obj->data->i.status_code = STATUS_OK;
	KBIT_SET(obj->index.flags, FLAG_IN_DISK | FLAG_BIG_OBJECT_PROGRESS);
	obj->cache_is_ready = 1;
	int size;
	char* buf = obj->build_aio_header(size, nullptr, 0);
	if (buf) {
		kfiber_file_write(fp, buf, size);
		aio_free_buffer(buf);
	}	
	kfiber_mutex_unlock(lock);
	return true;
}
KGL_RESULT KSharedBigObject::write(KHttpObject* obj, int64_t offset, const char* buf, int length) {
	//printf("sbo Write fiber=[%p] from=[" INT64_FORMAT "] length=[%d]\n", kfiber_self(), offset, length);
	std::list<BigObjectReadQueue*> noticeQueues;
	kfiber_mutex_lock(lock);
	if (fp == NULL || kfiber_file_seek(fp, seekBegin, offset + obj->index.head_size) != 0) {
		kfiber_mutex_unlock(lock);
		return KGL_EIO;
	}
	int left = length;
	if (!kfiber_file_write_full(fp, buf, &left)) {
		kfiber_mutex_unlock(lock);
		return KGL_EIO;
	}
	KGL_RESULT result = KGL_OK;
	//查找块
	krb_node* node = find_block_node(offset);
	assert(node);
	KBigObjectBlock* block = (KBigObjectBlock*)node->data;
	assert(block);
	//assert(block->readRequest == rq);
	int64_t len = offset + (int64_t)length - block->file_block.to;
	if (len <= 0) {
		//from和block->to不相等
		kfiber_mutex_unlock(lock);
		return KGL_OK;
	}
	block->file_block.to += len;
	int64_t preloaded_length = block->file_block.to - block->read_point;
	assert(preloaded_length >= 0);
	//查找下一块
	krb_node* next = rb_next(node);
	if (next) {
		KBigObjectBlock* nextBlock = (KBigObjectBlock*)next->data;
		if (block->file_block.to >= nextBlock->file_block.from) {
			//和下一块合并
			block->file_block.to = nextBlock->file_block.to;
			//合并下一块等待队列
#ifndef NDEBUG
			if (nextBlock->net_fiber == NULL) {
				assert(nextBlock->wait_queue.size() == 0);
			}
#endif
			block->wait_queue.merge(nextBlock->wait_queue);
			block->net_fiber = nextBlock->net_fiber;
			nextBlock->wait_queue.clear();
			//删除下一块
			rb_erase(next, &blocks);
			delete nextBlock;
			delete next;
			result = KGL_END;
		}
	}
	std::list<BigObjectReadQueue*>::iterator it;
	//复制满足条件的等待队列请求
	for (it = block->wait_queue.begin(); it != block->wait_queue.end();) {
		assert(read_refs > 0);
		if ((*it)->from < block->file_block.to || block->net_fiber == NULL) {
			//满足该请求数据
			noticeQueues.push_back((*it));
			it = block->wait_queue.erase(it);
		} else {
			//不满足，继续等待
			it++;
		}
	}
	kfiber_mutex_unlock(lock);
	for (it = noticeQueues.begin(); it != noticeQueues.end(); it++) {
		//通知等待队列
		int64_t buf_start = (*it)->from - offset;
		assert(buf_start >= 0);
		int64_t block_length = (int64_t)length - buf_start;
		block_length = KGL_MIN((int64_t)(*it)->length, block_length);
		memcpy((*it)->buf, buf + (int)buf_start, (int)block_length);
		kfiber_wakeup2((*it)->selector, (*it)->rq, (*it)->buf, (int)block_length);
		delete (*it);
	}
	if (result == KGL_OK) {
		if (preloaded_length > UPSTREAM_AUTO_DELAY_BUFFER_SIZE) {
			//智能对上游限速,可能缓冲越大，delay越多
			int us_delay_msec = (int)((preloaded_length - UPSTREAM_AUTO_DELAY_BUFFER_SIZE) / 2000);
			int max_delay_msec = length / 8;
			if (us_delay_msec > max_delay_msec) {
				us_delay_msec = max_delay_msec;
			}
			kfiber_msleep(us_delay_msec);
		}
	}
	return result;
}
kgl_satisfy_status KSharedBigObject::can_satisfy(kgl_request_range* range, KHttpObject* obj) {
	if (body_complete) {
		return kgl_satisfy_all;
	}
	if (!range) {
		return kgl_satisfy_part;
	}
	kgl_satisfy_status status = kgl_satisfy_none;
	kfiber_mutex_lock(lock);
	krb_node* node = find_block_node(range->from);
	if (node == NULL) {
		status = create_if_range(range, NULL);
		kfiber_mutex_unlock(lock);
		return status;
	}
	KBigObjectBlock* block = (KBigObjectBlock*)node->data;
	if (block->file_block.to == range->from) {
		//block->to是没有数据的。
		status = create_if_range(range, node);
		kfiber_mutex_unlock(lock);
		return status;
	}
	if (block->file_block.to >= obj->index.content_length) {
		//最后一块
		kfiber_mutex_unlock(lock);
		return kgl_satisfy_all;
	}
	if (range->to >= 0) {
		//块数据结束能满足
		if (block->file_block.to >= range->to) {
			status = kgl_satisfy_all;
		}
	}
	if (status!= kgl_satisfy_all) {
		status = create_if_range(range, node);
	}
	kfiber_mutex_unlock(lock);
	return status;
}

bool KSharedBigObject::save_progress(KHttpObject* obj) {
	assert(!kfiber_is_main());
	char* filename = obj->get_filename(true);
	if (filename == NULL) {
		return false;
	}
	kfiber_file* fp = kfiber_file_open(filename, fileWrite, 0);
	if (!fp) {
		free(filename);
		return false;
	}
	free(filename);
	krb_node* node = rb_first(&blocks);
	while (node) {
		KBigObjectBlock* block = (KBigObjectBlock*)node->data;
		int len = sizeof(block->file_block);
		kfiber_file_write_full(fp, (char*)&block->file_block, &len);
		node = rb_next(node);
	}
	kfiber_file_close(fp);
	return true;
}
bool KSharedBigObject::restore(char* buf, int length) {
	KFileBlock* pi = (KFileBlock*)buf;
	kfiber_mutex_lock(lock);
	while (length >= (int)sizeof(KFileBlock)) {
		length -= sizeof(KFileBlock);
		bool new_obj;
		krb_node* node = insert(pi->from, new_obj);
		KBigObjectBlock* block = (KBigObjectBlock*)node->data;
		if (!new_obj) {
			break;
		}
		block->file_block.to = pi->to;
		pi = pi + 1;
	}
	kfiber_mutex_unlock(lock);
	return length == 0;
}
void KSharedBigObject::save_last_verified(KHttpObject* obj) {
#if 0
	lock.Lock();
	obj->index.last_verified = kgl_current_sec;
	lock.Unlock();
#endif
}
#endif

