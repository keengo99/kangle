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
	KAcl *new_instance() override {
		return new KSSLSerialAcl();
	}
	const char *getName() override {
		return "ssl_serial";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) override {
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
