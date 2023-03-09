#include "KFetchBigObject.h"
#include "KFilterContext.h"

#ifdef ENABLE_BIG_OBJECT
kfiber_file* kgl_open_big_file(KHttpRequest* rq, KHttpObject* obj, INT64 start) {
	if (obj->dk.filename1 == 0) {
		auto url = obj->uk.url->getUrl();
		klog(KLOG_ERR, "BUG bigobj filename1 is 0 url=[%s]\n", url.get());
		return NULL;
	}
	auto filename = obj->get_filename();
	if (filename == NULL) {
		return NULL;
	}
	kfiber_file* fp = kfiber_file_open(filename.get(), fileRead, 0);
	if (fp == NULL) {
		obj->Dead();
		auto url = obj->uk.url->getUrl();
		klog(KLOG_ERR, "cann't open bigobj cache file [%s] url=[%s]\n", filename.get(), url.get());
		return NULL;
	}
	kfiber_file_seek(fp, seekBegin, (INT64)obj->index.head_size + start);
	return fp;
}
#endif

