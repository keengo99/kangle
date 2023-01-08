#include "kasync_file.h"
#include "kfile.h"
#include "kmalloc.h"
#include "katom.h"
#include "kfiber.h"
#include "KAsyncFileApi.h"

static KSELECTOR aio_get_selector(KASYNC_FILE fp)
{
	kfiber_file *af = (kfiber_file*)fp;
	return kasync_file_get_selector(af);
}
static FILE_HANDLE aio_get_handle(KASYNC_FILE fp)
{
	kfiber_file* af = (kfiber_file*)fp;
	return kasync_file_get_handle(af);
}
static const char* adjust_read_buffer(KASYNC_FILE fp, char* buf)
{
	kfiber_file *af = (kfiber_file *)fp;
	return kfiber_file_adjust(af, buf);
}
kgl_file_function async_file_provider = {
	(KASYNC_FILE(*)(const char* , fileModel ,int))kfiber_file_open,
	aio_get_selector,
	aio_get_handle,
	(void(*)(KASYNC_FILE))kfiber_file_close,
	(int(*)(KASYNC_FILE ,seekPosion , int64_t))kfiber_file_seek,
	(int64_t(*)(KASYNC_FILE))kfiber_file_tell,
	(int64_t(*)(KASYNC_FILE))kfiber_file_size,
	(int (*)(KASYNC_FILE , char* , int))kfiber_file_read,
	(int (*)(KASYNC_FILE , const char* , int))kfiber_file_write,
	adjust_read_buffer,
	aio_alloc_buffer,
	aio_free_buffer
};
