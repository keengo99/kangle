/*
 * KAjpMessage.h
 * ajp协议信息包处理类
 *  Created on: 2010-7-31
 *      Author: keengo
 */

#ifndef KAJPMESSAGE_H_
#define KAJPMESSAGE_H_
#define AJP_PACKAGE   8192
#define AJP_BUFF_SIZE 8186
#include <string>
#include "global.h"
#include "KHttpStream.h"
#define JK_AJP13_FORWARD_REQUEST 2
#define JK_AJP13_SEND_BODY_CHUNK 3
#define JK_AJP13_SEND_HEADERS    4
#define JK_AJP13_END_RESPONSE    5
#define JK_AJP13_GET_BODY_CHUNK  6

#define JK_AJP13_ERROR           0

class KAjpMessage {
public:
	KAjpMessage();
	KAjpMessage(KWriteStream *st);
	KAjpMessage(char *buf,int length);
	virtual ~KAjpMessage();
	int getLen()
	{
		return len-pos;
	}
	char *getBytes()
	{
		return (char *)(buf+pos);
	}
	bool peekByte(unsigned char *val)
	{
		if(getLen()<1){
			return false;
		}
		*val = buf[pos];
		return true;
	}
	unsigned char getType()
	{
		return buf[0];
	}
	bool getByte(unsigned char *val)
	{
		if(getLen()<=0){
			return false;
		}
		*val = buf[pos++];
		return true;
	}
	bool getShort(unsigned short *val)
	{
		if(getLen()<2){
			return false;
		}
		*val = buf[pos] << 8 | buf[pos+1];
		pos+=2;
		return true;
	}
	bool getString(char ** val)
	{
		unsigned short slen;
		if(!getShort(&slen)){
			return false;
		}
		if(getLen()<slen+1){
			return false;
		}
		*val = (char *)buf + pos;
		pos+=slen+1;
		return true;
	}
	bool putByte(unsigned char value);
	bool putShort(unsigned short value);
	bool putInt(unsigned value);
	bool putString(const char *str,int len);
	bool putString(const std::string &str);
	bool putString(const char *str);
	void reset();
	bool end();
private:
	bool checkSend(int len);
	bool send();
	unsigned char *buf;
	int pos;
	int len;
	KWriteStream *st;
};

class KAjpMessageParser {
public:
	KAjpMessageParser()
	{
		memset(this, 0, sizeof(*this));
	}
	void init(const char *buf,int len)
	{
		this->buf = (unsigned char *)buf;
		pos = 0;
		this->len = len;
	}
	int getLen()
	{
		return len - pos;
	}
	char *getBytes()
	{
		return (char *)(buf + pos);
	}
	bool peekByte(unsigned char *val)
	{
		if (getLen() < 1) {
			return false;
		}
		*val = buf[pos];
		return true;
	}
	unsigned char getType()
	{
		return buf[0];
	}
	bool getByte(unsigned char *val)
	{
		if (getLen() <= 0) {
			return false;
		}
		*val = buf[pos++];
		return true;
	}
	bool getShort(unsigned short *val)
	{
		if (getLen() < 2) {
			return false;
		}
		*val = buf[pos] << 8 | buf[pos + 1];
		pos += 2;
		return true;
	}
	int getString(char ** val)
	{
		unsigned short slen;
		if (!getShort(&slen)) {
			return -1;
		}
		if (getLen() < slen + 1) {
			return -1;
		}
		*val = (char *)buf + pos;
		pos += slen + 1;
		return slen;
	}
private:
	unsigned char *buf;
	int pos;
	int len;
};
#endif /* KAJPMESSAGE_H_ */
