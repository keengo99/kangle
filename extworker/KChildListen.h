/*
 * KChildListen.h
 *
 *  Created on: 2010-7-10
 *      Author: keengo
 * kangle子进程监听处理类，kangle子进程通过监听端口供主进程连接。
 * 一有连接进来就会把该连接加入到选择器中。
 */

#ifndef KCHILDLISTEN_H_
#define KCHILDLISTEN_H_
#include <string>

#include "ksocket.h"
#include "KApiRedirect.h"
#include "KPipeStream.h"
#include "KSocketStream.h"

class KChildListen {
public:
	KChildListen();
	virtual ~KChildListen();
	void unlink_unix()
	{
		if(unix_path.size()>0){
			unlink(unix_path.c_str());			
		}
	}
	bool canRead();
	KPipeStream *st;
	SOCKET sockfd;
	std::string unix_path;
};
extern KChildListen *cl;
#endif /* KCHILDLISTEN_H_ */
