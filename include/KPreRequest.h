#ifndef KPREREQUEST_H_99
#define KPREREQUEST_H_99
#include <string>
#include "kconnection.h"
void handle_connection(kconnection *cn,void *ctx);
std::string get_connect_per_ip();
#endif
