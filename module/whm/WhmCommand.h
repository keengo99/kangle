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
	WhmCommand(KString&file);
	virtual ~WhmCommand();
	bool init(KString &whmFile) override;

	int call(const char *callName,const char *eventType,WhmContext *context) override;
	bool runAsUser;
	const char *getType() override
	{
		return "command";
	}
	bool startElement(KXmlContext* context) override {
		return true;
	}
private:
	std::vector<KString> args;
};

#endif /* WHMCOMMAND_H_ */
