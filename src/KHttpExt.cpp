/*
 * HttpExt.cpp
 * 为isapi接口提供服务，仅提供沟通作用，实际服务提供由KApiFetchObject类提供。
 *
 *  Created on: 2010-6-13
 *      Author: keengo
 */
#include <errno.h>
#include <vector>
#include <string.h>
#include "KApiFetchObject.h"
#include "export.h"
#include "KStringBuf.h"
#include "kmalloc.h"
#include "KApiService.h"
#ifndef _WIN32
//使用gcc中的libiconv
#define LIBICONV_PLUG 1
#endif
#include "iconv.h"

using namespace std;
static struct {
	const char *name;
	const char *alias;
} varAlais[] = {
	{ "HTTPS_KEYSIZE", "CERT_KEYSIZE" },
	{ "HTTPS_SERVER_ISSUER","CERT_SERVER_ISSUER" },
	{ "HTTPS_SERVER_SUBJECT", "CERT_SERVER_SUBJECT" }, 
	{"SCRIPT_TRANSLATED", "SCRIPT_FILENAME" }, 
	{"APPL_PHYSICAL_PATH", "DOCUMENT_ROOT" }, 
	{ "REMOTE_HOST","REMOTE_ADDR" }, 
	{ "URL", "REQUEST_URI" },
	{ NULL,	NULL } 
};

BOOL setVariable(LPVOID lpvBuffe, LPDWORD lpdwSize, const char *val,
		bool unicode = false) {
	char *buffer = (char *) lpvBuffe;
	if (val == NULL) {
		*lpdwSize = 1;
		if (buffer) {
			buffer[0] = 0;
		}
		if (unicode) {
			if (buffer) {
				buffer[1] = '\0';
			}
			*lpdwSize += 1;
		}
		SetLastError(ERROR_NO_DATA);
		return FALSE;
	}

	unsigned len = strlen(val);
//{{ent
#ifdef _WIN32
	if(unicode) {
		len = MultiByteToWideChar(CP_ACP,0,val,len,(LPWSTR)lpvBuffe,*lpdwSize/2);
		if(buffer) {
			buffer[2*len] = '\0';
			buffer[2*len+1] = '\0';
		}
		*lpdwSize = (len+1)*2;
	} else {
#endif
//}}
	if (*lpdwSize > len || buffer == NULL) {
		*lpdwSize = len + 1;
		if (buffer) {
			strncpy(buffer, val, *lpdwSize);
		}
	}else{
		*lpdwSize = len + 1;
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return FALSE;
		
	}
//{{ent
#ifdef _WIN32
}
#endif
//}}
	return TRUE;
}
BOOL WINAPI GetServerVariable(HCONN hConn, LPSTR lpszVariableName,LPVOID lpvBuffer, LPDWORD lpdwSize) {
	KStringBuf s(512);
	KApiService *fo = (KApiService *) hConn;
	//debug("fo=[%p]\n",fo);
	assert(fo);
	if (fo == NULL) {
		return FALSE;
	}
	BOOL result = TRUE;
	bool findAlias = false;
	//debug("api try get [%s],max_size=%d\n", lpszVariableName,(int)*lpdwSize);
	bool unicode = false;
	if (strncasecmp(lpszVariableName, "UNICODE_", 8) == 0) {
		unicode = true;
		lpszVariableName += 8;
	}
	if (strcasecmp(lpszVariableName, "CACHE_URL") == 0) {
		s << "http://" << fo->env.getEnv("SERVER_NAME") << ":"
				<< fo->env.getEnv("SERVER_PORT") << fo->env.getEnv(
				"SCRIPT_NAME");
		result = setVariable(lpvBuffer, lpdwSize, s.getString(), unicode);
		goto done;
	}
	if (strcasecmp(lpszVariableName, "ALL_HTTP") == 0) {
		if(fo->env.getAllHttp((char *) lpvBuffer, (int *) lpdwSize)){
			result = TRUE;
		}else{
			result = FALSE;
		}
		goto done;
	}

	if (strcasecmp(lpszVariableName, "ALL_RAW") == 0) {
		fo->env.getAllRaw(s);
		result = setVariable(lpvBuffer, lpdwSize, s.getString(), unicode);
		goto done;
	}
	for (int i = 0;; i++) {
		if (varAlais[i].name == NULL) {
			break;
		}
		if (strcasecmp(lpszVariableName, varAlais[i].name) == 0) {
			result = setVariable(lpvBuffer, lpdwSize, fo->env.getEnv(
					varAlais[i].alias), unicode);
			findAlias = true;
			break;
		}
	}
	if (!findAlias) {
		result = setVariable(lpvBuffer, lpdwSize, fo->env.getEnv(
				lpszVariableName), unicode);
	}
	done:
	//
	//
#if 0
	if(result==FALSE) {
		debug("failed\n");
	} else {
		if(unicode) {
			for(int i=0;i<*lpdwSize;i++) {
				debug("%c",((char *)lpvBuffer)[i]);
			}
			debug("\n");
		} else {
			debug("get value [%s],len=%d\n", lpvBuffer, *lpdwSize);
		}
	}
#endif
	return result;
}

BOOL WINAPI WriteClient(HCONN hConn, LPVOID Buffer, LPDWORD lpdwBytes,
		DWORD dwReserved) {
	KApiService *fo = (KApiService *) hConn;
	assert(fo);
	if (fo == NULL) {
		return FALSE;
	}
	int len = fo->writeClient((char *) Buffer, *lpdwBytes);
	//	debug("success writeClient size=%d\n",*lpdwBytes);
	if (len < 0) {
		return FALSE;
	}
	*lpdwBytes = len;
	return TRUE;
}

BOOL WINAPI ReadClient(HCONN hConn, LPVOID lpvBuffer, LPDWORD lpdwSize) {
	KApiService *fo = (KApiService *) hConn;
	assert(fo);
	if (fo == NULL || fo->ecb.cbLeft <= 0) {
		return FALSE;
	}
	//	debug("try to readClient size=%d\n",*lpdwSize);
	//int len = fo->tr.rq->sink->read((char *) lpvBuffer, *lpdwSize);
	INT64 read_len = MIN((INT64)*lpdwSize, fo->ecb.cbLeft);
	int len = fo->readClient((char *)lpvBuffer, (int)read_len);
	//	debug("success readClient size=%d\n",*lpdwSize);
	if (len <= 0) {
		return FALSE;
	}
	*lpdwSize = len;
	fo->ecb.cbLeft -= len;
	return TRUE;
}

BOOL WINAPI ServerSupportFunction(HCONN hConn, DWORD dwHSERequest,
		LPVOID lpvBuffer, LPDWORD lpdwSize, LPDWORD lpdwDataType) {
	KApiService *fo = (KApiService *) hConn;
	assert(fo);
	if (fo == NULL) {
		return FALSE;
	}
	//debug("api call function id=%d\n", dwHSERequest);
	if (dwHSERequest == HSE_REQ_MAP_URL_TO_PATH_EX || dwHSERequest == HSE_REQ_MAP_URL_TO_PATH) {
		char *buffer = (char *) lpvBuffer;
		const char *script_name = fo->env.getEnv("SCRIPT_NAME");
		if (strcmp(buffer,script_name)==0) {	
			BOOL result ;
			if (dwHSERequest == HSE_REQ_MAP_URL_TO_PATH_EX) {
				HSE_URL_MAPEX_INFO *info = (HSE_URL_MAPEX_INFO *) lpdwDataType;
				memset(info, 0, sizeof(HSE_URL_MAPEX_INFO));
				info->dwFlags = HSE_URL_FLAGS_READ | HSE_URL_FLAGS_EXECUTE;
				*lpdwSize = MAX_PATH;
				info->cchMatchingURL = strlen(buffer) + 1;
				result = setVariable(info->lpszPath, lpdwSize, fo->env.getEnv("SCRIPT_FILENAME"), false);
				info->cchMatchingPath = *lpdwSize;
			} else {
				result = setVariable(lpvBuffer, lpdwSize, fo->env.getEnv("SCRIPT_FILENAME"), false);
			}
			return result;
		}
		HSE_URL_MAPEX_INFO *info = (HSE_URL_MAPEX_INFO *) lpdwDataType;
		const char *root = fo->env.getEnv("DOCUMENT_ROOT");
		if (root == NULL) {
			debug("DOCUMENT_ROOT is NULL\n");
			return true;
		}
		KStringBuf s(256);
		s << root;
		if (buffer[0] == '/') {
			s << buffer + 1;
		} else {
			s << buffer;
		}
		char *val = s.getString();
		int len = strlen(val);
		if(dwHSERequest == HSE_REQ_MAP_URL_TO_PATH){
			if((int)*lpdwSize < len + 1){
				*lpdwSize = len + 1;
				SetLastError(ERROR_INSUFFICIENT_BUFFER);
				return FALSE;
			}
			strncpy(buffer,val,len);
			buffer[len] = '\0';
		}else{
			if (len > MAX_PATH) {
				debug("len is too long\n");
				return FALSE;
			}		
			memset(info, 0, sizeof(HSE_URL_MAPEX_INFO));
			strncpy(info->lpszPath, val, len);
			info->dwFlags = HSE_URL_FLAGS_READ | HSE_URL_FLAGS_EXECUTE;
			info->cchMatchingPath = len + 1;
			info->cchMatchingURL = strlen(buffer) + 1;
		}
		*lpdwSize = len + 1;		
		//		debug("success map[%s] to [%s] len=%d\n", buffer, val, len);
		return TRUE;
	}
	if (dwHSERequest == HSE_REQ_SEND_RESPONSE_HEADER_EX) {
		HSE_SEND_HEADER_EX_INFO *info = (HSE_SEND_HEADER_EX_INFO *) lpvBuffer;
		//debug("info.status=%s\n",info->pszStatus);
		if (info->cchStatus > 0) {
			fo->setStatusCode(info->pszStatus, info->cchStatus);
		}
		if (info->cchHeader > 0) {
			fo->addHeader(info->pszHeader, info->cchHeader);
		}
		//debug("header = [%s]\n",info->pszHeader);
		return true;

	}
	if (dwHSERequest == HSE_REQ_SEND_RESPONSE_HEADER) {
		char * status = (char *) lpvBuffer;
		char * header = (char *) lpdwDataType;
		if (status) {
			fo->setStatusCode(status, strlen(status));
		}
		if (header) {
			fo->addHeader(header, strlen(header));
		}
		return true;
	}
//{{ent
#ifdef _WIN32
	if(dwHSERequest == HSE_REQ_VECTOR_SEND) {
		HSE_RESPONSE_VECTOR *info = (HSE_RESPONSE_VECTOR *)lpvBuffer;
		bool asyn = false;
		if(KBIT_TEST(info->dwFlags,HSE_IO_ASYNC)) {
			asyn = true;
		}
		if(KBIT_TEST(info->dwFlags,HSE_IO_SEND_HEADERS)) {
			fo->setStatusCode(info->pszStatus);
			fo->addHeader(info->pszHeaders);
		}
		for(size_t i=0;i<info->nElementCount;i++) {
			if(info->lpElementArray[i].ElementType==HSE_VECTOR_ELEMENT_TYPE_MEMORY_BUFFER) {
				char *buffer = (char *)(info->lpElementArray[i].pvContext);
				fo->writeClient(buffer+info->lpElementArray[i].cbOffset,(int)info->lpElementArray[i].cbSize,asyn);
			} else {
				debug("ISAPI: unsupport ElementType=%d\n",info->lpElementArray[i].ElementType);
			}
		}
		return TRUE;
	}
	if(dwHSERequest == HSE_REQ_GET_TRACE_INFO) {
		HSE_TRACE_INFO *info = (HSE_TRACE_INFO *)lpvBuffer;
		memset(info,0,sizeof(HSE_TRACE_INFO));
		info->fTraceRequest = FALSE;
		return TRUE;
	}
#endif
//}}
	if (dwHSERequest == HSE_REQ_GET_IMPERSONATION_TOKEN) {
		Token_t token = fo->getToken();
		if (token) {
			kgl_memcpy(lpvBuffer, &token, sizeof(Token_t));
			return TRUE;
		}
		return FALSE;
	}
	if (dwHSERequest == HSE_REQ_UTF8_TO_LOCALE) {
		const char *buffer = (const char *) lpvBuffer;
		lpdwSize = (LPDWORD) utf82charset(buffer, strlen(buffer), "UNICODE");
		return TRUE;
	}
	switch(dwHSERequest) {
	case HSE_APPEND_LOG_PARAMETER:
		{
			klog(KLOG_ERR,"%s",(const char *)lpvBuffer);
			return TRUE;
		}
	default:
		debug("*****call unsupported function %d\n", dwHSERequest);
		return FALSE;
	}
}
#ifndef _WIN32
void SetLastError(DWORD errorCode)
{
	errno = errorCode;
}
#endif
char *utf82charset(const char *str, size_t len, const char *charset) {
	iconv_t cp = iconv_open(charset, "UTF-8");
	if (cp == (iconv_t) -1) {
		return NULL;
	}
	size_t buf_len = 2 * len + 3;
	char *buf = (char *) malloc(buf_len);
	char *buf_str = buf;
	size_t ret_len;
	memset(buf, 0, buf_len);
#ifdef _LIBICONV_VERSION
	ret_len = iconv(cp, (const char **)&str, &len, &buf_str, &buf_len);
#else
	ret_len = iconv(cp, (char **)&str, &len, &buf_str, &buf_len);
#endif
	iconv_close(cp);
	char *ret_str = strdup(buf);
	free(buf);
	return ret_str;

}
