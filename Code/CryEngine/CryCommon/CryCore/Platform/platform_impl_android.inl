// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

void CryFindEngineRootFolder(unsigned int engineRootPathSize, char* szEngineRootPath)
{
	// Hack! Android currently does not support a directory layout, there is an explicit search in main for GameSDK/GameData.pak
	// and the executable folder is not related to the engine or game folder. - 18/03/2016
	cry_strcpy(szEngineRootPath, engineRootPathSize, CryGetProjectStoragePath());
}