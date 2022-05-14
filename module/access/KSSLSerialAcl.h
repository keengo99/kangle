#ifndef KSSLSERIALACL_H
#define KSSLSERIALACL_H
#include "KMultiAcl.h"
#include "ksocket.h"
#include "ssl_utils.h"
#ifdef KSOCKET_SSL
class KSSLSerialAcl: public KMultiAcl {
public:
	KSSLSerialAcl(){

	}
	KAcl *newInstance() {
		return new KSSLSerialAcl();
	}
	const char *getName() {
		return "ssl_serial";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {		
		kssl_session *ssl = rq->sink->get_ssl();
		if (ssl) {
			char *serial = ssl_var_lookup(ssl->ssl, "CERT_SERIALNUMBER");
			if (serial == NULL) {
				return false;
			}
			bool result = KMultiAcl::match(serial);
			OPENSSL_free(serial);
			return result;
		}
		return false;
	}
};
#endif
#endif
