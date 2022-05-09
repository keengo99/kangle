#include <string.h>
#include <stdio.h>
#include "KChunked.h"
#include "kmalloc.h"
#include "kstring.h"

KChunked::KChunked(KWriteStream *st,bool autoDelete) : KHttpStream(st,autoDelete)
{
	firstPackage = true;
}
KChunked::~KChunked()
{

}
StreamState KChunked::write_all(void *rq, const char *buf,int size)
{
	if (size<=0) {
		return STREAM_WRITE_SUCCESS;
	}
	char header[32];
	int size_len ;
	if(firstPackage){
		size_len = sprintf(header,"%x\r\n",size);
	} else {
		size_len = sprintf(header,"\r\n%x\r\n",size);
	}
	firstPackage = false;
	StreamState result = st->write_all(rq, header,size_len);
	if(result!=STREAM_WRITE_SUCCESS){
		return result;
	}
	return st->write_all(rq, buf,size);
}
StreamState KChunked::write_end(void*rq, KGL_RESULT result)
{
	if (result == KGL_OK) {
		//仅全部正常时，才发送结尾，避免下游非正常缓存
		if (firstPackage) {
			KHttpStream::write_all(rq, kgl_expand_string("0\r\n\r\n"));
		} else {
			KHttpStream::write_all(rq, kgl_expand_string("\r\n0\r\n\r\n"));
		}
	}
	return KHttpStream::write_end(rq, result);
}
