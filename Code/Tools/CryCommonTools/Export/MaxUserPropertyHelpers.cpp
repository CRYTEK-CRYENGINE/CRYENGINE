// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MaxUserPropertyHelpers.h"
#include "StringHelpers.h"
#include "MaxHelpers.h"


std::string MaxUserPropertyHelpers::GetNodeProperties(INode* node)
{
	if (node == 0)
	{
		return std::string();
	}

	MSTR buf;
	node->GetUserPropBuffer(buf);

	return MaxHelpers::CreateAsciiString(buf);
}


std::string MaxUserPropertyHelpers::GetStringNodeProperty(INode* node, const char* name, const char* defaultValue)
{
	if (node == 0)
	{
		return defaultValue;
	}

	MSTR val;
	if (!node->GetUserPropString(MaxHelpers::CreateMaxStringFromAscii(name), val))
	{
		return defaultValue;
	}

	return MaxHelpers::CreateAsciiString(val);
}


float MaxUserPropertyHelpers::GetFloatNodeProperty(INode* node, const char* name, float defaultValue)
{
	if (node == 0)
	{
		return defaultValue;
	}

	float val;
	if (!node->GetUserPropFloat(MaxHelpers::CreateMaxStringFromAscii(name), val))
	{
		return defaultValue;
	}

	return val;
}


int MaxUserPropertyHelpers::GetIntNodeProperty(INode* node, const char* name, int defaultValue)
{
	if (node == 0)
	{
		return defaultValue;
	}

	int val;
	if (!node->GetUserPropInt(MaxHelpers::CreateMaxStringFromAscii(name), val))
	{
		return defaultValue;
	}

	return val;
}


bool MaxUserPropertyHelpers::GetBoolNodeProperty(INode* node, const char* name, bool defaultValue)
{
	if (node == 0)
	{
		return defaultValue;
	}

	BOOL val;
	if (!node->GetUserPropBool(MaxHelpers::CreateMaxStringFromAscii(name), val))
	{
		return defaultValue;
	}

	return (val != 0);
}
