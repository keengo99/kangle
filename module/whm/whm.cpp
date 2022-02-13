// whm.cpp : 定义 DLL 应用程序的导出函数。
//

//#include "stdafx.h"
#include "kgl_ssl.h"
#include "kforwin32.h"
#include <string.h>
#include "KISAPIServiceProvider.h"
#include "KWhmService.h"
#include "whm.h"
#include "WhmPackageManage.h"
#include "WhmShellSession.h"


DLL_PUBLIC BOOL WINAPI Whm_GetExtensionVersion(HSE_VERSION_INFO *pVer) {
	strcpy(pVer->lpszExtensionDesc, "whm");
	return TRUE;
}
DLL_PUBLIC DWORD WINAPI Whm_HttpExtensionProc(EXTENSION_CONTROL_BLOCK *pECB) {
	KISAPIServiceProvider provider;
	provider.setECB(pECB);
	KWhmService servicer;
	if (servicer.service(&provider)) {
		provider.getOutputStream()->write_end(KGL_OK);
		return TRUE;
	}
	return FALSE;
}
DLL_PUBLIC BOOL WINAPI Whm_TerminateExtension( DWORD dwFlags )
{
	packageManage.clean();
	return TRUE;
}
