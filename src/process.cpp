/*
* Copyright (c) 2010, NanChang BangTeng Inc
*
* kangle web server              http://www.kangleweb.net/
* ---------------------------------------------------------------------
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
*  See COPYING file for detail.
*
*  Author: KangHongjiu <keengo99@gmail.com>
*/
#include <errno.h>
#include "KStringBuf.h"
#include "utils.h"
#include "KPipeStream.h"
#include "log.h"
#include "extern.h"
volatile int quit_program_flag = PROGRAM_NO_QUIT;
#ifndef _WIN32
int my_uid = -1;
#endif
static bool open_std_file(const char *filename,KFile *file)
{
	if (*filename == '+') {
		if (!file->open(filename+1,fileAppend)) {
			return false;
		}
	} else {
		if (!file->open(filename,fileWrite)) {
			return false;
		}
	}
	return true;
}
bool open_process_std(kgl_process_std *std,KFile *file)
{
	if (std->hstdin==INVALIDE_PIPE && std->stdin_file) {
		if (!file[0].open(std->stdin_file,fileRead)) {
			return false;
		}
		std->hstdin = file[0].getHandle();
	}
	if (std->hstdout==INVALIDE_PIPE && std->stdout_file) {
		if (!open_std_file(std->stdout_file,&file[1])) {
			return false;
		}
		std->hstdout = file[1].getHandle();
	}
	if (std->hstderr == INVALIDE_PIPE && std->stderr_file) {
		if (kflike(file[1].getHandle()) && filecmp(std->stderr_file,std->stdout_file)==0) {
			std->hstderr = std->hstdout;
		} else {
			if (!open_std_file(std->stderr_file,&file[2])) {
				return false;
			}
			std->hstderr = file[2].getHandle();
		}
	}
	return true;
}
//{{ent
#ifdef _WIN32
#include <dbghelp.h>
KMutex closeExecLock;
#ifdef NDEBUG
#define CREATEPROCESS_FLAGS  (CREATE_SUSPENDED  | DETACHED_PROCESS | CREATE_NO_WINDOW)
#else
#define CREATEPROCESS_FLAGS  (CREATE_SUSPENDED)
#endif
#define DESKTOP_ALL (DESKTOP_READOBJECTS | DESKTOP_CREATEWINDOW | \
	DESKTOP_CREATEMENU | DESKTOP_HOOKCONTROL | DESKTOP_JOURNALRECORD | \
	DESKTOP_JOURNALPLAYBACK | DESKTOP_ENUMERATE | DESKTOP_WRITEOBJECTS | \
	DESKTOP_SWITCHDESKTOP | STANDARD_RIGHTS_REQUIRED)

#define WINSTA_ALL (WINSTA_ENUMDESKTOPS | WINSTA_READATTRIBUTES | \
	WINSTA_ACCESSCLIPBOARD | WINSTA_CREATEDESKTOP | \
	WINSTA_WRITEATTRIBUTES | WINSTA_ACCESSGLOBALATOMS | \
	WINSTA_EXITWINDOWS | WINSTA_ENUMERATE | WINSTA_READSCREEN | \
	STANDARD_RIGHTS_REQUIRED)

#define GENERIC_ACCESS (GENERIC_READ | GENERIC_WRITE | \
	GENERIC_EXECUTE | GENERIC_ALL)

BOOL AddAceToWindowStation(HWINSTA hwinsta, PSID psid);

BOOL AddAceToDesktop(HDESK hdesk, PSID psid);
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib,"Dbghelp.lib")

VOID FreeLogonSID (PSID *ppsid) 
{
	HeapFree(GetProcessHeap(), 0, (LPVOID)*ppsid);
}
BOOL GetLogonSID (HANDLE hToken, PSID *ppsid) 
{
	BOOL bSuccess = FALSE;
	DWORD dwIndex;
	DWORD dwLength = 0;
	PTOKEN_GROUPS ptg = NULL;

	// Verify the parameter passed in is not NULL.
	if (NULL == ppsid)
		goto Cleanup;

	// Get required buffer size and allocate the TOKEN_GROUPS buffer.

	if (!GetTokenInformation(
		hToken,         // handle to the access token
		TokenGroups,    // get information about the token's groups 
		(LPVOID) ptg,   // pointer to TOKEN_GROUPS buffer
		0,              // size of buffer
		&dwLength       // receives required buffer size
		)) 
	{
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) 
			goto Cleanup;

		ptg = (PTOKEN_GROUPS)HeapAlloc(GetProcessHeap(),
			HEAP_ZERO_MEMORY, dwLength);

		if (ptg == NULL)
			goto Cleanup;
	}

	// Get the token group information from the access token.

	if (!GetTokenInformation(
		hToken,         // handle to the access token
		TokenGroups,    // get information about the token's groups 
		(LPVOID) ptg,   // pointer to TOKEN_GROUPS buffer
		dwLength,       // size of buffer
		&dwLength       // receives required buffer size
		)) 
	{
		goto Cleanup;
	}

	// Loop through the groups to find the logon SID.

	for (dwIndex = 0; dwIndex < ptg->GroupCount; dwIndex++) 
		if ((ptg->Groups[dwIndex].Attributes & SE_GROUP_LOGON_ID)
			==  SE_GROUP_LOGON_ID) 
		{
			// Found the logon SID; make a copy of it.

			dwLength = GetLengthSid(ptg->Groups[dwIndex].Sid);
			*ppsid = (PSID) HeapAlloc(GetProcessHeap(),
				HEAP_ZERO_MEMORY, dwLength);
			if (*ppsid == NULL)
				goto Cleanup;
			if (!CopySid(dwLength, *ppsid, ptg->Groups[dwIndex].Sid)) 
			{
				HeapFree(GetProcessHeap(), 0, (LPVOID)*ppsid);
				goto Cleanup;
			}
			break;
		}

		bSuccess = TRUE;

Cleanup: 

		// Free the buffer for the token groups.

		if (ptg != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)ptg);

		return bSuccess;
}

BOOL EnablePrivilege(HANDLE hToken,PTSTR szPriv, BOOL fEnabled=TRUE) 
{

	// First lookup the system unique luid for the privilege
	LUID luid;
	if(!LookupPrivilegeValue(NULL, szPriv, &luid)) 
	{
		// If the name is bogus...
		return FALSE;
	}
	// Set up our token privileges "array" (in our case an array of one)
	TOKEN_PRIVILEGES tp;
	tp.PrivilegeCount             = 1;
	tp.Privileges[0].Luid         = luid;
	tp.Privileges[0].Attributes = fEnabled ? SE_PRIVILEGE_ENABLED : 0;

	// Adjust our token privileges by enabling or disabling this one
	if(!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES),
		NULL, NULL)) 
	{
		return FALSE;
	}
	return TRUE;  
}

void EnableNeccessPrivilege()
{    
	HANDLE hToken = NULL;
	// Then get the processes token
	if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES,
		&hToken)) 
	{
		klog(KLOG_ERR,"Cann't OpenProcessToken\n");
		return;
	}
	if(!EnablePrivilege(hToken,SE_ASSIGNPRIMARYTOKEN_NAME)){
		klog(KLOG_ERR,"Cann't enable SE_ASSIGNPRIMARYTOKEN_NAME privilege\n");
	}
	if(!EnablePrivilege(hToken,SE_INCREASE_QUOTA_NAME)){
		klog(KLOG_ERR,"Cann't enable SE_INCREASE_QUOTA_NAME privilege\n");
	}
	if(!EnablePrivilege(hToken,SE_AUDIT_NAME)){
		klog(KLOG_ERR,"Cann't enable SE_AUDIT_NAME privilege\n");
	}
	if(!EnablePrivilege(hToken,SE_RESTORE_NAME)){
		klog(KLOG_WARNING,"Cann't enable SE_RESTORE_NAME privilege\n");
	}
	CloseHandle(hToken);

}
BOOL RemoveAceFromWindowStation(HWINSTA hwinsta, PSID psid)
{
	ACL_SIZE_INFORMATION aclSizeInfo;
	BOOL                 bDaclExist;
	BOOL                 bDaclPresent;
	BOOL                 bSuccess = FALSE;
	DWORD                dwNewAclSize;
	DWORD                dwSidSize = 0;
	DWORD                dwSdSizeNeeded;
	PACL                 pacl;
	PACL                 pNewAcl;
	PSECURITY_DESCRIPTOR psd = NULL;
	PSECURITY_DESCRIPTOR psdNew = NULL;
	ACCESS_ALLOWED_ACE*  pTempAce;
	SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
	unsigned int         i;

	__try
	{
		// Obtain the DACL for the window station.

		if (!GetUserObjectSecurity(
			hwinsta,
			&si,
			psd,
			dwSidSize,
			&dwSdSizeNeeded)
			)
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				psd = (PSECURITY_DESCRIPTOR)HeapAlloc(
					GetProcessHeap(),
					HEAP_ZERO_MEMORY,
					dwSdSizeNeeded);

				if (psd == NULL)
					__leave;

				psdNew = (PSECURITY_DESCRIPTOR)HeapAlloc(
					GetProcessHeap(),
					HEAP_ZERO_MEMORY,
					dwSdSizeNeeded);

				if (psdNew == NULL)
					__leave;

				dwSidSize = dwSdSizeNeeded;

				if (!GetUserObjectSecurity(
					hwinsta,
					&si,
					psd,
					dwSidSize,
					&dwSdSizeNeeded)
					)
					__leave;
			}
			else
				__leave;

		// Create a new DACL.

		if (!InitializeSecurityDescriptor(
			psdNew,
			SECURITY_DESCRIPTOR_REVISION)
			)
			__leave;

		// Get the DACL from the security descriptor.

		if (!GetSecurityDescriptorDacl(
			psd,
			&bDaclPresent,
			&pacl,
			&bDaclExist)
			)
			__leave;

		// Initialize the ACL.

		ZeroMemory(&aclSizeInfo, sizeof(ACL_SIZE_INFORMATION));
		aclSizeInfo.AclBytesInUse = sizeof(ACL);

		// Call only if the DACL is not NULL.

		if (pacl != NULL)
		{
			// get the file ACL size info
			if (!GetAclInformation(
				pacl,
				(LPVOID)&aclSizeInfo,
				sizeof(ACL_SIZE_INFORMATION),
				AclSizeInformation)
				)
				__leave;
		}

		// Compute the size of the new ACL.

		dwNewAclSize = aclSizeInfo.AclBytesInUse +
			(2*sizeof(ACCESS_ALLOWED_ACE)) + (2*GetLengthSid(psid)) -
			(2*sizeof(DWORD));

		// Allocate memory for the new ACL.

		pNewAcl = (PACL)HeapAlloc(
			GetProcessHeap(),
			HEAP_ZERO_MEMORY,
			dwNewAclSize);

		if (pNewAcl == NULL)
			__leave;

		// Initialize the new DACL.

		if (!InitializeAcl(pNewAcl, dwNewAclSize, ACL_REVISION))
			__leave;

		// If DACL is present, copy it to a new DACL.

		if (bDaclPresent)
		{
			// Copy the ACEs to the new ACL.
			if (aclSizeInfo.AceCount)
			{
				for (i=0; i < aclSizeInfo.AceCount; i++)
				{
					// Get an ACE.
					if (!GetAce(pacl, i, reinterpret_cast<void**>(&pTempAce)))
						__leave;

					if ( !EqualSid( psid, &pTempAce->SidStart ) )
					{

						// Add the ACE to the new ACL.
						if (!AddAce(
							pNewAcl,
							ACL_REVISION,
							MAXDWORD,
							pTempAce,
							((PACE_HEADER)pTempAce)->AceSize)
							)
							__leave;
					}
				}
			}
		}

		// Set a new DACL for the security descriptor.

		if (!SetSecurityDescriptorDacl(
			psdNew,
			TRUE,
			pNewAcl,
			FALSE)
			)
			__leave;

		// Set the new security descriptor for the window station.

		if (!SetUserObjectSecurity(hwinsta, &si, psdNew))
			__leave;

		// Indicate success.

		bSuccess = TRUE;
	}
	__finally
	{
		// Free the allocated buffers.

		if (pNewAcl != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)pNewAcl);

		if (psd != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)psd);

		if (psdNew != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)psdNew);
	}

	return bSuccess;

}

BOOL RemoveAceFromDesktop(HDESK hdesk, PSID psid)
{
	ACL_SIZE_INFORMATION aclSizeInfo;
	BOOL                 bDaclExist;
	BOOL                 bDaclPresent;
	BOOL                 bSuccess = FALSE;
	DWORD                dwNewAclSize;
	DWORD                dwSidSize = 0;
	DWORD                dwSdSizeNeeded;
	PACL                 pacl;
	PACL                 pNewAcl;
	PSECURITY_DESCRIPTOR psd = NULL;
	PSECURITY_DESCRIPTOR psdNew = NULL;
	ACCESS_ALLOWED_ACE*  pTempAce;
	SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
	unsigned int         i;

	__try
	{
		// Obtain the security descriptor for the desktop object.

		if (!GetUserObjectSecurity(
			hdesk,
			&si,
			psd,
			dwSidSize,
			&dwSdSizeNeeded))
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				psd = (PSECURITY_DESCRIPTOR)HeapAlloc(
					GetProcessHeap(),
					HEAP_ZERO_MEMORY,
					dwSdSizeNeeded );

				if (psd == NULL)
					__leave;

				psdNew = (PSECURITY_DESCRIPTOR)HeapAlloc(
					GetProcessHeap(),
					HEAP_ZERO_MEMORY,
					dwSdSizeNeeded);

				if (psdNew == NULL)
					__leave;

				dwSidSize = dwSdSizeNeeded;

				if (!GetUserObjectSecurity(
					hdesk,
					&si,
					psd,
					dwSidSize,
					&dwSdSizeNeeded)
					)
					__leave;
			}
			else
				__leave;
		}

		// Create a new security descriptor.

		if (!InitializeSecurityDescriptor(
			psdNew,
			SECURITY_DESCRIPTOR_REVISION)
			)
			__leave;

		// Obtain the DACL from the security descriptor.

		if (!GetSecurityDescriptorDacl(
			psd,
			&bDaclPresent,
			&pacl,
			&bDaclExist)
			)
			__leave;

		// Initialize.

		ZeroMemory(&aclSizeInfo, sizeof(ACL_SIZE_INFORMATION));
		aclSizeInfo.AclBytesInUse = sizeof(ACL);

		// Call only if NULL DACL.

		if (pacl != NULL)
		{
			// Determine the size of the ACL information.

			if (!GetAclInformation(
				pacl,
				(LPVOID)&aclSizeInfo,
				sizeof(ACL_SIZE_INFORMATION),
				AclSizeInformation)
				)
				__leave;
		}

		// Compute the size of the new ACL.

		dwNewAclSize = aclSizeInfo.AclBytesInUse +
			sizeof(ACCESS_ALLOWED_ACE) +
			GetLengthSid(psid) - sizeof(DWORD);

		// Allocate buffer for the new ACL.

		pNewAcl = (PACL)HeapAlloc(
			GetProcessHeap(),
			HEAP_ZERO_MEMORY,
			dwNewAclSize);

		if (pNewAcl == NULL)
			__leave;

		// Initialize the new ACL.

		if (!InitializeAcl(pNewAcl, dwNewAclSize, ACL_REVISION))
			__leave;

		// If DACL is present, copy it to a new DACL.

		if (bDaclPresent)
		{
			// Copy the ACEs to the new ACL.
			if (aclSizeInfo.AceCount)
			{
				for (i=0; i < aclSizeInfo.AceCount; i++)
				{
					// Get an ACE.
					if (!GetAce(pacl, i, reinterpret_cast<void**>(&pTempAce)))
						__leave;

					if ( !EqualSid( psid, &pTempAce->SidStart ) )
					{

						// Add the ACE to the new ACL.
						if (!AddAce(
							pNewAcl,
							ACL_REVISION,
							MAXDWORD,
							pTempAce,
							((PACE_HEADER)pTempAce)->AceSize)
							)
							__leave;
					}
				}
			}
		}

		// Set new DACL to the new security descriptor.

		if (!SetSecurityDescriptorDacl(
			psdNew,
			TRUE,
			pNewAcl,
			FALSE)
			)
			__leave;

		// Set the new security descriptor for the desktop object.

		if (!SetUserObjectSecurity(hdesk, &si, psdNew))
			__leave;

		// Indicate success.

		bSuccess = TRUE;
	}
	__finally
	{
		// Free buffers.

		if (pNewAcl != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)pNewAcl);

		if (psd != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)psd);

		if (psdNew != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)psdNew);
	}

	return bSuccess;
}

BOOL StartInteractiveClientProcess2 (
	HANDLE hToken,
	const char * lpApplication,
	const char * lpCommandLine ,
	const char * currentDirectory,
	kgl_process_std *std,
	LPVOID env,
	HANDLE &hProcess
	) 
{
	if (hToken) {
		init_winuser(false);
		if (!ImpersonateLoggedOnUser(hToken)) {
			return FALSE;
		}
	}
	kgl_process_std s;
	kgl_memcpy(&s,std,sizeof(s));
	KFile file[3];
	if (!open_process_std(&s,file)) {
		if (hToken) {
			RevertToSelf();
		}
		return FALSE;
	}
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	BOOL bResult = FALSE;
	int errorcode = 0;
	ZeroMemory(&si, sizeof(STARTUPINFO));
	GetStartupInfo(&si);
	si.cb= sizeof(STARTUPINFO);
	si.lpDesktop = TEXT("winsta0\\default");
	BOOL inheritHandle = FALSE;
	if (s.hstdin!=INVALID_HANDLE_VALUE || s.hstdout!=INVALID_HANDLE_VALUE || s.hstderr!=INVALID_HANDLE_VALUE) {
		inheritHandle = TRUE;
	}
	char *curdir = NULL ;
	char *cmd = strdup(lpCommandLine);
	if(currentDirectory){
		curdir = (char *)currentDirectory;
	} else if (lpApplication) {
		curdir = getPath(lpApplication);
	}
	closeExecLock.Lock();
	si.dwFlags = STARTF_USESHOWWINDOW|STARTF_USESTDHANDLES;
	si.wShowWindow = SW_HIDE;
	si.hStdInput = s.hstdin;
	si.hStdOutput = s.hstdout;
	si.hStdError = s.hstderr;
	if (INVALID_HANDLE_VALUE!=s.hstdin) {
		kfile_close_on_exec(s.hstdin,false);
	}
	if (INVALID_HANDLE_VALUE!=s.hstdout) {
		kfile_close_on_exec(s.hstdout,false);
	}
	if (INVALID_HANDLE_VALUE!=s.hstderr) {
		kfile_close_on_exec(s.hstderr,false);
	}  
	if (hToken) {
		bResult = CreateProcessAsUser(
			hToken,            // client's access token
			lpApplication,     // file to execute
			cmd,     // command line
			NULL,              // pointer to process SECURITY_ATTRIBUTES
			NULL,              // pointer to thread SECURITY_ATTRIBUTES
			inheritHandle,             // handles are not inheritable
			CREATEPROCESS_FLAGS ,   // creation flags
			env,              // pointer to new environment block 
			curdir,              // name of current directory 
			&si,               // pointer to STARTUPINFO structure
			&pi                // receives information about new process
			);
	} else {
		bResult = CreateProcess(
			lpApplication,              // file to execute
			cmd,     // command line
			NULL,              // pointer to process SECURITY_ATTRIBUTES
			NULL,              // pointer to thread SECURITY_ATTRIBUTES
			inheritHandle,             // handles are not inheritable
			CREATEPROCESS_FLAGS ,   // creation flags,   // creation flags
			env,              // pointer to new environment block 
			curdir,              // name of current directory 
			&si,               // pointer to STARTUPINFO structure
			&pi                // receives information about new process
			);
	}
	if (INVALID_HANDLE_VALUE!=s.hstdin) {
		kfile_close_on_exec(s.hstdin,true);
	}
	if (INVALID_HANDLE_VALUE!=s.hstdout) {
		kfile_close_on_exec(s.hstdout,true);
	}
	if (INVALID_HANDLE_VALUE!=s.hstderr) {
		kfile_close_on_exec(s.hstderr,true);
	} 
	if (bResult) {
		ResumeThread(pi.hThread);
	} 
	closeExecLock.Unlock();
	free(cmd);
	if (curdir && currentDirectory==NULL) {
		free(curdir);
	}
	if (hToken) {
		RevertToSelf();
	}
	if (!bResult) {
		errorcode = GetLastError();
		klog(KLOG_ERR,"CreateProcess error code=%d\n",errorcode);
	}
	if (pi.hThread != INVALID_HANDLE_VALUE) {
		CloseHandle(pi.hThread);  
	}
	hProcess = pi.hProcess;
	return bResult;
}
BOOL StartInteractiveClientProcess (
	HANDLE hToken,
	LPCSTR lpApplication,
	LPTSTR lpCommandLine ,
	KPipeStream *st,
	int isCgi,
	LPVOID env
	) 
{
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	BOOL bResult = FALSE;
	int errorcode = 0;
	ZeroMemory(&si, sizeof(STARTUPINFO));
	GetStartupInfo(&si);
	si.cb= sizeof(STARTUPINFO);
	si.lpDesktop = TEXT("winsta0\\default");
	// bool execlocked = false;
	//execlocked = true; 
	char *curdir = getPath(lpApplication);
	closeExecLock.Lock();
	if(isCgi==RDSTD_ALL || isCgi==RDSTD_INPUT){
		si.dwFlags = STARTF_USESHOWWINDOW|STARTF_USESTDHANDLES;
		si.wShowWindow = SW_HIDE;
		if(isCgi==RDSTD_ALL){
			si.hStdInput = st->fd2[0];
			si.hStdOutput = st->fd[1];
			if (INVALID_HANDLE_VALUE!=si.hStdInput) {
				kfile_close_on_exec(si.hStdInput,false);
			}
			if (INVALID_HANDLE_VALUE!=si.hStdOutput) {
				kfile_close_on_exec(si.hStdOutput,false);
			}
		}else{
			si.hStdInput = st->fd[0];
			kfile_close_on_exec(si.hStdInput,false);
			si.hStdOutput = INVALID_HANDLE_VALUE;
		}
		si.hStdError = si.hStdOutput;
	}


	//RevertToSelf();
	if(hToken){
		init_winuser(false);
		if (!ImpersonateLoggedOnUser(hToken)) {
			errorcode = GetLastError();
			//klog(KLOG_ERR,"Cann't ImpersonateLoggedOnUser error = %d\n",errorcode);
		}else{
			bResult = CreateProcessAsUser(
				hToken,            // client's access token
				lpApplication,     // file to execute
				lpCommandLine,     // command line
				NULL,              // pointer to process SECURITY_ATTRIBUTES
				NULL,              // pointer to thread SECURITY_ATTRIBUTES
				(isCgi==RDSTD_ALL||isCgi==RDSTD_INPUT),             // handles are not inheritable
				CREATEPROCESS_FLAGS ,   // creation flags
				env,              // pointer to new environment block 
				curdir,              // name of current directory 
				&si,               // pointer to STARTUPINFO structure
				&pi                // receives information about new process
				);
		}
	}else{
		bResult = CreateProcess(
			lpApplication,              // file to execute
			lpCommandLine,     // command line
			NULL,              // pointer to process SECURITY_ATTRIBUTES
			NULL,              // pointer to thread SECURITY_ATTRIBUTES
			(isCgi==RDSTD_ALL||isCgi==RDSTD_INPUT),             // handles are not inheritable
			CREATEPROCESS_FLAGS ,   // creation flags,   // creation flags
			env,              // pointer to new environment block 
			curdir,              // name of current directory 
			&si,               // pointer to STARTUPINFO structure
			&pi                // receives information about new process
			);
	}
	if(isCgi==RDSTD_ALL){
		if (INVALID_HANDLE_VALUE!=si.hStdInput) {
			kfile_close_on_exec(si.hStdInput,true);
		}
		if (INVALID_HANDLE_VALUE!=si.hStdOutput) {
			kfile_close_on_exec(si.hStdOutput,true);
			assert(si.hStdOutput);
		}
	}else if(isCgi==RDSTD_INPUT) {
		kfile_close_on_exec(si.hStdInput,true);
	}
	closeExecLock.Unlock();
	if (curdir) {
		free(curdir);
	}
	if(bResult){
		st->process.bind(pi.hProcess);
		if(isCgi==RDSTD_ALL){
			st->setPipe(0);
			ResumeThread(pi.hThread);
		}else if(isCgi==0){
			KStringBuf read_file(64),write_file(64);
			read_file << "\\\\.\\pipe\\kangle" << (int)pi.dwProcessId << "r";
			write_file << "\\\\.\\pipe\\kangle" << (int)pi.dwProcessId << "w";
			st->create_name(read_file.getString(),write_file.getString());
			bResult = FALSE;
			if(ResumeThread(pi.hThread)!=-1){
				OVERLAPPED ol;
				memset(&ol,0,sizeof(ol));
				ol.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
				if(ol.hEvent != INVALID_HANDLE_VALUE){					
					if(ConnectNamedPipe(st->fd[0],&ol)==0){
						int error = GetLastError();
						if(error == ERROR_PIPE_CONNECTED){
							bResult = TRUE;
						}else if(error==ERROR_IO_PENDING && WAIT_OBJECT_0 == WaitForSingleObject(ol.hEvent,3000)){					
							bResult = TRUE;
						}
					}
					CloseHandle(ol.hEvent);
				}
			}
		} else {
			ResumeThread(pi.hThread);
		}
	}  

	if (hToken) {
		RevertToSelf();
	}
	if(!bResult){
		errorcode = GetLastError();
		klog(KLOG_ERR,"CreateProcess error code=%d\n",errorcode);	  
	}	
	if (pi.hThread != INVALID_HANDLE_VALUE){
		CloseHandle(pi.hThread);  
	}
	return bResult;
}

BOOL AddAceToWindowStation(HWINSTA hwinsta, PSID psid)
{
	ACCESS_ALLOWED_ACE   *pace = NULL;
	ACL_SIZE_INFORMATION aclSizeInfo;
	BOOL                 bDaclExist;
	BOOL                 bDaclPresent;
	BOOL                 bSuccess = FALSE;
	DWORD                dwNewAclSize;
	DWORD                dwSidSize = 0;
	DWORD                dwSdSizeNeeded;
	PACL                 pacl;
	PACL                 pNewAcl;
	PSECURITY_DESCRIPTOR psd = NULL;
	PSECURITY_DESCRIPTOR psdNew = NULL;
	PVOID                pTempAce;
	SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
	unsigned int         i;

	__try
	{
		// Obtain the DACL for the window station.

		if (!GetUserObjectSecurity(
			hwinsta,
			&si,
			psd,
			dwSidSize,
			&dwSdSizeNeeded)
			)
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				psd = (PSECURITY_DESCRIPTOR)HeapAlloc(
					GetProcessHeap(),
					HEAP_ZERO_MEMORY,
					dwSdSizeNeeded);

				if (psd == NULL)
					__leave;

				psdNew = (PSECURITY_DESCRIPTOR)HeapAlloc(
					GetProcessHeap(),
					HEAP_ZERO_MEMORY,
					dwSdSizeNeeded);

				if (psdNew == NULL)
					__leave;

				dwSidSize = dwSdSizeNeeded;

				if (!GetUserObjectSecurity(
					hwinsta,
					&si,
					psd,
					dwSidSize,
					&dwSdSizeNeeded)
					)
					__leave;
			}
			else
				__leave;

		// Create a new DACL.

		if (!InitializeSecurityDescriptor(
			psdNew,
			SECURITY_DESCRIPTOR_REVISION)
			)
			__leave;

		// Get the DACL from the security descriptor.

		if (!GetSecurityDescriptorDacl(
			psd,
			&bDaclPresent,
			&pacl,
			&bDaclExist)
			)
			__leave;

		// Initialize the ACL.

		ZeroMemory(&aclSizeInfo, sizeof(ACL_SIZE_INFORMATION));
		aclSizeInfo.AclBytesInUse = sizeof(ACL);

		// Call only if the DACL is not NULL.

		if (pacl != NULL)
		{
			// get the file ACL size info
			if (!GetAclInformation(
				pacl,
				(LPVOID)&aclSizeInfo,
				sizeof(ACL_SIZE_INFORMATION),
				AclSizeInformation)
				)
				__leave;
		}

		// Compute the size of the new ACL.

		dwNewAclSize = aclSizeInfo.AclBytesInUse +
			(2*sizeof(ACCESS_ALLOWED_ACE)) + (2*GetLengthSid(psid)) -
			(2*sizeof(DWORD));
		// Allocate memory for the new ACL.
		pNewAcl = (PACL)HeapAlloc(
			GetProcessHeap(),
			HEAP_ZERO_MEMORY,
			dwNewAclSize);

		if (pNewAcl == NULL)
			__leave;

		// Initialize the new DACL.

		if (!InitializeAcl(pNewAcl, dwNewAclSize, ACL_REVISION))
			__leave;

		// If DACL is present, copy it to a new DACL.

		if (bDaclPresent)
		{
			// Copy the ACEs to the new ACL.
			if (aclSizeInfo.AceCount)
			{
				for (i=0; i < aclSizeInfo.AceCount; i++)
				{
					// Get an ACE.
					if (!GetAce(pacl, i, &pTempAce))
						__leave;

					// Add the ACE to the new ACL.
					if (!AddAce(
						pNewAcl,
						ACL_REVISION,
						MAXDWORD,
						pTempAce,
						((PACE_HEADER)pTempAce)->AceSize)
						)
						__leave;
				}
			}
		}

		// Add the first ACE to the window station.

		pace = (ACCESS_ALLOWED_ACE *)HeapAlloc(
			GetProcessHeap(),
			HEAP_ZERO_MEMORY,
			sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(psid) -
			sizeof(DWORD));

		if (pace == NULL)
			__leave;

		pace->Header.AceType  = ACCESS_ALLOWED_ACE_TYPE;
		pace->Header.AceFlags = CONTAINER_INHERIT_ACE |
			INHERIT_ONLY_ACE | OBJECT_INHERIT_ACE;
		pace->Header.AceSize  = (WORD)(sizeof(ACCESS_ALLOWED_ACE) +
			GetLengthSid(psid) - sizeof(DWORD));
		pace->Mask            = GENERIC_ACCESS;
		if (!CopySid(GetLengthSid(psid), &pace->SidStart, psid))
			__leave;

		if (!AddAce(
			pNewAcl,
			ACL_REVISION,
			MAXDWORD,
			(LPVOID)pace,
			pace->Header.AceSize)
			)
			__leave;

		// Add the second ACE to the window station.

		pace->Header.AceFlags = NO_PROPAGATE_INHERIT_ACE;
		pace->Mask            = WINSTA_ALL;

		if (!AddAce(
			pNewAcl,
			ACL_REVISION,
			MAXDWORD,
			(LPVOID)pace,
			pace->Header.AceSize)
			)
			__leave;

		// Set a new DACL for the security descriptor.

		if (!SetSecurityDescriptorDacl(
			psdNew,
			TRUE,
			pNewAcl,
			FALSE)
			)
			__leave;

		// Set the new security descriptor for the window station.

		if (!SetUserObjectSecurity(hwinsta, &si, psdNew))
			__leave;

		// Indicate success.

		bSuccess = TRUE;
	}
	__finally
	{
		// Free the allocated buffers.

		if (pace != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)pace);

		if (pNewAcl != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)pNewAcl);

		if (psd != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)psd);

		if (psdNew != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)psdNew);
	}

	return bSuccess;

}

BOOL AddAceToDesktop(HDESK hdesk, PSID psid)
{
	ACL_SIZE_INFORMATION aclSizeInfo;
	BOOL                 bDaclExist;
	BOOL                 bDaclPresent;
	BOOL                 bSuccess = FALSE;
	DWORD                dwNewAclSize;
	DWORD                dwSidSize = 0;
	DWORD                dwSdSizeNeeded;
	PACL                 pacl;
	PACL                 pNewAcl;
	PSECURITY_DESCRIPTOR psd = NULL;
	PSECURITY_DESCRIPTOR psdNew = NULL;
	PVOID                pTempAce;
	SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
	unsigned int         i;

	__try
	{
		// Obtain the security descriptor for the desktop object.

		if (!GetUserObjectSecurity(
			hdesk,
			&si,
			psd,
			dwSidSize,
			&dwSdSizeNeeded))
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				psd = (PSECURITY_DESCRIPTOR)HeapAlloc(
					GetProcessHeap(),
					HEAP_ZERO_MEMORY,
					dwSdSizeNeeded );

				if (psd == NULL)
					__leave;

				psdNew = (PSECURITY_DESCRIPTOR)HeapAlloc(
					GetProcessHeap(),
					HEAP_ZERO_MEMORY,
					dwSdSizeNeeded);

				if (psdNew == NULL)
					__leave;

				dwSidSize = dwSdSizeNeeded;

				if (!GetUserObjectSecurity(
					hdesk,
					&si,
					psd,
					dwSidSize,
					&dwSdSizeNeeded)
					)
					__leave;
			}
			else
				__leave;
		}

		// Create a new security descriptor.

		if (!InitializeSecurityDescriptor(
			psdNew,
			SECURITY_DESCRIPTOR_REVISION)
			)
			__leave;

		// Obtain the DACL from the security descriptor.

		if (!GetSecurityDescriptorDacl(
			psd,
			&bDaclPresent,
			&pacl,
			&bDaclExist)
			)
			__leave;

		// Initialize.

		ZeroMemory(&aclSizeInfo, sizeof(ACL_SIZE_INFORMATION));
		aclSizeInfo.AclBytesInUse = sizeof(ACL);

		// Call only if NULL DACL.

		if (pacl != NULL)
		{
			// Determine the size of the ACL information.

			if (!GetAclInformation(
				pacl,
				(LPVOID)&aclSizeInfo,
				sizeof(ACL_SIZE_INFORMATION),
				AclSizeInformation)
				)
				__leave;
		}

		// Compute the size of the new ACL.

		dwNewAclSize = aclSizeInfo.AclBytesInUse +
			sizeof(ACCESS_ALLOWED_ACE) +
			GetLengthSid(psid) - sizeof(DWORD);
		// Allocate buffer for the new ACL.

		pNewAcl = (PACL)HeapAlloc(
			GetProcessHeap(),
			HEAP_ZERO_MEMORY,
			dwNewAclSize);

		if (pNewAcl == NULL)
			__leave;

		// Initialize the new ACL.

		if (!InitializeAcl(pNewAcl, dwNewAclSize, ACL_REVISION))
			__leave;

		// If DACL is present, copy it to a new DACL.

		if (bDaclPresent)
		{
			// Copy the ACEs to the new ACL.
			if (aclSizeInfo.AceCount)
			{
				for (i=0; i < aclSizeInfo.AceCount; i++)
				{
					// Get an ACE.
					if (!GetAce(pacl, i, &pTempAce))
						__leave;

					// Add the ACE to the new ACL.
					if (!AddAce(
						pNewAcl,
						ACL_REVISION,
						MAXDWORD,
						pTempAce,
						((PACE_HEADER)pTempAce)->AceSize)
						)
						__leave;
				}
			}
		}

		// Add ACE to the DACL.

		if (!AddAccessAllowedAce(
			pNewAcl,
			ACL_REVISION,
			DESKTOP_ALL,
			psid)
			)
			__leave;

		// Set new DACL to the new security descriptor.

		if (!SetSecurityDescriptorDacl(
			psdNew,
			TRUE,
			pNewAcl,
			FALSE)
			)
			__leave;

		// Set the new security descriptor for the desktop object.

		if (!SetUserObjectSecurity(hdesk, &si, psdNew))
			__leave;

		// Indicate success.

		bSuccess = TRUE;
	}
	__finally
	{
		// Free buffers.

		if (pNewAcl != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)pNewAcl);

		if (psd != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)psd);

		if (psdNew != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)psdNew);
	}

	return bSuccess;
}

//Caller FreeSID!!!
PSID Name2SID(LPCTSTR pszUserName, LPCTSTR pszDomainName)
{
	SID_NAME_USE sidUsage;
	DWORD cbSid = 0;
	DWORD cchReferencedDomainName = MAX_PATH;
	TCHAR ReferencedDomainName[MAX_PATH];
	BOOL bRet = ::LookupAccountName(pszDomainName, pszUserName, NULL, &cbSid,
		ReferencedDomainName, &cchReferencedDomainName, &sidUsage);
	if(!bRet && (GetLastError() != ERROR_INSUFFICIENT_BUFFER))
	{
		//		err_handler(EvtLogName, _T("Name2SID LookupAccountName Size Failed"));
		return NULL;
	}
	LPVOID lpSID = ::LocalAlloc(LPTR, cbSid);
	bRet = ::LookupAccountName(pszDomainName, pszUserName, lpSID, &cbSid,
		ReferencedDomainName, &cchReferencedDomainName, &sidUsage);
	if(!bRet)
	{
		// err_handler(EvtLogName, _T("Name2SID LookupAccountName Failed"));
		::LocalFree(lpSID);
		return NULL;
	}
	return lpSID;
}
BOOL init_winuser(bool first_run)
{
	HWINSTA     hwinsta = NULL;
	HDESK       hdesk = NULL;
	HWINSTA      hwinstaSave = NULL;  
	if (first_run) {
		EnableNeccessPrivilege();
	}	
	PSID kangle_sid = Name2SID("kangle",NULL);
	PSID user_sid = Name2SID("Users",NULL);
	if(user_sid==NULL){
		klog(KLOG_ERR,"Cann't find users sid\n");
		return FALSE;
	}
	if ( (hwinstaSave = GetProcessWindowStation() ) == NULL){
		klog(KLOG_ERR,"Cann't GetProcessWindowStation\n");
		goto Cleanup;
	}
	// Get a handle to the interactive window station.
	hwinsta = OpenWindowStation(
		"winsta0",                   // the interactive window station 
		FALSE,                       // handle is not inheritable
		READ_CONTROL | WRITE_DAC);   // rights to read/write the DACL

	if (hwinsta == NULL) {
		klog(KLOG_ERR,"Cann't OpenWindowStation\n");
		goto Cleanup;
	}

	// To get the correct default desktop, set the caller's 
	// window station to the interactive window station.

	if (!SetProcessWindowStation(hwinsta)){
		klog(KLOG_ERR,"Cann't SetProcessWindowStation\n");
		goto Cleanup;
	}

	// Get a handle to the interactive desktop.

	hdesk = OpenDesktop(
		"default",     // the interactive window station 
		0,             // no interaction with other desktop processes
		FALSE,         // handle is not inheritable
		READ_CONTROL | // request the rights to read and write the DACL
		WRITE_DAC | 
		DESKTOP_WRITEOBJECTS | 
		DESKTOP_READOBJECTS);

	// Restore the caller's window station.

	if (!SetProcessWindowStation(hwinstaSave)) {
		klog(KLOG_ERR,"Cann't restore WindowStation\n");
		goto Cleanup;
	}
	if (hdesk==NULL){ 
		klog(KLOG_ERR,"Cann't OpenDesktop\n");
		goto Cleanup;
	}		
	if(kangle_sid){
#ifdef NDEBUG
		//	   RemoveAceFromWindowStation(hwinsta,kangle_sid);
		//	   RemoveAceFromDesktop(hdesk,kangle_sid);

#endif
		AddAceToWindowStation(hwinsta,kangle_sid);
		AddAceToDesktop(hdesk,kangle_sid);
	}

	// Allow logon SID full access to interactive window station.
	//AddAceToWindowStation(hwinsta, pSid) ;
#ifdef NDEBUG
	//   RemoveAceFromWindowStation(hwinsta,user_sid);
	//   RemoveAceFromDesktop(hdesk,user_sid);
#endif
	if (! AddAceToWindowStation(hwinsta, user_sid) ) {
		//klog(KLOG_ERR,"Cann't add ACE to Windows Station\n");
		goto Cleanup;
	}	  
	if (! AddAceToDesktop(hdesk, user_sid) ) {
		//klog(KLOG_ERR,"Cann't add ACE to desktop\n");
		goto Cleanup;
	}
	// klog(KLOG_NOTICE,"Set Users to WindowStation and Desktop access right done\n");
Cleanup:
	if (user_sid){
		::LocalFree(user_sid);
	}
	if(kangle_sid){
		::LocalFree(kangle_sid);
	}
	// Close the handles to the interactive window station and desktop.
	if (hwinsta){
		CloseWindowStation(hwinsta);
	}
	if (hdesk){
		CloseDesktop(hdesk);
	}
	return TRUE;
}
void coredump(DWORD pid,HANDLE hProcess,PEXCEPTION_POINTERS pExInfo)
{
	std::stringstream s;
	char buf[512];
	GetModuleFileName(NULL,buf,sizeof(buf));
	s << buf << "_" << pid <<  ".dmp";
	// Create the dump file name
	//sFile.Format(_T("%s\\%s.dmp"), getenv("TEMP"), CUtility::getAppName());

	// Create the file
	HANDLE hFile = CreateFile(
		s.str().c_str(),
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	//
	// Write the minidump to the file
	//
	if (hFile)
	{
		MINIDUMP_EXCEPTION_INFORMATION eInfo;
		eInfo.ThreadId = GetCurrentThreadId();
		eInfo.ExceptionPointers = pExInfo;
		eInfo.ClientPointers = FALSE;

		// MINIDUMP_CALLBACK_INFORMATION cbMiniDump;
		//   cbMiniDump.CallbackRoutine = CExceptionReport::miniDumpCallback;
		//  cbMiniDump.CallbackParam = 0;

		MiniDumpWriteDump(
			hProcess,
			pid,
			hFile,
			MiniDumpWithDataSegs,
			pExInfo ? &eInfo : NULL,
			NULL,
			NULL); 
		CloseHandle(hFile);
	}
}
LONG WINAPI CustomUnhandledExceptionFilter(PEXCEPTION_POINTERS pExInfo)
{
	if (quit_program_flag==PROGRAM_NO_QUIT) {
		//只有在正常运行出现异常才coredump
		coredump(GetCurrentProcessId(),GetCurrentProcess(),pExInfo);
	}
	TerminateProcess(GetCurrentProcess(),1);
	return EXCEPTION_EXECUTE_HANDLER;
}
#endif
//}}
void closeAllFile(int start_fd) {
#ifndef _WIN32
	int max_fd = sysconf(_SC_OPEN_MAX);
	if (max_fd == -1) {
		max_fd = 2048;
	}
	for (int i = start_fd; i < max_fd; i++) {
		::close(i);
	}
#endif
}
void append_cmd_arg(KStringBuf &s , const char *arg)
{
	const char *hot = arg;
	char lastChar='\0';
	while(*hot){
		if(*hot == '"'){
			s << "\\";
		}
		lastChar = *hot;
		s << *hot;
		hot++;
	}
	if (lastChar=='\\') {
		s << "\\";
	}
}

bool createProcess(Token_t token, char * args[],KCmdEnv *envs,char *curdir,PIPE_T in,PIPE_T out,PIPE_T err,pid_t &pid)
{
	if (quit_program_flag==PROGRAM_QUIT_IMMEDIATE) {
		return false;
	}
	if (args == NULL || args[0] == NULL) {
		return false;
	}
	KStringBuf arg(512);
	for(int i=0;;i++) {
		if(args[i]==NULL) {
			break;
		}
		if(i>0) {
			arg << " ";
		}
		arg << "\"";
		append_cmd_arg(arg,args[i]);
		arg << "\"";
	}
	klog(KLOG_NOTICE,"now create process [%s]\n",arg.getString());
	//{{ent
#ifdef _WIN32
	kgl_process_std std;
	memset(&std,0,sizeof(std));
	std.hstdin = in;
	std.hstdout = out;
	std.hstderr = err;
	if(!StartInteractiveClientProcess2(token,args[0],arg.getString(),curdir,&std,(envs?envs->dump_env():NULL),pid)) {
		return false;
	}
#else
	//}}
	pid = fork();
	if (pid == -1) {
		klog(KLOG_ERR, "cann't fork errno=%d\n", errno);
		return false;
	}	
	if (pid == 0) {
		signal(SIGTERM, SIG_DFL);
		if (token && my_uid == 0) {
			setgid(token[1]);
			setuid(token[0]);
		}
#if 0
		if (conf.program.size() == 0) {
			debug("api_child_start failed\n");
			delete st;
			exit(0);
		}
#endif
		//	printf("socket = %d\n",st->fd[0]);
		//st->closeAllOtherFile();
		//printf("socket = %d\n",st->fd[0]);
		if (in>0) {
			close(0);
			dup2(in,0);
		}
		if (out>=0 && out!=1) {
			close(1);
			dup2(out,1);
		}
		if (err>=0 && err!=2) {
			close(2);
			dup2(err,2);
		}
		closeAllFile(3);
		if (curdir==NULL) {
			curdir = strdup(args[0]);
			char *p = strrchr(curdir,'/');
			if (p) {
				*p = '\0';
			}
			chdir(curdir);
			free(curdir);
		} else {		
			chdir(curdir);
		}
		execve(args[0], args, (envs ? envs->dump_env() : NULL));
		//execv(args[0], args);
		fprintf(stderr, "run cmd[%s] error=%d %s\n", args[0], errno, strerror(
			errno));
		debug("child end\n");
		exit(127);	
	}
	//{{ent
#endif
	//}}
	return true;
}
bool createProcess(KPipeStream *st, Token_t token, char * args[],KCmdEnv *envs, int rdstd) {
	if (quit_program_flag==PROGRAM_QUIT_IMMEDIATE) {
		return false;
	}
	bool pipeCreated = false;
	if (
#ifndef _WIN32
		rdstd==RDSTD_NAME_PIPE || 
#endif
		rdstd==RDSTD_ALL){
			pipeCreated = true;  
			if(!st->create()) {
				return false;
			}
	}
	if (args == NULL || args[0] == NULL) {
		return false;
	}
	KStringBuf arg(512);
	for(int i=0;;i++) {
		if(args[i]==NULL) {
			break;
		}
		if(i>0) {
			arg << " ";
		}
		arg << "\"";
		append_cmd_arg(arg,args[i]);
		arg << "\"";
	}
	klog(KLOG_NOTICE,"now create process [%s]\n",arg.getString());
	//{{ent
#ifdef _WIN32
	if(!StartInteractiveClientProcess(token,args[0],arg.getString(),st,rdstd,(envs?envs->dump_env():NULL))) {
		return false;
	}
#else
	//}}
	int c_pid = fork();
	if (c_pid == -1) {
		klog(KLOG_ERR, "cann't fork errno=%d\n", errno);
		return false;
	}
	if(pipeCreated){
		st->setPipe(c_pid);
	}else{
		st->process.bind(c_pid);
	}
	if (c_pid == 0) {
		signal(SIGTERM, SIG_DFL);
		if (token && my_uid == 0) {
			setgid(token[1]);
			setuid(token[0]);
		}
#if 0
		if (conf.program.size() == 0) {
			debug("api_child_start failed\n");
			delete st;
			exit(0);
		}
#endif
		//	printf("socket = %d\n",st->fd[0]);
		st->closeAllOtherFile();
		//printf("socket = %d\n",st->fd[0]);
		if (rdstd == RDSTD_ALL) {
			close(0);
			close(1);
			dup2(st->fd[0], 0);
			dup2(st->fd[1], 1);
			close(st->fd[0]);
			close(st->fd[1]);
		} else if (rdstd == RDSTD_NAME_PIPE) {
			close(4);
			close(5);
			dup2(st->fd[0], 4);
			dup2(st->fd[1], 5);
			close(st->fd[0]);
			close(st->fd[1]);
		} else if (rdstd == RDSTD_INPUT) {
			close(0);
			dup2(st->fd[0], 0);
			close(st->fd[0]);
			//close(1);
			//close(2);
		}
		char *curdir = strdup(args[0]);
		char *p = strrchr(curdir,'/');
		if(p){
			*p = '\0';
		}
		chdir(curdir);
		free(curdir);
		execve(args[0], args, (envs ? envs->dump_env() : NULL));
		//execv(args[0], args);
		fprintf(stderr, "run cmd[%s] error=%d %s\n", args[0], errno, strerror(
			errno));
		debug("child end\n");
		exit(0);
	}
	//{{ent
#endif
	//}}
	return true;

}
KPipeStream * createProcess(Token_t token, char * args[],
							KCmdEnv *envs, int rdstd) {
								if (args == NULL && args[0] == NULL) {
									return NULL;
								}
								KPipeStream *st = new KPipeStream();
								if (!createProcess(st, token, args, envs, rdstd)) {
									delete st;
									return NULL;
								}
								return st;
}
