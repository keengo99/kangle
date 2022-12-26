#include "KSboFile.h"
#include "KHttpObject.h"
#include "KBigObjectContext.h"
#if 0
//{{ent
#ifdef ENABLE_BIG_OBJECT_206
KSboFile::KSboFile()
{
	aio_file = NULL;
	sbo = NULL;
	write_buffer = write_hot = NULL;
	filename = NULL;
}
KSboFile::~KSboFile()
{
	if (aio_file) {
		kasync_file_close(aio_file);
		xfree(aio_file);
	}
	if (write_buffer) {
		aio_free_buffer(write_buffer);
	}
	if (filename) {
		xfree(filename);
	}
}
kev_result KSboFile::read(KHttpRequest *rq, char *buffer,INT64 offset, int *length, aio_callback cb, INT64 &next_from)
{
	//printf("try read offset=[" INT64_FORMAT "] length=[%d]\n",offset,length);
	INT64 block_length;
	if (write_buffer && offset >= write_offset) {
		INT64 block_start = kgl_align(write_offset, kgl_aio_align_size);
		int write_pad = 0;
		if (block_start != write_offset) {
			write_pad = kgl_aio_align_size - (int)(block_start - write_offset);
		}
		char *write_align_buffer = write_buffer + write_pad;
		int write_buffer_used = write_hot - write_align_buffer;
		assert(write_buffer_used >= 0);
		INT64 offset_diff = offset - write_offset;
		block_length = (INT64)write_buffer_used - offset_diff;
		//printf("write_align_buffer=[%p] write_hot=[%p] block_length=[%d]\n", write_align_buffer, write_hot,(int)block_length);
		if (block_length>0) {
			block_length = KGL_MIN(block_length, (INT64)*length);
			memmove(buffer, write_align_buffer + offset_diff, (int)block_length);
			*length = (int)block_length;
			return cb(aio_file, rq, buffer,(int)block_length);
		}
	}
	block_length = sbo->find_block(offset, &next_from);
	//printf("find_block offset=[" INT64_FORMAT "] length=[" INT64_FORMAT "]\n",offset,block_length);
	if ((block_length>0 && block_length < rq->bo_ctx->left_read) || (next_from>0 && next_from <= (offset + rq->bo_ctx->left_read))) {
		rq->ctx->cache_hit_part = true;
	}
	if (block_length <= 0) {
		*length = 0;
		return kev_err;
	}
	block_length = KGL_MIN(block_length, (INT64)*length);
	*length = (int)block_length;
	//printf("read from disk buffer=[%p]\n",buffer);
	if (!kgl_selector_module.aio_read(rq->sink->get_selector(), aio_file, buffer, offset + rq->bo_ctx->gobj->index.head_size, (int)block_length, cb, rq)) {
		return cb(aio_file, rq, NULL, -1);
	}
	return kev_ok;
}
bool KSboFile::open(KHttpObject *obj, bool create_flag)
{
#if 0
	assert(obj->data->type == BIG_OBJECT_PROGRESS);
	sbo = obj->data->sbo;
	kassert(filename == NULL);
	if (filename) {
		xfree(filename);
	}
	filename = obj->getFileName();
	fileModel model = fileReadWrite;
	if (create_flag) {
		model = fileWriteRead;
	}
	FILE_HANDLE fp = kfopen(filename, model, KFILE_ASYNC);
	//printf("open sbo file=[%s] result=[%d]\n", filename, open_result);
	//free(filename);
	if (!kflike(fp)) {
		return false;
	}
	assert(aio_file == NULL);
	aio_file = (kasync_file *)xmemory_newz(sizeof(kasync_file));
	kgl_selector_module.aio_open(selector,aio_file, fp);
#endif
	return true;
}
bool KSboFile::OpenWrite(INT64 offset, INT64 length)
{
	return false;
}
KGL_RESULT KSboFile::write(const char *buf, int len)
{
	//printf("************sbo write len=[%d]\n", len);
	//fwrite(buf,1,len,stdout);
	//printf("\n");
	if (write_buffer == NULL) {
		INT64 block_start = kgl_align(write_offset, kgl_aio_align_size);
		int write_pad = 0;
		if (block_start != write_offset) {
			write_pad = kgl_aio_align_size - (int)(block_start - write_offset);
		}
		write_buffer_size = MAX(len, (int)conf.io_buffer);
		write_buffer_size = kgl_align(write_buffer_size, IO_BLOCK_SIZE);
		write_left = write_buffer_size - write_pad;
		write_buffer = (char *)aio_alloc_buffer(write_buffer_size);
		write_hot = write_buffer + write_pad;
	} else if (write_left < len) {
		int used_len = write_hot - write_buffer;
		write_buffer_size = len + used_len;
		write_buffer_size = MAX(write_buffer_size, (int)conf.io_buffer);
		write_buffer_size = kgl_align(write_buffer_size, IO_BLOCK_SIZE);
		char *new_aio_buffer = (char *)aio_alloc_buffer(write_buffer_size);
		write_left = write_buffer_size - used_len;
		kgl_memcpy(new_aio_buffer, write_buffer, used_len);
		write_hot = new_aio_buffer + used_len;
		aio_free_buffer(write_buffer);
		write_buffer = new_aio_buffer;
	}
	kgl_memcpy(write_hot, buf, len);
	write_hot += len;
	write_left -= len;
	return KGL_OK;
}
bool KSboFile::start_flush()
{
	if (write_buffer == NULL) {
		return false;
	}
	INT64 block_start = kgl_align(write_offset, kgl_aio_align_size);
	//int write_pad = kgl_aio_align_size - (int)(block_start - write_offset);
	char *write_align_buffer = write_buffer;
	if (block_start != write_offset) {
		write_align_buffer += kgl_aio_align_size;
		if (write_hot <= write_align_buffer ) {
			//²»¹»
			aio_free_buffer(write_buffer);
			write_buffer = write_hot = NULL;
			return false;
		}
		memmove(write_buffer, write_align_buffer, write_hot - write_align_buffer);
	}
	write_left = write_hot - write_align_buffer;
	//printf("start_flush write_buffer=[%p] write_align_buffer=[%p %d] write_hot=[%p] total_flush_data=[%d],write_pad=[%d]\n", write_buffer ,write_align_buffer, write_align_buffer- write_buffer,write_hot, write_left, write_pad);
	write_hot = write_buffer;
	write_offset = block_start;
	//printf("start_flush total_flush_data=[%d]\n",  write_left);
	return true;
}
#if 0
kev_result KSboFile::flush(KHttpRequest *rq,KHttpObject *obj, aio_callback cb)
{
	if (write_buffer==NULL) {
		return kev_err;
	}
	INT64 aio_buffer_used = write_hot - write_buffer;
	INT64 sb_offset = write_offset + aio_buffer_used;
	bool last_block = (write_left > 0 && sb_offset + write_left == obj->index.content_length);
	if (write_left >= kgl_aio_align_size || last_block) {
		int write_length = write_left;
		if (!last_block) {
			write_length = kgl_align(write_left, kgl_aio_align_size);
			if (write_length != write_left) {
				write_length -= kgl_aio_align_size;
			}
		}
		if (!raw_write(rq, write_hot, sb_offset + obj->index.head_size, write_length, cb)) {
			return cb(aio_file, rq, NULL, -1);
		}
		//printf("aio_write sb_offset=[" INT64_FORMAT "] length=[%d]\n", sb_offset, write_length);
		/*
		if (!kgl_selector_module.aio_write(rq->sink->get_selector(), aio_file, write_hot, sb_offset + obj->index.head_size, write_length, cb, rq)) {
			return cb(aio_file, rq, NULL, -1);
		}
		*/
		return kev_ok;
	}	
	//printf("aio_buffer_used=[" INT64_FORMAT "] aio_write_offset=[" INT64_FORMAT "]\n", aio_buffer_used, aio_write_offset);
	if (aio_buffer_used>0 && sbo->add_data(obj, write_offset, aio_buffer_used)) {
		aio_free_buffer(write_buffer);
		write_buffer = write_hot = NULL;
		return rq->bo_ctx->sbo_complete(rq);
	}
	write_offset += aio_buffer_used;
	if (write_left > 0) {
		memmove(write_buffer, write_hot, write_left);
		write_hot = write_buffer + write_left;
		write_left = write_buffer_size - write_left;
	} else {
		aio_free_buffer(write_buffer);
		write_buffer = write_hot = NULL;
	}
	return kev_err;
}
#endif
void KSboFile::flush_result(int got)
{
	//printf("flush_result=[%d]\n",got);
	assert(write_buffer);
	write_hot += got;
	write_left -= got;
}
bool KSboFile::raw_write(KHttpRequest *rq, char *buffer, INT64 offset, int length, aio_callback cb)
{
	klog(KLOG_NOTICE, "sbo_write [%s] offset [" INT64_FORMAT "] length=[%d] sign=[%x]\n",
		filename,
		offset,
		length,
		(u_char)buffer[0]);
	return kgl_selector_module.aio_write(rq->sink->get_selector(), aio_file, buffer, offset, length, cb, rq);
}
#endif//}}
#endif