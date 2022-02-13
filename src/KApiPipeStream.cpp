/*
 * KApiPipeStream.cpp
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */
#include <vector>
#include "KApiPipeStream.h"
#include "fastcgi.h"
#include "extern.h"
#include "KVirtualHost.h"
#include "kmalloc.h"
using namespace std;
KApiPipeStream::KApiPipeStream() {
	chrooted = false;
}
KApiPipeStream::~KApiPipeStream() {
	
}
bool KApiPipeStream::isLoaded(KApiRedirect *rd) {
	std::map<u_short,bool>::iterator it;
	it = apis.find(rd->id);
	if (it != apis.end()) {
		return  true;
	}
	return false;
}
bool KApiPipeStream::shutdown() {
	FCGI_Header header;
	memset(&header, 0, sizeof(header));
	header.type = API_CHILD_SHUTDOWN;
	if (write_all((char *) &header, sizeof(header)) == STREAM_WRITE_SUCCESS) {
		return true;
	}
	return false;
}
bool KApiPipeStream::logon(std::string &user, std::string &password) {
	FCGI_Header header;
	memset(&header, 0, sizeof(header));
	header.type = API_CHILD_LOGON;
	header.contentLength = htons((u_short)user.size() + (u_short)password.size() + 8);
	if (write_all((char *) &header, sizeof(header)) != STREAM_WRITE_SUCCESS) {
		return false;
	}
	if (!writeString(user.c_str())) {
		return false;
	}
	if (!writeString(password.c_str())) {
		return false;
	}
	if (!read_all((char *) &header, sizeof(header))) {
		return false;
	}
	if (header.type != API_CHILD_LOGON_RESULT) {
		return false;
	}
	assert(header.contentLength==0);
	if (header.id == 1) {
		return true;
	}
	return false;
}
bool KApiPipeStream::setuid(int uid, int gid) {
	if (uid == 0 && gid == 0) {
		return true;
	}
	api_child_t_uidgid body;
	body.gid = gid;
	body.uid = uid;
	FCGI_Header header;
	memset(&header, 0, sizeof(header));
	header.type = API_CHILD_SETUID;
	header.contentLength = htons(sizeof(api_child_t_uidgid));
	if (write_all((char *) &header, sizeof(header)) != STREAM_WRITE_SUCCESS) {
		debug("cann't write setuid package head to child\n");
		return false;
	}
	if (write_all((char *) &body, sizeof(body)) != STREAM_WRITE_SUCCESS) {
		debug("cann't write setuid package body to child\n");
		return false;
	}
	if (!read_all((char *) &header, sizeof(header))) {
		debug("cann't read setuid package from child\n");
		return false;
	}
	if (header.type != API_CHILD_SETUID_RESULT) {
		debug("the header type[%d] is wrong\n", header.type);
		return false;
	}
	assert(header.contentLength==0);
	if (header.id == 0) {
		return true;
	}
	klog(KLOG_ERR, "cann't setuid to #%d:#%d result=%d\n", uid, gid, header.id);
	return false;
}
bool KApiPipeStream::chroot(const char *dir) {
	FCGI_Header header;
	memset(&header, 0, sizeof(header));
	header.type = API_CHILD_CHROOT;
	//std::string path = rd->path;
	u_short len = (u_short) (strlen(dir) + 1);
	header.contentLength = htons(len);
	if (write_all((char *) &header, sizeof(header)) != STREAM_WRITE_SUCCESS) {
		return false;
	}
	if (write_all(dir, len) != STREAM_WRITE_SUCCESS) {
		return false;
	}
	if (!read_all((char *) &header, sizeof(header))) {
		return false;
	}
	if (header.type != API_CHILD_CHROOT_RESULT) {
		return false;
	}
	assert(header.contentLength == 0);
	if (header.id == 0) {
		chrooted = true;
		return true;
	}
	klog(KLOG_ERR, "api child process cann't chroot to [%s]\n", dir);
	return false;
}
bool KApiPipeStream::loadApi(KApiRedirect *rd) {
	FCGI_Header header;
	memset(&header, 0, sizeof(header));
	header.type = API_CHILD_LOAD;
	std::string path = rd->dso.path;
	u_short len = (u_short) (path.size() + 1);
	header.id = rd->id;
	header.contentLength = htons(len);
	if (write_all((char *) &header, sizeof(header)) != STREAM_WRITE_SUCCESS) {
		return false;
	}
	if (write_all(path.c_str(), len) != STREAM_WRITE_SUCCESS) {
		return false;
	}
	if (!read_all((char *) &header, sizeof(header))) {
		return false;
	}
	if (header.type != API_CHILD_LOAD_RESULT) {
		return false;
	}
	assert(header.contentLength == 0);
	if (header.id > 0) {
		klog(KLOG_ERR, "child load api [%s] failed\n", path.c_str());
		return false;
	}
	apis.insert(pair<u_short,bool> (rd->id,true));
	return true;	

}
bool KApiPipeStream::listen(u_short port, sp_info *info,bool unix_socket) {
	FCGI_Header header;
	memset(&header, 0, sizeof(header));
	header.type = API_CHILD_LISTEN;
#ifdef KSOCKET_UNIX	
	if (unix_socket) {
		header.type = API_CHILD_LISTEN_UNIX;
	}
#endif
	header.id = port;
	if (write_all((char *) &header, sizeof(header)) != STREAM_WRITE_SUCCESS) {
		return false;
	}
	if (!read_all((char *) &header, sizeof(header))) {
		return false;
	}
	if (header.type != API_CHILD_LISTEN_RESULT) {
		return false;
	}
	u_short len = ntohs(header.contentLength);
	assert(len == sizeof(sp_info));
	if (len != sizeof(sp_info)) {
		return false;
	}
	if (!read_all((char *) info, sizeof(sp_info))) {
		return false;
	}
	return true;
}
bool KApiPipeStream::init(KVirtualHost *vh, int workType) {
	if (!loadAllApi(vh, workType)) {
		return false;
	}
#ifdef ENABLE_VH_RUN_AS
#ifndef _WIN32
	if (my_uid == 0) {
		if (vh->chroot) {
			if (!chroot(vh->doc_root.c_str())) {
				return false;
			}
		}
		if (!setuid(vh->id[0],vh->id[1])) {
			return false;
		}
	}
#else
#if 0
	/*
	* windows系统上先改变身份，再load api
	*/
	if(!conf.createProcessAsUser && vh->user.size()>0) {
		if(!logon(vh->user,vh->group)) {
			return false;
		}
	}
#endif
#endif
#endif
	return true;
}
bool KApiPipeStream::loadAllApi(KVirtualHost *vh, int workType) {
	return vh->loadApiRedirect(this, workType);
}
