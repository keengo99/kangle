/*
 * KHttpFieldValue.cpp
 *
 *  Created on: 2010-6-3
 *      Author: keengo
 */
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include "KHttpFieldValue.h"
#include "kforwin32.h"

KHttpFieldValue::KHttpFieldValue(const char *val) {
	this->val = val;
}
KHttpFieldValue::~KHttpFieldValue() {
}
bool KHttpFieldValue::next() {
	val = strchr(val, ',');
	if (val == NULL) {
		return false;
	}
	val++;
	while (*val && isspace((unsigned char)*val)) {
		val++;
	}
	if (val[0] == '\0') {
		return false;
	}
	return true;
}
bool KHttpFieldValue::have(const char *field) {
	for (;;) {
		if (is(field)) {
			return true;
		}
		if (!next()) {
			return false;
		}
	}
}
bool KHttpFieldValue::is2(const char *field, int n)
{
	return strncasecmp(val, field, n) == 0;
}
bool KHttpFieldValue::is(const char *field) {
	if (strncasecmp(val, field, strlen(field)) == 0) {
		return true;
	}
	return false;
}
bool KHttpFieldValue::is(const char *field, int *n) {
	int len = strlen(field);
	if (strncasecmp(val, field, len) == 0) {
		*n = atoi(val + len);
		return true;
	}
	return false;
}
