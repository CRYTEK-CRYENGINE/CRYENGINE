// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>

namespace Schematyc2
{
	namespace FileUtils
	{
		enum class EFileEnumFlags
		{
			None                     = 0,
			IgnoreUnderscoredFolders = BIT(0),
			Recursive                = BIT(1),
		};

		DECLARE_ENUM_CLASS_FLAGS(EFileEnumFlags)

		typedef TemplateUtils::CDelegate<void (const char*, unsigned)> FileEnumCallback;

		void EnumFilesInFolder(const char* szFolderName, const char* szExtension, FileEnumCallback callback, EFileEnumFlags flags = EFileEnumFlags::Recursive);
	}
}