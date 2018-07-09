// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "CryName.h"

// CCryName TypeInfo

TYPE_INFO_BASIC(CCryName)

string ToString(CCryName const& val)
{
	return string(val.c_str());
}
bool FromString(CCryName& val, const char* s)
{
	val = s;
	return true;
}
