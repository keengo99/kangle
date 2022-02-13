/*
 * KPoolableStream.cpp
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#include "KUpstream.h"
#include "KPoolableSocketContainer.h"
#include "log.h"
#include "KHttpProxyFetchObject.h"
#include "KHttpRequest.h"
#include "kselector.h"
struct kgl_upstream_delay_io
{
	KUpstream *us;
	void *arg;
	buffer_callback buffer;
	result_callback result;
};
KUpstream::~KUpstream()
{
	if (container) {
		container->unbind(this);
	}
}
void KUpstream::IsBad(BadStage stage)
{
	if (container) {
		container->isBad(this, stage);
	}
}
void KUpstream::IsGood()
{
	if (container) {
		container->isGood(this);
	}
}
int KUpstream::GetLifeTime()
{
	if (container) {
		return container->getLifeTime();
	}
	return 0;
}
kgl_refs_string *KUpstream::GetParam()
{
	if (container == NULL) {
		return NULL;
	}
	return container->GetParam();
}