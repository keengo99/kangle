#ifndef KSESSION_H
#define KSESSION_H
#include "ksapi.h"
void session_start(kgl_filter_context * ctx);
const char * session_get(kgl_filter_context * ctx,const char *name);
void session_set(kgl_filter_context * ctx,const char *name,const char *value);
void session_del(kgl_filter_context * ctx,const char *name);
#endif
