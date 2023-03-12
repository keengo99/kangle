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
	bool load(const char *filename,const KXmlAttribute &attribute);
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
	void ListUpstream(KWStream &s);
	void ListTarget(std::vector<KString> &target);
	KRedirect *RefsRedirect(KString &name);
	void shutdown();
	kgl_dso_init_f kgl_dso_init;
	kgl_dso_finit_f kgl_dso_finit;
	kgl_dso_version version;
	KXmlAttribute attribute;
private:
	std::map<const char *, KDsoRedirect *, lessp> upstream;	
	char *name;
	char *filename;
	char *orign_filename;
	KDsoModule dso;
};
#endif

