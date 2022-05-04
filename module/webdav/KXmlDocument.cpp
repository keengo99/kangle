#include <stdlib.h>
#include <string.h>
#include <vector>
#include "KXml.h"
#include "KXmlDocument.h"
using namespace std;
KXmlDocument::KXmlDocument(void)
{
	curNode=NULL;
}

KXmlDocument::~KXmlDocument(void)
{
	if(curNode){
		delete curNode;
	}
	std::list<KXmlNode *>::iterator it;
	for(it=nodeStack.begin();it!=nodeStack.end();it++){
		delete (*it);
	}
}
KXmlNode *KXmlDocument::getNode(std::string name)
{
	KXmlNode *rootNode = getRootNode();
	if(rootNode==NULL){
		return NULL;
	}
	std::string tag;
	int pos = (int)name.find('/');
	if(pos!=-1){
		tag = name.substr(0,pos);
	}else{
		tag = name;
	}
	if(rootNode->getTag()!=tag){
		return NULL;
	}
	if(pos==-1){
		return rootNode;
	}
	return rootNode->getChild(name.substr(pos+1));
}
KXmlNode *KXmlDocument::getRootNode()
{
	return curNode;
}
KXmlNode *KXmlDocument::parse(char *str)
{
	KXml xml;
	xml.setEvent(this);
	xml.startParse(str);
	return curNode;
}
bool KXmlDocument::startElement(KXmlContext *context, std::map<std::string,
			std::string> &attribute)
{
	if(curNode){
		nodeStack.push_front(curNode);
	}
	curNode = new KXmlNode();
	curNode->attributes.swap(attribute);
	string::size_type pos = context->qName.find(':');
	if(pos!=string::npos){
		curNode->ns = context->qName.substr(0,pos);
		curNode->tag = context->qName.substr(pos+1);
	}else{
		curNode->tag = context->qName;
	}
	return true;
	
}
bool KXmlDocument::startCharacter(KXmlContext *context, char *character, int len)
{
	if(curNode==NULL){
		return false;
	}
	curNode->character = character;
	return true;
}
bool KXmlDocument::endElement(KXmlContext *context)
{
	if(curNode==NULL){
		return false;
	}
	if(nodeStack.size()==0){
		return true;
	}
	KXmlNode *parentNode = *nodeStack.begin();
	parentNode->addChild(curNode);
	curNode = parentNode;
	nodeStack.pop_front();
	return true;
}
