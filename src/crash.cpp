#include "kforwin32.h"
#include <sstream>
#include <string.h>
#include "do_config.h"
#include "directory.h"
#include "utils.h"
#include "zlib.h"
#include "KSimulateRequest.h"

//{{ent
#ifdef _WIN32
#pragma comment(lib,"netapi32.lib")
#endif
#include "KLicense.h"
//}}
using namespace std;
struct kgl_upload_context
{
	KFile fp;
	std::string filename;
	std::string bounder;
	kgl_pool_t *pool;
	int state;
	char *data;
	int data_left;
};
static kev_result upload_read(void *async_http_arg, KOPAQUE data, void *arg, result_callback result, buffer_callback buffer)
{
	kgl_upload_context *file_ctx = (kgl_upload_context *)async_http_arg;
	//printf("file [%s] read\n", file_ctx->filename.c_str());
	WSABUF buf;
	int bc = buffer(data, arg, &buf, 1);
	if (bc != 1) {
		return result(data, arg, -1);
	}
restart:
	if (file_ctx->state==0 || file_ctx->state==2) {
		int got = MIN(file_ctx->data_left, (int)buf.iov_len);
		memcpy(buf.iov_base, file_ctx->data, got);
		file_ctx->data += got;
		file_ctx->data_left -= got;
		if (file_ctx->data_left == 0) {
			file_ctx->state++;
		}
		return result(data, arg, got);
	}
	if (file_ctx->state==1) {
		if (file_ctx->fp.opened()) {
			int got = file_ctx->fp.read((char *)buf.iov_base, buf.iov_len);
			if (got > 0) {
				return result(data, arg, got);
			}
			file_ctx->fp.close();			
		}
		file_ctx->state++;
		file_ctx->data = (char *)kgl_pnalloc(file_ctx->pool, file_ctx->bounder.size() + 8);
		char *hot = file_ctx->data;
		memcpy(hot, kgl_expand_string("\r\n--"));
		hot += 4;
		memcpy(hot, file_ctx->bounder.c_str(), file_ctx->bounder.size());
		hot += file_ctx->bounder.size();
		memcpy(hot, kgl_expand_string("--\r\n"));
		file_ctx->data_left = file_ctx->bounder.size() + 8;
		goto restart;
	}
	return result(data, arg, -1);
}
static kev_result upload_write(void *async_http_arg, KOPAQUE data, void *arg, result_callback result, buffer_callback buffer)
{
	WSABUF buf;
	int bc = buffer(data, arg, &buf, 1);
	if (bc != 1) {
		return result(data, arg, -1);
	}
	fwrite(buf.iov_base, 1, buf.iov_len, stdout);
	return result(data, arg, buf.iov_len);
}
static void upload_close(void *async_http_arg, int exptected_done)
{

	kgl_upload_context *file_ctx = (kgl_upload_context *)async_http_arg;
	//printf("file [%s] closed\n", file_ctx->filename.c_str());
	file_ctx->fp.close();
	unlink(file_ctx->filename.c_str());
	kgl_destroy_pool(file_ctx->pool);
	delete file_ctx;
}
void upload_dmp_file(const char *file,const char *url)
{
	//printf("upload core file =[%s]\n",file);
	//kgl_upload_context *file_ctx = new kgl_upload_context;
	KFile fp;
    if (!fp.open(file,fileRead)) {
		return ;
    }
	stringstream gz_file_name;
	gz_file_name << file << ".gz";
	gzFile gz = gzopen(gz_file_name.str().c_str(),"wb");
	if (gz==NULL) {
		return;
	}
	char buf[8192];
	for (;;) {
		int n = fp.read(buf,sizeof(buf));
		if (n<=0) {
			break;
		}
		if (gzwrite(gz,buf,n)<0) {
			break;
		}
	}
	gzclose(gz);
	kgl_upload_context *file_ctx = new kgl_upload_context;
	file_ctx->pool = kgl_create_pool(512);
	file_ctx->filename = gz_file_name.str();
	file_ctx->state = 0;
	if (!file_ctx->fp.open(file_ctx->filename.c_str(), fileRead)) {
		delete file_ctx;
		return;
	}
	kgl_async_http ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.arg = file_ctx;
	/*
	ctx.read = upload_read;
	ctx.write = upload_write;
	ctx.close = upload_close;
	*/
	ctx.meth = (char *)"POST";
	ctx.url = (char *)url;
	ctx.queue = "upload_1_0";
	stringstream s;
	std::stringstream bounder;
	bounder << "---------------------------4664151417711";
	bounder << rand();
	file_ctx->bounder = bounder.str();


	s << "--" << file_ctx->bounder << "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"" << file_ctx->filename << "\"\r\n";
	s << "Content-Type: text/plain" << "\r\n\r\n";
	file_ctx->data_left = s.str().size();
	file_ctx->data = (char *)kgl_pnalloc(file_ctx->pool, file_ctx->data_left);
	memcpy(file_ctx->data, s.str().c_str(), file_ctx->data_left);
	
	s.str("");
	s << "multipart/form-data; boundary=" << file_ctx->bounder;
	KHttpHeader *header = new_pool_http_header(file_ctx->pool, kgl_expand_string("Content-Type"), s.str().c_str(), s.str().size());
	ctx.rh = header;
	ctx.post_len = (int)file_ctx->fp.getFileSize() + file_ctx->data_left + file_ctx->bounder.size()+8;
	if (kgl_simuate_http_request(&ctx) != 0) {
		upload_close((void *)file_ctx, -1);
	}
}
int crash_report(const char *file,void *param)
{
	char *path = (char *)param;
#ifdef _WIN32
	const char *p = strrchr(file,'.');
	if(p==NULL){
		return 0;
	}
	if(!(strcasecmp(p+1,"leak")==0 || strcasecmp(p+1,"dmp")==0)){
		return 0;
	}
#else
	if (strncasecmp(file,"core",4)!=0) {
		return 0;
	}
#endif
	stringstream s;
	s << path << file;
	stringstream u;
	u << "http://crash.kangleweb.net/v2.php?version=" << VERSION << "&type=" << getServerType() << "&f=";
	upload_dmp_file(s.str().c_str(),u.str().c_str());
	unlink(s.str().c_str());
	return 0;
}
KTHREAD_FUNCTION crash_report_thread(void* arg)
{
#ifdef _WIN32
	char buf[512];
	::GetModuleFileName(NULL, buf, sizeof(buf)-1);
	char *p = strrchr(buf,'\\');
	if (p) {
		p++;
		*p = '\0';
	}
#else
	const char *buf = conf.path.c_str();
#endif
	list_dir(buf,crash_report,(void *)buf);
	KTHREAD_RETURN;
}