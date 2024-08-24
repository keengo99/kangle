#ifndef KGL_DEFAULT_FETCH_OBJECT_H
#define KGL_DEFAULT_FETCH_OBJECT_H
#include "KFetchObject.h"
class KDefaultFetchObject : public KFetchObject
{
public:
	KDefaultFetchObject() : KFetchObject(0) {

	}
	KGL_RESULT Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) override;
};
#endif
