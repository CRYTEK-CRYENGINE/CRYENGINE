// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Util.h"

namespace Cry {

namespace SchematycEd {

//////////////////////////////////////////////////////////////////////////
void GetSubFoldersAndFileNames(const char* szFolderName, const char* szExtension, bool bIgnorePakFiles, TStringVector& subFolderNames, TStringVector& fileNames)
{
	CRY_ASSERT(szFolderName);
	if (szFolderName)
	{
		string searchPath = szFolderName;
		searchPath.append("/");
		searchPath.append(szExtension ? szExtension : "*.*");
		_finddata_t findData;
		intptr_t    handle = gEnv->pCryPak->FindFirst(searchPath.c_str(), &findData);
		if (handle >= 0)
		{
			do
			{
				if (findData.name[0] != '.')
				{
					if (findData.attrib & _A_SUBDIR)
					{
						bool bIgnoreSubDir = false;
						if (bIgnorePakFiles)
						{
							stack_string fileName = gEnv->pCryPak->GetGameFolder();
							fileName.append("/");
							fileName.append(szFolderName);
							fileName.append("/");
							fileName.append(findData.name);
							if (GetFileAttributes(fileName.c_str()) == INVALID_FILE_ATTRIBUTES)
							{
								bIgnoreSubDir = true;
							}
						}
						if (!bIgnoreSubDir)
						{
							subFolderNames.push_back(findData.name);
						}
					}
					else if (!bIgnorePakFiles || ((findData.attrib & _A_IN_CRYPAK) == 0))
					{
						fileNames.push_back(findData.name);
					}
				}
			} while (gEnv->pCryPak->FindNext(handle, &findData) >= 0);
		}
	}
}

} // namespace SchematycEd

} // namespace Cry