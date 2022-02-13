/*
 * export.h
 *
 *  Created on: 2010-6-13
 *      Author: keengo
 */

#ifndef EXPORT_H_
#define EXPORT_H_
#include "khttpext.h"
#ifdef __cplusplus
extern "C" {
#endif
BOOL WINAPI GetServerVariable(HCONN hConn, LPSTR lpszVariableName,
		LPVOID lpvBuffer, LPDWORD lpdwSize);

BOOL  WINAPI WriteClient(HCONN ConnID, LPVOID Buffer, LPDWORD lpdwBytes,
		DWORD dwReserved);

BOOL  WINAPI ReadClient(HCONN ConnID, LPVOID lpvBuffer, LPDWORD lpdwSize);

BOOL  WINAPI ServerSupportFunction(HCONN hConn, DWORD dwHSERequest,
		LPVOID lpvBuffer, LPDWORD lpdwSize, LPDWORD lpdwDataType);
#ifdef __cplusplus
}
#endif
#endif /* EXPORT_H_ */
