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
	KAttribute *getAttribute();
	bool rename(const char *newName);
	bool remove();
	bool isDirectory();
	bool open(bool writeFlag=true);
	bool copy(KResource *dst) override;
	bool close();
	int write(const char *buf,int len);
	int read(char *buf,int len);
	bool bind(const char *name, KAttribute *attribute) ;
	bool unbind(const char *name) ;
	bool rebind(const char *name, KAttribute *attribute) ;
	void listChilds(std::list<KResource *> &result);
	time_t getLastModified()
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
