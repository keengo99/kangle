#ifndef KHTTPCLIENT_H_99
#define KHTTPCLIENT_H_99
#include "KHttpHeader.h"
#include "KSimulateRequest.h"

class KHttpClient {
public:
	int Request(const char* meth, const char* url, int64_t content_length, KHttpHeader* header);
	bool Post(const char* data, size_t length);
	KHttpHeader* GetResponseHeader();
	uint16_t GetResponseStatus();
	int Read(char* buf, int length);
private:

};
#endif

