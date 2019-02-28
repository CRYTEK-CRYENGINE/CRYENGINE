// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>

namespace Serialization
{

struct ResourceFilePath
{
	enum { STRIP_EXTENSION = 1 << 0 };

	string* path;
	string  filter;
	string  startFolder;
	int     flags;

	//! Filters are defined in the following format:
	//! "All Images (bmp, jpg, tga)|*.bmp;*.jpg;*.tga|Targa (tga)|*.tga".
	explicit ResourceFilePath(string& path, const char* filter = "All files|*.*", const char* startFolder = "", int flags = 0)
		: path(&path)
		, filter(filter)
		, startFolder(startFolder)
		, flags(flags)
	{
	}

	//! This function should stay virtual to ensure cross-dll calls are using right heap.
	virtual void SetPath(const char* path) { *this->path = path; }
};

bool Serialize(Serialization::IArchive& ar, Serialization::ResourceFilePath& value, const char* name, const char* label);

}

#include "ResourceFilePathImpl.h"
