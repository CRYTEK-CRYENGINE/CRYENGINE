// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "PlatformHandler_GamePC.h"

#if defined(LIVECREATE_FOR_PC) && !defined(NO_LIVECREATE)

namespace LiveCreate
{

//-----------------------------------------------------------------------------

PlatformHandler_GamePC::PlatformHandler_GamePC(IPlatformHandlerFactory* pFactory, const char* szTargetName)
	: PlatformHandlerBase(pFactory, szTargetName)
{
	// get path to the executable file
	char szExeName[MAX_PATH];
	GetModuleFileNameA(CryGetCurrentModule(), szExeName, MAX_PATH);
	char* szName = strrchr(szExeName, '\\');
	if (szName) szName[1] = 0;

	// get path to the executable directory (Bin32 or Bin64)
	char szFullPath[MAX_PATH];
	GetFullPathNameA(szExeName, MAX_PATH, szFullPath, NULL);
	m_exeDirectory = szFullPath;

	// get the base directory (one level up the exe directory)
	GetFullPathNameA(m_exeDirectory + "..\\", MAX_PATH, szFullPath, NULL);
	m_baseDirectory = szFullPath;

	// format the root directory (drive name only)
	m_rootDirectory = m_baseDirectory.Left(3);

	// we share data between editor and PC - do not copy to much
	m_flags |= eFlag_SharedDataDirectory;
}

PlatformHandler_GamePC::~PlatformHandler_GamePC()
{
}

void PlatformHandler_GamePC::Delete()
{
	delete this;
}

bool PlatformHandler_GamePC::Launch(const char* pExeFilename, const char* pWorkingFolder, const char* pArgs) const
{
	const string fullExePath = pExeFilename;
	const string workingDir = pWorkingFolder;

	STARTUPINFO startupInfo;
	ZeroMemory(&startupInfo, sizeof(startupInfo));
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	startupInfo.wShowWindow = SW_SHOW;

	PROCESS_INFORMATION processInfo;
	ZeroMemory(&processInfo, sizeof(processInfo));

	if (!CreateProcessA(
	      fullExePath.c_str(),
	      (LPSTR)pArgs,
	      NULL,
	      NULL,
	      FALSE,
	      CREATE_DEFAULT_ERROR_MODE,
	      NULL,
	      workingDir.c_str(),
	      &startupInfo,
	      &processInfo))
	{
		return false;
	}

	return true;
}

bool PlatformHandler_GamePC::Reset(EResetMode aMode) const
{
	return false;
}

bool PlatformHandler_GamePC::IsOn() const
{
	return true;
}

bool PlatformHandler_GamePC::ScanFolder(const char* pFolder, IPlatformHandlerFolderScan& outResult) const
{
	string searchPath = pFolder;

	if (searchPath.empty())
	{
		return false;
	}

	char lastChar = searchPath[searchPath.length() - 1];
	if (lastChar != '\\' && lastChar != '/')
	{
		searchPath += "\\";
	}

	searchPath += "*.*";

	WIN32_FIND_DATA findData;
	HANDLE hFind = FindFirstFile(searchPath.c_str(), &findData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			// skip the . and .. folders
			if (findData.cFileName[0] == '.')
			{
				continue;
			}

			const bool bIsDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
			if (bIsDirectory)
			{
				outResult.OnFolder(pFolder, findData.cFileName);
			}
			else
			{
				const bool bIsExecutable = (NULL != strstr(findData.cFileName, ".exe")); \
				  outResult.OnFile(pFolder, findData.cFileName, bIsExecutable);
			}
		}
		while (FindNextFile(hFind, &findData));
	}

	return false;
}

const char* PlatformHandler_GamePC::GetRootPath() const
{
	return m_rootDirectory.c_str();
}

//-----------------------------------------------------------------------------

const char* PlatformHandlerFactory_GamePC::GetPlatformName() const
{
	return "GamePC";
}

IPlatformHandler* PlatformHandlerFactory_GamePC::CreatePlatformHandlerInstance(const char* pTargetMachineName)
{
	return new PlatformHandler_GamePC(this, pTargetMachineName);
}

bool PlatformHandlerFactory_GamePC::ResolveAddress(const char* pTargetMachineName, char* pOutIpAddress, uint32 aMaxOutIpAddressSize)
{
	// auto resolve the local host address :)
	if (0 == stricmp(pTargetMachineName, "localhost"))
	{
		cry_strcpy(pOutIpAddress, aMaxOutIpAddressSize, "127.0.0.1");
		return true;
	}

	// only local games for now
	return false;
}

uint32 PlatformHandlerFactory_GamePC::ScanForTargets(TargetInfo* outTargets, const uint maxTargets)
{
	if (maxTargets >= 1)
	{
		cry_strcpy(outTargets[0].targetName, "localhost");
		return 1;
	}

	return 0;
}

//-----------------------------------------------------------------------------

}
#endif
