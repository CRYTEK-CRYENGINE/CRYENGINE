// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "resource.h"

// Insert your headers here
#include <CryCore/Platform/CryWindows.h>
#include <ShellAPI.h> // requires <windows.h>

// We need shell api for Current Root Extraction.
#include "shlwapi.h"
#pragma comment(lib, "shlwapi.lib")

#include <CryCore/Platform/CryLibrary.h>

#include <CrySystem/IConsole.h>
#include <CrySystem/File/ICryPak.h>

#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/Profilers/FrameProfiler/FrameProfiler_impl.h>
#include <CryString/StringUtils.h>

// Advise notebook graphics drivers to prefer discrete GPU when no explicit application profile exists
extern "C"
{
	// nVidia
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	// AMD
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	SSystemInitParams startupParams;
	startupParams.sLogFileName = "Game.log";

	// Note: lpCmdLine does not contain the filename.
	string cmdLine = CryStringUtils::ANSIToUTF8(GetCommandLineA());
	cry_strcpy(startupParams.szSystemCmdLine, cmdLine.c_str());

	return CryInitializeEngine(startupParams) ? EXIT_SUCCESS : EXIT_FAILURE;
}
