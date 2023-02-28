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
#ifndef KLISTENCONFIGPARSER_H
#define KLISTENCONFIGPARSER_H
#include <string>
#include "KXmlEvent.h"
#include "do_config.h"
#include "KConfigTree.h"

void on_listen_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent *ev);
#endif
