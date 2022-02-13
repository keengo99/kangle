//{{ent
#include "KLicense.h"
#include "do_config.h"
#include "KVirtualHost.h"
#include "KFileName.h"
#include "KLineFile.h"
#include "utils.h"
#include "log.h"
#include "KGzip.h"
#include "lib.h"
#include "KSimulateRequest.h"
using namespace std;
#ifndef KANGLE_FREE
KLicense license;
volatile vl_result license_result = vl_success;
struct sync_download_param {
	KCondWait cw;
	int code;
};
void kgl_check_license(void *arg, time_t nowTime)
{
	if (!license.CheckOrDownload()) {
		klog(KLOG_ERR, "license expired.\n");
		my_exit(100);
	}
}
kev_result result_license_download(KOPAQUE data, void *arg,int got)
{
	sync_download_param *param = (sync_download_param *)arg;
	param->code = got;
	param->cw.notice();
	return kev_ok;
}
bool KLicense::LoadOrDownload()
{
	return Load();
}
bool KLicense::Load() {
	std::stringstream file;
	file << conf.path << "license.txt";
	return load(file.str().c_str());
}
bool KLicense::load(const char *filename)
{
	KFileName file;
	if (!file.setName(filename)) {
		fprintf(stderr, "cann't read license file [%s]\n",filename);
		return false;
	}
	if (file.fileSize > 1024 * 1024 || file.fileSize < 10) {
		fprintf(stderr, "licse error=1\n");
		return false;
	}
	FILE *fp = fopen(file.getName(), "rb");
	if (fp == NULL) {
		fprintf(stderr, "cann't read license file2 [%s]\n",filename);
		return false;
	}
	char *buf = (char *) xmalloc((int)file.fileSize+1);
	int len = fread(buf, 1, (int)file.fileSize, fp);
	fclose(fp);
	if (len <= 0) {
		xfree(buf);
		fprintf(stderr, "cann't read license file\n");
		return false;
	}
	buf[len] = '\0';
	char *hot = strchr(buf,'\n');
	if(hot==NULL){
		fprintf(stderr,"license file format is error\n");
		return false;
	}
	*hot = '\0';
	int license_version = atoi(buf);
	if(license_version!=LICENSE_VERSION){
		fprintf(stderr,"license file version is error\n");
		return false;
	}
	hot++;
	char *dst = buf;
	while (*hot) {
		if (!isspace((unsigned char) *hot)) {
			*dst = *hot;
			dst++;
		}
		hot++;
	}
	*dst = '\0';
	//printf("dst=[%s]\n", buf);
	//	int len = strlen(buf);
	len = strlen(buf);
	char *msg = b64decode((unsigned char *) buf, &len);
	xfree(buf);
	KStringBuf gs;
	KBridgeStream2 gs2(&gs, false);
	for(int i=0;i<3;i++){
		if (msg == NULL) {
			fprintf(stderr, "license error=2\n");
			return false;
		}
		gs.init(1024);
		KGzipDecompress *degzip = new KGzipDecompress(false,&gs2,false);
		degzip->write_all(NULL, msg,len);
		degzip->write_end(NULL, KGL_OK);
		delete degzip;
		xfree(msg);
		len = gs.getSize();
		msg = gs.stealString();
	}
	if (msg == NULL) {
		//fprintf(stderr, "license error=2\n");
		return false;
	}
	KLineFile lf;
	lf.give(msg);
	char *line;
	line = lf.readLine();
	if (line == NULL) {
		//fprintf(stderr, "license error=3\n");
		return false;
	}
	line = lf.readLine();
	if (line == NULL) {
		//fprintf(stderr, "license error=3\n");
		return false;
	}
	license.id = line;
	line = lf.readLine();
	if (line == NULL) {
		//fprintf(stderr, "license error=4\n");
		return false;
	}
	license.name = line;
	line = lf.readLine();
	if (line == NULL) {
		//fprintf(stderr, "license error=5\n");
		return false;
	}
	license.ips = line;
	for(;;){
		char *p = strchr(line,',');
		if(p){
			*p = '\0';
		}
		license.ip.push_back(line);
		if (p==NULL) {
			break;
		}
		line = p+1;
	}
	line = lf.readLine();
	if (line == NULL) {
		//fprintf(stderr, "license error=6\n");
		return false;
	}
	license.expireTime = string2int(line);

	line = lf.readLine();
	if (line == NULL) {
		//fprintf(stderr, "license error=7\n");
		return false;
	}
	license.cpu = atoi(line);

	line = lf.readLine();
	if (line == NULL) {
		//fprintf(stderr, "license error=8\n");
		return false;
	}
	license.passwd = line;
	for (int i=0;i<2;i++) {
		line = lf.readLine();
		if (line == NULL) {
			//fprintf(stderr, "license error=9\n");
			return false;
		}
		strncpy(license.sn[i], line, sizeof(license.sn[i]));
	}
	if (license.expireTime>0 && time(NULL) > license.expireTime) {
		return false;
	}
	loaded = true;
	return true;
}
#if 0
bool KLicense::download()
{
	std::stringstream s;
	s << "https://vl2.kangleweb.net/user/?c=console&a=getLicense";
	if (license.id.size() > 0) {
		std::stringstream r;
		r << time(NULL) << rand();
		char sign[33];
		std::stringstream p;
		p << license.id << r.str() << license.passwd;
		KMD5(p.str().c_str(), p.str().size(), sign);
		s << "&license_id=" << license.id << "&r=" << r.str() << "&s=" << sign;
	}
	std::stringstream filename;
	filename << conf.path << PATH_SPLIT_CHAR << "license.txt";
	std::stringstream filenameTmp;
	filenameTmp << filename.str() << ".tmp";
	sync_download_param param;
	param.code = 0;
	async_download(s.str().c_str(), filenameTmp.str().c_str(), result_license_download, &param);
	param.cw.wait();
	if (param.code != 200) {
		unlink(filenameTmp.str().c_str());
		return false;
	}
	unlink(filename.str().c_str());
	rename(filenameTmp.str().c_str(), filename.str().c_str());
	return this->Load();
}
#endif
#endif
//}}
