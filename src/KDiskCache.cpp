#include <string.h>
#include <stdlib.h>
#include <string>

#include "KDiskCache.h"
#include "do_config.h"
#include "directory.h"
#include "cache.h"
#include "KHttpLib.h"
#include "KHttpObject.h"
#include "KHttpObjectHash.h"
#include "kmalloc.h"
#include "KObjectList.h"
#include "http.h"
#include "kfile.h"
#ifdef ENABLE_DB_DISK_INDEX
#include "KDiskCacheIndex.h"
#include "KSqliteDiskCacheIndex.h"
#endif
#ifdef ENABLE_DISK_CACHE
#ifdef LINUX
#include <sys/vfs.h>
#endif

#ifdef ENABLE_BIG_OBJECT_206
static std::list<std::string> partfiles;
static std::map<std::string, bool> partobjs;
#endif
//扫描进程是否存在
volatile bool index_progress = false;
index_scan_state_t index_scan_state;
static int load_count = 0;
static INT64 recreate_start_time = 0;
using namespace std;
bool obj_can_disk_cache(KHttpRequest* rq, KHttpObject* obj)
{
	if (KBIT_TEST(rq->filter_flags, RF_NO_DISK_CACHE)) {
		return false;
	}
	if (KBIT_TEST(obj->index.flags, ANSW_LOCAL_SERVER | FLAG_NO_DISK_CACHE)) {
		return false;
	}
	return cache.IsDiskCacheReady();
}
bool kgl_dc_skip_string(char** hot, int& hotlen)
{
	if (hotlen <= (int)sizeof(int)) {
		return false;
	}
	int len;
	kgl_memcpy(&len, *hot, sizeof(int));
	(*hot) += sizeof(int);
	hotlen -= sizeof(int);
	if (hotlen <= len) {
		return false;
	}
	(*hot) += len;
	hotlen -= len;
	return true;
}
bool kgl_dc_skip_string(KFile* file)
{
	int len;
	if (file->read((char*)&len, sizeof(len)) != sizeof(len)) {
		return false;
	}
	return file->seek(len, seekCur);
}
char* kgl_dc_read_string(char** hot, int& hotlen, int& len)
{
	if (hotlen < (int)sizeof(int)) {
		len = -1;
		return NULL;
	}
	kgl_memcpy(&len, *hot, sizeof(int));
	hotlen -= sizeof(int);
	(*hot) += sizeof(int);
	if (len < 0 || len>131072) {
		klog(KLOG_ERR, "string len[%d] is too big\n", len);
		len = -1;
		return NULL;
	}
	if (hotlen < len) {
		len = -1;
		return NULL;
	}
	char* buf = (char*)xmalloc(len + 1);
	buf[len] = '\0';
	if (len > 0) {
		kgl_memcpy(buf, *hot, len);
		hotlen -= len;
		(*hot) += len;
	}
	return buf;
}
char* kgl_dc_read_string(KFile* file, int& len)
{
	if (file->read((char*)&len, sizeof(len)) != sizeof(len)) {
		len = -1;
		return NULL;
	}
	if (len < 0 || len>131072) {
		len = -1;
		klog(KLOG_ERR, "string len[%d] is too big\n", len);
		return NULL;
	}
	char* buf = (char*)xmalloc(len + 1);
	buf[len] = '\0';
	if (len > 0 && (int)file->read(buf, len) != len) {
		xfree(buf);
		len = -1;
		return NULL;
	}
	return buf;
}
int kgl_dc_write_string(char* hot, const char* str, int len)
{
	kgl_memcpy(hot, (char*)&len, sizeof(int));
	if (len > 0) {
		kgl_memcpy(hot + sizeof(int), str, len);
	}
	return len + sizeof(int);
}
int kgl_dc_write_string(KBufferFile* file, const char* str, int len)
{
	if (str) {
		if (len == 0) {
			len = (int)strlen(str);
		}
	}
	int ret = file->write((char*)&len, sizeof(len));
	if (len > 0) {
		ret += file->write(str, len);
	}
	return ret;
}
bool read_obj_head(KHttpObjectBody* data, char** hot, int& hotlen)
{
	assert(data->headers == NULL);
	KHttpHeader* last = NULL;
	for (;;) {
		int buf_len;
		//printf("before attr hotlen=[%d]\n",hotlen);
		char* buf = kgl_dc_read_string(hot, hotlen, buf_len);
		//printf("after attr before val hotlen=[%d]\n",hotlen);
		if (buf_len == -1) {
			return false;
		}
		if (buf == NULL) {
			return true;
		}
		if (hotlen < sizeof(uint32_t)) {
			xfree(buf);
			return false;
		}

		//printf("val=[%s]\n",val);
		KHttpHeader* header = (KHttpHeader*)xmalloc(sizeof(KHttpHeader));
		if (header == NULL) {
			xfree(buf);
			return false;
		}
		header->buf = buf;
		header->name_attribute = *(uint32_t*)(*hot);
		*hot += sizeof(uint32_t);
		hotlen -= sizeof(uint32_t);

		if (!header->name_is_know) {
			header->val_len = buf_len - header->name_len - 2;
			header->val_offset = header->name_len + 1;
			if (header->know_header>=kgl_header_unknow) {
				klog(KLOG_ERR,"invalid know_header value=[%d]\n",header->know_header);
				xfree_header(header);
				return false;
			}
		} else {
			header->val_len = buf_len - 1;
			header->val_offset = 0;
		}
		if (last == NULL) {
			data->headers = header;
		} else {
			last->next = header;
		}
		last = header;
	}
}
char* getCacheIndexFile()
{
	KStringBuf s;
	if (*conf.disk_cache_dir) {
		s << conf.disk_cache_dir;
	} else {
		s << conf.path << "cache" << PATH_SPLIT_CHAR;
	}
	s << "index";
	return s.stealString();
}
void get_index_scan_state_filename(KStringBuf& s)
{
	if (*conf.disk_cache_dir) {
		s << conf.disk_cache_dir;
	} else {
		s << conf.path << "cache" << PATH_SPLIT_CHAR;
	}
	s << "index.scan";
}
bool save_index_scan_state()
{
	KStringBuf s;
	get_index_scan_state_filename(s);
	KFile fp;
	if (!fp.open(s.getString(), fileWrite)) {
		return false;
	}
	bool result = true;
	if (sizeof(index_scan_state_t) != fp.write((char*)&index_scan_state, sizeof(index_scan_state_t))) {
		result = false;
	}
	fp.close();
	return result;
}
bool load_index_scan_state()
{
	KStringBuf s;
	get_index_scan_state_filename(s);
	KFile fp;
	if (!fp.open(s.getString(), fileRead)) {
		return false;
	}
	bool result = true;
	if (sizeof(index_scan_state_t) != fp.read((char*)&index_scan_state, sizeof(index_scan_state_t))) {
		result = false;
	}
	fp.close();
	return result;
}
int get_index_scan_progress()
{
	return (index_scan_state.first_index * 100) / CACHE_DIR_MASK1;
}
bool saveCacheIndex()
{
	klog(KLOG_ERR, "save cache index now...\n");
#ifndef NDEBUG
	cache.flush_mem_to_disk();
#endif
	cache.syncDisk();
#ifdef ENABLE_DB_DISK_INDEX
	if (dci) {
		dci->start(ci_close, NULL);
		while (!dci->allWorkedDone()) {
			my_msleep(100);			
		}
		
	}
#endif
	klog(KLOG_ERR,"save cache index done.\n");
	return true;
}
cor_result create_http_object2(KHttpObject* obj, char* url, uint32_t flag_encoding, const char* verified_filename)
{
	char* vary = strchr(url, '\n');
	if (vary != NULL) {
		*vary = '\0';
		vary++;
	}
	KAutoUrl m_url;
	if (!parse_url(url, m_url.u) || m_url.u->host == NULL) {
		fprintf(stderr, "cann't parse url[%s]\n", url);
		return cor_failed;
	}
	KBIT_CLR(obj->index.flags, FLAG_IN_MEM);
	KBIT_SET(obj->index.flags, FLAG_IN_DISK);
	assert(obj->uk.url == NULL);
	obj->uk.url = m_url.u->refs();
	obj->uk.url->flag_encoding = flag_encoding;
	if (vary) {
		char* vary_val = strchr(vary, '\n');
		if (vary_val) {
			*vary_val = '\0';
			vary_val++;
			obj->uk.vary = new KVary;
			obj->uk.vary->key = strdup(vary);
			obj->uk.vary->val = strdup(vary_val);
		}
	}
	if (verified_filename) {
		obj->h = cache.hash_url(obj->uk.url);
		if (cache.getHash(obj->h)->FindByFilename(&obj->uk, verified_filename)) {
			KBIT_CLR(obj->index.flags, FLAG_IN_DISK);
			klog(KLOG_NOTICE, "filename [%s] already in cache\n", verified_filename);
			return cor_incache;
		}
	}
	if (KBIT_TEST(obj->index.flags, FLAG_IN_DISK)) {
		if (obj->index.head_size != kgl_align(obj->index.head_size, kgl_aio_align_size)) {
			char* url = obj->uk.url->getUrl();
			char* filename = obj->getFileName();
			klog(KLOG_ERR, "disk cache file head_size=[%d] is not align by size=[%d], url=[%s] file=[%s] now dead it.\n", obj->index.head_size, kgl_aio_align_size, url, filename);
			free(filename);
			free(url);
			KBIT_SET(obj->index.flags, FLAG_DEAD);
		}
#ifndef ENABLE_BIG_OBJECT_206
		if (KBIT_TEST(obj->index.flags, FLAG_BIG_OBJECT_PROGRESS)) {
			char* url = obj->uk.url->getUrl();
			char* filename = obj->getFileName();
			klog(KLOG_ERR, "disk cache file head_size=[%d] is part file that is not support by now size=[%d], url=[%s] file=[%s]. now dead it.\n", obj->index.head_size, kgl_aio_align_size, url, filename);
			free(filename);
			free(url);
			KBIT_SET(obj->index.flags, FLAG_DEAD);
		}
#endif
	}
	if (stored_obj(obj, LIST_IN_DISK)) {
		return cor_success;
	}
	KBIT_CLR(obj->index.flags, FLAG_IN_DISK);
	return cor_failed;
}
cor_result create_http_object(KHttpObject* obj, const char* url, uint32_t flag_encoding, const char* verified_filename)
{
	//printf("url=[%s]\n", url);
	char* buf = strdup(url);
	cor_result ret = create_http_object2(obj, buf, flag_encoding, verified_filename);
	xfree(buf);
	return ret;
}
cor_result create_http_object(KFile* fp, KHttpObject* obj, uint32_t url_flag_encoding, const char* verified_filename = NULL)
{
	int len;
	char* url = kgl_dc_read_string(fp, len);
	if (url == NULL) {
		fprintf(stderr, "read url is NULL\n");
		return cor_failed;
	}
	cor_result ret = create_http_object2(obj, url, url_flag_encoding, verified_filename);
	free(url);
	return ret;
}
int create_file_index(const char* file, void* param)
{
	KStringBuf s;
	cor_result result = cor_failed;
	KHttpObject* obj;
	s << (char*)param << PATH_SPLIT_CHAR << file;
	unsigned f1 = 0;
	unsigned f2 = 0;
	char* file_name = s.getString();
	KFile fp;
	if (!fp.open(s.getString(), fileRead)) {
		fprintf(stderr, "cann't open file[%s]\n", s.getString());
		return 0;
	}
	if (recreate_start_time > 0) {
		INT64 t = fp.getCreateTime();
		if (t > recreate_start_time) {
			klog(KLOG_DEBUG, "file [%s] is new file t=%d\n", file_name, (int)(t - recreate_start_time));
			return 0;
		}
	}
	if (strstr(file, ".part")) {
#ifdef ENABLE_BIG_OBJECT_206
		std::string obj_file = file;
		obj_file = obj_file.substr(0, obj_file.size() - 5);
		partfiles.push_back(obj_file);
		return 0;
#endif
		goto failed;
	}
	if (sscanf(file, "%x_%x", &f1, &f2) != 2) {
		goto failed;
	}
	KHttpObjectFileHeader header;
	if (fp.read((char*)&header, sizeof(KHttpObjectFileHeader)) != sizeof(KHttpObjectFileHeader)) {
		fprintf(stderr, "cann't read head size [%s]\n", file_name);
		goto failed;
	}
	if (!is_valide_dc_head_size(header.dbi.index.head_size)) {
		klog(KLOG_ERR, "disk cache [%s] head_size=[%d] is not valide\n", file_name, header.dbi.index.head_size);
		goto failed;
	}
	if (!is_valide_dc_sign(&header)) {
		klog(KLOG_ERR, "disk cache [%s] is not valide file\n", file_name);
		goto failed;
	}
	/*
	if (header.body_complete) {
		klog(KLOG_ERR, "disk cache [%s] is not complete.\n", file_name);
		goto failed;
	}
	*/
	obj = new KHttpObject;
	kgl_memcpy(&obj->index, &header.dbi.index, sizeof(obj->index));
	obj->dk.filename1 = f1;
	obj->dk.filename2 = f2;
#ifdef ENABLE_BIG_OBJECT_206
	if (KBIT_TEST(obj->index.flags, FLAG_BIG_OBJECT_PROGRESS)) {
		partobjs[file] = true;
	}
#endif
	result = create_http_object(&fp, obj, header.dbi.url_flag_encoding, file_name);
	if (result == cor_success) {
#ifdef ENABLE_DB_DISK_INDEX
		if (dci) {
			dci->start(ci_add, obj);
		}
#endif
		load_count++;
	}
	obj->release();
failed:
	fp.close();
	if (result == cor_failed) {
		klog(KLOG_NOTICE, "create http object failed,remove file[%s]\n", file_name);
		unlink(file_name);
	}
	return 0;
}
void clean_disk_orphan_files(const char* cache_dir)
{
	//{{ent
#ifdef ENABLE_BIG_OBJECT_206
	std::list<std::string>::iterator it;
	for (it = partfiles.begin(); it != partfiles.end(); it++) {
		KFile partFile;
		std::map<std::string, bool>::iterator it2 = partobjs.find((*it));
		if (it2 == partobjs.end()) {
			KStringBuf p;
			p << cache_dir << PATH_SPLIT_CHAR << (*it).c_str() << ".part";
			klog(KLOG_DEBUG, "part file [%s] is orphan,now delete it.\n", p.getString());
			unlink(p.getString());
		}
	}
	partfiles.clear();
	partobjs.clear();
#endif//}}
}
void recreate_index_dir(const char* cache_dir)
{
	klog(KLOG_NOTICE, "scan cache dir [%s]\n", cache_dir);
	//{{ent
#ifdef ENABLE_BIG_OBJECT_206
	partfiles.clear();
	partobjs.clear();
#endif//}}
	list_dir(cache_dir, create_file_index, (void*)cache_dir);
	clean_disk_orphan_files(cache_dir);
}
bool recreate_index(const char* path, int& first_dir_index, int& second_dir_index, KTimeMatch* tm = NULL)
{
	KStringBuf s;
	for (; first_dir_index <= CACHE_DIR_MASK1; first_dir_index++) {
		for (; second_dir_index <= CACHE_DIR_MASK2; second_dir_index++) {
			if (tm && !tm->checkTime(time(NULL))) {
				return false;
			}
			s << path;
			s.addHex(first_dir_index);
			s << PATH_SPLIT_CHAR;
			s.addHex(second_dir_index);
			recreate_index_dir(s.getString());
			s.clean();
			save_index_scan_state();
		}
		second_dir_index = 0;
	}
	return true;
}
void recreate_index(time_t start_time)
{
	if (index_progress) {
		return;
	}
	recreate_start_time = start_time;
	index_progress = true;
	klog(KLOG_ERR, "now recreate the index file,It may be use more time.Please wait...\n");
	string path;
	if (*conf.disk_cache_dir) {
		path = conf.disk_cache_dir;
	} else {
		path = conf.path;
		path += "cache";
		path += PATH_SPLIT_CHAR;
	}
	KStringBuf s;
	load_count = 0;
	int i = 0;
	int j = 0;
	recreate_index(path.c_str(), i, j, NULL);
	klog(KLOG_ERR, "create index done. total find %d object.\n", load_count);
	index_progress = false;
}
void init_disk_cache(bool firstTime)
{
	//printf("sizeof(KHttpObjectFileHeader)=%d\n", sizeof(KHttpObjectFileHeader));
#ifdef ENABLE_SQLITE_DISK_INDEX
	if (dci) {
		return;
	}
	memset(&index_scan_state, 0, sizeof(index_scan_state));
	load_index_scan_state();
	dci = new KSqliteDiskCacheIndex;
	char* file_name = getCacheIndexFile();
	KStringBuf sqliteIndex;
	//remove old version sqt
	bool remove_old_index = false;
	for (int i = 1; i < CACHE_DISK_VERSION; i++) {
		sqliteIndex << file_name << ".sqt";
		if (i > 1) {
			sqliteIndex << i;
		}
		if (0 == unlink(sqliteIndex.getString())) {
			remove_old_index = true;
		}
		sqliteIndex.clean();
	}
	if (remove_old_index) {
		rescan_disk_cache();
	}
	sqliteIndex << file_name << ".sqt" << CACHE_DISK_VERSION;
	free(file_name);
	KFile fp;
	if (fp.open(sqliteIndex.getString(), fileRead)) {
		fp.close();
		if (dci->open(sqliteIndex.getString())) {
			dci->start(ci_load, NULL);
		} else {
			klog(KLOG_ERR, "recreate the disk cache index database\n");
			dci->close();
			unlink(sqliteIndex.getString());
			dci->create(sqliteIndex.getString());
			rescan_disk_cache();
		}
	} else {
		if (!dci->create(sqliteIndex.getString())) {
			delete dci;
			dci = NULL;
		}
	}
#else
	memset(&index_scan_state, 0, sizeof(index_scan_state));
	load_index_scan_state();
	m_thread.start(NULL, load_cache_index);
#endif
}
void get_disk_base_dir(KStringBuf& s)
{
	if (*conf.disk_cache_dir) {
		s << conf.disk_cache_dir;
	} else {
		s << conf.path << "cache" << PATH_SPLIT_CHAR;
	}
}
KTHREAD_FUNCTION scan_disk_cache_thread(void* param)
{
	string path;
	if (*conf.disk_cache_dir) {
		path = conf.disk_cache_dir;
	} else {
		path = conf.path;
		path += "cache";
		path += PATH_SPLIT_CHAR;
	}
	if (recreate_index(path.c_str(), index_scan_state.first_index, index_scan_state.second_index, &conf.diskWorkTime)) {
		index_scan_state.need_index_progress = 0;
		index_scan_state.last_scan_time = time(NULL);
		save_index_scan_state();
	}
	index_progress = false;
	KTHREAD_RETURN;
}
void scan_disk_cache()
{
	if (index_progress || cache.is_disk_shutdown()) {
		return;
	}
	if (!index_scan_state.need_index_progress) {
		return;
	}
	if (!conf.diskWorkTime.checkTime(time(NULL))) {
		return;
	}
	index_progress = true;
	if (!kthread_pool_start(scan_disk_cache_thread, NULL)) {
		index_progress = false;
	}
}

void rescan_disk_cache()
{
	index_scan_state.first_index = 0;
	index_scan_state.second_index = 0;
	index_scan_state.need_index_progress = 1;
	save_index_scan_state();
}
bool get_disk_size(INT64& total_size, INT64& free_size) {
	KStringBuf path;
	get_disk_base_dir(path);
#if defined(_WIN32)
	ULARGE_INTEGER FreeBytesAvailable, TotalNumberOfBytes, TotalNumberOfFreeBytes;
	if (!GetDiskFreeSpaceEx(path.getString(), &FreeBytesAvailable, &TotalNumberOfBytes, &TotalNumberOfFreeBytes)) {
		return false;
	}
	total_size = TotalNumberOfBytes.QuadPart;
	free_size = FreeBytesAvailable.QuadPart;
	return true;
#elif defined(LINUX)
	struct statfs buf;
	if (statfs(path.getString(), &buf) != 0) {
		return false;
	}
	total_size = (INT64)buf.f_blocks * (INT64)buf.f_bsize;
	free_size = (INT64)buf.f_bsize * (INT64)buf.f_bavail;
	return true;
#endif
	return false;
}
INT64 get_need_free_disk_size(int used_radio)
{
	if (used_radio >= 98) {
		used_radio = 98;
	}
	if (used_radio <= 0) {
		used_radio = 1;
	}
	INT64 total_size = 0;
	INT64 free_size = 0;
	if (!get_disk_size(total_size, free_size)) {
		return 0;
	}
	INT64 min_free_size = total_size * (100 - used_radio) / 100;
	return min_free_size - free_size;
}
#endif
