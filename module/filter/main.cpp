#include "ksapi.h"
void register_access(kgl_dso_version *ver);

DLL_PUBLIC BOOL  kgl_dso_init(kgl_dso_version * ver)
{
	if (!IS_KSAPI_VERSION_COMPATIBLE(ver->api_version)) {
		return FALSE;
	}	
	ver->api_version = KSAPI_VERSION;
	ver->module_version = MAKELONG(0, 1);
	register_access(ver);
	return TRUE;
}
DLL_PUBLIC BOOL  kgl_dso_finit(DWORD flag)
{
	return TRUE;
}
