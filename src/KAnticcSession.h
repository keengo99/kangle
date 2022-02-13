#ifndef KANTICCSESSION_H
#define KANTICCSESSION_H
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "global.h"
class KAnticcSession
{
public:
	KAnticcSession(char *url)
	{		
		unsigned r = rand();
		//È¡4Î»Êý
		number = (r % 10000);
		this->url = url;
	}
	KAnticcSession()
	{
		memset(this,0,sizeof(KAnticcSession));
	}
	~KAnticcSession()
	{
		if (url) {
			free(url);
		}
	}
	int key;
	int number;
	char *url;
	time_t expire_time;
	KAnticcSession *next;
	KAnticcSession *prev;
};
int create_session_number(char *url);
int anticc_session_number(int key);
char *anticc_session_verify(int key,int &number);
void anticc_session_flush(time_t now_time);
void init_anticc_session();
#endif
