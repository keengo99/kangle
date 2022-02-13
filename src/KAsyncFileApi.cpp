#include "kasync_file.h"
#include "kfile.h"
#include "kmalloc.h"
#include "katom.h"
#include "KAsyncFileApi.h"
typedef struct _kgl_aio_file
{
	kasync_file fp;
	result_callback result;
	int64_t offset;
} kgl_aio_file;

static kev_result aio_file_callback(kasync_file *fp, void *arg, char *buf, int length)
{
	kgl_aio_file *af = (kgl_aio_file *)fp;
	if (length > 0) {
		af->offset += length;
	}
	return af->result(kasync_file_get_opaque(fp), arg, length);
}
static KASYNC_FILE aio_open(const char *filename, fileModel model, int kf_flags)
{
	kselector *selector = kgl_get_tls_selector();
	if (selector == NULL) {
		return NULL;
	}
	FILE_HANDLE fp = kfopen(filename, model, kf_flags|KFILE_ASYNC);
	if (!kflike(fp)) {
		return NULL;
	}
	kgl_aio_file *af = (kgl_aio_file *)xmemory_newz(sizeof(kgl_aio_file));
	kgl_selector_module.aio_open(selector, &af->fp, fp);
	return af;
}
static void aio_set_opaque(KASYNC_FILE fp, KOPAQUE data)
{
	kasync_file_bind_opaque((kasync_file *)fp, data);
}
static void aio_close(KASYNC_FILE fp)
{
	kgl_aio_file *af = (kgl_aio_file *)fp;
	kasync_file_close(&af->fp);
	xfree(af);
}
static bool aio_seek(KASYNC_FILE fp, seekPosion pos, int64_t offset)
{
	kgl_aio_file *af = (kgl_aio_file *)fp;
	switch (pos) {
	case seekBegin:
		af->offset = offset;
		return true;
	case seekCur:
		af->offset += offset;
		return true;
	default:
		return false;
	}
}
static int64_t aio_get_position(KASYNC_FILE fp)
{
	kgl_aio_file *af = (kgl_aio_file *)fp;
	return af->offset;
}
kev_result aio_write(KASYNC_FILE fp, result_callback result, buffer_callback buffer, void *arg)
{
	kgl_aio_file *af = (kgl_aio_file *)fp;
	WSABUF buf;
	buffer(kasync_file_get_opaque(&af->fp), arg, &buf, 1);
	af->result = result;
	if (!kgl_selector_module.aio_write(kasync_file_get_selector(&af->fp),
		&af->fp,
		(char *)buf.iov_base,
		af->offset,
		buf.iov_len,
		aio_file_callback,
		arg)) {
		return result(kasync_file_get_opaque(&af->fp), arg, -1);
	}
	return kev_ok;

}
kev_result aio_read(KASYNC_FILE fp, result_callback result, buffer_callback buffer, void *arg)
{
	kgl_aio_file *af = (kgl_aio_file *)fp;
	WSABUF buf;
	buffer(kasync_file_get_opaque(&af->fp), arg, &buf, 1);
	af->result = result;
	if (!kgl_selector_module.aio_read(kasync_file_get_selector(&af->fp),
		&af->fp,
		(char *)buf.iov_base,
		af->offset,
		buf.iov_len,
		aio_file_callback,
		arg)) {
		return result(kasync_file_get_opaque(&af->fp), arg, -1);
	}
	return kev_ok;
}
static KSELECTOR aio_get_selector(KASYNC_FILE fp)
{
	kgl_aio_file *af = (kgl_aio_file *)fp;
	return kasync_file_get_selector(&af->fp);
}
kgl_async_file_function async_file_provider = {
	aio_open,
	aio_set_opaque,
	aio_close,
	aio_seek,
	aio_get_position,
	aio_get_selector,
	aio_write,
	aio_read
};
