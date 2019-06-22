// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Schematyc2
{
	struct CVars
	{
		static ICVar* sc2_FileFormat;
		static ICVar* sc2_RootFolder;
		static int    sc2_IgnorePAKFiles;
		static int    sc2_IgnoreUnderscoredFolders;
		static int    sc2_DiscardOnSave;

		static ICVar* sc2_LogToFile;
		static ICVar* sc2_LogFileStreams;
		static ICVar* sc2_LogFileMessageTypes;
		static int    sc2_DisplayCriticalErrors;

		static int    sc2_LegacyMode;
		static int    sc2_RunUnitTests;
		static int    sc2_MaxRecursionDepth;
		static ICVar* sc2_ExperimentalFeatures;
		
		static float  sc2_FunctionTimeLimit;
		static int    sc2_UseNewGraphPipeline;

		static float  sc2_RelevanceGridCellSize;
		static int    sc2_RelevanceGridDebugStatic;

		static void Register();
		static void Unregister();

		static inline const char* GetStringSafe(const ICVar* pCVar)
		{
			if(pCVar)
			{
				const char* szResult = pCVar->GetString();
				if(szResult)
				{
					return szResult;
				}
			}
			return "";
		}
	};
}
