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
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "KLogElement.h"
#include "kforwin32.h"
#include "kmalloc.h"
#include "KLogHandle.h"
#include "utils.h"
#include "log.h"
#include "ksocket.h"
KLogElement accessLogger;
KLogElement errorLogger;
using namespace std;
#ifndef _WIN32
static void split_path_parts(const char* path, std::vector<std::string>& parts) {
	const char* start = path;
	for (;;) {
		const char* end = strchr(start, '/');
		std::string part = end ? std::string(start, end - start) : std::string(start);
		if (!part.empty() && part != ".") {
			parts.push_back(part);
		}
		if (!end) {
			break;
		}
		start = end + 1;
	}
}
int klog_open_dir(const char* dir, bool create_last) {
	if (dir == NULL || *dir != '/') {
		errno = EINVAL;
		return -1;
	}
	std::vector<std::string> parts;
	split_path_parts(dir, parts);
	// pending is a stack: its back is the next component to open.
	std::vector<std::string> pending(parts.rbegin(), parts.rend());
	const int flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC;
	int current = ::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	int links = 0;
	while (current >= 0 && !pending.empty()) {
		std::string part = pending.back();
		pending.pop_back();
		int fd = openat(current, part.c_str(), flags);
		if (fd < 0 && errno == ENOENT && create_last && pending.empty()) {
			if (mkdirat(current, part.c_str(), 0755) == 0 || errno == EEXIST) {
				fd = openat(current, part.c_str(), flags);
			}
		}
		if (fd < 0 && (errno == ELOOP || errno == ENOTDIR)) {
			struct stat link, parent;
			char target[PATH_MAX];
			ssize_t length = -1;
			if (fstatat(current, part.c_str(), &link, AT_SYMLINK_NOFOLLOW) == 0 && S_ISLNK(link.st_mode)
				&& link.st_uid == 0 && fstat(current, &parent) == 0 && parent.st_uid == 0 && ++links <= 40) {
				length = readlinkat(current, part.c_str(), target, sizeof(target));
			}
			if (length > 0 && (size_t)length < sizeof(target)) {
				std::string destination(target, length);
				std::vector<std::string> expanded;
				split_path_parts(destination.c_str(), expanded);
				for (std::vector<std::string>::reverse_iterator it = expanded.rbegin(); it != expanded.rend(); ++it) {
					pending.push_back(*it);
				}
				if (destination[0] == '/') {
					::close(current);
					current = ::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
				}
				continue;
			}
			errno = ELOOP;
		}
		int err = errno;
		::close(current);
		current = fd;
		if (fd < 0) {
			errno = err;
		}
	}
	return current;
}
void klog_rotated_parts(const char* path, std::string& stem, std::string& ext) {
	const char* base = strrchr(path, '/');
	base = base ? base + 1 : path;
	const char* dot = strrchr(base, '.');
	if (dot) {
		stem.assign(base, dot - base);
		ext = dot + 1;
	} else {
		stem = base;
		ext.clear();
	}
}
bool klog_is_rotated_name(const char* name, const std::string& stem, const std::string& ext) {
	// rotateLog appends MMDD_HHMMSS (11 characters) to the stem.
	const size_t length = strlen(name);
	if (stem.empty() || length != stem.size() + 11 + (ext.empty() ? 0 : ext.size() + 1)
		|| strncmp(name, stem.c_str(), stem.size()) != 0) {
		return false;
	}
	const char* p = name + stem.size();
	for (int i = 0; i < 11; i++) {
		if (i == 4 ? p[i] != '_' : !isdigit((unsigned char)p[i])) {
			return false;
		}
	}
	return ext.empty() || (p[11] == '.' && strcmp(p + 12, ext.c_str()) == 0);
}
static bool split_dir_base(const KString& path, std::string& dir, std::string& base) {
	const char* str = path.c_str();
	const char* slash = strrchr(str, '/');
	if (slash == NULL || slash[1] == '\0') {
		return false;
	}
	dir = slash == str ? std::string("/") : std::string(str, slash - str);
	base = slash + 1;
	return true;
}
bool KLogElement::open_site_log() {
	std::string dir, base;
	if (!split_dir_base(path, dir, base)) {
		return false;
	}
	int dfd = klog_open_dir(dir.c_str(), mkdir_flag);
	if (dfd < 0) {
		if (mkdir_flag) {
			fprintf(stderr, "cann't open log dir [%s] without following site links, errno=%d\n", dir.c_str(), errno);
			mkdir_flag = false;
		}
		return false;
	}
	int fd = openat(dfd, base.c_str(), O_WRONLY | O_APPEND | O_CREAT | O_NOFOLLOW | O_NOCTTY | O_NONBLOCK | O_CLOEXEC, 0644);
	::close(dfd);
	if (fd < 0) {
		return false;
	}
	struct stat st;
	// A hard link could alias a file outside the log directory; a FIFO could block.
	int mode = fcntl(fd, F_GETFL);
	if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1
		|| mode < 0 || fcntl(fd, F_SETFL, mode & ~O_NONBLOCK) != 0) {
		::close(fd);
		return false;
	}
	log_file_size = st.st_size;
	fp.setHandle(fd);
	return true;
}
int KLogElement::rename_site_log(const char* to) {
	std::string dir, base;
	const char* to_base = strrchr(to, '/');
	if (!split_dir_base(path, dir, base) || to_base == NULL
		|| (size_t)(to_base - to) != dir.size() || strncmp(to, dir.c_str(), dir.size()) != 0) {
		errno = EINVAL;
		return -1;
	}
	int dfd = klog_open_dir(dir.c_str(), false);
	if (dfd < 0) {
		return -1;
	}
	int ret = renameat(dfd, base.c_str(), dfd, to_base + 1);
	int err = errno;
	::close(dfd);
	errno = err;
	return ret;
}
#endif

KLogElement::KLogElement() {
	mkdir_flag = false;
	place = LOG_PRINT;
	rotate_size = 0;
	log_file_size = 0;
	//errorLog = false;
	logs_day = 0;
	logs_size = 0;
	log_handle = true;
#ifndef _WIN32
	uid = 0;
	gid = 0;
#endif
}

KLogElement::~KLogElement() {
	close();
}
bool KLogElement::open() {
	if (path.empty()) {
		return false;
	}
	if (path == "/nolog") {
		place = LOG_NONE;
		return true;
	}
	if (fp.opened()) {
		return true;
	}
	log_file_size = 0;
#ifdef ENABLE_PIPE_LOG
	if (!path.empty() && path[0] == '|') {
		//pipe log
		std::vector<char*> args;
		char* cmd_buf = xstrdup(path.c_str() + 1);
		explode_cmd(cmd_buf, args);
		int args_count = args.size() + 1;
		char** arg = new char* [args_count];
		int i = 0;
		for (unsigned j = 0; j < args.size(); j++) {
			if (args[j] == NULL) {
				continue;
			}
			arg[i] = args[j];
			i++;
		}
		arg[i] = NULL;
		PIPE_T fd[2];
		bool result = false;
		if (KPipeStream::create(fd)) {
			pid_t pid;
			result = createProcess(NULL, arg, NULL, NULL, fd[READ_PIPE], INVALIDE_PIPE, INVALIDE_PIPE, pid);
			ClosePipe(fd[READ_PIPE]);
			if (result) {
				fp.setHandle(fd[WRITE_PIPE]);
#ifdef _WIN32
				CloseHandle(pid);
#endif
			}
		}
		delete[] arg;
		free(cmd_buf);
		return result;
	}
#endif
#ifndef _WIN32
	if (uid > 0 || gid > 0) {
		return open_site_log();
	}
#endif
	struct _stati64 sbuf;
	if (lstat(path.c_str(), &sbuf) == 0) {
		log_file_size = sbuf.st_size;
	}
	int flag = 0;
#ifndef _WIN32
	if (uid > 0 || gid > 0) {
		flag = KFILE_NOFOLLOW;
	}
#endif
	fp.open(path.c_str(), fileAppend, flag);
	if (!fp.opened() && mkdir_flag) {
		fprintf(stderr, "cann't open log file=[%s] try to create the dir\n", path.c_str());
		char* str = strdup(path.c_str());
		char* p = str + strlen(str) - 1;
		while (p > str) {
			if (*p == '/' || *p == '\\') {
				*p = 0;
				break;
			}
			p--;
		}
		int dir_mode = 0755;
#ifndef _WIN32
		// 系统全局日志目录(uid、gid 均为 0，如 /vhs/kangle/var)收紧为 0700，
		// 防止以站点 uid 运行的 PHP 读取 access/server 日志(含 API 签名、明文密码)；
		// 虚拟主机自身日志目录保持 0755，不影响站点查看自己的日志。
		if (uid == 0 && gid == 0) {
			dir_mode = 0700;
		}
#endif
		mkdir(str, dir_mode);
		xfree(str);
		fp.open(path.c_str(), fileAppend, flag);
		if (!fp.opened()) {
			mkdir_flag = false;
		}
	}
	return fp.opened();
}
void KLogElement::close() {
	fp.close();
}
bool KLogElement::checkRotate(time_t nowTime) {
	lock.Lock();
	if (!rotate.isOpen() && rotate.check(nowTime)) {
		rotateLog();
		lock.Unlock();
		return true;
	}
	lock.Unlock();
	return false;
}
void KLogElement::rotateLog() {
	struct tm tm;
	if (!fp.opened()) {
		return;
	}
#ifdef ENABLE_PIPE_LOG
	if (!path.empty() && path[0] == '|') {
		//pipe
		return;
	}
#endif
	KStringBuf nf(256);
	char* t = xstrdup(path.c_str());
	if (t == NULL) {
		return;
	}
	char* p = strrchr(t, '.');
	if (p) {
		*p = '\0';
	}
	time_t now_tm;
	now_tm = time(NULL);
	localtime_r(&now_tm, &tm);
	nf << t;
	nf.add(tm.tm_mon + 1, "%02d");
	nf.add(tm.tm_mday, "%02d");
	nf << "_";
	nf.add(tm.tm_hour, "%02d");
	nf.add(tm.tm_min, "%02d");
	nf.add(tm.tm_sec, "%02d");
	if (p) {
		nf << "." << (p + 1);
	}
	close();
#ifndef _WIN32
	int ret = (uid > 0 || gid > 0) ? rename_site_log(nf.c_str()) : rename(path.c_str(), nf.c_str());
#else
	int ret = rename(path.c_str(), nf.c_str());
#endif
	int err = GetLastError();
	open();
	xfree(t);
	char tms[30];
	CTIME_R(&now_tm, tms, sizeof(tms));
	tms[19] = 0;
	if (ret == 0) {
		errorLogger.log("%s|rotate log file [%s] to [%s] success\n", tms, path.c_str(), nf.c_str());
		logHandle.handle(this, nf.c_str());
	} else {
		errorLogger.log("%s|cann't rotate log file [%s] to [%s],error=[%d]\n", tms, path.c_str(), nf.c_str(), err);
	}
}

void KLogElement::setPath(KString path) {
	lock.Lock();
	if (path != "/nolog" && worker_index > 0) {
		KStringBuf s;
		s << path << worker_index;
		s.str().swap(this->path);
	} else {
		this->path.swap(path);
	}
	close();
	lock.Unlock();
}
bool KLogElement::open_locked() {
	lock.Lock();
	bool result = open();
	lock.Unlock();
	return result;
}
