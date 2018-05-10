// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Schematyc2
{
	struct CVars
	{
		static ICVar* sc_FileFormat;
		static ICVar* sc_RootFolder;
		static int    sc_IgnorePAKFiles;
		static int    sc_IgnoreUnderscoredFolders;
		static int    sc_DiscardOnSave;

		static ICVar* sc_LogToFile;
		static ICVar* sc_LogFileStreams;
		static ICVar* sc_LogFileMessageTypes;
		static int    sc_DisplayCriticalErrors;

		static int    sc_RunUnitTests;
		static int    sc_MaxRecursionDepth;
		static ICVar* sc_ExperimentalFeatures;
		
		static float  sc_FunctionTimeLimit;
		static int    sc_UseNewGraphPipeline;

		static float  sc_RelevanceGridCellSize;
		static int    sc_RelevanceGridDebugStatic;

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
