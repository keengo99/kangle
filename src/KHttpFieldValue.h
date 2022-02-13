/*
 * KHttpFieldValue.h
 *
 *  Created on: 2010-6-3
 *      Author: keengo
 */

#ifndef KHTTPFIELDVALUE_H_
#define KHTTPFIELDVALUE_H_

class KHttpFieldValue {
public:
	KHttpFieldValue(const char *val);
	virtual ~KHttpFieldValue();
	bool have(const char *field);
	bool is2(const char *field, int n);
	bool is(const char *field,int *n);
	bool is(const char *field);
	bool next();
private:
	const char *val;
	int val_len;
};

#endif /* KHTTPFIELDVALUE_H_ */
