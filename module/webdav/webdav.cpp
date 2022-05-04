
#include "KWebDavService.h"
#include "KISAPIServiceProvider.h"
#include "KXml.h"
#include "kfile.h"

DLL_PUBLIC BOOL  WINAPI   GetExtensionVersion(HSE_VERSION_INFO* pVer)
{
	strcpy(pVer->lpszExtensionDesc, "webdav");
	KXml::fopen = kfopen;
	KXml::fclose = (kxml_fclose)kfclose;
	KXml::fsize = kfsize;
	KXml::fread = kfread;
	return TRUE;
}
DLL_PUBLIC DWORD WINAPI   HttpExtensionProc(EXTENSION_CONTROL_BLOCK* pECB)
{
	KISAPIServiceProvider provider;
	provider.setECB(pECB);
	KWebDavService servicer;
	if (servicer.service(&provider)) {
		provider.getOutputStream()->write_end(KGL_OK);
		return HSE_STATUS_SUCCESS;
	}
	provider.getOutputStream()->write_end(KGL_EUNKNOW);
	return HSE_STATUS_ERROR;
}
