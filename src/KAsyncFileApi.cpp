#include "kasync_file.h"
#include "kfile.h"
#include "kmalloc.h"
#include "katom.h"
#include "kfiber.h"
#include "KAsyncFileApi.h"

static const char* adjust_read_buffer(KASYNC_FILE fp, char* buf)
{
	kasync_file *af = (kasync_file *)fp;
	return kfiber_file_adjust(af, buf);
}
static bool file_api_direct(KASYNC_FILE fp,bool on_flag)
{
	return kasync_file_direct((kasync_file *)fp,on_flag);
}
kgl_file_function async_file_provider = {
	(KASYNC_FILE(*)(const char* , fileModel ,int))kfiber_file_open,
	(KSELECTOR (*)(KASYNC_FILE ))kasync_file_get_selector,
	(FILE_HANDLE (*)(KASYNC_FILE))kasync_file_get_handle,
	(void(*)(KASYNC_FILE))kfiber_file_close,
	(int(*)(KASYNC_FILE ,seekPosion , int64_t))kfiber_file_seek,
	(int64_t(*)(KASYNC_FILE))kfiber_file_tell,
	(int64_t(*)(KASYNC_FILE))kfiber_file_size,
	(int (*)(KASYNC_FILE , char* , int))kfiber_file_read,
	(int (*)(KASYNC_FILE , const char* , int))kfiber_file_write,
	adjust_read_buffer,
	file_api_direct,
	aio_alloc_buffer,
	aio_free_buffer
};
