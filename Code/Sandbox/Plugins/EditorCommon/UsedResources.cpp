// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UsedResources.h"
#include <io.h>
#include <CrySystem/File/ICryPak.h>

CUsedResources::CUsedResources()
{
}

void CUsedResources::Add(const char* pResourceFileName)
{
	if (pResourceFileName && strcmp(pResourceFileName, ""))
	{
		files.insert(pResourceFileName);
	}
}

void CUsedResources::Validate()
{
	ICryPak* pPak = gEnv->pCryPak;

	for (TResourceFiles::iterator it = files.begin(); it != files.end(); ++it)
	{
		const string& filename = *it;

		bool fileExists = pPak->IsFileExist(filename);

		if (!fileExists)
		{
			const char* ext = PathUtil::GetExt(filename);
			if (!stricmp(ext, "tif") ||
			    !stricmp(ext, "hdr"))
			{
				fileExists = gEnv->pCryPak->IsFileExist(PathUtil::ReplaceExtension(filename.GetString(), "dds"));
			}
			else if (!stricmp(ext, "dds"))
			{
				fileExists = gEnv->pCryPak->IsFileExist(PathUtil::ReplaceExtension(filename.GetString(), "tif")) ||
				             gEnv->pCryPak->IsFileExist(PathUtil::ReplaceExtension(filename.GetString(), "hdr"));
			}
		}

		if (!fileExists)
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Resource File %s not found,", (const char*)filename);
	}
}

