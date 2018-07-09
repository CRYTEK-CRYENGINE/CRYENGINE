// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MAXUSERPROPERTYHELPERS_H__
#define __MAXUSERPROPERTYHELPERS_H__

#include <string>

class INode;

namespace MaxUserPropertyHelpers
{
	std::string GetNodeProperties(INode* node);
	std::string GetStringNodeProperty(INode* node, const char* name, const char* defaultValue);
	float GetFloatNodeProperty(INode* node, const char* name, float defaultValue);
	int GetIntNodeProperty(INode* node, const char* name, int defaultValue);
	bool GetBoolNodeProperty(INode* node, const char* name, bool defaultValue);
}

#endif
