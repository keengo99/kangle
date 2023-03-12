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
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#include "global.h"

#include <stdio.h>
#ifdef _WIN32

#else
#define _USE_BSD
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <grp.h>
#include <pwd.h>
#include <syslog.h>
#ifdef HAVE_POLL
#include <poll.h>
#endif
#endif
#include "utils.h"
#include "log.h"
#include "do_config.h"
#include "kforwin32.h"
#include "http.h"
#include "kthread.h"
#include "kmalloc.h"
#include "KBuffer.h"
#include "KServerListen.h"
#include "kmd5.h"
#include "lang.h"
#include "extern.h"
#include "KWinCgiEnv.h"
#include "KCgiEnv.h"
#include "kfiber.h"
#include "klib.h"
using namespace std;
/*
 const char ap_month_snames[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
 */

int split_host_port(char *host, char separator, size_t host_len) {

	int point;
	int size;
	if (host_len == 0) {
		size = (int)strlen(host);
	} else {
		size = KGL_MIN((int)strlen(host),(int)host_len);
	}
	for (point = size - 1; point > 0; point--) {
		if (host[point] == separator)
			break;
	}
	if (point == 0)
		return 0;
	host[point] = 0;
	return atoi(host + point + 1);

}
#ifdef _WIN32
int get_param(int argc, char **argv, int &i,const char *param, char *value) {	
	if (strcmp(argv[i], param) == 0) {
		if (value == NULL) {
			return 1;
		}
		if (i < argc - 1) {
			if (strlen(argv[i + 1]) < 255)
				strcpy(value, argv[i + 1]);
			else
				value[0] = 0;
			i++;
			return 1;
		}
		value[0] = 0;
		return 1;
	}
	return 0;
}
#endif
int get_path(char *argv0, KString &path) {
	//	int size = strlen(argv0);
	char *p = strrchr(argv0, PATH_SPLIT_CHAR);
	if (p) {
		conf.program = p + 1;
		*p = '\0';
	} else {
		conf.program = argv0;
	}
#ifndef _WIN32
	if (argv0[0] != '/') {
		char *pwd = getenv("PWD");
		if (pwd)
			path = pwd;
		path = path + "/";
	}
#endif
	path += argv0;
	if (p) {
		return 1;
	} else {
#ifndef _WIN32
		return 0;
#else
		path=".\\";
		return 1;
#endif
	}
}
/*
std::string string2lower(std::string str) {
	stringstream s;
	for (size_t i = 0; i < str.size(); i++) {
		s << (char) tolower(str[i]);
	}
	return s.str();
}

void explode(const char *str, const char split,
		map<char *, bool, lessp> *result, int limit) {
	char *tmp;
	char *msg;
	char *ptr;
	char *tmp2 = strdup(str);
	int i = 0;
	if (tmp2 == NULL)
		return;
	tmp = tmp2;
	while ((msg = my_strtok(tmp, split, &ptr)) != NULL) {
		tmp = NULL;
		//KString *s = new KString(msg);
		if (result->find(msg) == result->end()) {
			result->insert(pair<char *, bool> (xstrdup(msg), true));
		}
		i++;
		if (limit > 0 && i >= limit)
			break;
	}
	xfree(tmp2);
	return;
}
*/
void explode_cmd(char *buf,std::vector<char *> &item)
{
	while(*buf) {
		char *hot = buf;
		while (*hot && isspace((unsigned char) *hot)) {
			hot++;
		}
		if (*hot=='\0') {
			break;
		}
		char startChar = *hot;
		if(*hot=='\'' || *hot == '"'){
			hot++;
			buf = hot;
			item.push_back(buf);
			char *dst = hot;
			bool slash = false;
			for(;;){
				if(*hot=='\0'){
					return;
				}
				if (!slash) {
					if(*hot == startChar){
						*dst = '\0';
						buf = hot+1;
						break;
					}
					if(*hot=='\\'){
						slash = true;
						hot++;
						continue;
					}
					*dst ++ = *hot ++;
				} else {
					if (*hot == '\\' || *hot==startChar) {
						*dst++ = *hot++;
					} else {
						*dst = '\\';
						dst++;
						*dst++ = *hot++;
					}
					slash = false;
				}
			}
		}else{
			buf = hot;
			item.push_back(buf);
			while(*buf && !isspace((unsigned char)*buf)){
				buf++;
			}
			if (*buf == '\0') {
				break;
			}
			*buf = '\0';
			buf++;
		}

	}

}
void explode(const char *str, const char split, vector<KString> &result,
		int limit) {
	char *tmp;
	char *msg;
	char *ptr;
	char *tmp2 = strdup(str);
	int i = 0;
	result.clear();
	if (tmp2 == NULL)
		return;
	tmp = tmp2;
	while ((msg = my_strtok(tmp, split, &ptr)) != NULL) {
		tmp = NULL;
		result.push_back(msg);
		i++;
		if (limit > 0 && i >= limit)
			break;
	}
	xfree(tmp2);
	return;
}

FILE *fopen_as(const char *file, const char *mode, int uid, int gid) {
	return fopen(file, mode);
	/*
	 fopenLock.Lock(__FILE__,__LINE__);
	 if(uid>0){
	 setfsuid(uid);
	 }
	 if(gid>0)
	 setfsgid(gid);
	 FILE *fp=fopen(file,mode);
	 if(uid>0){
	 setfsuid(geteuid());
	 }
	 if(gid>0)
	 setfsgid(getegid());
	 fopenLock.Unlock();
	 return fp;
	 */
}
void my_msleep(int msec) {
	if (kfiber_is_main()) {
		kgl_msleep(100);
	} else {
		kfiber_msleep(500);
	}
}

bool name2uid(const char *name, int &uid, int &gid) {
#ifndef _WIN32
	if (name == NULL) {
		return false;
	}
	if (name[0] == '#') {
		uid = atoi(name + 1);
		return true;
	}
	if (isdigit((unsigned char) *name)) {
		uid = atoi(name);
		return true;
	}
	char buf[512];
	struct passwd pwd;
	struct passwd *result;
	if (getpwnam_r(name, &pwd, buf, sizeof(buf), &result) != 0) {
		return false;
	}
	if (result == NULL) {
		return false;
	}
	uid = result->pw_uid;
	gid = result->pw_gid;
#endif
	return true;
}
bool name2gid(const char *name, int &gid) {
#ifndef _WIN32
	if (name == NULL) {
		return false;
	}
	if (name[0] == '#') {
		gid = atoi(name + 1);
		return true;
	}
	if (isdigit((unsigned char) *name)) {
                gid = atoi(name);
                return true;
        }
	char buf[512];
	struct group grp;
	struct group *result;
	if (getgrnam_r(name, &grp, buf, sizeof(buf), &result) != 0) {
		return false;
	}
	if (result == NULL) {
		return false;
	}
	gid = result->gr_gid;
#endif
	return true;
}
void change_admin_password_crypt_type() {
	if (conf.passwd_crypt == CRYPT_TYPE_PLAIN) {
		conf.passwd_crypt = CRYPT_TYPE_KMD5;
		KStringBuf s;
		if (conf.auth_type == AUTH_BASIC) {
			s << conf.admin_passwd.c_str();
		} else if (conf.auth_type == AUTH_DIGEST) {
			s << conf.admin_user.c_str() << ":" << PROGRAM_NAME << ":"
					<< conf.admin_passwd.c_str();
		}
		char md5result[33];
		KMD5(s.c_str(), s.size(),md5result);
		conf.admin_passwd = md5result;
	}
}
/*
创建一个进程外工作，并等待完成
*/
bool startProcessWork(Token_t token, char * args[], KCmdEnv *envs)
{	
	KPipeStream *st = createProcess(token,  args, envs,
#ifdef _WIN32
		RDSTD_NONE
#else
		RDSTD_ALL
#endif
		);
	if (st) {
		//等待子进程结束
		st->waitClose();
		delete st;
		return true;
	}
	return false;
}

pid_t createProcess(Token_t token,const char *cmd,KCmdEnv *envs,const char *curdir,kgl_process_std *std)
{
	if (quit_program_flag==PROGRAM_QUIT_IMMEDIATE) {
		return false;
	}
	pid_t pid;
	//{{ent
#ifdef _WIN32
	if(!StartInteractiveClientProcess2(token,NULL,cmd,curdir,std,(char *)(envs?envs->dump_env():NULL),pid)) {
		return INVALID_HANDLE_VALUE;
	}
#else
	//}}
	if (cmd==NULL || *cmd=='\0') {
		return -1;
	}

	pid = fork();
	if (pid == -1) {
		klog(KLOG_ERR, "cann't fork errno=%d\n", errno);
		return -1;
	}
	if (pid == 0) {
		signal(SIGTERM, SIG_DFL);
		if (token && my_uid == 0) {
			setgid(token[1]);
			setuid(token[0]);
		}
		char *buf = strdup(cmd);
		std::vector<char *> args;
		explode_cmd(buf,args);
		int args_count = args.size();
		char **arg = new char *[args_count+1];
		int index=0;
		for (;index<args_count;) {
			arg[index] = args[index];
			index++;
		}
		arg[index] = NULL;

		KFile file[3];
		if (!open_process_std(std,file)) {
			exit(127);
		}
		if (std->hstdin>0) {
			close(0);
			dup2(std->hstdin,0);
		}
		if (std->hstdout>=0 && std->hstdout!=1) {
			close(1);
			dup2(std->hstdout,1);
		}
		if (std->hstderr>=0 && std->hstderr!=2) {
			close(2);
			dup2(std->hstderr,2);
		}
		closeAllFile(3);
		if (curdir==NULL) {
			char *curdir2 = strdup(arg[0]);
			char *p = strrchr(curdir2,'/');
			if (p) {
				*p = '\0';
			}
			chdir(curdir2);
			free(curdir2);
		} else {
			chdir(curdir);
		}
		execve(arg[0], arg, (envs ? envs->dump_env() : NULL));
		//execv(args[0], args);
		fprintf(stderr, "run cmd[%s] error=%d %s\n", args[0], errno, strerror(
			errno));
		debug("child end\n");
		exit(127);
	}
	//{{ent	
#endif
	//}}
	return pid;
}
