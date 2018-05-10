// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "QtUtil.h"

#include <CrySerialization/yasli/JSONOArchive.h>

namespace Private_SessionData
{

struct SSessionData
{
	// For now, command line arguments are sufficient.
	string commandLineArguments;

	void Serialize(yasli::Archive& ar)
	{
		ar(commandLineArguments, "cmdline");
	}
};

} // namespace Private_SessionData

void WriteSessionData()
{
	Private_SessionData::SSessionData sessionData;
	sessionData.commandLineArguments = GetCommandLineA();

	yasli::JSONOArchive ar;
	ar(sessionData, "sessionData");

	const QString filePath = QtUtil::GetAppDataFolder() + "/LastSession.json";
	FILE* const pFile = fopen(filePath.toLocal8Bit().constData(), "w");
	if (pFile)
	{
		fprintf(pFile, "%s", ar.c_str());
		fclose(pFile);
	}
}

