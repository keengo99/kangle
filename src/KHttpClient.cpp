#include "KHttpClient.h"
int KHttpClient::Request(const char* meth, const char* url, int64_t content_length, KHttpHeader* header)
{
	return -1;
}
bool KHttpClient::Post(const char* data, size_t length)
{
	return false;
}
KHttpHeader* KHttpClient::GetResponseHeader()
{
	return NULL;
}
uint16_t KHttpClient::GetResponseStatus()
{
	return 0;
}
int KHttpClient::Read(char* buf, int length)
{
	return -1;
}