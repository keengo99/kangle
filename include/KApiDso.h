#ifndef KAPIDSO_H
#define KAPIDSO_H
#include <string>
#include "khttpext.h"
#ifndef HIWORD
#define LOWORD(l)           ((WORD)(((unsigned long)(l)) & 0xffff))
#define HIWORD(l)           ((WORD)((((unsigned long)(l)) >> 16) & 0xffff))
#endif
typedef BOOL(WINAPI * GetExtensionVersionf)(HSE_VERSION_INFO *pVer);
typedef DWORD(WINAPI * HttpExtensionProcf)(EXTENSION_CONTROL_BLOCK *pECB);
typedef BOOL(WINAPI * TerminateExtensionf)(DWORD dwFlags);
typedef BOOL(WINAPI * DllMainf)(HINSTANCE hinstDLL, DWORD fdwReason,LPVOID lpvReserved);
enum {
	STATE_LOAD_SUCCESS, STATE_LOAD_FAILED, STATE_LOAD_UNKNOW
};
class KApiDso
{
public:
	KApiDso();
	~KApiDso();
public:
	
	const char *getError();
	bool load(std::string file);
	bool load();
	bool reload();
	void unload();

	GetExtensionVersionf GetExtensionVersion;
	HttpExtensionProcf HttpExtensionProc;
	TerminateExtensionf TerminateExtension;
public:
	const char *getInfo() {
		return apiInfo;
	}
	const char *getStateString() {
		switch (state) {
		case STATE_LOAD_SUCCESS:
			return "success";
		case STATE_LOAD_FAILED:
			return "failed";
		}
		return "unknow";
	}
	int getState()
	{
		return state;
	}
	std::string path;

	/*
	 * api×´Ì¬
	 */
	int state;
	/*
	 * apiµÄÐÅÏ¢
	 */
	char apiInfo[256];
	bool init();
	HCONN     ConnID;
	ServerFreef ServerFree;
	ServerSupportFunctionf ServerSupportFunction;
private:
	HMODULE handle;
};
#endif
