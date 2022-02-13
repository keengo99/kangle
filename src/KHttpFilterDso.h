#ifndef KHTTPFILTERDSO_H
#define KHTTPFILTERDSO_H
#include "global.h"
#include "ksapi.h"
#include "KDsoModule.h"
#include <map>
#include <string>
#ifdef ENABLE_KSAPI_FILTER
#if 0
typedef BOOL (WINAPI *kgl_filter_init_f)(kgl_filter_version *pVer);
typedef DWORD (WINAPI *kgl_filter_process_f)(kgl_filter_context *pfc,
										   DWORD notificationType,
										   LPVOID pvNotification);
typedef BOOL (WINAPI *kgl_filter_finit_f)(DWORD dwFlags);

class KHttpFilterDso
{
public:
	KHttpFilterDso(const char *name);
	~KHttpFilterDso();
	bool load(const char *filename);
	const char *get_name()
	{
		return name;
	}
	const char *get_filename()
	{
		return filename;
	}
	const char *get_orign_filename()
	{
		return orign_filename;
	}
	kgl_filter_version *get_version();
	kgl_filter_init_f kgl_filter_init;
	kgl_filter_process_f kgl_filter_process;
	kgl_filter_finit_f kgl_filter_finit;
	KHttpFilterDso *next;
	kgl_filter_version version;
	int index;
	std::map<std::string, std::string> attribute;
private:
	char *name;
	char *filename;
	char *orign_filename;
	KDsoModule dso;
};
#endif
#endif
#endif
