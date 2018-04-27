// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Utils/Delegate.h"
#include "CrySchematyc/Utils/EnumFlags.h"

namespace Schematyc
{
namespace FileUtils
{
enum class EFileEnumFlags
{
	None                     = 0,
	IgnoreUnderscoredFolders = BIT(0),
	Recursive                = BIT(1),
};

typedef CEnumFlags<EFileEnumFlags>              FileEnumFlags;

typedef std::function<void (const char*, unsigned)> FileEnumCallback;

void EnumFilesInFolder(const char* szFolderName, const char* szExtension, FileEnumCallback callback, const FileEnumFlags& flags = EFileEnumFlags::Recursive);
} // FileUtils
} // Schematyc
