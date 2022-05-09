#ifndef KRESOURCE_H
#define KRESOURCE_H
#include <list>
#include "KAttribute.h"

class KResource
{
public:
	KResource(void);
	virtual ~KResource(void);
	virtual KAttribute *getAttribute() = 0;
	virtual bool rename(const char *newName) = 0;
	virtual bool copy(KResource *dst) = 0;
	virtual bool remove() = 0;
	virtual bool bind(const char *name, KAttribute *attribute) = 0;
	virtual bool unbind(const char *name) = 0;
	virtual bool rebind(const char *name, KAttribute *attribute) = 0;
	virtual void listChilds(std::list<KResource *> &result) = 0;
	virtual time_t getLastModified() = 0;
	virtual bool isDirectory()
	{
		return false;
	}
	char *getPath()
	{
		return path;
	}
	void setPath(char *path)
	{
		this->path = path;
	}
	const char *getName()
	{
		return name;
	}
	virtual bool open(bool writeFlag=true)
	{
		return false;
	}
	virtual bool close()
	{
		return false;
	}
	virtual int write(const char *buf,int len)
	{
		return -1;
	}
	virtual int read(char *buf,int len)
	{
		return -1;
	}
	void setName(char *name);
protected:
	char *name;
	char *path;
};
#endif
