// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <CrySerialization/IArchive.h>
#include <CrySerialization/STL.h>

// FIXME: this should be replaced by CryGUID instead of using Windows-specific type
// from guiddef.h
#ifndef GUID_DEFINED
	#define GUID_DEFINED
typedef struct _GUID
{
	unsigned long  Data1;
	unsigned short Data2;
	unsigned short Data3;
	unsigned char  Data4[8];
} GUID;
#endif
// ^^^^

#include <CryCore/ToolsHelpers/GuidUtil.h>

inline bool Serialize(Serialization::IArchive& ar, GUID& guid, const char* name, const char* label)
{
	string str = GuidUtil::ToString(guid);
	if (!ar(str, name, label))
		return false;
	if (ar.isInput())
		guid = GuidUtil::FromString(str.c_str());
	return true;
}

