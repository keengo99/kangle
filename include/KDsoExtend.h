#ifndef KDSOEXTEND_H_99
#define KDSOEXTEND_H_99
#include "KDsoModule.h"
#include "utils.h"
#include "KDsoRedirect.h"

KGL_RESULT global_support_function(PVOID ctx, DWORD req, PVOID data, PVOID *ret);
KGL_RESULT global_get_variable(PVOID ctx, KGL_GVAR type, const char *name, LPVOID lpvBuffer, LPDWORD lpdwSize);

typedef BOOL(*kgl_dso_init_f)(kgl_dso_version *ver);

typedef BOOL(*kgl_dso_finit_f)(int32_t flags);

class KDsoExtend
{
public:
	KDsoExtend(const char *name);
	~KDsoExtend();
	bool load(const char *filename,std::map<std::string,std::string> &attribute);
	const char *GetName()
	{
		return name;
	}
	const char *GetFileName()
	{
		return filename;
	}
	const char *GetOrignFileName()
	{
		return orign_filename;
	}
	bool RegisterUpstream(kgl_upstream *us);
	void ListUpstream(std::stringstream &s);
	void ListTarget(std::vector<std::string> &target);
	KRedirect *RefsRedirect(std::string &name);
	void shutdown();
	kgl_dso_init_f kgl_dso_init;
	kgl_dso_finit_f kgl_dso_finit;
	kgl_dso_version version;
	bool cur_config_ext;
	std::map<std::string, std::string> attribute;
private:
	std::map<const char *, KDsoRedirect *, lessp> upstream;	
	char *name;
	char *filename;
	char *orign_filename;
	KDsoModule dso;
};
#endif

