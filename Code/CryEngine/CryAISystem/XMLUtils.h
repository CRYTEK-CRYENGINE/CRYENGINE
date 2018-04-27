// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _XMLUtils_h__
#define _XMLUtils_h__

#pragma once

namespace XMLUtils
{
enum BoolType
{
	Invalid = -1,
	False   = 0,
	True    = 1,
};

inline BoolType ToBoolType(const char* str)
{
	if (!stricmp(str, "1") || !stricmp(str, "true") || !stricmp(str, "yes"))
		return True;

	if (!stricmp(str, "0") || !stricmp(str, "false") || !stricmp(str, "no"))
		return False;

	return Invalid;
}

inline BoolType GetBoolType(const XmlNodeRef& node, const char* attribute, const BoolType& deflt)
{
	if (node->haveAttr(attribute))
	{
		const char* value;
		node->getAttr(attribute, &value);

		return ToBoolType(value);
	}

	return deflt;
}
}

#endif // XMLUtils
