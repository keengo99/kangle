#ifndef KCHUNKED_H
#define KCHUNKED_H
#include "global.h"
#include "KSendable.h"
#include "KHttpStream.h"
/*
定义块大小
buffer前面要留出5个字节大小,做chunk头,后面要留出2个字节做结尾，结构如下:

xxxx\r\n******buffer data******\r\n

注意如果CHUNKED_BUFFER_SIZE的值设置大于4096时,CHUNKED_BUFFER_HEAD_SIZE则要变成6
否则出问题
*/
#define CHUNKED_BUFFER_HEAD_SIZE    5
#define CHUNKED_BUFFER_SIZE         4096
#define CHUNKED_BUFFER_END_SIZE     2
/*
最大buffer数据，由总大小减掉前面头和后面结尾
*/
#define MAX_BUFFER_SIZE             (CHUNKED_BUFFER_SIZE-CHUNKED_BUFFER_HEAD_SIZE-CHUNKED_BUFFER_END_SIZE)
/*
本类和KDeChunked类相反，是以块发送
*/
#if 0
class KChunked : public KHttpStream
{
public:
	KChunked(KWriteStream *st,bool autoDelete);
	~KChunked();
	StreamState write_all(void *rq, const char *buf,int size) override;
	StreamState write_end(void *rq, KGL_RESULT result) override;
private:
	bool firstPackage;
};
#endif
#endif
