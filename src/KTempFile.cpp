#include "KTempFile.h"
#include "KHttpRequest.h"
#include "kmalloc.h"
#include "kselector.h"
#include "http.h"
#include "directory.h"
#include "kforwin32.h"
#include "KHttpFilterStream.h"
#include "KHttpFilterManage.h"
#ifdef ENABLE_TF_EXCHANGE
#ifdef _WIN32
#define tunlink(f)             
#else
#define tunlink(f)             unlink(f)
#endif

struct kgl_tempfile_input_stream : public kgl_input_stream {
	kgl_input_stream* up;
	KTempFile tmp_file;
};

struct kgl_tempfile_output_stream : public kgl_forward_stream {
	KTempFile tmp_file;
	KREQUEST rq;
	kfiber* write_fiber;
};

KTempFile::KTempFile()
{
	write_offset = 0;
	read_offset = 0;
	fp = NULL;
	wait_read = NULL;
	write_is_end = false;
	lock = kfiber_mutex_init();
}
void KTempFile::Close()
{
	kfiber_mutex_lock(lock);
	if (fp) {
		kfiber_file_close(fp);
		tunlink(file.c_str());
		fp = NULL;
	}
	kfiber_mutex_unlock(lock);
}
KTempFile::~KTempFile()
{
	Close();
	kfiber_mutex_destroy(lock);
	assert(wait_read == NULL);
}
bool KTempFile::Init()
{
	write_offset = 0;
	read_offset = 0;
	write_is_end = false;
	if (fp) {
		kfiber_file_close(fp);
		tunlink(file.c_str());
	}
	std::stringstream s;
	s << conf.tmppath << "krf" << m_ppid << "_" << m_pid << "_" << (void*)this;
	s.str().swap(file);
	fp = kfiber_file_open(file.c_str(), fileWriteRead, KFILE_TEMP_MODEL);
	return fp != NULL;
}
bool KTempFile::WriteAll(const char* buf, int len)
{
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
void KTempFile::WriteEnd()
{
	write_is_end = true;
	kfiber* fiber = wait_read;
	if (fiber != NULL) {
		wait_read = NULL;
		kfiber_wakeup(fiber,this, 0);
	}
}
int KTempFile::Write(const char *buf, int len)
{
	kfiber_mutex_lock(lock);
	if (fp == NULL) {
		kfiber_mutex_unlock(lock);
		return -1;
	}
	kfiber_file_seek(fp, seekBegin, write_offset);
	int got = kfiber_file_write(fp, buf, len);
	kfiber_mutex_unlock(lock);
	//printf("write got=[%d]\n", got);
	if (got > 0) {
		write_offset += got;
		kfiber* fiber = wait_read;
		if (write_offset > read_offset && fiber != NULL) {			
			wait_read = NULL;
			//printf("write wakeup wait_read fiber=[%p]\n", fiber);
			kfiber_wakeup(fiber, this, 0);
		}
	}
	return got;
}
int KTempFile::Read(char* buf, int len)
{
	if (fp == NULL || wait_read) {
		return -1;
	}
	for (;;) {
		int64_t have_data = GetLeft();
		if (have_data <= 0) {
			if (write_is_end) {
				return 0;
			}
			wait_read = kfiber_self();
			//printf("no more data to read now wait write. fiber=[%p]\n", wait_read);
			kfiber_wait(this);
			continue;
		}
		assert(wait_read == NULL);
		len = (int)KGL_MIN((int64_t)len, have_data);
		kfiber_mutex_lock(lock);
		kfiber_file_seek(fp, seekBegin, read_offset);
		int got = kfiber_file_read(fp, buf, len);
		kfiber_mutex_unlock(lock);
		if (got > 0) {
			read_offset += got;
		}
		return got;
	}
	return -1;
}
int listHandleTempFile(const char *file,void *param)
{
	if (filencmp(file,"krf",3)!=0) {
		return 0;
	}
	int pid = atoi(file+3);
	if (pid == m_ppid) {
		return 0;
	}
	std::stringstream s;
	s << conf.tmppath << file;
	klog(KLOG_NOTICE,"remove uncleaned tmpfile [%s]\n",s.str().c_str());
	unlink(s.str().c_str());
	return 0;
}
KTHREAD_FUNCTION clean_tempfile_thread(void *param)
{
	klog(KLOG_DEBUG,"start to clean tmp file thread...\n");
	list_dir(conf.tmppath.c_str(),listHandleTempFile,NULL);
	klog(KLOG_DEBUG,"clean tmp file done.\n");
	KTHREAD_RETURN;
}

static int64_t tmpfile_input_get_read_left(kgl_input_stream* st, KREQUEST r)
{
	kgl_tempfile_input_stream* in = (kgl_tempfile_input_stream*)st;
	return in->tmp_file.GetLeft();
}
static int tmpfile_input_read(kgl_input_stream* st, KREQUEST r, char* buf, int len)
{
	kgl_tempfile_input_stream* in = (kgl_tempfile_input_stream*)st;
	return in->tmp_file.Read(buf, len);
}
static void tmpfile_input_release(kgl_input_stream* st)
{
	kgl_tempfile_input_stream* in = (kgl_tempfile_input_stream*)st;
	delete in;
}
static kgl_input_stream_function tempfile_input_stream_function = {
	tmpfile_input_get_read_left,
	tmpfile_input_read,
	tmpfile_input_release
};

bool new_tempfile_input_stream(KHttpRequest* rq, kgl_input_stream** in)
{
	if ((*in)->f->get_read_left(*in, rq) == 0) {
		return true;
	}
	kgl_tempfile_input_stream* st = new kgl_tempfile_input_stream;
	st->f = &tempfile_input_stream_function;
	st->up = *in;
	if (!st->tmp_file.Init()) {
		delete st;
		return false;
	}
	char* buf = (char*)malloc(TEMPFILE_POST_CHUNK_SIZE);
	bool result = false;
	while ((*in)->f->get_read_left(*in, rq) != 0) {
		int got = (*in)->f->read_body(*in, rq, buf, TEMPFILE_POST_CHUNK_SIZE);
		if (got < 0) {
			goto err;
		}
		if (got == 0) {
			break;
		}
		if (!st->tmp_file.WriteAll(buf, got)) {
			goto err;
		}
	}
	st->tmp_file.WriteEnd();
	*in = st;
	free(buf);
	return true;
err:
	delete st;
	free(buf);
	return false;
}
int tempfile_write_fiber(void* arg, int got)
{
	kgl_tempfile_output_stream* out = (kgl_tempfile_output_stream*)arg;
	KGL_RESULT result = KGL_OK;
	char* buf = (char*)malloc(8192);
	for (;;) {
		int len = out->tmp_file.Read(buf, 8192);
		if (len == 0) {
			break;
		}
		if (len < 0) {
			result = KGL_EIO;
			break;
		}
		KGL_RESULT ret = out->down_stream->f->write_body(out->down_stream, out->rq, buf, len);
		if (ret != KGL_OK) {
			out->tmp_file.Close();
			result = ret;
			break;
		}
	}
	free(buf);
	return (int)result;
}
static KGL_RESULT tempfile_write_body(kgl_output_stream* out, KREQUEST r, const char* buf, int len)
{
	kassert(!kfiber_is_main());
	kgl_tempfile_output_stream* tmp_out = (kgl_tempfile_output_stream*)out;
	if (!tmp_out->tmp_file.WriteAll(buf, len)) {
		return KGL_EIO;
	}
	if (tmp_out->write_fiber == NULL) {
		kfiber_create(tempfile_write_fiber, tmp_out, 0, conf.fiber_stack_size, &tmp_out->write_fiber);
		if (tmp_out->write_fiber == NULL) {
			return KGL_ESYSCALL;
		}
	}
	return KGL_OK;
	//return g->down_stream->f->write_body(g->down_stream, r, buf, len);
}
KGL_RESULT tempfile_write_end(kgl_output_stream* out, KREQUEST rq, KGL_RESULT result)
{
	kgl_tempfile_output_stream* tmp_out = (kgl_tempfile_output_stream*)out;
	tmp_out->tmp_file.WriteEnd();
	if (tmp_out->write_fiber) {
		int ret = -1;
		kfiber_join(tmp_out->write_fiber, &ret);
		tmp_out->write_fiber = NULL;
		if (result == KGL_OK) {
			result = (KGL_RESULT)ret;
		}
	}
	return tmp_out->down_stream->f->write_end(tmp_out->down_stream, rq, result);
}
static void tempfile_release(kgl_output_stream* out)
{
	kgl_tempfile_output_stream* tmp_out = (kgl_tempfile_output_stream*)out;
	assert(tmp_out->write_fiber == NULL);
	tmp_out->down_stream->f->release(tmp_out->down_stream);
	delete tmp_out;
}
static kgl_output_stream_function tempfile_output_function = {
	forward_write_status,
	forward_write_header,
	forward_write_unknow_header,
	forward_write_header_finish,
	tempfile_write_body,
	forward_write_message,
	tempfile_write_end,
	tempfile_release
};
bool new_tempfile_output_stream(KHttpRequest* rq, kgl_output_stream** out)
{
	kgl_tempfile_output_stream* st = new kgl_tempfile_output_stream;
	st->f = &tempfile_output_function;
	st->write_fiber = NULL;
	if (!st->tmp_file.Init()) {
		delete st;
		return false;
	}
	st->down_stream = *out;
	st->rq = rq;
	*out = st;
	return true;
}
#endif
