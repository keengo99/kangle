#ifndef KSERVERNODE_H
#define KSERVERNODE_H
#include "global.h"
#include <string>
#include <map>
#include <stdlib.h>
class KHttpRequest;
enum ServerNodeType
{
	normalNode,
	linkServerNode,
	linkMServerNode,
};
class KServerNode
{
public:
	KServerNode()
	{
		next = prev = NULL;
		hit = 0;
	}
	virtual ServerNodeType getNodeType()
	{
		return normalNode;
	}
	virtual bool isNodeChanged(KServerNode *a) = 0;
	virtual void refNode() = 0;
	virtual void releaseNode() = 0;
	virtual bool nodeIsEnable() = 0;
	virtual void enableNode() = 0;
	virtual void connectNode(KHttpRequest *rq) = 0;
	virtual std::string getHost() = 0;
	virtual int getNodeSize() = 0;
	virtual void parseNode(std::map<std::string, std::string> &attr) = 0;
	virtual int getPort()
	{
		return 0;
	}
	virtual int getNodeLifeTime()
	{
		return 0;
	}
	KServerNode *next;
	KServerNode *prev;
	int hit;
	int weight;
protected:
	virtual ~KServerNode()
	{

	}
};
#endif
