/*
 * WhmCommand.h
 *
 *  Created on: 2010-8-31
 *      Author: keengo
 */

#ifndef WHMCOMMAND_H_
#define WHMCOMMAND_H_
#include <vector>
#include "WhmExtend.h"
class WhmCommand: public WhmExtend {
public:
	WhmCommand(std::string &file);
	virtual ~WhmCommand();
	bool init(std::string &whmFile);

	int call(const char *callName,const char *eventType,WhmContext *context);
	bool runAsUser;
	const char *getType()
	{
		return "command";
	}
private:
	std::vector<std::string> args;
};

#endif /* WHMCOMMAND_H_ */
