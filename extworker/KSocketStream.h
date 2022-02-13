#ifndef KSOCKETSTREAM_H
#define KSOCKETSTREAM_H
#include "ksocket.h"
#include "KHttpStream.h"

class KSocketStream : public KWriteStream
{
public:
	KSocketStream(SOCKET sockfd)
	{
		this->sockfd = sockfd;
	}
	~KSocketStream()
	{
		ksocket_close(sockfd);
	}
	void set_nodelay()
	{
		ksocket_no_delay(sockfd,false);
	}
	void set_delay()
	{
		ksocket_delay(sockfd);
	}
	int write(KHttpRequest *rq, const char *str, int len)
	{
		return send(sockfd, str, len, 0);
	}
	bool read_all(char *str, int len)
	{
		while (len > 0) {
			int read_len = recv(sockfd, str, len, 0);
			//printf("read_len=[%d],len=[%d]\n", read_len,len);
			if (read_len <= 0) {
				return false;
			}
			str += read_len;
			len -= read_len;
		}
		return true;
	}
private:
	SOCKET sockfd;
};
#endif

