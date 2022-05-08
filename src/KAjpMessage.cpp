/*
 * KAjpMessage.cpp
 *
 *  Created on: 2010-7-31
 *      Author: keengo
 */
#include <string.h>
#include "KAjpMessage.h"
#include "kmalloc.h"
KAjpMessage::KAjpMessage()
{
	buf = NULL;
}
KAjpMessage::KAjpMessage(KWStream *st) {
	buf = (unsigned char *) xmalloc(AJP_PACKAGE);
	reset();
	this->st = st;
}
KAjpMessage::KAjpMessage(char *buf,int length)
{
	this->buf = (unsigned char *)buf;
	this->len = length;
	pos = 0;
}
void KAjpMessage::reset() {
	pos = 4;
}
KAjpMessage::~KAjpMessage() {
	if (buf) {
		xfree(buf);
	}
}
bool KAjpMessage::putByte(unsigned char val) {
	if (!checkSend(1)) {
		return false;
	}
	buf[pos++] = val;
	return true;
}
bool KAjpMessage::putShort(unsigned short val) {
	if (!checkSend(2)) {
		return false;
	}
	buf[pos++] = (unsigned char) ((val >> 8) & 0xFF);
	buf[pos++] = (unsigned char) (val & 0xFF);
	return true;
}
bool KAjpMessage::putInt(unsigned val) {
	if (!checkSend(4)) {
		return false;
	}
	buf[pos++] = (unsigned char) ((val >> 24) & 0xFF);
	buf[pos++] = (unsigned char) ((val >> 16) & 0xFF);
	buf[pos++] = (unsigned char) ((val >> 8) & 0xFF);
	buf[pos++] = (unsigned char) (val & 0xFF);
	return true;
}
bool KAjpMessage::putString(const char *str, int len) {
	if (!putShort(len)) {
		return false;
	}
	if (!checkSend(len + 1)) {
		return false;
	}
	kgl_memcpy(buf + pos, str, len + 1);
	pos+=(len+1);
	return true;
}
bool KAjpMessage::putString(const std::string &str) {
	return putString(str.c_str(), (int)str.size());
}
bool KAjpMessage::putString(const char *str) {
	return putString(str, (int)strlen(str));
}
bool KAjpMessage::end() {
	if(pos>4){
		return send();
	}
	return true;
}
bool KAjpMessage::send() {
	int dLen = pos - 4;
	buf[0] = (unsigned char) 0x12;
	buf[1] = (unsigned char) 0x34;
	buf[2] = (unsigned char) ((dLen >> 8) & 0xFF);
	buf[3] = (unsigned char) (dLen & 0xFF);
	/*
	for(int i=0;i<pos;i++){
		printf("%02x",buf[i]);
		if(buf[i]>=0x20 && buf[i] < 0x7e){
			printf("[%c]",buf[i]);
		}
		printf(" ");
	}
	printf("\n");
	*/
	if (st->write_all((char *) buf, pos) != STREAM_WRITE_SUCCESS) {
		return false;
	}
	reset();
	return true;
}
bool KAjpMessage::checkSend(int len) {
	if (pos + len >= AJP_PACKAGE) {
		return send();
	}
	return true;
}
