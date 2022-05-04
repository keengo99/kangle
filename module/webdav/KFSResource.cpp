#ifdef _WIN32
#include <direct.h>
#endif
#include <sstream>
#include "KFSResource.h"
#include "directory.h"
#include "kforwin32.h"
static const char *days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
#ifdef _WIN32
#define T(x) L(x)
wchar_t *utf8toUnicode(const char *str)
{
	int len = strlen(str);
	wchar_t *s = (wchar_t *)xmalloc(2*(len+1));
	len = MultiByteToWideChar(CP_UTF8,0,str,len,(LPWSTR)s,len);
	s[len] = L'\0';
	return s;
}
char *unicodeToUtf8(const wchar_t *str)
{
	int len = wcslen(str);
	char *s = (char *)xmalloc(3*len+1);
	len = WideCharToMultiByte(CP_UTF8,0,str,len,s,3*len,NULL,NULL);
	s[len] = '\0';
	return s;
}
#endif
struct KDirectoryListResult
{
	std::list<KResource *> *result;
	KResource *rs;
};
int fsdirListHandle(const char *file,void *param)
{
	KDirectoryListResult *dir = (KDirectoryListResult *)param;
	std::stringstream name,path;
	name << dir->rs->getName() << PATH_SPLIT_CHAR << file;
	path << dir->rs->getPath() << "/" << file;
	//printf("file=[%s]\n",file);
	KResource *nrs = rsMaker->bindResource(name.str().c_str(),path.str().c_str());
	if(nrs){
		dir->result->push_back(nrs);
	}
	return 0;
}
#ifdef _WIN32
int fsdirListHandlew(const wchar_t *file,void *param)
{
	KDirectoryListResult *dir = (KDirectoryListResult *)param;
	char *ufile = unicodeToUtf8(file);
	if(ufile==NULL){
		return 0;
	}
	std::stringstream name,path;

	name << dir->rs->getName() << PATH_SPLIT_CHAR << ufile;
	path << dir->rs->getPath() << "/" << ufile;
	//printf("file=[%s]\n",ufile);
	KResource *nrs = rsMaker->bindResource(name.str().c_str(),path.str().c_str());
	if(nrs){
		dir->result->push_back(nrs);
	}
	xfree(ufile);
	return 0;
}
#endif
KFSResource::KFSResource(struct stat *buf)
{
	this->buf = buf;
	fp = NULL;
}

KFSResource::~KFSResource(void)
{
	delete buf;
	if(fp){
		fclose(fp);
	}
}
bool KFSResource::open(bool writeFlag)
{
	if(fp){
		return true;
	}
#ifdef _WIN32
	wchar_t *wname = utf8toUnicode(name);
	if(wname==NULL){
		return false;
	}
	fp = _wfopen(wname,writeFlag?L"wb":L"rb");
	xfree(wname);
#else
	fp = fopen(name,writeFlag?"wb":"rb");
#endif
	return fp!=NULL;
}
bool KFSResource::close()
{
	if(fp){
		fclose(fp);
		fp = NULL;
	}
	return true;
}
int KFSResource::write(const char *buf,int len)
{
	if(fp==NULL){
		return -1;
	}
	return fwrite(buf,1,len,fp);
}
int KFSResource::read(char *buf,int len)
{
	return fread(buf,1,len,fp);
}
KResource *KFSResourceMaker::makeFile(const char *name,const char *path)
{
	FILE *fp = NULL;
#ifdef _WIN32
	wchar_t *wname = utf8toUnicode(name);
	if(wname==NULL){
		return false;
	}
	fp = _wfopen(wname,L"wb");
	xfree(wname);
#else
	fp = fopen(name,"wb");
#endif
	if(fp==NULL){
		return NULL;
	}
	KFSResource *rs = (KFSResource *)bindResource(name,path);
	if(rs==NULL){
		fclose(fp);
		return NULL;
	}
	rs->fp = fp;
	return rs;
}
KResource *KFSResourceMaker::makeDirectory(const char *name,const char *path)
{
#ifdef _WIN32
	wchar_t *wname = utf8toUnicode(name);
	if(wname==NULL){
		return false;
	}
	_wmkdir(wname);
	xfree(wname);
#else
	mkdir(name,0755);
#endif
	return bindResource(name,path);
}
KResource *KFSResourceMaker::bindResource(const char *name,const char *path)
{
#ifdef _WIN32
/*	char *hot = name + strlen(name) -1;
	if(*hot=='/'){
		*hot = '\0';
	}
	*/
#endif
	struct stat *buf = new struct stat;
	int ret;
#ifdef _WIN32
	wchar_t *wname = utf8toUnicode(name);
	if(wname==NULL){
		ret = stat(name,buf);
	}else{
		ret = _wstat(wname,(struct _stat64i32 *)buf);
		xfree(wname);
	}
#else
	ret = stat(name, buf) ;
#endif
	if(ret != 0) {
		delete buf;
		return NULL;
	}
	KResource *rs =  new KFSResource(buf);
	rs->setName(xstrdup(name));
	rs->setPath(xstrdup(path));
	return rs;
}


KAttribute *KFSResource::getAttribute()
{
	KAttribute *ab = new KAttribute;
	struct tm tm;
	time_t holder = buf->st_mtime;
	gmtime_r(&holder, &tm);
	char tbuf[64];
	snprintf(tbuf,sizeof(tbuf),"%d-%02d-%02dT%02d:%02d:%02dZ",tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour, tm.tm_min, tm.tm_sec);
	ab->add("creationdate",(const char *)tbuf);
	snprintf(tbuf, sizeof(tbuf), "%s, %02d %s %d %02d:%02d:%02d GMT", days[tm.tm_wday],
			tm.tm_mday, months[tm.tm_mon], tm.tm_year + 1900, tm.tm_hour,
			tm.tm_min, tm.tm_sec);
	//mk1123time(buf->st_mtime,tbuf,sizeof(tbuf));
	ab->add("getlastmodified",(const char *)tbuf);
	if(isDirectory()){
		ab->add("getcontenttype","httpd/unix-directory");
	}else{
		sprintf(tbuf,"%d",(int)buf->st_size);
		ab->add("getcontentlength",(const char *)tbuf);
	}
	return ab;
}
bool KFSResource::rename(const char *newName) 
{
#ifdef _WIN32
	wchar_t *wname = utf8toUnicode(name);
	if(wname==NULL){
		return false;
	}
	wchar_t *wnewName = utf8toUnicode(newName);
	if(wnewName==NULL){
		xfree(wname);
		return false;
	}
	int ret = _wrename(wname,wnewName);
	xfree(wname);
	xfree(wnewName);
	return ret==0;
#else
	return ::rename(name,newName)==0;
#endif
}
bool KFSResource::isDirectory()
{
	return S_ISDIR(buf->st_mode)>0;
}
bool KFSResource::remove() 
{
#ifdef _WIN32
	int ret;
	wchar_t *wname = utf8toUnicode(name);
	if(wname==NULL){
		return false;
	}
	if(!isDirectory()){
		ret = _wunlink(wname);
	}else{
		ret = _wrmdir(wname);
	}
	xfree(wname);
	return ret==0;
#else
	if(!isDirectory()){
		return unlink(name)==0;
	}
	return rmdir(name)==0;
#endif
}
void KFSResource::listChilds(std::list<KResource *> &result)
{
	KDirectoryListResult listResult;
	listResult.result = &result;
	listResult.rs = this;
#ifdef _WIN32
	wchar_t *wname = utf8toUnicode(name);
	if(wname==NULL){
		return;
	}
	list_dirw(wname,fsdirListHandlew,&listResult);
	xfree(wname);
#else
	list_dir(name,fsdirListHandle,&listResult);
#endif
}
bool KFSResource::bind(const char *name, KAttribute *attribute)
{
	return false;
}
bool KFSResource::unbind(const char *name)
{
	return false;
}
bool KFSResource::rebind(const char *name, KAttribute *attribute)
{
	return false;
}
