/*
 * WhmExtend.cpp
 *
 *  Created on: 2010-8-31
 *      Author: keengo
 */

#include "WhmExtend.h"
#include "KDynamicString.h"
#include "WhmPackage.h"
#include "WhmPackageManage.h"
using namespace std;
WhmExtend::WhmExtend() {

}

WhmExtend::~WhmExtend() {

}
bool WhmExtend::init(KString &whmFile) {
	KDynamicString ds;
	auto dpath = ds.parseString(file.c_str());
	if(dpath){
		file = dpath.get();
	}
	if (!isAbsolutePath(file.c_str())) {
		auto path = getPath(whmFile.c_str());
		KStringBuf newPath;
		newPath << path.get();
		newPath << PATH_SPLIT_CHAR << file;
		newPath.str().swap(file);
	}
	return true;
}