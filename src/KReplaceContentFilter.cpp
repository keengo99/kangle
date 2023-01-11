#include "KReplaceContentFilter.h"
#include "KReplaceContentMark.h"
#include "KRewriteMarkEx.h"
#include "KHttpRequest.h"

#define MAX_FILTER_OVECTOR   300
KReplaceContentFilter::KReplaceContentFilter()
{
	prevData = NULL;
	param = NULL;
	callBack = NULL;
	endCallBack = NULL;
	max_buffer = 0;
	stoped = false;
}
KReplaceContentFilter::~KReplaceContentFilter()
{
	if (endCallBack) {
		endCallBack(param);
	}
	if (prevData) {
		free(prevData);
	}
}
bool KReplaceContentFilter::dumpBuffer(void *rq)
{
	StreamState result = STREAM_WRITE_SUCCESS;
	kbuf *buf = b.stealBuffFast();
	kbuf *head = buf;
	while (head) {
		buf = head->next;
		if (st==NULL) {
			break;
		}
		result = st->write_all(rq, head->data,head->used);
		free(head->data);
		free(head);
		head = buf;
		if (result!=STREAM_WRITE_SUCCESS) {
			break;
		}		
	}
	if (head) {
		destroy_kbuf(head);
	}
	return result == STREAM_WRITE_SUCCESS;
}
bool KReplaceContentFilter::writeBuffer(void *rq, const char *str,int len)
{
	if (max_buffer==0) {
		if (KHttpStream::write_all(rq, str,len)!=STREAM_WRITE_SUCCESS) {
			return false;
		}
		return true;
	}
	if (stoped) {
		if (KHttpStream::write_all(rq, str,len)!=STREAM_WRITE_SUCCESS) {
			return false;
		}
		return true;
	}
	b.write_all(str,len);
	if ((int)b.getLen() > max_buffer) {
		return dumpBuffer(rq);
	}
	return true;
}
StreamState KReplaceContentFilter::write_all(void *rq, const char *buf, int len)
{
	if (stoped) {
		return STREAM_WRITE_SUCCESS;
	}
	const char *hot = buf;
	//todo:buffer要删除
	char *buffer = NULL;
	StreamState result = STREAM_WRITE_SUCCESS;
	if (prevData) {
		int new_len = len + prevDataLength;
		buffer = (char *)malloc(new_len + 1);
		kgl_memcpy(buffer,prevData,prevDataLength);
		kgl_memcpy(buffer + prevDataLength,buf,len);
		free(prevData);
		prevData = NULL;
		hot = buffer;
		len = new_len;
	}
	int ovector[MAX_FILTER_OVECTOR];
	while (len>0) {
		int ret = match(param,hot,len,ovector,MAX_FILTER_OVECTOR);
		if (ret>0) {
			KRegSubString *subString = KReg::makeSubString(hot,ovector,MAX_FILTER_OVECTOR,ret);
			KStringBuf *replaced_string = new KStringBuf;
			if (!callBack(param,subString,ovector,replaced_string)) {
				//result = STREAM_WRITE_FAILED;
				b.clean();
				stoped = true;
			} else {
				//再发送前面部分
				if (!writeBuffer(rq, hot, ovector[0])) {
					result = STREAM_WRITE_FAILED;
					stoped = true;
				}
			}
			delete subString;
			if (!writeBuffer(rq, replaced_string->getBuf(),replaced_string->getSize())) {
				result = STREAM_WRITE_FAILED;
			}
			delete replaced_string;
			if (stoped) {
				break;
			}
			if (ovector[1] <= 0) {
				klog(KLOG_ERR, "may have a BUG!! in %s:%d\n", __FILE__, __LINE__);
				break;
			}
			hot += ovector[1];
			len -= ovector[1];
			continue;
		}
		if (ret<-1) {
			prevDataLength = len;
			prevData = (char *)malloc(prevDataLength);
			kgl_memcpy(prevData,hot,prevDataLength);
			break;
		}
		if (!writeBuffer(rq, hot,len)) {
			result = STREAM_WRITE_FAILED;
		}
		break;
	}
	if (buffer) {
		free(buffer);
	}
	return result;
}
StreamState KReplaceContentFilter::write_end(void *rq, KGL_RESULT result)
{
	dumpBuffer(rq);
	if (prevData) {
		if (st) {
			if (stoped) {
				return KHttpStream::write_end(rq, result);
			}
			StreamState result2 = st->write_all(rq, prevData,prevDataLength);
			if (result!=STREAM_WRITE_SUCCESS) {
				return KHttpStream::write_end(rq, result2);
			}
		}
	}
	return KHttpStream::write_end(rq, result);
}
