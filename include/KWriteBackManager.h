#ifndef KWRITEBACKMANAGER_H_
#define KWRITEBACKMANAGER_H_
#include<vector>
#include<string>
#include "global.h"
#include "KWriteBack.h"
#include "KRWLock.h"
#ifdef ENABLE_WRITE_BACK
class KWriteBackManager: public KXmlSupport {
public:
	KWriteBackManager();
	virtual ~KWriteBackManager();
	void copy(KWriteBackManager &a) {
		lock.WLock();
		writebacks.swap(a.writebacks);
		lock.WUnlock();
	}
	void destroy();
	bool delWriteBack(std::string name, std::string &err_msg);
	bool addWriteBack(KWriteBack *wb);
	KWriteBack * refsWriteBack(std::string name);
	bool newWriteBack(std::string name, std::string msg, std::string &err_msg);
	bool editWriteBack(std::string name, KWriteBack &m_a, std::string &err_msg);
	std::string writebackList(std::string name = "");
	std::vector<std::string> getWriteBackNames();
	bool startElement(std::string &context, std::string &qName, std::map<
			std::string, std::string> &attribute);
	bool startCharacter(std::string &context, std::string &qName,
			char *character, int len);
	//	bool endElement(std::string context, std::string qName);
	void buildXML(std::stringstream &s, int flag);
	friend class KAccess;
private:
	KWriteBack * getWriteBack(std::string table_name);
	KRWLock lock;
	std::map<std::string,KWriteBack *> writebacks;
};
extern KWriteBackManager writeBackManager;
#endif
#endif /*KWRITEBACKMANAGER_H_*/
