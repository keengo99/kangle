/*
 * KApiPipeStream.h
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */
#ifndef KAPIPIPESTREAM_H
#define KAPIPIPESTREAM_H
#include "global.h"
#include "KPipeStream.h"
#include "KApiRedirect.h"
#include "fastcgi.h"
#include "utils.h"
class KApiPipeStream: public KPipeStream {
public:
	KApiPipeStream();
	~KApiPipeStream();
	bool loadApi(KApiRedirect *rd);
	bool shutdown();
	/*
	 unix下用setuid
	 */
	bool setuid(int uid, int gid);
	/*
	 windows下用logon
	 */
	bool logon(std::string &user, std::string &password);
	bool chroot(const char *dir);
	bool isChrooted() {
		return chrooted;
	}
	bool isLoaded(KApiRedirect *rd);
	bool listen(u_short port, sp_info *info,bool unix_socket);

	/*
	 * first create must call init
	 */
	bool init(KVirtualHost *vh, int workType);
private:
	bool loadAllApi(KVirtualHost *vh, int workType);
	std::map<u_short,bool> apis;
	bool chrooted;
};
#endif /* KAPIPIPESTREAM_H_ */
