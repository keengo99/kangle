#include "KHttpObjectSwaping.h"
#include "KHttpObject.h"
#include "KCache.h"
#include "kfile.h"
#ifdef ENABLE_DISK_CACHE
#ifdef ENABLE_BIG_OBJECT_206
swap_in_result KHttpObjectSwaping::swapin_proress(KHttpObject* obj, KHttpObjectBody* data)
{
	auto filename = obj->get_filename(true);
	if (filename == NULL) {
		return swap_in_failed_other;
	}
	kfiber_file* file = kfiber_file_open(filename.get(), fileRead, 0);
	if (file == NULL) {
		return swap_in_failed_open;
	}
	if (!kasync_file_direct(file,true)) {
		kfiber_file_close(file);
		return swap_in_failed_open;
	}
	int size = (int)kfiber_file_size(file);
	size = (int)(KGL_MIN(16384, size));
	char* buf = (char*)aio_alloc_buffer(size);
	char* hot = buf;
	int left = size;
	swap_in_result result = swap_in_success;
	while (left > 0) {
		int got = kfiber_file_read(file, hot, left);
		if (got <= 0) {
			result = swap_in_failed_read;
			goto clean;
		}
		hot += got;
		left -= got;
	}

	if (!data->sbo->restore(buf, size)) {
		result = swap_in_failed_parse;
	}
clean:
	kfiber_file_close(file);
	aio_free_buffer(buf);
	return result;
}
#endif
void KHttpObjectSwaping::swapin_body_result(KHttpObjectBody* data, char* buf, int got, kbuf** last)
{
	kbuf* tmp = (kbuf*)malloc(sizeof(kbuf));
	tmp->used = got;
	tmp->data = (char*)malloc(got);
	kgl_memcpy(tmp->data, buf, got);
	tmp->flags = 0;
	tmp->next = NULL;
	if ((*last) == NULL) {
		assert(data->bodys == NULL);
		data->bodys = tmp;
	} else {
		(*last)->next = tmp;
	}
	(*last) = tmp;
}
swap_in_result KHttpObjectSwaping::swapin_head(kfiber_file* fp, KHttpObject* obj, KHttpObjectBody* data)
{
	char* buf = (char*)aio_alloc_buffer(obj->index.head_size);
	char* hot = buf;
	int left_read = obj->index.head_size;
	swap_in_result result = swap_in_success;
	while (left_read > 0) {
		int got = kfiber_file_read(fp, hot, left_read);
		if (got <= 0) {
			result = swap_in_failed_read;
			goto clean;
		}
		hot += got;
		left_read -= got;
	}
	if (!data->restore_header(obj, buf, obj->index.head_size)) {
		result = swap_in_failed_parse;
		goto clean;
	}
#ifdef ENABLE_BIG_OBJECT_206
	if (data->i.type == BIG_OBJECT_PROGRESS) {
		result = swapin_proress(obj, data);
	}
#endif
clean:
	aio_free_buffer(buf);
	return result;
}
swap_in_result KHttpObjectSwaping::swapin_head_body(kfiber_file* fp, KHttpObject* obj, KHttpObjectBody* data)
{
	INT64 left_read = obj->index.content_length + obj->index.head_size;
	INT64 alloc_size = KGL_MIN((INT64)conf.io_buffer, left_read);
	alloc_size = KGL_MAX(alloc_size, obj->index.head_size);
	alloc_size = kgl_align(alloc_size, kgl_aio_align_size);
	char* buf = (char*)aio_alloc_buffer((int)alloc_size);
	swap_in_result result = swap_in_success;
	char* hot = buf;
	int buf_left = (int)alloc_size;
	kbuf* last = NULL;
	while (left_read > 0) {
		int read_len = (int)KGL_MIN(left_read, (INT64)buf_left);
		int got = kfiber_file_read(fp, hot, read_len);
		if (got <= 0) {
			result = swap_in_failed_read;
			goto clean;
		}
		hot += got;
		buf_left -= got;
		left_read -= got;
		if (data->i.status_code == 0) {
			int buf_size = (int)(hot - buf);
			if (buf_size < (int)obj->index.head_size) {
				continue;
			}
			if (!data->restore_header(obj, buf, obj->index.head_size)) {
				result = swap_in_failed_parse;
				goto clean;
			}
			swapin_body_result(data, buf + obj->index.head_size, buf_size - obj->index.head_size, &last);
		} else {
			swapin_body_result(data, buf, (int)(hot - buf), &last);
		}
		hot = buf;
		buf_left = (int)alloc_size;
	}
clean:
	aio_free_buffer(buf);
	return result;
}
swap_in_result KHttpObjectSwaping::swapin(KHttpRequest* rq, KHttpObject* obj)
{
	KHttpObjectBody* data = NULL;
	auto filename = obj->get_filename();
	swap_in_result result = swap_in_failed_other;
	kfiber_file* file = kfiber_file_open(filename.get(), fileRead, 0);
	if (file == NULL) {
		result = swap_in_failed_open;
		goto clean;
	}
	if (!kasync_file_direct(file,true)) {
		result = swap_in_failed_open;
		goto clean;
	}
	if (!is_valide_dc_head_size(obj->index.head_size)) {
		result = swap_in_failed_format;
		goto clean;
	}
	data = new KHttpObjectBody();
	data->create_type(&obj->index);
#ifdef ENABLE_BIG_OBJECT
	if (data->i.type != MEMORY_OBJECT) {
		result = swapin_head(file, obj, data);
		goto clean;
	}
#endif
	result = swapin_head_body(file, obj, data);
clean:
	if (file) {
		kfiber_file_close(file);
	}
	this->notice(obj, data, result);
	return result;
}
swap_in_result KHttpObjectSwaping::wait(KMutex* lock)
{
	kfiber* fiber = kfiber_self();
	KHttpObjectSwapWaiter* waiter = new KHttpObjectSwapWaiter;
	waiter->fiber = fiber;
	assert(waiter->fiber);
	waiter->next = this->waiter;
	lock->Unlock();
	return (swap_in_result)__kfiber_wait(fiber, this);
}
void KHttpObjectSwaping::notice(KHttpObject* obj, KHttpObjectBody* data, swap_in_result result)
{
	assert(obj->data->i.type == SWAPING_OBJECT);
	KHttpObjectBody* osData = obj->data;
	assert(osData && osData->os == this);
	KMutex* lock = obj->getLock();
	lock->Lock();
	obj->data = data;
	if (result == swap_in_success) {
		KBIT_SET(obj->index.flags, FLAG_IN_MEM);
	} else if (result != swap_in_busy) {
		KBIT_SET(obj->index.flags, FLAG_DEAD);
	}
	lock->Unlock();
	if (result == swap_in_success) {
		kassert(obj->data->i.status_code > 0);
		cache.getHash(obj->h)->IncMemObjectSize(obj);
	}
	KHttpObjectSwapWaiter* next;
	while (waiter) {
		next = waiter->next;
		kfiber_wakeup_ts(waiter->fiber, this, (int)result);
		delete waiter;
		waiter = next;
	}
	delete osData;
}
#endif
