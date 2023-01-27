#include "KFetchBigObject.h"
#include "KFilterContext.h"

#ifdef ENABLE_BIG_OBJECT
kfiber_file* kgl_open_big_file(KHttpRequest* rq, KHttpObject* obj, INT64 start) {
	if (obj->dk.filename1 == 0) {
		char* url = obj->uk.url->getUrl();
		klog(KLOG_ERR, "BUG bigobj filename1 is 0 url=[%s]\n", url);
		free(url);
		return NULL;
	}
	char* filename = obj->get_filename();
	if (filename == NULL) {
		return NULL;
	}
	kfiber_file* fp = kfiber_file_open(filename, fileRead, 0);
	if (fp == NULL) {
		obj->Dead();
		char* url = obj->uk.url->getUrl();
		klog(KLOG_ERR, "cann't open bigobj cache file [%s] url=[%s]\n", filename, url);
		xfree(filename);
		xfree(url);
		return NULL;
	}
	xfree(filename);
	kfiber_file_seek(fp, seekBegin, (INT64)obj->index.head_size + start);
	return fp;
}
#endif

