/*
 * KHttpDigestAuth.cpp
 *
 *  Created on: 2010-7-15
 *      Author: keengo
 */
#include "utils.h"
#include "KHttpDigestAuth.h"
#include "KHttpRequest.h"
#include "log.h"
#include "KHttpField.h"
#include "http.h"
#include "kmd5.h"

#include "kmalloc.h"
#ifdef ENABLE_DIGEST_AUTH
std::map<char*, KHttpDigestSession*, lessp> KHttpDigestAuth::sessions;
KMutex KHttpDigestAuth::lock;
using namespace std;
KHttpDigestSession::KHttpDigestSession(const char* realm) {
	this->realm = xstrdup(realm);
	lastActive = time(NULL);
}
KHttpDigestSession::~KHttpDigestSession() {
	xfree(realm);
}
bool KHttpDigestSession::verify_rq(KHttpRequest* rq) {
	/*
	bool result = true;
	int inc = strtol(nc, NULL, 16);
	//int inc = atoi(nc);
	//	debug("last_nc = %x,inc=%x\n",last_nc,inc);
	if (inc == last_nc) {
		//�طŹ���
		//		debug("replay attack,last_nc=%d,inc=%d\n",last_nc,inc);
		return false;
	} else if (inc < last_nc) {
		list<int>::reverse_iterator it;
		result = false;
		//		debug("recent nc=");
		for (it = recent.rbegin(); it != recent.rend(); it++) {
			//			debug("%x ",(*it));
			if (*it == inc) {
				result = false;
				break;
			}
			if ((*it) < inc) {
				result = true;
				break;
			}
		}
		//		debug("\nresult=%d\n",result);
	} else {
		//��������nc
		last_nc = inc;
	}
	if (result) {
		lastActive = time(NULL);
		recent.push_back(inc);
		if (recent.size() > MAX_SAVE_DIGEST_SESSION) {
			recent.pop_front();
		}
	}
	return result;
	*/
	if (ksocket_addr_compare(rq->sink->get_peer_addr(), &addr) == 0) {
		lastActive = time(NULL);
		return true;
	}
	return false;
}
KHttpDigestAuth::KHttpDigestAuth() :
	KHttpAuth(AUTH_DIGEST) {
	nonce = NULL;
	nc = NULL;
	cnonce = NULL;
	qop = NULL;
	response = NULL;
	orig_str = NULL;
	uri = NULL;
}
KHttpDigestAuth::~KHttpDigestAuth() {
	if (realm) {
		xfree(realm);
	}
	if (nonce) {
		xfree(nonce);
	}
	if (user) {
		xfree(user);
	}
	if (nc) {
		xfree(nc);
	}
	if (qop) {
		xfree(qop);
	}
	if (response) {
		xfree(response);
	}
	if (uri) {
		xfree(uri);
	}
	if (cnonce) {
		xfree(cnonce);
	}
}
bool KHttpDigestAuth::verifySession(KHttpRequest* rq) {
	if (uri == NULL || rq->sink->data.raw_url.path == NULL || rq->sink->data.raw_url.host == NULL) {
		return false;
	}
	KSafeUrl url2(new KUrl(true));
	parse_url(uri, url2.get());
	if (
		(url2->host && strcmp(url2->host, rq->sink->data.raw_url.host) != 0)
		|| (url2->path == NULL)
		|| (strcmp(url2->path, rq->sink->data.raw_url.path) != 0)
		|| (url2->param && (rq->sink->data.raw_url.param == NULL || strcmp(url2->param, rq->sink->data.raw_url.param) != 0))
		) {

		return false;
	}

	bool sessionright = false;
	lock.Lock();
	map<char*, KHttpDigestSession*, lessp>::iterator it;
	it = sessions.find(nonce);
	if (it != sessions.end()) {
		if (strcmp(realm, (*it).second->realm) == 0) {
			sessionright = (*it).second->verify_rq(rq);
		} else {
			debug("realm[%s] is not eq session realm[%s]", realm, (*it).second->realm);
		}
	} else {
		debug("cann't find such session[%s]\n", nonce);
	}
	lock.Unlock();
	//debug("sessionright=%d\n", sessionright);
	//        //      printf("str=[%s]\n", str);
	//                return sessionright;
	//
	return sessionright;
}

bool KHttpDigestAuth::parse(KHttpRequest* rq, const char* str) {
	/*
	 if (rq->auth == NULL || rq->auth->getType() != AUTH_DIGEST) {
	 debug("û����һ�ε�����\n");
	 //û����һ�ε�����
	 return false;
	 }
	 */
	 //orig_str = str;
	 /*	if(rq->auth && rq->auth->getType() == AUTH_DIGEST){
	  KHttpDigestAuth *auth = (KHttpDigestAuth *) rq->auth;
	  if(auth->nonce){
	  nonce = strdup(auth->nonce);
	  }
	  }
	  */
	  /*
	   * username="test",
	   * realm="kangle",
	   * nonce="4c3f0cc87fb164ac",
	   * uri="/",
	   * response="28d20212e5252a21193cf0177510a6b2",
	   * qop=auth,
	   * nc=00000001,
	   * cnonce="3a8b2e24a4aeca05"
	   */
	   //char *username = NULL;
	   //if (auth->realm == NULL || auth->nonce == NULL) {
	   //	return false;
	   //}
	   //this->realm = xstrdup(auth->realm);
	   //this->nonce = xstrdup(auth->nonce);
	char* realm = NULL;
	char* nonce = NULL;
	char* uri = NULL;
	char* response = NULL;
	char* nc = NULL;
	char* cnonce = NULL;
	char* qop = NULL;
	KHttpField field;
	field.parse(str, ',');
	http_field_t* header = field.getHeader();
	while (header) {
		if (strcasecmp(header->attr, "username") == 0) {
			if (user == NULL && header->val) {
				user = xstrdup(header->val);
			} else {
				//				debug("username is empty\n");
				return false;
			}
		}
		if (strcasecmp(header->attr, "realm") == 0) {
			realm = header->val;
		}
		if (strcasecmp(header->attr, "nonce") == 0) {
			nonce = header->val;
		}
		if (strcasecmp(header->attr, "uri") == 0) {
			uri = header->val;
		}
		if (strcasecmp(header->attr, "response") == 0) {
			response = header->val;
		}
		if (strcasecmp(header->attr, "qop") == 0) {
			qop = header->val;
		}
		if (strcasecmp(header->attr, "nc") == 0) {
			nc = header->val;
		}
		if (strcasecmp(header->attr, "cnonce") == 0) {
			cnonce = header->val;
		}
		header = header->next;
	}
	//check the digest
	if (user == NULL) {
		//		debug("û���û���\n");
		return false;
	}
	if (realm == NULL) {
		//|| this->realm == NULL || strcmp(realm, this->realm) != 0) {
		//		debug("realm ����\n");
		return false;
	}
	if (nonce == NULL) {//|| this->nonce == NULL || strcmp(nonce, this->nonce) != 0) {
		//		debug("nonce����\n");
		return false;
	}

	if (uri == NULL) {
		return false;
	}
	if (response == NULL) {
		//		debug("response����Ϊ��\n");
		return false;
	}

	if (nc == NULL) {
		//		debug("nc����Ϊ��\n");
		return false;
	}
	if (cnonce == NULL) {
		//		debug("cnonce empty\n");
		return false;
	}
	if (qop == NULL) {
		//		debug("qop empty\n");
		return false;
	}
	this->response = xstrdup(response);
	this->nc = xstrdup(nc);
	this->cnonce = xstrdup(cnonce);
	this->qop = xstrdup(qop);
	this->nonce = xstrdup(nonce);
	this->realm = xstrdup(realm);
	this->uri = xstrdup(uri);
	return true;
}
bool KHttpDigestAuth::verify(KHttpRequest* rq, const char* password,
	int passwordType) {
	char ha1[33];
	if (passwordType == CRYPT_TYPE_PLAIN) {
		KStringBuf a1;
		a1 << user << ":" << realm << ":" << password;
		//		printf("a1=[%s]\n", a1.getString());
		KMD5(a1.c_str(), a1.size(), ha1);
	} else {
		strncpy(ha1, password, sizeof(ha1));
		ha1[32] = '\0';
	}
	KStringBuf a2;
	a2 << rq->get_method() << ":" << uri;
	//	printf("a2=[%s]\n", a2.getString());
	char ha2[33];
	KMD5(a2.c_str(), a2.size(), ha2);
	KStringBuf ar;
	ar << ha1 << ":" << nonce << ":" << nc << ":" << cnonce << ":" << qop << ":" << ha2;
	//	printf("response=[%s]\n", ar.getString());
	char hresponse[33];
	KMD5(ar.c_str(), ar.size(), hresponse);
	if (this->response && strcmp(this->response, hresponse) == 0) {
		//		debug("verified success\n");
		return true;
	}
	//	debug("this->response=[%s],hresponse=[%s],failed\n", this->response,
	//			hresponse);
	return false;
}
void KHttpDigestAuth::insertHeader(KHttpRequest* rq) {
	if (realm == NULL || nonce == NULL) {
		return;
	}
	KStringBuf s;
	s << "Digest ";
	s << "realm=\"" << realm << "\"";
	s << ", qop=\"auth\"";
	s << ", nonce=\"" << nonce << "\"";
	const char* auth_header = this->get_auth_header();
	rq->response_header(auth_header, (hlen_t)strlen(auth_header), s.buf(), s.size());
}
KGL_RESULT KHttpDigestAuth::response_header(kgl_output_stream* out) {
	if (realm == NULL || nonce == NULL) {
		return KGL_OK;
	}
	KStringBuf s;
	s << "Digest ";
	s << "realm=\"" << realm << "\"";
	s << ", qop=\"auth\"";
	s << ", nonce=\"" << nonce << "\"";
	const char* auth_header = this->get_auth_header();
	return out->f->write_unknow_header(out->ctx, auth_header, (hlen_t)strlen(auth_header), s.buf(), s.size());
}
void KHttpDigestAuth::flushSession(time_t nowTime) {
	map<char*, KHttpDigestSession*, lessp>::iterator it, it2;
	lock.Lock();
	for (it = sessions.begin(); it != sessions.end();) {
		if (nowTime > (*it).second->lastActive + MAX_SESSION_LIFETIME) {
			it2 = it;
			it++;
			xfree((*it2).first);
			delete (*it2).second;
			sessions.erase(it2);
		} else {
			it++;
		}
	}
	lock.Unlock();
}
void KHttpDigestAuth::init(KHttpRequest* rq, const char* realm) {
	this->realm = xstrdup(realm);
	KStringStream s(64);
	s.add((int)time(NULL), "%x");
	s.add(rand(), "%x");
	this->nonce = s.steal().release();
	KHttpDigestSession* session = new KHttpDigestSession(realm);
	kgl_memcpy(&session->addr, rq->sink->get_peer_addr(), sizeof(sockaddr_i));
	lock.Lock();
	//debug("add nonce[%s] to session\n",this->nonce);
	sessions.insert(pair<char*, KHttpDigestSession*>(
		xstrdup(this->nonce), session));
	lock.Unlock();
}
#endif
