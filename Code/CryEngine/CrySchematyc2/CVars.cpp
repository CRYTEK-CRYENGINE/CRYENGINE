// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "CVars.h"
#include <CrySystem/ConsoleRegistration.h>

namespace Schematyc2
{
	ICVar* CVars::sc2_FileFormat               = nullptr;
	ICVar* CVars::sc2_RootFolder               = nullptr;
	int    CVars::sc2_IgnorePAKFiles           = 0;
	int    CVars::sc2_IgnoreUnderscoredFolders = 1;
	int    CVars::sc2_DiscardOnSave            = 0;

	ICVar* CVars::sc2_LogToFile                = nullptr;
	ICVar* CVars::sc2_LogFileStreams           = nullptr;
	ICVar* CVars::sc2_LogFileMessageTypes      = nullptr;
	int    CVars::sc2_DisplayCriticalErrors    = 0;

	int    CVars::sc2_LegacyMode               = 0;
	int    CVars::sc2_RunUnitTests             = 0;
	int    CVars::sc2_MaxRecursionDepth        = 100;
	ICVar* CVars::sc2_ExperimentalFeatures     = nullptr;

	float  CVars::sc2_FunctionTimeLimit        = 0.0f;
	float  CVars::sc2_RelevanceGridCellSize    = 16.0f;
	int    CVars::sc2_RelevanceGridDebugStatic = 0;
	int    CVars::sc2_UseNewGraphPipeline      = 0;

	static const char* g_szDisplayCriticalErrorsDescription = "Display/hide Schematyc critical error message alerts\n"
																													  "\t0 = hide\n"
																													  "\t1 = display\n";

	void CVars::Register()
	{
		sc2_FileFormat = REGISTER_STRING("sc2_FileFormat", "xml", VF_READONLY, "Format of Schematyc files");
		sc2_RootFolder = REGISTER_STRING("sc2_RootFolder", "libs/schematyc", VF_READONLY, "Root folder for Schematyc files");
		sc2_IgnorePAKFiles = REGISTER_CVAR(sc2_IgnorePAKFiles, sc2_IgnorePAKFiles, VF_READONLY, "Ignore PAK files")->GetIVal();
		sc2_IgnoreUnderscoredFolders = REGISTER_CVAR(sc2_IgnoreUnderscoredFolders, sc2_IgnoreUnderscoredFolders, VF_READONLY, "Ignore folders whose names begin with an underscore")->GetIVal();
		sc2_DiscardOnSave = REGISTER_CVAR(sc2_DiscardOnSave, sc2_DiscardOnSave, VF_READONLY, "Discard broken script elements when saving files")->GetIVal();
		
		sc2_LogToFile = REGISTER_INT("sc2_LogToFile", 0, VF_NULL, "Configure Schematyc log file output\n\t0 = disabled\n\t1 = write to file\n\tforward to standard log");
		sc2_LogFileStreams = REGISTER_STRING("sc2_LogFileStreams", "Default System Compiler Game", VF_NULL, "Set which streams should be sent to log file");
		sc2_LogFileMessageTypes = REGISTER_STRING("sc2_LogFileMessageTypes", "Warning Error CriticalError", VF_NULL, "Set which message types should be sent to log file");
		sc2_DisplayCriticalErrors = REGISTER_CVAR(sc2_DisplayCriticalErrors, sc2_DisplayCriticalErrors, VF_NULL, g_szDisplayCriticalErrorsDescription)->GetIVal();

		sc2_RunUnitTests = REGISTER_CVAR(sc2_LegacyMode, sc2_LegacyMode, VF_NULL, "Enable/disable legacy new/open; 0 - Diable; 1 - Enable")->GetIVal();
		sc2_RunUnitTests = REGISTER_CVAR(sc2_RunUnitTests, sc2_RunUnitTests, VF_READONLY, "Enable/disable unit tests on startup")->GetIVal();
		sc2_MaxRecursionDepth = REGISTER_CVAR(sc2_MaxRecursionDepth, sc2_MaxRecursionDepth, VF_NULL, "Maximum recursion depth")->GetIVal();
		sc2_ExperimentalFeatures = REGISTER_STRING("sc2_ExperimentalFeatures", "", VF_NULL, "Enable one or more experimental features");

		sc2_FunctionTimeLimit = REGISTER_CVAR(sc2_FunctionTimeLimit, sc2_FunctionTimeLimit, VF_NULL, "Display critical error if Schematyc function takes more than x(s) to process")->GetFVal();
		sc2_RelevanceGridCellSize = REGISTER_CVAR(sc2_RelevanceGridCellSize, sc2_RelevanceGridCellSize, VF_NULL, "This is the grid cell size of the relevance map")->GetFVal();
		sc2_RelevanceGridDebugStatic = REGISTER_CVAR(sc2_RelevanceGridDebugStatic, sc2_RelevanceGridDebugStatic, VF_DEV_ONLY,
			_HELP("Enable debug draw of static entities in the relevance grid.\n"
				"Possible values:\n"
				" 0 - disabled\n"
				" 1 - draw non-empty relevant cells and basic info\n"
				" 2 - draw all relevant cells\n"
				" 3 - print extra information about cells\n"
			))->GetIVal();
		sc2_UseNewGraphPipeline = REGISTER_CVAR(sc2_UseNewGraphPipeline, sc2_UseNewGraphPipeline, VF_READONLY, "Enable/disable new graph pipeline")->GetIVal();
	}

	void CVars::Unregister()
	{
		if (IConsole* pConsole = gEnv->pConsole)
		{
			pConsole->UnregisterVariable("sc2_FileFormat");
			pConsole->UnregisterVariable("sc2_RootFolder");
			pConsole->UnregisterVariable("sc2_IgnorePAKFiles");
			pConsole->UnregisterVariable("sc2_IgnoreUnderscoredFolders");
			pConsole->UnregisterVariable("sc2_DiscardOnSave");

			pConsole->UnregisterVariable("sc2_LogToFile");
			pConsole->UnregisterVariable("sc2_LogFileStreams");
			pConsole->UnregisterVariable("sc2_LogFileMessageTypes");
			pConsole->UnregisterVariable("sc2_DisplayCriticalErrors");

			pConsole->UnregisterVariable("sc2_LegacyMode");
			pConsole->UnregisterVariable("sc2_RunUnitTests");
			pConsole->UnregisterVariable("sc2_MaxRecursionDepth");
			pConsole->UnregisterVariable("sc2_ExperimentalFeatures");

			pConsole->UnregisterVariable("sc2_FunctionTimeLimit");
			pConsole->UnregisterVariable("sc2_RelevanceGridCellSize");
			pConsole->UnregisterVariable("sc2_RelevanceGridDebugStatic");
			pConsole->UnregisterVariable("sc2_UseNewGraphPipeline");
		}
	}
}
