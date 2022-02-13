/*
 * KISAPIServiceProvider.cpp
 *
 *  Created on: 2010-8-7
 *      Author: keengo
 */
#include <sstream>
#include "KISAPIServiceProvider.h"
using namespace std;
KISAPIServiceProvider::KISAPIServiceProvider() {
	meth = METH_UNSET;
	headSended = false;
}

KISAPIServiceProvider::~KISAPIServiceProvider() {
	// TODO Auto-generated destructor stub
}
int KISAPIServiceProvider::read(char *buf, int len) {
	if (pECB->ReadClient(pECB->ConnID, buf, (LPDWORD) &len) == FALSE) {
		return -1;
	}
	return len;
}
void KISAPIServiceProvider::log(const char *str)
{
	pECB->ServerSupportFunction(pECB->ConnID,HSE_APPEND_LOG_PARAMETER,(LPVOID)str,NULL,NULL);
}
bool KISAPIServiceProvider::execUrl(const char *url) {
	HSE_EXEC_URL_INFO info;
	memset(&info, 0, sizeof(info));
	info.pszUrl = (LPSTR)url;
	info.pszMethod = (char *)"GET";
	return pECB->ServerSupportFunction(pECB->ConnID, HSE_REQ_EXEC_URL,
			(LPVOID) &info, NULL, NULL) == TRUE;
}
bool KISAPIServiceProvider::getEnv(const char *name, char *value, int *len) {
	return pECB->GetServerVariable(pECB->ConnID, (LPSTR) name, (LPVOID) value,
			(LPDWORD) len) == TRUE;
}
void KISAPIServiceProvider::checkHeaderSend() {
	if (!headSended) {
		headSended = true;
		if (s.getSize() > 0 || status.getSize() > 0) {
			s << "\r\n";
			pECB->ServerSupportFunction(pECB->ConnID,
					HSE_REQ_SEND_RESPONSE_HEADER, (LPVOID) status.getString(),
					0, (LPDWORD) s.getString());
		}
	}
}
StreamState KISAPIServiceProvider::write_end() {
	checkHeaderSend();
	return STREAM_WRITE_SUCCESS;
}
int KISAPIServiceProvider::write(const char *buf, int len) {
	checkHeaderSend();
	if (pECB->WriteClient(pECB->ConnID, (void *) buf, (LPDWORD) &len, 0)
			== FALSE) {
		return -1;
	}
	return len;
}
bool KISAPIServiceProvider::sendStatus(int statusCode, const char *statusLine) {
	status.clean();
	status << statusCode << " " << (statusLine ? statusLine : "unknow");
	return true;
	//std::stringstream s;
	//s << statusCode << " " << 
	//return pECB->ServerSupportFunction(pECB->ConnID,HSE_REQ_SEND_RESPONSE_HEADER,(void *)s.str().c_str(),0,NULL)==TRUE;	
}
bool KISAPIServiceProvider::sendUnknowHeader(const char *attr, const char *val) {
	s << attr << ": " << val << "\r\n";
	return true;
}
