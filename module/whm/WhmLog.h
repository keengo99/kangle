#ifndef WHMLOG_H
#define WHMLOG_H

#define WHM_LOG_ERROR   0
#define WHM_LOG_WARNING 1
#define WHM_LOG_NOTICE  2
#define WHM_LOG_DEBUG   3

void WhmLog(int level, const char *fmt, ...);
void WhmError(const char *fmt,...);
void WhmWarning(const char *fmt,...);
void WhmNotice(const char *fmt,...);
void WhmDebug(const char *fmt,...);
#endif
