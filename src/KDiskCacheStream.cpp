#include "KDiskCacheStream.h"
#include "KHttpRequest.h"
#include "KHttpObject.h"
#ifdef LINUX_EPOLL
#include <unistd.h>
#endif
#ifdef ENABLE_DISK_CACHE

bool KDiskCacheStream::Open(KHttpObject *obj)
{
	/* Keep a successfully closed cache file, but discard an incomplete one. */
	reset(fp != nullptr);
	filename = obj->get_filename().release();
	if (filename == nullptr) {
		return false;
	}
	fp = kfiber_file_open(filename, fileWrite, 0);
	if (fp == nullptr) {
		reset(false);
		return false;
	}
	if (!kasync_file_direct(fp,true)) {
		reset(true);
		return false;
	}
	if (kfiber_file_seek(fp, seekBegin, obj->GetHeaderSize(0)) != 0) {
		reset(true);
		return false;
	}
	return true;
}
int64_t KDiskCacheStream::GetLength(KHttpObject* obj)
{
	assert(fp);
	return kfiber_file_tell(fp) - obj->index.head_size;
}
bool KDiskCacheStream::Write(KHttpObject *obj, const char *buf, int len)
{
	if (fp == nullptr || len < 0 || (len > 0 && buf == nullptr)) {
		return false;
	}
	while (len > 0) {
		if (buffer == NULL) {
			buffer_size = conf.io_buffer;
			if (buffer_size < 4096) {
				buffer_size = 4096;
			}
			/* direct I/O writes need a length aligned to the block size. */
			buffer_size = (int)kgl_align((size_t)buffer_size, kgl_aio_align_size);
			buffer_left = buffer_size;
			buffer = (char *)aio_alloc_buffer(buffer_size);
			if (buffer == nullptr) {
				buffer_left = buffer_size = 0;
				return false;
			}
			hot = buffer;
		}
		int this_len = KGL_MIN(buffer_left, len);
		memcpy(hot, buf, this_len);
		hot += this_len;
		buf += this_len;
		buffer_left -= this_len;
		len -= this_len;
		if (buffer_left <= 0) {
			if (!FlushBuffer()) {
				return false;
			}
		}
	}
	return true;
}

bool KDiskCacheStream::Close(KHttpObject *obj)
{
	if (fp == nullptr) {
		return false;
	}
	int64_t data_end = kfiber_file_tell(fp) + (buffer ? (int64_t)(hot - buffer) : 0);
	if (!FlushBuffer(true)) {
		return false;
	}
#ifdef LINUX_EPOLL
	if (fp->st.direct_io && kfiber_file_tell(fp) != data_end) {
		/* drop the zero padding written by the last aligned direct I/O block. */
		if (ftruncate(kasync_file_get_handle(fp), data_end) != 0) {
			return false;
		}
		kfiber_file_seek(fp, seekBegin, data_end);
	}
#endif
	int64_t content_length = GetLength(obj);
	if (content_length < 0) {
		return false;
	}
	obj->index.content_length = content_length;
	//Open时只预留了文件头空间，swapin和重启加载都要校验文件头
	uint32_t reserved_head_size = obj->index.head_size;
	obj->index.head_size = 0;
	int need_head_size = obj->GetHeaderSize(0);
	obj->index.head_size = reserved_head_size;
	if (need_head_size <= 0 || (uint32_t)need_head_size > reserved_head_size) {
		//build_header不检查边界，文件头超出预留空间时不能写
		return false;
	}
	int header_size = 0;
	auto header = obj->build_aio_header(header_size, nullptr, 0);
	if (!header || header_size != (int)obj->index.head_size) {
		return false;
	}
	if (kfiber_file_seek(fp, seekBegin, 0) != 0 || !kfiber_file_write_full(fp, header.get(), &header_size)) {
		return false;
	}
	kfiber_file_close(fp);
	fp = NULL;
	return true;
}
bool KDiskCacheStream::FlushBuffer(bool last)
{
	if (fp == nullptr) {
		return false;
	}
	if (buffer == nullptr) {
		return true;
	}
	int size = (int)(hot - buffer);
#ifdef LINUX_EPOLL
	if (last && fp->st.direct_io) {
		/*
		 * O_DIRECT rejects a write whose length is not block aligned, so the
		 * final partial block used to fail and the whole cache file was
		 * dropped. Pad with zeros here; Close() truncates the padding.
		 */
		int aligned = (int)kgl_align((size_t)size, kgl_aio_align_size);
		if (aligned > size && aligned <= buffer_size) {
			memset(hot, 0, aligned - size);
			size = aligned;
		}
	}
#endif
	hot = buffer;
	while (size > 0) {
		int got = kfiber_file_write(fp, hot, size);
		if (got <= 0) {
			return false;
		}
		hot += got;
		size -= got;
	}
	hot = buffer;
	buffer_left = buffer_size;
	return true;
}
#endif
