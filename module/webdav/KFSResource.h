#ifndef KFSRESOURCE_H
#define KFSRESOURCE_H
#include <stdio.h>
#include <list>
#include "KResource.h"
#include "KResourceMaker.h"
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#endif
class KFSResource :
	public KResource
{
public:
	KFSResource(struct stat *buf);
	~KFSResource(void);
	KAttribute *getAttribute() override;
	bool rename(const char *newName) override;
	bool remove() override;
	bool isDirectory() override;
	bool open(bool writeFlag=true) override;
	bool copy(KResource *dst) override;
	bool close() override;
	int write(const char *buf,int len) override;
	int read(char *buf,int len) override;
	bool bind(const char *name, KAttribute *attribute) override;
	bool unbind(const char *name) override;
	bool rebind(const char *name, KAttribute *attribute) override;
	void listChilds(std::list<KResource *> &result) override;
	time_t getLastModified() override
	{
		return buf->st_mtime;
	}
	friend class KFSResourceMaker;
protected:
	struct stat *buf;
	FILE *fp;
};
class KFSResourceMaker : public KResourceMaker
{
public:
	KResource *bindResource(const char *name,const char *path) ;
	KResource *makeFile(const char *name,const char *path);
	KResource *makeDirectory(const char *name,const char *path);	
};
extern KResourceMaker *rsMaker;
#endif
