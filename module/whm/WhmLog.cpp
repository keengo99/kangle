#include "WhmLog.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "log.h"
void Whmvlog(int level, const char *fmt, va_list ap) {	
	vklog(level, fmt, ap);
}
void WhmLog(int level, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	Whmvlog(level, fmt, ap);
	va_end(ap);
}
void WhmError(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	Whmvlog(WHM_LOG_ERROR, fmt, ap);
	va_end(ap);
}
void WhmWarning(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	Whmvlog(WHM_LOG_WARNING, fmt, ap);
	va_end(ap);
}
void WhmNotice(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	Whmvlog(WHM_LOG_NOTICE, fmt, ap);
	va_end(ap);
}
void WhmDebug(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	Whmvlog(WHM_LOG_DEBUG, fmt, ap);
	va_end(ap);
}
