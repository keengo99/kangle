#include <string.h>
#ifndef _WIN32
#include <dirent.h>
#else
#include <sstream>
#endif
#include "directory.h"
#include "kmalloc.h"
#include "kforwin32.h"
int list_dir(const char *dir,list_dir_handle_f file_handle,void *param)
{
	int ret = 0;
#ifndef _WIN32
	DIR *dp = opendir(dir);
	if (dp == NULL) {
		return -1;
	}
#else
	HANDLE hList;
	WIN32_FIND_DATA FileData;
	std::stringstream s;
	s << dir << "\\*";
	hList = FindFirstFile(s.str().c_str(), &FileData);
	if (hList == INVALID_HANDLE_VALUE) {
		//fprintf(stderr,"cann't load directory [%s]\n",s.getString());
		return -1;// printf("No files found\n\n");
	}
#endif
#ifndef _WIN32
	for (;;) {
		dirent *fp = readdir(dp);
		
		if (fp == NULL) {
			break;
		}
		if (strcmp(fp->d_name, ".") == 0 || strcmp(fp->d_name, "..") == 0) {
			continue;
		}
		ret = file_handle(fp->d_name,param);
		if(ret!=0){
			break;
		}
	}
	closedir(dp);
#else
	for(;;) {
		if ((strcmp(FileData.cFileName, ".")==0) 
			|| (strcmp(FileData.cFileName, "..")==0)){
			goto next_file;
		}
		ret = file_handle(FileData.cFileName,param);
		if(ret!=0){
			break;
		}
		next_file:
		if (!FindNextFile(hList, &FileData)) {
			if (GetLastError() == ERROR_NO_MORE_FILES) {
				break;
			}

		}
	}
	FindClose(hList);
#endif
	return ret;
}
#ifdef WIN32
int list_dirw(const wchar_t *dir,list_dir_handlew_f file_handle,void *param)
{
	HANDLE hList;
	WIN32_FIND_DATAW FileData;
	std::wstringstream s;
	s << dir << L"/*";
	hList = FindFirstFileW(s.str().c_str(), &FileData);
	if (hList == INVALID_HANDLE_VALUE) {
		//fprintf(stderr,"cann't load directory [%s]\n",s.getString());
		return -1;// printf("No files found\n\n");
	}
	int ret = 0;
	for(;;) {
		if ((wcscmp(FileData.cFileName, L".")==0) 
			|| (wcscmp(FileData.cFileName, L"..")==0)){
			goto next_file;
		}
		ret = file_handle(FileData.cFileName,param);
		if (ret!=0) {
			break;
		}
		next_file:
		if (!FindNextFileW(hList, &FileData)) {
			if (GetLastError() == ERROR_NO_MORE_FILES) {
				break;
			}

		}
	}
	FindClose(hList);
	return ret;
}
#endif
