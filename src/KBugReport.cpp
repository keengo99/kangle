#include "kforwin32.h"
#include "global.h"
#ifdef _WIN32
LONG WINAPI catch_exception(struct _EXCEPTION_POINTERS *ExceptionInfo)
{

	printf("catch_exception\n");
	return 1;
}
#endif
