/*
 * KHttpTransfer.h
 *
 *  Created on: 2010-5-4
 *      Author: keengo
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */

#ifndef KHTTPTRANSFER_H_
#define KHTTPTRANSFER_H_
#include "KHttpRequest.h"
#include "KHttpObject.h"
#include "KDeChunked.h"
#include "KCompressStream.h"
#include "KSendable.h"
#include "KChunked.h"
#include "KCacheStream.h"
//{{ent
#ifdef ENABLE_DELTA_ENCODE
#include "KDeltaEncode.h"
#endif//}}

/*
 * This class use to transfer data to client
 * It support compress(use gzip) and chunk transfer encoding.

 数据流向:
 数据源(localfetch) -->   <--数据源(push模式)
 chunked接收(有或者没有)-->
 degzip(数据解压缩,如果有内容过滤变换)-->
 (KHttpTransfer::write_all，并且也发送http头)
 内容过滤/变换 KHttpRequest的checkFilter -->
 gzip压缩-->
 缓存数据-->
 chunked发送-->
 socket-->
 write hook
 */
class KHttpTransfer: public KHttpStream {
public:
	KHttpTransfer(KHttpRequest *rq, KHttpObject *obj);
	KHttpTransfer();
	~KHttpTransfer();
	void init(KHttpRequest *rq, KHttpObject *obj);
	bool support_sendfile();
	/*
	 * send actual data to client. cann't head data or chunked head
	 */
	StreamState write_all(KHttpRequest *rq, const char *str, int len);
	/*
	 * 写数据流结束。如果没有发送(可能存在buffer里)，则要立即发送数据。
	 */
	StreamState write_end(KHttpRequest *rq, KGL_RESULT result);
	StreamState sendHead(KHttpRequest *rq, bool isEnd);
	kev_result TryWrite(KHttpRequest *rq);
	bool TrySyncWrite(KHttpRequest *rq);
	friend class KDeChunked;
	friend class KGzip;
public:
	KHttpObject *obj;
	KReadWriteBuffer buffer;
private:
	bool loadStream(KHttpRequest *rq, KCompressStream *compress_st, cache_model cache_layer);
	//bool isHeadSend;

};

#endif /* KHTTPTRANSFER_H_ */
