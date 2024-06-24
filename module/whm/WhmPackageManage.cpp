/*
 * WhmPackageManage.cpp
 *
 *  Created on: 2010-8-31
 *      Author: keengo
 */
#include <vector>
#include <string.h>
#include <stdlib.h>
#include "KXml.h"
#include "WhmLog.h"
#include "WhmPackageManage.h"

#include "kmalloc.h"
WhmPackageManage packageManage;
static void flushThread(void* arg, time_t now_time) {
	packageManage.flush();
}
WhmPackageManage::WhmPackageManage() {}
WhmPackageManage::~WhmPackageManage() {
	clean();
}
void WhmPackageManage::flush() {
	lock.Lock();
	std::map<std::string, WhmPackage*>::iterator it;
	for (it = packages.begin(); it != packages.end(); it++) {
		(*it).second->flush();
	}
	lock.Unlock();
}
void WhmPackageManage::clean() {
	lock.Lock();
	std::map<std::string, WhmPackage*>::iterator it;
	for (it = packages.begin(); it != packages.end(); it++) {
		(*it).second->release();
	}
	packages.clear();
	lock.Unlock();
}
WhmPackage* WhmPackageManage::find(const char* name) {
	std::map<std::string, WhmPackage*>::iterator it;
	it = packages.find(name);
	if (it == packages.end()) {
		return NULL;
	}
	return (*it).second;
}
WhmPackage* WhmPackageManage::load(const char* file) {
	lock.Lock();
	if (!flush_thread_started) {
		flush_thread_started = true;
		register_gc_service(flushThread, NULL);
	}
	WhmPackage* package = find(file);
	if (package == NULL) {
		KXml xml;
		package = new WhmPackage();
		package->setFile(file);
		try {
			xml.setEvent(package);
			xml.parseFile(file);
		} catch (KXmlException& e) {
			WhmError("cann't load package[%s],exception=[%s]\n", file, e.what());
			package->release();
			lock.Unlock();
			return NULL;
		}
		packages[file] = package;
	}
	if (package) {
		package->addRef();
	}
	lock.Unlock();
	return package;
}
