/*
 * KUserManageInterface.h
 *
 *  Created on: 2010-5-10
 *      Author: keengo
 */

#ifndef KUSERMANAGEINTERFACE_H_
#define KUSERMANAGEINTERFACE_H_
#include <string>
#include "KDomainUser.h"
#if 0
namespace user {
enum {
	ERROR_UNKNOW,
	ERROR_DOMAIN_NAME_LENGTH,
	ERROR_DOMAIN_FORMAT,
	ERROR_DOMAIN_EMPTY,
	ERROR_DOMAIN_DUPLICATE,
	ERROR_USER_NAME_LENGTH,
	ERROR_USER_NAME_EMPTY,
	ERROR_USER_DUPLICATE,
	ERROR_USER_NOT_EXSIT,
	ERROR_USER_FORMAT,
	ERROR_DOMAIN_NOT_EXSIT,
	ERROR_PASSWORD_LENGTH,
	ERROR_PASSWORD_EMPTY,
	ERROR_PASSWORD_FORMAT,
	ERROR_OLD_PASSWORD,
	ERROR_TWO_PASSWORD_EQUALE,
	ERROR_CRYPT_TYPE,
	ERROR_PASSWORD,
	ERROR_CONNECTION
};
}
/*
 * user manage interface is a abstract class.
 * It used to manage all domains users and its password.
 *
 */
class KUserManageInterface {
public:
	KUserManageInterface();
	virtual ~KUserManageInterface();
	virtual bool connect(std::string connectString, int *errorCode) = 0;
	virtual bool close()=0;
	virtual bool addDomain(std::string domain, int cryptType, int *errorCode)=0;
	virtual bool delDomain(std::string domain, int *errorCode)=0;
	virtual bool
	addUser(std::string domainUser, std::string passwd, int *errorCode)=0;
	virtual bool delUser(std::string domainUser, int *errorCode)=0;
	virtual bool checkUser(KUserInfo *authUser, int *errorCode)=0;
	virtual bool changPasswd(std::string domainUser, std::string newPasswd,
			int *errorCode)=0;
	virtual const char *getErrorString(int errorCode);
};
#endif
#endif /* KUSERMANAGEINTERFACE_H_ */
