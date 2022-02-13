/*
 * KHttpField.cpp
 *
 *  Created on: 2010-7-15
 *      Author: keengo
 */

#include "KHttpField.h"
#include "kmalloc.h"

KHttpField::KHttpField() {
	header = NULL;
	buf = NULL;
}

KHttpField::~KHttpField() {
	if(buf){
		xfree(buf);
	}
	while (header) {
		http_field_t *next = header->next;
		delete header;
		header = next;
	}
}
void KHttpField::parse(const char *str,const u_char split) {
	if(buf){
		xfree(buf);
	}
	buf = xstrdup(str);
	char *hot = buf;
	for (;;) {
		if (!*hot) {
			break;
		}
		http_field_t *new_t = new http_field_t();
		hot = new_t->parse(hot, split);
		new_t->next = header;
		header = new_t;
		if (hot==NULL) {
			break;
		}
	}

}
