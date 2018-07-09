// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/yasli/Config.h>

namespace yasli {

class Archive;

struct FileOpen
{
	string* pathPointer;
	string path;
	string filter;
	string relativeToFolder;
	int flags;

	enum {
		STRIP_EXTENSION = 1
	};

	// filter is defined in the following format:
	// "All Images (*.bmp *.jpg *.tga);; Bitmap (*.bmp);; Targa (*.tga)"
	FileOpen(string& path, const char* filter, const char* relativeToFolder = "", int flags = 0)
	: pathPointer(&path)
	, filter(filter)
	, relativeToFolder(relativeToFolder)
	, flags(flags)
	{
		this->path = path;
	}

    FileOpen() : pathPointer(0), flags(0) { }

	FileOpen& operator=(const FileOpen& rhs)
	{
		path = rhs.path;
        flags = rhs.flags;
		if (rhs.pathPointer) {
			filter = rhs.filter;
			relativeToFolder = rhs.relativeToFolder;
		}
		return *this;
	}

	~FileOpen()
	{
		if (pathPointer)
			*pathPointer = path;
	}

	void YASLI_SERIALIZE_METHOD(Archive& ar);
};

bool YASLI_SERIALIZE_OVERRIDE(Archive& ar, FileOpen& value, const char* name, const char* label);

}

#if YASLI_INLINE_IMPLEMENTATION
#include <CrySerialization/yasli/decorators/FileOpenImpl.h>
#endif
