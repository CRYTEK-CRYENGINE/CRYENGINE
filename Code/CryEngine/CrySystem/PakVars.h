// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _CRY_SYSTEM_PAK_VARS_HDR_
#define _CRY_SYSTEM_PAK_VARS_HDR_

enum EPakPriority
{
	ePakPriorityFileFirst         = 0,
	ePakPriorityPakFirst          = 1,
	ePakPriorityPakOnly           = 2,
	ePakPriorityFileFirstModsOnly = 3,
};

// variables that control behaviour of CryPak/StreamEngine subsystems
struct PakVars
{
	int nSaveLevelResourceList;
	int nSaveFastloadResourceList;
	int nSaveTotalResourceList;
	int nSaveMenuCommonResourceList;
	int nLogMissingFiles;
	int nLoadCache;
	int nLoadModePaks;
	int nReadSlice;
	int nPriority;
	int nMessageInvalidFileAccess;
	int nValidateFileHashes;
	int nTotalInMemoryPakSizeLimit;
	int nStreamCache;
	int nInMemoryPerPakSizeLimit;
	int nLogInvalidFileAccess;
	int nLoadFrontendShaderCache;
	int nUncachedStreamReads;
#ifndef _RELEASE
	int nLogAllFileAccess;
#endif
	int nDisableNonLevelRelatedPaks;

	PakVars()
		: nPriority(0)
		, nReadSlice(0)
		, nLogMissingFiles(0)
		, nInMemoryPerPakSizeLimit(0)
		, nSaveTotalResourceList(0)
		, nSaveFastloadResourceList(0)
		, nSaveMenuCommonResourceList(0)
		, nSaveLevelResourceList(0)
		, nValidateFileHashes(0)
		, nUncachedStreamReads(1)
	{
		nInMemoryPerPakSizeLimit = 6;    // 6 Megabytes limit
		nTotalInMemoryPakSizeLimit = 30; // Megabytes

		nLoadCache = 0;       // Load in memory paks from _FastLoad folder
		nLoadModePaks = 0;    // Load menucommon/gamemodeswitch paks
		nStreamCache = 0;

#if defined(_RELEASE)
		nPriority = ePakPriorityPakOnly;  // Only read from pak files by default
#else
		nPriority = ePakPriorityFileFirst;
#endif

		nMessageInvalidFileAccess = 0;
		nLogInvalidFileAccess = 0;

#ifndef _RELEASE
		nLogAllFileAccess = 0;
#endif

#if defined(_DEBUG)
		nValidateFileHashes = 1;
#endif

		nLoadFrontendShaderCache = 0;
		nDisableNonLevelRelatedPaks = 1;
	}
};

#endif
