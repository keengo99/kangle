#ifndef KREPORT_IP_H
#define KREPORT_IP_H
#include "global.h"
#define REPORT_IP_BLACK 0
#define REPORT_IP_WHITE 1
#ifdef ENABLE_SIMULATE_HTTP
void report_black_list(const char *ip);
void report_white_list(const char *host,const char *vh,const char *ip);
void add_report_ip(const char *ips);
#endif
#endif
