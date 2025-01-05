/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>  2011-07-18
 */
#ifndef UTILS_H_93427598324987234kjh234k
#define UTILS_H_93427598324987234kjh234k
#include <stdio.h>
#ifndef _WIN32
#include <pthread.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#endif
#include "do_config.h"
#include "global.h"
#include "KHttpRequest.h"
#include "kforwin32.h"
#include "kselectable.h"
#include "kserver.h"
#include "KPipeStream.h"
#include "KCgiEnv.h"
#include "KWinCgiEnv.h"
#include "klib.h"
#define RDSTD_NAME_PIPE   0
#define RDSTD_ALL         1
#define RDSTD_NONE        2
#define RDSTD_INPUT       3
#ifdef _WIN32
typedef KWinCgiEnv KCmdEnv;
#else
typedef KCgiEnv KCmdEnv;
#endif
//#ifdef _WIN32
#define USER_T	KString
//#else
//#define USER_T	int
//#endif
#define isAbsolutePath kgl_is_absolute_path
int get_path(char *argv0, KString &path);
int get_param(int argc, char **argv, int &i,const char *param, char *value);
KTHREAD_FUNCTION time_thread(void *arg);
void register_gc_service(void(*flush)(void *,time_t),void *arg);

void my_msleep(int msec);
void explode_cmd(char *str,std::vector<char *> &result);
void explode(const char *str, const char split,std::vector<KString> &result, int limit = -1);
//void explode(const char *str, const char split,	std::map<char *, bool, lessp> *result, int limit = -1);
//std::string string2lower(std::string str);

//kbuf *deflate_buff(kbuf *in_buf, int level, INT64 &len, bool fast);
char *utf82charset(const char *str, size_t len, const char *charset);

FILE *fopen_as(const char *file, const char *mode, int uid, int gid);
bool name2uid(const char *name, int &uid, int &gid);
bool name2gid(const char *name, int &gid);
wchar_t *toUnicode(const char *str,int len=0,int cp_code=0);
void change_admin_password_crypt_type();
void loadExtConfigFile();
void buildAttribute(char *buf, std::map<char *, char *, lessp_icase> &attibute);
void split(char *buf, std::vector<char *> &item);
KString endTag();
void addCurrentEnv(KCmdEnv *env);
/*
����һ�������⹤�������ȴ����
*/
bool startProcessWork(Token_t token, char * args[], KCmdEnv *envs);
/*
����һ������.
rdstd = 0 ʹ��namedPipe
rdstd = 1 �ض���std
rdstd = 2 ������
*/
KPipeStream * createProcess(Token_t token,char * args[],KCmdEnv *envs, int rdstd);
/*
����һ������.
rdstd = 0 ʹ��namedPipe
rdstd = 1 �ض���std
rdstd = 2 ������
*/
bool createProcess(KPipeStream *st,Token_t token, char * args[], KCmdEnv *envs, int rdstd);
bool createProcess(Token_t token, char * args[],KCmdEnv *envs,char *cur_dir,PIPE_T in,PIPE_T out,PIPE_T err,pid_t &pid);
pid_t createProcess(Token_t token,const char *cmd,KCmdEnv *envs,const char *curdir,kgl_process_std *std);
bool killProcess(KVirtualHost *vh);
bool killProcess(KString process, KString user, int pid);

#ifdef _WIN32
extern KMutex closeExecLock;
BOOL StartInteractiveClientProcess (
		HANDLE hToken,
		LPCSTR lpApplication,
		LPTSTR lpCommandLine ,
		KPipeStream *st,int isCgi,LPVOID env
);
BOOL init_winuser(bool first_run);
#define PATH_SPLIT_CHAR		'\\'
#else
#define PATH_SPLIT_CHAR		'/'
#endif
#define CRYPT_TYPE_PLAIN	0
#define CRYPT_TYPE_KMD5		1
#define CRYPT_TYPE_SALT_MD5 2
#define CRYPT_TYPE_SIGN     3
#ifdef ENABLE_HTPASSWD_CRYPT
#define CRYPT_TYPE_HTPASSWD 4
#define TOTAL_CRYPT_TYPE	5
#else
#define TOTAL_CRYPT_TYPE    4
#endif
inline int parseCryptType(const char *type) {
	if (strcasecmp(type, "md5") == 0) {
		return CRYPT_TYPE_KMD5;
	}
#ifdef ENABLE_HTPASSWD_CRYPT
	if (strcasecmp(type,"htpasswd")==0) {
		return CRYPT_TYPE_HTPASSWD;
	}
#endif
	if (strcasecmp(type,"smd5")==0) {
		return CRYPT_TYPE_SALT_MD5;
	}
	if (strcasecmp(type,"sign")==0) {
		return CRYPT_TYPE_SIGN;
	}
	return CRYPT_TYPE_PLAIN;
}
inline const char *buildCryptType(int type) {
	switch (type) {
	case CRYPT_TYPE_PLAIN:
		return "plain";
	case CRYPT_TYPE_KMD5:
		return "md5";
#ifdef ENABLE_HTPASSWD_CRYPT
	case CRYPT_TYPE_HTPASSWD:
		return "htpasswd";
#endif
	case CRYPT_TYPE_SALT_MD5:
		return "smd5";
	case CRYPT_TYPE_SIGN:
		return "sign";
	}
	return "unknow";
}
bool checkPassword(const char *toCheck, const char *password, int cryptType);
inline void string2lower2(char *str,size_t n) {
	kgl_strlow((u_char *)str, (u_char *)str, n);
	return;
}
inline const char *getWorkModelName(int model) {
	if (KBIT_TEST(model,(WORK_MODEL_SSL | WORK_MODEL_MANAGE))==(WORK_MODEL_SSL | WORK_MODEL_MANAGE)) {
		return "manages";
	}
	if (KBIT_TEST(model, WORK_MODEL_SSL)) {
		return "https";
	}
#ifdef WORK_MODEL_TCP
	if (KBIT_TEST(model,WORK_MODEL_TCP)) {
#ifdef HTTP_PROXY
		if (KBIT_TEST(model,WORK_MODEL_SSL)) {
			return "tcps";
		}
#endif
		return "tcp";
	}
#endif
	if (KBIT_TEST(model, WORK_MODEL_MANAGE)) {
		return "manage";
	}
	return "http";
}
inline bool parseWorkModel(const char *type, int &model) {
	model = 0;
	if (strcasecmp(type, "https") == 0) {
		KBIT_SET(model,WORK_MODEL_SSL);
	} else if (strcasecmp(type, "manage") == 0) {
		KBIT_SET(model,WORK_MODEL_MANAGE);
	} else if (strcasecmp(type, "manages") == 0) {
		KBIT_SET(model,WORK_MODEL_SSL|WORK_MODEL_MANAGE);
#ifdef WORK_MODEL_TCP
	} else if (strcasecmp(type, "portmap") == 0 || strcasecmp(type, "tcp") == 0) {
		KBIT_SET(model, WORK_MODEL_TCP);
#ifdef HTTP_PROXY
	} else if (strcasecmp(type,"tcps")==0) {
		KBIT_SET(model, WORK_MODEL_TCP | WORK_MODEL_SSL);
#endif
#endif
	} else if (strcasecmp(type, "http") != 0) {
		fprintf(stderr, "cann't recognize the listen type=%s\n", type);
		return false;
	}
	return true;
}
inline void strip_path_end(std::string &path)
{
	size_t path_size = path.size();
	if (path_size==0) {
		return;
	}
	const char c = path[path_size - 1];
	if (c == '/') {
		path = path.substr(0, path_size - 1);
		return;
	}
#ifdef _WIN32
	else if (c == '\\') {
		path = path.substr(0, path_size - 1);
		return;
	}
#endif
}

inline void strip_path_end(KString& path) {
	auto path_size = path.size();
	if (path_size == 0) {
		return;
	}
	const char c = path[path_size - 1];
	if (c == '/') {
		path = path.substr(0, path_size - 1);
		return;
	}
#ifdef _WIN32
	else if (c == '\\') {
		path = path.substr(0, path_size - 1);
		return;
	}
#endif
}
/*
 * be sure path is ended with '/'
 */
inline void pathEnd(std::string &path) {
	size_t path_size = path.size();
	if (path_size == 0) {
		path = "/";
		return;
	}
	const char c = path[path_size - 1];
	if (c == '/') {
		return;
	}
#ifdef _WIN32
	else if(c=='\\') {
		return;
	}
#endif
	path = path + PATH_SPLIT_CHAR;
}
/*
 * be sure path is ended with '/'
 */
inline void pathEnd(KString& path) {
	auto path_size = path.size();
	if (path_size == 0) {
		path = "/";
		return;
	}
	const char c = path[path_size - 1];
	if (c == '/') {
		return;
	}
#ifdef _WIN32
	else if (c == '\\') {
		return;
	}
#endif
	path = path + PATH_SPLIT_CHAR;
}
inline const char *getServerType() {
#ifdef HTTP_PROXY
	return "proxy";
#else
	return "web";
#endif
}
inline const char *getOsType()
{
#if defined(_WIN32)
	return "windows";
#elif defined(LINUX)
	return "linux";
#endif
	return "unix";
}
inline char *rand_password(int len)
{
	std::stringstream s;
	const char *base_password = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	int base_len=(int)strlen(base_password);
    if(len<8){
        len=8;
    }
    for(int i=0;i<len;i++){
        s << base_password[rand()%base_len];
    }
    return strdup(s.str().c_str());
}

//{{ent
#ifdef _WIN32
LONG WINAPI CustomUnhandledExceptionFilter(PEXCEPTION_POINTERS pExInfo);
KTHREAD_FUNCTION crash_report_thread(void* arg);
void coredump(DWORD pid,HANDLE hProcess,PEXCEPTION_POINTERS pExInfo);
BOOL StartInteractiveClientProcess2 (
	HANDLE hToken,
	const char * lpApplication,
	const char * lpCommandLine ,
	const char * currentDirectory,
	kgl_process_std *std,
	LPVOID env,
	HANDLE &hProcess
) ;
#endif
//}}
bool open_process_std(kgl_process_std *std,KFile *file);
void closeAllFile(int start_fd);
#endif
