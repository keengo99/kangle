/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#ifndef KFileName_h_slkdjf981223
#define KFileName_h_slkdjf981223
#include <string>
#include <stdarg.h>
#include <stdio.h>
#ifndef _WIN32
#include <sys/stat.h> 
#endif
#include "global.h"
#include "kforwin32.h"
#include "kfile.h"
#include "kasync_file.h"
#include "kmalloc.h"
#ifdef _WIN32
#define ENABLE_UNICODE_FILE 1
#endif
#define FOLLOW_LINK_ALL  1
#define FOLLOW_LINK_OWN  2
#define FOLLOW_PATH_INFO 4
char *my_strtok(char *msg, char split, char **ptrptr);
enum CheckLinkState 
{
	CheckLinkFailed,
	CheckLinkContinue,
	CheckLinkIsFile
};
class KFileName {
public:
	KFileName();
	~KFileName();
	/*
	 调用这个函数之前请对于path,请先调用 tripDir2函数
	 */
	bool setName(const char *docRoot, const char *triped_path, int follow_link);
	bool setName(const char *path);
	//bool giveName(char *path);
	const char *getName();
#ifdef ENABLE_UNICODE_FILE
	const wchar_t *getNameW();
#endif

	const char* get_base();
	CheckLinkState checkLink(const char *path, int follow_link);
	bool operator ==(KFileName &a);
	static bool tripDir(std::string &dir);
	static char *tripDir2(const char *dir, const char split_char);
	static void tripDir3(char *path,const char split_char);
	static char *concatDir(const char *doc_root,const char *file);

	bool isDirectory();
	bool isPrevDirectory()
	{
		return prev_dir;
	}
	bool canExecute();
	time_t getLastModified() const;
	int64_t get_file_size()
	{
		return buf.st_size;
	}
	static char *makeExt(const char *file);
	const char *getExt();
	
	const char *getIndex(){
		return index;
	}
	void setIndex(const char *index);
	
	bool isLinkChecked()
	{
		return linkChecked;
	}
	unsigned getPathInfoLength()
	{
		return pathInfoLength;
	}
	FILE_HANDLE open(fileModel model, int flag)
	{
#ifdef _WIN32
		const wchar_t *wname = getNameW();
		if (wname == NULL) {
			return INVALID_HANDLE_VALUE;
		}
		return kfopen_w(wname, model, flag);
#else
		return kfopen(getName(), model, flag);
#endif
	}
private:
	bool getFileInfo(int name_len);
#ifdef ENABLE_UNICODE_FILE
	wchar_t *wname;
#endif
	char *name;
	char *ext;
	char *index;
	struct _stat64 buf;
	//int name_len;	
	//path_info时，url的长度
	unsigned pathInfoLength;
	bool prev_dir;
	bool linkChecked;	
};
class KFile
{
public:
	KFile()
	{
		kfinit(fp);
	}
	~KFile()
	{
		this->close();
	}
	void flush()
	{
#ifdef _WIN32
		FlushFileBuffers(fp);
#else
		//fflush(fp);
#endif
	}
	INT64 getFileSize()
	{
		return kfsize(fp);
	}
	INT64 getCreateTime()
	{
		return kftime(fp);
	}
	bool open(const char *path, fileModel model, int flag = 0)
	{
		fp = kfopen(path, model, flag);
		return kflike(fp);
	}
	void sync_data()
	{
#if defined(_WIN32)
		FlushFileBuffers(fp);
#elif defined(LINUX)
		fdatasync(fp);
#else
		fsync(fp);
#endif
	}
	int vfprintf(const char *fmt, va_list ap)
	{
		int len;
		char buf[512];
		len = vsnprintf(buf, sizeof(buf), fmt, ap);
		return write(buf, len);
	}
	int fprintf(const char *fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		int len = vfprintf(fmt, ap);
		va_end(ap);
		return len;
	}
	int read(char *buf, int len)
	{
		return ::kfread(fp, buf, len);
	}
	int write(const char *buf, int len)
	{
		return ::kfwrite(fp, buf, len);
	}
	void close()
	{
		if (kflike(fp)) {
			::kfclose(fp);
			kfinit(fp);
		}
	}
	bool opened()
	{
		return kflike(fp);
	}
	bool seek(INT64 len, seekPosion position)
	{
		return kfseek(fp, len, position);
	}
	FILE_HANDLE stealHandle()
	{
		FILE_HANDLE fp2 = fp;
		kfinit(fp);
		return fp2;
	}
	FILE_HANDLE getHandle()
	{
		return fp;
	}
	void swap(KFile *file)
	{
		FILE_HANDLE t = file->fp;
		file->fp = fp;
		fp = t;
	}
	void setHandle(FILE_HANDLE fp)
	{
		this->fp = fp;
	}
private:
	FILE_HANDLE fp;
};
#define KGL_MAX_BUFFER_FILE_SIZE 4194304
class KBufferFile : public KFile
{
public:
	~KBufferFile()
	{
		this->flush();
		aio_free_buffer(buffer);
	}
	KBufferFile()
	{
		buffer = (char *)aio_alloc_buffer(KGL_MAX_BUFFER_FILE_SIZE);
		init();
	}
	char* get_buffer(int *len)
	{
		*len = buffer_left;
		return hot;
	}
	void write_success(int got)
	{
		hot += got;
		buffer_left -= got;
		total_write += got;
		if (buffer_left <= 0) {
			flush();
		}
	}
	void init()
	{
		total_write = 0;
		hot = buffer;
		buffer_left = KGL_MAX_BUFFER_FILE_SIZE;
	}
	void close()
	{
		this->flush();
		KFile::close();
	}
	int write(const char *buf, int len)
	{
		int orig_len = len;
		while (len > 0) {
			int write_len = KGL_MIN(len, buffer_left);
			if (write_len <= 0) {
				if (!flush()) {
					return -1;
				}
				continue;
			}
			if (buf != NULL) {
				kgl_memcpy(hot, buf, write_len);
				buf += write_len;
			}
			hot += write_len;
			len -= write_len;
			buffer_left -= write_len;
			total_write += write_len;
		};
		return orig_len;
	}
	INT64 get_total_write()
	{
		return total_write;
	}
private:
	bool flush()
	{
		if (!this->opened()) {
			return false;
		}
		int len = (int)(hot - buffer);
		bool result = KFile::write(buffer, len) == len;
		hot = buffer;
		buffer_left = KGL_MAX_BUFFER_FILE_SIZE;
		return result;
	}
	INT64 total_write;
	int buffer_left;
	char *buffer;
	char *hot;
};
#endif
