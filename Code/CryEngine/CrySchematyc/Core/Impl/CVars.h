// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Schematyc
{
struct CVars
{
	static ICVar* sc_RootFolder;
	static int    sc_IgnorePAKFiles;
	static int    sc_IgnoreUnderscoredFolders;
	static int    sc_EnableScriptPartitioning;

	static int    sc_LogToFile;
	static ICVar* sc_LogFileStreams;
	static ICVar* sc_LogFileMessageTypes;
	static int    sc_DisplayCriticalErrors;

	static int    sc_RunUnitTests;
	static ICVar* sc_ExperimentalFeatures;

	static int sc_allowFlowGraphNodes;

	static void               Register();
	static void               Unregister();

	static inline const char* GetStringSafe(const ICVar* pCVar)
	{
		if (pCVar)
		{
			const char* szResult = pCVar->GetString();
			if (szResult)
			{
				return szResult;
			}
		}
		return "";
	}
};
} // Schematyc
