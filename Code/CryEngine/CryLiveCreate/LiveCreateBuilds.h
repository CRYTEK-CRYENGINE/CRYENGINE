// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace LiveCreate
{
// Date and time of the build
struct SDateTime
{
	SDateTime()
		: year(0)
		, month(0)
		, dayOfWeek(0)
		, day(0)
		, hour(0)
		, minute(0)
		, second(0)
		, milliseconds(0)
	{}

	uint16 year;
	uint16 month;
	uint16 dayOfWeek;
	uint16 day;
	uint16 hour;
	uint16 minute;
	uint16 second;
	uint16 milliseconds;
};

// Used to get information about the builds on the machines (from BuildInfo.txt)
struct SGameBuildInfo
{
	static const int kMaxBuildNameSize = 256;
	static const int kMaxBranchNameSize = 256;
	static const int kMaxPathSize = 256;
	static const int kMaxExeNameListSize = 512;

	SGameBuildInfo()
		: buildNumber(0)
		, buildChangelist(0)
		, codeChangelist(0)
		, assetChangelist(0)
	{
		executables[0] = 0;
		buildName[0] = 0;
		codeBranch[0] = 0;
		assetBranch[0] = 0;
		buildPath[0] = 0;
	}

	int       buildNumber;
	char      buildName[kMaxBuildNameSize];
	SDateTime buildTime;
	int       buildChangelist;
	int       codeChangelist;
	int       assetChangelist;
	char      codeBranch[kMaxBranchNameSize];
	char      assetBranch[kMaxBranchNameSize];
	char      buildPath[kMaxPathSize];
	// comma separated exe names, like: "Crysis3.xex,Crysis3Profile.xex"
	char      executables[kMaxExeNameListSize];
};
}
