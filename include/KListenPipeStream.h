#ifndef KLISTENPIPESTREAM_H
#define KLISTENPIPESTREAM_H
#include <errno.h>
#include "KPipeStream.h"
#include "global.h"
#include "KListNode.h"
#include "KPoolableSocketContainer.h"
class KListenPipeStream : public KPipeStream
{
public:
	KListenPipeStream()
	{
		port = 0;
	}
	virtual ~KListenPipeStream()
	{
		closeServer();
	}
	void unlink_unix()
	{
#ifdef KSOCKET_UNIX
		if(!unix_path.empty()){
			if (unlink(unix_path.c_str())!=0) {
				int err = errno;
				klog(KLOG_ERR,"cann't unlink unix socket [%s] error =%d %s\n",unix_path.c_str(),err,strerror(err));
			} else {
				unix_path.clear();
			}
		}
#endif
	}
	void closeServer()
	{
		if (ksocket_opened((SOCKET)fd[0])) {
			::ksocket_close((SOCKET)fd[0]);
			fd[0] = INVALIDE_PIPE;
		}		
	}
#ifdef KSOCKET_UNIX	
	bool listen(const char *path)
	{
		this->unix_path = path;
		struct sockaddr_un addr;
		ksocket_unix_addr(path,&addr);
		SOCKET sockfd = ksocket_listen((sockaddr_i *)&addr, 0);
		if (!ksocket_opened(sockfd)) {
			return false;
		}
		assert(fd[0]==INVALIDE_PIPE);
#ifdef _WIN32
		fd[0] = (HANDLE)sockfd;
#else
		fd[0] = sockfd;
#endif
		return true;
	}
#endif
	bool listen(int port=0,const char *host="127.0.0.1")
	{
		sockaddr_i addr;
		ksocket_getaddr(host, port, AF_UNSPEC, AI_NUMERICHOST,&addr);
		SOCKET sockfd = ksocket_listen(&addr, KSOCKET_BLOCK);
		if (!ksocket_opened(sockfd)) {
			return false;
		}
		if (port == 0) {
			socklen_t addr_size = (socklen_t)ksocket_addr_len(&addr);
			getsockname(sockfd,(struct sockaddr *)&addr, &addr_size);
		}
		this->port = ksocket_addr_port(&addr);
		assert(fd[0]==INVALIDE_PIPE);
#ifdef _WIN32
		fd[0] = (HANDLE)sockfd;
#else
		fd[0] = sockfd;
#endif
		return true;
	}
	int getPort()
	{
		return port;
	}
	void setPort(int port)
	{
		this->port = port;
	}
	friend class KCmdPoolableRedirect;
#ifdef KSOCKET_UNIX
	std::string unix_path;
#endif
private:
	int port;

};
#endif
