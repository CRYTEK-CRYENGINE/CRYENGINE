// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "CVars.h"

namespace Schematyc2
{
	ICVar* CVars::sc_FileFormat               = nullptr;
	ICVar* CVars::sc_RootFolder               = nullptr;
	int    CVars::sc_IgnorePAKFiles           = 0;
	int    CVars::sc_IgnoreUnderscoredFolders = 1;
	int    CVars::sc_DiscardOnSave            = 0;

	ICVar* CVars::sc_LogToFile                = nullptr;
	ICVar* CVars::sc_LogFileStreams           = nullptr;
	ICVar* CVars::sc_LogFileMessageTypes      = nullptr;
	int    CVars::sc_DisplayCriticalErrors    = 0;

	int    CVars::sc_RunUnitTests             = 0;
	int    CVars::sc_MaxRecursionDepth        = 100;
	ICVar* CVars::sc_ExperimentalFeatures     = nullptr;

	float  CVars::sc_FunctionTimeLimit        = 0.0f;
	float  CVars::sc_RelevanceGridCellSize    = 16.0f;
	int    CVars::sc_RelevanceGridDebugStatic = 0;
	int    CVars::sc_UseNewGraphPipeline      = 0;

	static const char* g_szDisplayCriticalErrorsDescription = "Display/hide Schematyc critical error message alerts\n"
																													  "\t0 = hide\n"
																													  "\t1 = display\n";

	void CVars::Register()
	{
		sc_FileFormat = REGISTER_STRING("sc_FileFormat", "xml", VF_READONLY, "Format of Schematyc files");
		sc_RootFolder = REGISTER_STRING("sc_RootFolder", "libs/schematyc", VF_READONLY, "Root folder for Schematyc files");
		sc_IgnorePAKFiles = REGISTER_CVAR(sc_IgnorePAKFiles, sc_IgnorePAKFiles, VF_READONLY, "Ignore PAK files")->GetIVal();
		sc_IgnoreUnderscoredFolders = REGISTER_CVAR(sc_IgnoreUnderscoredFolders, sc_IgnoreUnderscoredFolders, VF_READONLY, "Ignore folders whose names begin with an underscore")->GetIVal();
		sc_DiscardOnSave = REGISTER_CVAR(sc_DiscardOnSave, sc_DiscardOnSave, VF_READONLY, "Discard broken script elements when saving files")->GetIVal();
		
		sc_LogToFile = REGISTER_INT("sc_LogToFile", 0, VF_NULL, "Configure Schematyc log file output\n\t0 = disabled\n\t1 = write to file\n\tforward to standard log");
		sc_LogFileStreams = REGISTER_STRING("sc_LogFileStreams", "Default System Compiler Game", VF_NULL, "Set which streams should be sent to log file");
		sc_LogFileMessageTypes = REGISTER_STRING("sc_LogFileMessageTypes", "Warning Error CriticalError", VF_NULL, "Set which message types should be sent to log file");
		sc_DisplayCriticalErrors = REGISTER_CVAR(sc_DisplayCriticalErrors, sc_DisplayCriticalErrors, VF_NULL, g_szDisplayCriticalErrorsDescription)->GetIVal();

		sc_RunUnitTests = REGISTER_CVAR(sc_RunUnitTests, sc_RunUnitTests, VF_READONLY, "Enable/disable unit tests on startup")->GetIVal();
		sc_MaxRecursionDepth = REGISTER_CVAR(sc_MaxRecursionDepth, sc_MaxRecursionDepth, VF_NULL, "Maximum recursion depth")->GetIVal();
		sc_ExperimentalFeatures = REGISTER_STRING("sc_ExperimentalFeatures", "", VF_NULL, "Enable one or more experimental features");

		sc_FunctionTimeLimit = REGISTER_CVAR(sc_FunctionTimeLimit, sc_FunctionTimeLimit, VF_NULL, "Display critical error if Schematyc function takes more than x(s) to process")->GetFVal();
		sc_RelevanceGridCellSize = REGISTER_CVAR(sc_RelevanceGridCellSize, sc_RelevanceGridCellSize, VF_NULL, "This is the grid cell size of the relevance map")->GetFVal();
		sc_RelevanceGridDebugStatic = REGISTER_CVAR(sc_RelevanceGridDebugStatic, sc_RelevanceGridDebugStatic, VF_DEV_ONLY,
			_HELP("Enable debug draw of static entities in the relevance grid.\n"
				"Possible values:\n"
				" 0 - disabled\n"
				" 1 - draw non-empty relevant cells and basic info\n"
				" 2 - draw all relevant cells\n"
				" 3 - print extra information about cells\n"
			))->GetIVal();
		sc_UseNewGraphPipeline = REGISTER_CVAR(sc_UseNewGraphPipeline, sc_UseNewGraphPipeline, VF_READONLY, "Enable/disable new graph pipeline")->GetIVal();
	}

	void CVars::Unregister()
	{
		if (IConsole* pConsole = gEnv->pConsole)
		{
			pConsole->UnregisterVariable("sc_FileFormat");
			pConsole->UnregisterVariable("sc_RootFolder");
			pConsole->UnregisterVariable("sc_IgnorePAKFiles");
			pConsole->UnregisterVariable("sc_IgnoreUnderscoredFolders");
			pConsole->UnregisterVariable("sc_DiscardOnSave");

			pConsole->UnregisterVariable("sc_LogToFile");
			pConsole->UnregisterVariable("sc_LogFileStreams");
			pConsole->UnregisterVariable("sc_LogFileMessageTypes");
			pConsole->UnregisterVariable("sc_DisplayCriticalErrors");

			pConsole->UnregisterVariable("sc_RunUnitTests");
			pConsole->UnregisterVariable("sc_MaxRecursionDepth");
			pConsole->UnregisterVariable("sc_ExperimentalFeatures");

			pConsole->UnregisterVariable("sc_FunctionTimeLimit");
			pConsole->UnregisterVariable("sc_RelevanceGridCellSize");
			pConsole->UnregisterVariable("sc_RelevanceGridDebugStatic");
			pConsole->UnregisterVariable("sc_UseNewGraphPipeline");
		}
	}
}
