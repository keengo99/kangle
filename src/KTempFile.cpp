#include "KTempFile.h"
#include "KHttpRequest.h"
#include "kmalloc.h"
#include "kselector.h"
#include "http.h"
#include "directory.h"
#include "kforwin32.h"
#include "KHttpServer.h"
#ifdef ENABLE_TF_EXCHANGE
#ifdef _WIN32
#define tunlink(f)             
#else
#define tunlink(f)             unlink(f)
#endif

struct kgl_tempfile_input_stream : kgl_forward_input_stream
{
	KTempFile tmp_file;
};

struct kgl_tempfile_output_ctx : kgl_forward_body
{
	KTempFile tmp_file;
	kfiber* write_fiber;
};

KTempFile::KTempFile() {
	write_offset = 0;
	read_offset = 0;
	fp = NULL;
	wait_read = NULL;
	mem = NULL;
	mem_size = 0;
	write_is_end = false;
	closed = true;
	lock = kfiber_mutex_init();
}
void KTempFile::Close() {
	kfiber* fiber = NULL;
	kfiber_mutex_lock(lock);
	if (fp) {
		kfiber_file_close(fp);
		tunlink(file.c_str());
		fp = NULL;
	}
	closed = true;
	write_is_end = true;
	fiber = wait_read;
	wait_read = NULL;
	kfiber_mutex_unlock(lock);
	if (fiber) {
		kfiber_wakeup(fiber, this, -1);
	}
}
KTempFile::~KTempFile() {
	Close();
	if (mem) {
		xfree(mem);
	}
	kfiber_mutex_destroy(lock);
	assert(wait_read == NULL);
}
bool KTempFile::Init() {
	write_offset = 0;
	read_offset = 0;
	write_is_end = false;
	if (fp) {
		kfiber_file_close(fp);
		tunlink(file.c_str());
		fp = NULL;
	}
	// the file is created when the data outgrows the memory buffer
	closed = false;
	return true;
}
bool KTempFile::open_file() {
	KStringBuf s;
	s << conf.tmppath << "krf" << m_ppid << "_" << m_pid << "_" << (uint64_t)this;
	s.str().swap(file);
	fp = kfiber_file_open(file.c_str(), fileWriteRead, KFILE_TEMP_MODEL);
	if (fp == NULL) {
		klog(KLOG_ERR, "cann't open tmp file [%s]\n", file.c_str());
		return false;
	}
	return true;
}
bool KTempFile::write_all(const char* buf, int len) {
	while (len > 0) {
		int got = Write(buf, len);
		if (got <= 0) {
			return false;
		}
		buf += got;
		len -= got;
	}
	return true;
}
void KTempFile::WriteEnd() {
	kfiber_mutex_lock(lock);
	write_is_end = true;
	kfiber* fiber = wait_read;
	if (fiber != NULL) {
		wait_read = NULL;
	}
	kfiber_mutex_unlock(lock);
	if (fiber != NULL) {
		kfiber_wakeup(fiber, this, 0);
	}
}
int KTempFile::Write(const char* buf, int len) {
	kfiber_mutex_lock(lock);
	if (closed) {
		kfiber_mutex_unlock(lock);
		return -1;
	}
	int got = -1;
	if (write_offset < TEMPFILE_MEMORY_SIZE) {
		int need = (int)write_offset + KGL_MIN(len, TEMPFILE_MEMORY_SIZE - (int)write_offset);
		if (need > mem_size) {
			// grow by doubling, so small responses use little memory
			int new_size = KGL_MAX(mem_size * 2, 4096);
			while (new_size < need) {
				new_size *= 2;
			}
			new_size = KGL_MIN(new_size, TEMPFILE_MEMORY_SIZE);
			char* new_mem = (char*)xmalloc(new_size);
			if (new_mem) {
				if (mem) {
					kgl_memcpy(new_mem, mem, (size_t)write_offset);
					xfree(mem);
				}
				mem = new_mem;
				mem_size = new_size;
			}
		}
		if (need <= mem_size) {
			got = need - (int)write_offset;
			kgl_memcpy(mem + write_offset, buf, got);
		}
	} else if (fp != NULL || open_file()) {
		if (kfiber_file_seek(fp, seekBegin, write_offset - TEMPFILE_MEMORY_SIZE) == 0) {
			got = kfiber_file_write(fp, buf, len);
		}
	}
	kfiber* fiber = NULL;
	if (got > 0) {
		write_offset += got;
		fiber = wait_read;
		if (write_offset > read_offset && fiber != NULL) {
			wait_read = NULL;
		}
	}
	kfiber_mutex_unlock(lock);
	if (fiber != NULL) {
		kfiber_wakeup(fiber, this, 0);
	}
	return got;
}
int KTempFile::Read(char* buf, int len) {
	for (;;) {
		kfiber_mutex_lock(lock);
		if (closed || wait_read) {
			kfiber_mutex_unlock(lock);
			return -1;
		}
		int64_t have_data = write_offset - read_offset;
		if (have_data <= 0) {
			if (write_is_end) {
				kfiber_mutex_unlock(lock);
				return 0;
			}
			wait_read = kfiber_self2();
			kfiber* fiber = wait_read;
			kfiber_mutex_unlock(lock);
			//printf("no more data to read now wait write. fiber=[%p]\n", wait_read);
			__kfiber_wait(fiber, this);
			continue;
		}
		assert(wait_read == NULL);
		len = (int)KGL_MIN((int64_t)len, have_data);
		int got = -1;
		if (read_offset < TEMPFILE_MEMORY_SIZE) {
			got = KGL_MIN(len, TEMPFILE_MEMORY_SIZE - (int)read_offset);
			kgl_memcpy(buf, mem + read_offset, got);
		} else if (fp != NULL && kfiber_file_seek(fp, seekBegin, read_offset - TEMPFILE_MEMORY_SIZE) == 0) {
			got = kfiber_file_read(fp, buf, len);
		}
		if (got > 0) {
			read_offset += got;
		}
		kfiber_mutex_unlock(lock);
		return got;
	}
	return -1;
}
int listHandleTempFile(const char* file, void* param) {
	if (filencmp(file, "krf", 3) != 0) {
		return 0;
	}
	int pid = atoi(file + 3);
	if (pid == m_ppid) {
		return 0;
	}
	KStringBuf s;
	s << conf.tmppath << file;
	klog(KLOG_NOTICE, "remove uncleaned tmpfile [%s]\n", s.str().c_str());
	unlink(s.str().c_str());
	return 0;
}
KTHREAD_FUNCTION clean_tempfile_thread(void* param) {
	klog(KLOG_DEBUG, "start to clean tmp file thread...\n");
	list_dir(conf.tmppath.c_str(), listHandleTempFile, NULL);
	klog(KLOG_DEBUG, "clean tmp file done.\n");
	KTHREAD_RETURN;
}

static int64_t tmpfile_input_get_read_left(kgl_request_body_ctx* st) {
	kgl_tempfile_input_stream* in = (kgl_tempfile_input_stream*)st;
	return in->tmp_file.GetLeft();
}
static int tmpfile_input_read(kgl_request_body_ctx* st, char* buf, int len) {
	kgl_tempfile_input_stream* in = (kgl_tempfile_input_stream*)st;
	return in->tmp_file.Read(buf, len);
}
static void tmpfile_input_release(kgl_request_body_ctx* st) {
	kgl_tempfile_input_stream* in = (kgl_tempfile_input_stream*)st;
	in->up_stream.f->body.close(in->up_stream.body_ctx);
	delete in;
}
static kgl_input_stream_function tempfile_input_stream_function = {	
	tmpfile_input_get_read_left,
	tmpfile_input_read,
	tmpfile_input_release,
	forward_get_url,
	forward_get_precondition,
	forward_get_range,
	forward_get_header_count,
	forward_get_header,
};
bool new_tempfile_input_stream(kgl_input_stream* in, int64_t max_size, bool* too_large) {
	if (too_large) {
		*too_large = false;
	}
	if ((in)->f->body.get_left(in->body_ctx) == 0) {
		return true;
	}
	kgl_tempfile_input_stream* st = new kgl_tempfile_input_stream;
	if (!st->tmp_file.Init()) {
		delete st;
		return false;
	}
	char* buf = (char*)malloc(TEMPFILE_POST_CHUNK_SIZE);
	if (buf == NULL) {
		delete st;
		return false;
	}
	int64_t total = 0;
	while ((in)->f->body.get_left(in->body_ctx) != 0) {
		int got = in->f->body.read(in->body_ctx, buf, TEMPFILE_POST_CHUNK_SIZE);
		if (got < 0) {
			goto err;
		}
		if (got == 0) {
			break;
		}
		total += got;
		if (max_size > 0 && total > max_size) {
			if (too_large) {
				*too_large = true;
			}
			goto err;
		}
		if (!st->tmp_file.write_all(buf, got)) {
			goto err;
		}
	}
	st->tmp_file.WriteEnd();
	free(buf);
	pipe_input_stream(st, &tempfile_input_stream_function, in);
	return true;
err:
	delete st;
	free(buf);
	return false;
}

int tempfile_write_fiber(void* arg, int got) {

	kgl_tempfile_output_ctx* out = (kgl_tempfile_output_ctx*)arg;
	KGL_RESULT result = KGL_OK;
	char* buf = (char*)malloc(8192);
	if (buf == NULL) {
		return (int)KGL_ENO_MEMORY;
	}
	for (;;) {
		int len = out->tmp_file.Read(buf, 8192);
		if (len == 0) {
			break;
		}
		if (len < 0) {
			result = KGL_EIO;
			break;
		}
		result  = forward_write((kgl_response_body_ctx*)out, buf, len);
		if (result != KGL_OK) {
			out->tmp_file.Close();
			break;
		}
	}
	free(buf);
	return (int)result;
}
int tempfile_end(kgl_tempfile_output_ctx* tmp_out) {
	tmp_out->tmp_file.WriteEnd();
	if (tmp_out->write_fiber) {
		int ret = -1;
		kfiber_join(tmp_out->write_fiber, &ret);
		tmp_out->write_fiber = NULL;
		return ret;
	}
	return 0;
}
KGL_RESULT tempfile_close(kgl_response_body_ctx* out, KGL_RESULT result) {
	kgl_tempfile_output_ctx* tmp_out = (kgl_tempfile_output_ctx*)out;
	int tempfile_result = tempfile_end(tmp_out);
	if (result == KGL_OK && tempfile_result != KGL_OK) {
		result = (KGL_RESULT)tempfile_result;
	}
	result = forward_close(out, result);
	delete tmp_out;
	return result;
}
static KGL_RESULT tempfile_write_body(kgl_response_body_ctx* ctx, const char* buf, int len) {
	kassert(!kfiber_is_main());
	kgl_tempfile_output_ctx* tmp_out = (kgl_tempfile_output_ctx*)ctx;
	if (!tmp_out->tmp_file.write_all(buf, len)) {
		return KGL_EIO;
	}
	if (tmp_out->write_fiber == NULL) {
		kfiber_create(tempfile_write_fiber, tmp_out, 0, http_config.fiber_stack_size, &tmp_out->write_fiber);
		if (tmp_out->write_fiber == NULL) {
			return KGL_ESYSCALL;
		}
	} else {
		int result = -1;
		if (kfiber_try_join(tmp_out->write_fiber, &result) == 0) {
			tmp_out->write_fiber = NULL;
			return (KGL_RESULT)result;
		}
	}
	return KGL_OK;
}
static kgl_response_body_function tempfile_output_function = {
	unsupport_writev<tempfile_write_body>,
	tempfile_write_body,
	kgl_empty_flush,
	support_sendfile_false,
	unsupport_sendfile,
	tempfile_close
};

bool tee_tempfile_body(kgl_response_body* body)
{
	kgl_tempfile_output_ctx* ctx = new kgl_tempfile_output_ctx;
	ctx->write_fiber = nullptr;
	if (!ctx->tmp_file.Init()) {
		delete ctx;
		return false;
	}
	pipe_response_body(ctx, &tempfile_output_function, body);
	return true;
}
#endif
