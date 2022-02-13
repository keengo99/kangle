#include <string.h>
#include "KHttpPost.h"
#include "kmalloc.h"
#include "KUrlParser.h"
#include "KFileName.h"
int get_tmp(char m_char) {
	if (m_char <= '9' && m_char >= '0')
		return 0x30;
	if (m_char <= 'f' && m_char >= 'a')
		return 0x57;
	if (m_char <= 'F' && m_char >= 'A')
		return 0x37;
	return 0;

}

bool KHttpPost::parseUrlParam(char *param,KUrlValue *uv) {
	char *name;
	char *value;
	char *tmp;
	char *msg;
	char *ptr;
	for (size_t i = 0; i < strlen(param); i++) {
		if (param[i] == '\r' || param[i] == '\n') {
			param[i] = 0;
			break;
		}
	}
	//	url_unencode(param);
	//printf("param=%s.\n",param);
	tmp = param;
	char split = '&';
	//	strcpy(split,"=");
	while ((msg = my_strtok(tmp, split, &ptr)) != NULL) {
		tmp = NULL;
		char *p = strchr(msg,'=');
		if(p==NULL){
			value=(char *)"";
		}else{
			*p = '\0';
			value = p+1;
			if(urldecode){
				url_decode(value,0,NULL,true);
			}			
		}
		name = msg;
		if(urldecode){
			url_decode(name,0,NULL,true);
		}
		uv->add(name,value);
	}
	return true;
}
KHttpPost::KHttpPost()
{
	buffer = NULL;
	urldecode = true;
}

KHttpPost::~KHttpPost(void)
{
	if(buffer){
		xfree(buffer);
	}
}
bool KHttpPost::init(int totalLen)
{
	if(totalLen>MAX_POST_SIZE){
		return false;
	} 
	this->totalLen = totalLen;
	buffer = (char *)xmalloc(totalLen+1);
	hot = buffer;
	return buffer!=NULL;

}
bool KHttpPost::addData(const char *data,int len)
{
	int used = hot - buffer;
	if(totalLen-used < len){
		return false;
	}
	kgl_memcpy(hot,data,len);
	hot+=len;
	return true;
}
bool KHttpPost::readData(KRStream *st)
{	
	int used = hot - buffer;
	int leftRead = totalLen - used;
	for(;;){
		if(leftRead<=0){
			return true;
		}
		int read_len = st->read(hot,leftRead);
		if(read_len<=0){
			return false;
		}
		leftRead-=read_len;
		hot+=read_len;
	}
}
bool KHttpPost::parse(KUrlValue *uv)
{
	*hot = '\0';
	//printf("post data=[%s]\n",buffer);
	return parseUrlParam(buffer,uv);
}
