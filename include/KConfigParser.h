/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#ifndef KCONFIGPARSER_H_
#define KCONFIGPARSER_H_
#include<string>
#include "KXmlEvent.h"
#include "do_config.h"
#include "kmalloc.h"
#include "KConfigTree.h"

void on_main_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
void on_ssl_client_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
void on_admin_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
void on_log_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
void on_cache_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
void on_io_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
void on_fiber_event( kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
void on_dns_event( kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
void on_run_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
void on_connect_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
void on_compress_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
void on_firewall_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
#endif /*KCONFIGPARSER_H_*/
