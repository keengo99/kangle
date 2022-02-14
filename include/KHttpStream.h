#ifndef KHTTPSTREAM_H
#define KHTTPSTREAM_H
#include "KStream.h"
#include "kmalloc.h"

class KHttpRequest;
class KWriteStream
{
public:
	virtual ~KWriteStream() {

	}
	virtual KGL_RESULT flush(KHttpRequest *rq)
	{
		return KGL_OK;
	}
	virtual KGL_RESULT write_all(KHttpRequest *rq, const char *buf, int len)
	{
		while (len > 0) {
			int r = write(rq, buf, len);
			if (r <= 0)
				return STREAM_WRITE_FAILED;
			len -= r;
			buf += r;
		}
		return STREAM_WRITE_SUCCESS;
	}
	virtual KGL_RESULT write_end(KHttpRequest *rq, KGL_RESULT result)
	{
		if (result != KGL_OK) {
			flush(rq);
			return result;
		}
		return flush(rq);
	}
	virtual KGL_RESULT write_direct(KHttpRequest *rq, char *buf, int len) {
		KGL_RESULT result = write_all(rq, buf, len);
		xfree(buf);
		return result;
	}

	//@deprecated
	inline KWriteStream & operator <<(const char *str)
	{
		if (KGL_OK != write_all(NULL, str, (int)strlen(str))) {
			fprintf(stderr, "cann't write to stream\n");
		}
		return *this;
	}
	//@deprecated
	inline bool add(const int c, const char *fmt) {
		char buf[16];
		int len = snprintf(buf, sizeof(buf) - 1, fmt, c);
		if (len > 0) {
			return write_all(NULL, buf, len) == STREAM_WRITE_SUCCESS;
		}
		return false;
	}
	//@deprecated
	inline bool add(const INT64 c, const char *fmt) {
		char buf[INT2STRING_LEN];
		int len = snprintf(buf, sizeof(buf) - 1, fmt, c);
		if (len > 0) {
			return write_all(NULL, buf, len) == STREAM_WRITE_SUCCESS;
		}
		return false;
	}
	//@deprecated
	inline KWriteStream & operator <<(const std::string str) {
		write_all(NULL, str.c_str(), (int)str.size());
		return *this;
	}
	//@deprecated
	inline KWriteStream & operator <<(const char c) {
		write_all(NULL, &c, 1);
		return *this;
	}
	inline KWriteStream & operator <<(const int value) {
		char buf[16];
		int len = snprintf(buf, 15, "%d", value);
		if (len <= 0) {
			return *this;
		}
		if (KGL_OK != write_all(NULL, buf, len)) {
			fprintf(stderr, "cann't write to stream\n");
		}
		return *this;
	}
	//@deprecated
	inline KWriteStream & operator <<(const INT64 value) {
		char buf[INT2STRING_LEN];
		int2string(value, buf, false);
		if (KGL_OK != write_all(NULL, buf, (int)strlen(buf))) {
			fprintf(stderr, "cann't write to stream\n");
		}
		return *this;
	}
	//@deprecated
	inline void addHex(const int value)
	{
		char buf[16];
		int len = snprintf(buf, 15, "%x", value);
		write_all(NULL, buf, len);
	}
	//@deprecated
	inline KWriteStream & operator <<(const unsigned value) {
		char buf[16];
		const char *fmt = "%u";
		//if (sfmt != stream::def) {
		//	fmt = stream::getFormat(sfmt);
		//}
		int len = snprintf(buf, 15, fmt, value);
		if (len <= 0) {
			return *this;
		}
		if (KGL_OK != write_all(NULL, buf, len)) {
			fprintf(stderr, "cann't write to stream\n");
		}
		return *this;
	}
protected:
	virtual int write(KHttpRequest *rq, const char *buf, int len) {
		return -1;
	}
};

class KBridgeStream : public KWStream
{
public:
	KBridgeStream() {
		st = NULL;
	}
	KBridgeStream(KWriteStream *st) {
		this->st = st;
		autoDelete = true;
	}
	KBridgeStream(KWriteStream *st, bool autoDelete) {
		this->st = st;
		this->autoDelete = autoDelete;
	}
	void connect(KWriteStream *st, bool autoDelete) {
		if (this->st && this->autoDelete) {
			delete this->st;
		}
		this->st = st;
		this->autoDelete = autoDelete;
	}
	virtual ~KBridgeStream() {
		if (autoDelete && st) {
			delete st;
		}
	}
	virtual KGL_RESULT flush() {
		if (st) {
			return st->flush(NULL);
		}
		return STREAM_WRITE_FAILED;
	}
	virtual KGL_RESULT write_all(const char *buf, int len) {
		if (st) {
			return st->write_all(NULL, buf, len);
		}
		return STREAM_WRITE_FAILED;
	}
	virtual KGL_RESULT write_end(KGL_RESULT result) {
		if (st) {
			return st->write_end(NULL, result);
		}
		return STREAM_WRITE_SUCCESS;
	}
protected:
	KWriteStream *st;
	bool autoDelete;

};
class KBridgeStream2 : public KWriteStream
{
public:
	KBridgeStream2() {
		st = NULL;
	}
	KBridgeStream2(KWStream *st) {
		this->st = st;
		autoDelete = true;
	}
	KBridgeStream2(KWStream *st, bool autoDelete) {
		this->st = st;
		this->autoDelete = autoDelete;
	}
	void connect(KWStream *st, bool autoDelete) {
		if (this->st && this->autoDelete) {
			delete this->st;
		}
		this->st = st;
		this->autoDelete = autoDelete;
	}
	virtual ~KBridgeStream2() {
		if (autoDelete && st) {
			delete st;
		}
	}
	virtual KGL_RESULT flush(KHttpRequest *rq) {
		if (st) {
			return st->flush();
		}
		return STREAM_WRITE_FAILED;
	}
	virtual KGL_RESULT write_all(KHttpRequest *rq, const char *buf, int len) {
		if (st) {
			return st->write_all(buf, len);
		}
		return STREAM_WRITE_FAILED;
	}
	virtual KGL_RESULT write_end(KHttpRequest *rq, KGL_RESULT result) {
		if (st) {
			return st->write_end(result);
		}
		return STREAM_WRITE_SUCCESS;
	}
protected:
	KWStream *st;
	bool autoDelete;
};
/*
 支持串级的流
 */
class KHttpStream : public KWriteStream
{
public:
	KHttpStream() {
		st = NULL;
	}
	KHttpStream(KWriteStream *st) {
		this->st = st;
		autoDelete = true;
	}
	KHttpStream(KWriteStream *st, bool autoDelete) {
		this->st = st;
		this->autoDelete = autoDelete;
	}
	void connect(KWriteStream *st, bool autoDelete) {
		if (this->st && this->autoDelete) {
			delete this->st;
		}
		this->st = st;
		this->autoDelete = autoDelete;
	}
	virtual ~KHttpStream() {
		if (autoDelete && st) {
			delete st;
		}
	}
	virtual KGL_RESULT flush(KHttpRequest *rq) {
		if (st) {
			return st->flush(rq);
		}
		return STREAM_WRITE_FAILED;
	}
	virtual KGL_RESULT write_all(KHttpRequest *rq, const char *buf, int len) {
		if (st) {
			return st->write_all(rq, buf, len);
		}
		return STREAM_WRITE_FAILED;
	}
	virtual KGL_RESULT write_end(KHttpRequest *rq, KGL_RESULT result) {
		if (st) {
			return st->write_end(rq, result);
		}
		return result;
	}
	KWriteStream *st;
	bool autoDelete;
};
#endif
