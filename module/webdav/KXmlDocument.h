#ifndef KXMLDOCUMENT_H
#define KXMLDOCUMENT_H
#include <list>
#include "KXmlEvent.h"
class KXmlNode
{
public:
	KXmlNode(){
		next = NULL;
	}
	~KXmlNode()
	{
		std::map<std::string,KXmlNode *>::iterator it;
		for(it=childs.begin();it!=childs.end();it++){
			delete (*it).second;
		}
		if(next){
			delete next;
		}
	}
	KXmlNode *getChild(std::string tag)
	{
		std::map<std::string,KXmlNode *>::iterator it;
		int pos = (int)tag.find('/');
		std::string childtag;
		if(pos!=-1){
			childtag = tag.substr(0,pos);
		}else{
			childtag = tag;
		}
		it = childs.find(childtag);
		if(it==childs.end()){
			return NULL;
		}
		if(pos!=-1){
			return (*it).second->getChild(tag.substr(pos+1));
		}
		return (*it).second;
	}
	KXmlNode *getNext()
	{
		return next;
	}
	std::string getNameSpace()
	{
		return ns;
	}
	void addChild(KXmlNode *node)
	{
		//printf("add child[%s] to [%s]\n",node->getTag().c_str(),getTag().c_str());
		KXmlNode *child_node = getChild(node->getTag());
		if(child_node==NULL){
			childs.insert(std::pair<std::string,KXmlNode *>(node->getTag(),node));
		}else{
			for(;;){
				KXmlNode *n = child_node->getNext();
				if(n==NULL){
					child_node->next = node;
					break;
				}
				child_node = n;
			}
		}
			
	}
	std::string getTag()
	{
		return tag;
	}
	std::string getAttribute(std::string name)
	{
		return attributes[name];
	}
	std::string getCharacter()
	{
		return character;
	}
	friend class KXmlDocument;
	std::map<std::string,KXmlNode *> childs;
private:
	KXmlNode *next;
	std::string ns;	
	std::string tag;
	std::string character;
	std::map<std::string,std::string> attributes;
};
class KXmlDocument :
	public KXmlEvent
{
public:
	KXmlDocument(void);
	~KXmlDocument(void);
	KXmlNode *parse(char *str);
	KXmlNode *getRootNode();
	KXmlNode *getNode(std::string name);
	bool startElement(KXmlContext *context, std::map<std::string,
			std::string> &attribute);
	bool startCharacter(KXmlContext *context, char *character, int len);
	bool endElement(KXmlContext *context);
private:
	KXmlNode *curNode;
	std::list<KXmlNode *> nodeStack;
};
#endif
