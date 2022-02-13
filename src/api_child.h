/*
 * api_child.h
 *
 *  Created on: 2010-8-17
 *      Author: keengo
 */

#ifndef API_CHILD_H_
#define API_CHILD_H_
#include "ksocket.h"
#include "global.h"
#include "KStream.h"
#include "KPipeStream.h"
//#include "KPoolableStream.h"
#include "KApiRedirect.h"
#include "fastcgi.h"

class KApiRedirect;
KTHREAD_FUNCTION api_child_thread(void *param);
void api_child_start(KApiRedirect *rd, KStream *st);
bool api_child_listen(u_short port, KPipeStream *st,bool unix_socket);
bool api_pipe_write(KPipeStream *st, const char *str, int len);
bool api_child_process(KStream *st);
void forapifork();
bool forcmdextend();
void seperate_work_model();
#endif /* API_CHILD_H_ */
