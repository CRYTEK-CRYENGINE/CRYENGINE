// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File: SystemCFG.cpp
//  Description: handles system cfg
//
//	History:
//	-Jan 21,2004: created
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "System.h"
#include <time.h>
#include "XConsole.h"
#include <CrySystem/File/CryFile.h>

#include <CryScriptSystem/IScriptSystem.h>
#include "SystemCFG.h"
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	#include <CryCore/Project/Version.h>
	#include <CrySystem/ILog.h>

#endif

#ifndef EXE_VERSION_INFO_0
	#define EXE_VERSION_INFO_0 1
#endif

#ifndef EXE_VERSION_INFO_1
	#define EXE_VERSION_INFO_1 0
#endif

#ifndef EXE_VERSION_INFO_2
	#define EXE_VERSION_INFO_2 0
#endif

#ifndef EXE_VERSION_INFO_3
	#define EXE_VERSION_INFO_3 1
#endif

#if CRY_PLATFORM_DURANGO
	#include <xdk.h>
#if _XDK_EDITION < 180400
	#error "Outdated XDK, please update to at least XDK edition April 2018"
#endif // #if _XDK_EDITION
#endif // #if CRY_PLATFORM_DURANGO

//////////////////////////////////////////////////////////////////////////
const SFileVersion& CSystem::GetFileVersion()
{
	return m_fileVersion;
}

//////////////////////////////////////////////////////////////////////////
const SFileVersion& CSystem::GetProductVersion()
{
	return m_productVersion;
}

//////////////////////////////////////////////////////////////////////////
const SFileVersion& CSystem::GetBuildVersion()
{
	return m_buildVersion;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SystemVersionChanged(ICVar* pCVar)
{
	if (CSystem* pThis = static_cast<CSystem*>(gEnv->pSystem))
	{
		pThis->SetVersionInfo(pCVar->GetString());
	}
}

void CSystem::SetVersionInfo(const char* const szVersion)
{
#ifndef _RELEASE
	m_fileVersion.Set(szVersion);
	m_productVersion.Set(szVersion);
	m_buildVersion.Set(szVersion);
	CryLog("SetVersionInfo '%s'", szVersion);
	CryLog("FileVersion: %d.%d.%d.%d", m_fileVersion.v[3], m_fileVersion.v[2], m_fileVersion.v[1], m_fileVersion.v[0]);
	CryLog("ProductVersion: %d.%d.%d.%d", m_productVersion.v[3], m_productVersion.v[2], m_productVersion.v[1], m_productVersion.v[0]);
	CryLog("BuildVersion: %d.%d.%d.%d", m_buildVersion.v[3], m_buildVersion.v[2], m_buildVersion.v[1], m_buildVersion.v[0]);
#endif
}

//////////////////////////////////////////////////////////////////////////
void CSystem::QueryVersionInfo()
{
#if !CRY_PLATFORM_WINDOWS
	//do we need some other values here?
	m_fileVersion.v[0] = m_productVersion.v[0] = EXE_VERSION_INFO_3;
	m_fileVersion.v[1] = m_productVersion.v[1] = EXE_VERSION_INFO_2;
	m_fileVersion.v[2] = m_productVersion.v[2] = EXE_VERSION_INFO_1;
	m_fileVersion.v[3] = m_productVersion.v[3] = EXE_VERSION_INFO_0;
	m_buildVersion = m_fileVersion;
#else
	char moduleName[_MAX_PATH];
	DWORD dwHandle;
	UINT len;

	char ver[1024 * 8];

	GetModuleFileName(NULL, moduleName, _MAX_PATH);  //retrieves the PATH for the current module

	#ifdef _LIB
	GetModuleFileName(NULL, moduleName, _MAX_PATH);  //retrieves the PATH for the current module
	#else                                    //_LIB
	cry_strcpy(moduleName, "CrySystem.dll"); // we want to version from the system dll
	#endif                                   //_LIB

	int verSize = GetFileVersionInfoSize(moduleName, &dwHandle);
	if (verSize > 0)
	{
		GetFileVersionInfo(moduleName, dwHandle, 1024 * 8, ver);
		VS_FIXEDFILEINFO* vinfo;
		VerQueryValue(ver, "\\", (void**)&vinfo, &len);

		const uint32 verIndices[4] = { 0, 1, 2, 3 };
		m_fileVersion.v[verIndices[0]] = m_productVersion.v[verIndices[0]] = vinfo->dwFileVersionLS & 0xFFFF;
		m_fileVersion.v[verIndices[1]] = m_productVersion.v[verIndices[1]] = vinfo->dwFileVersionLS >> 16;
		m_fileVersion.v[verIndices[2]] = m_productVersion.v[verIndices[2]] = vinfo->dwFileVersionMS & 0xFFFF;
		m_fileVersion.v[verIndices[3]] = m_productVersion.v[verIndices[3]] = vinfo->dwFileVersionMS >> 16;
		m_buildVersion = m_fileVersion;

		struct LANGANDCODEPAGE
		{
			WORD wLanguage;
			WORD wCodePage;
		}* lpTranslate;

		UINT count = 0;
		char path[256];
		char* version = NULL;

		VerQueryValue(ver, "\\VarFileInfo\\Translation", (LPVOID*)&lpTranslate, &count);
		if (lpTranslate != NULL)
		{
			cry_sprintf(path, "\\StringFileInfo\\%04x%04x\\InternalName", lpTranslate[0].wLanguage, lpTranslate[0].wCodePage);
			VerQueryValue(ver, path, (LPVOID*)&version, &count);
			if (version)
			{
				m_buildVersion.Set(version);
			}
		}
	}
#endif // !CRY_PLATFORM_WINDOWS
}

//////////////////////////////////////////////////////////////////////////
void CSystem::LogVersion()
{
	// Get time.
	time_t ltime;
	time(&ltime);
	tm* today = localtime(&ltime);

	char s[1024];

	strftime(s, 128, "%d %b %y (%H %M %S)", today);

	const SFileVersion& ver = GetFileVersion();

	CryLogAlways("BackupNameAttachment=\" Build(%d) %s\"  -- used by backup system\n", ver.v[0], s);      // read by CreateBackupFile()

	// Use strftime to build a customized time string.
	strftime(s, 128, "Log Started at %c", today);
	CryLogAlways("%s", s);

	CryLogAlways("Built on " __DATE__ " " __TIME__);

#if CRY_PLATFORM_DURANGO
	CryLogAlways("Running 64 bit Durango version XDK VER:%d", _XDK_VER);
#elif CRY_PLATFORM_ORBIS
	CryLogAlways("Running 64 bit Orbis version SDK VER:0x%08X", SCE_ORBIS_SDK_VERSION);
#elif CRY_PLATFORM_ANDROID
	CryLogAlways("Running 32 bit Android version API VER:%d", __ANDROID_API__);
#elif CRY_PLATFORM_IOS
	CryLogAlways("Running 64 bit iOS version");
#elif CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	CryLogAlways("Running 64 bit Windows version");
#elif CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT
	CryLogAlways("Running 32 bit Windows version");
#elif (CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT)
	CryLogAlways("Running 64 bit Linux version");
#elif (CRY_PLATFORM_LINUX && CRY_PLATFORM_32BIT)
	CryLogAlways("Running 32 bit Linux version");
#elif CRY_PLATFORM_MAC
	CryLogAlways("Running 64 bit Mac version");
#endif
	CryLogAlways("Command Line: %s", m_pCmdLine->GetCommandLine());
#if !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_APPLE && !CRY_PLATFORM_DURANGO && !CRY_PLATFORM_ORBIS
	GetModuleFileName(NULL, s, sizeof(s));
	CryLogAlways("Executable: %s", s);
#endif

	CryLogAlways("FileVersion: %d.%d.%d.%d", m_fileVersion.v[3], m_fileVersion.v[2], m_fileVersion.v[1], m_fileVersion.v[0]);
	CryLogAlways("ProductVersion: %d.%d.%d.%d", m_productVersion.v[3], m_productVersion.v[2], m_productVersion.v[1], m_productVersion.v[0]);
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
class CCVarSaveDump : public ICVarDumpSink
{
public:

	CCVarSaveDump(FILE* pFile)
	{
		m_pFile = pFile;
	}

	virtual void OnElementFound(ICVar* pCVar)
	{
		if (!pCVar)
			return;
		int nFlags = pCVar->GetFlags();
		if (((nFlags & VF_DUMPTODISK) && (nFlags & VF_MODIFIED)) || (nFlags & VF_WASINCONFIG))
		{
			string szValue = pCVar->GetString();

			size_t pos = 1;
			for (;; )
			{
				pos = szValue.find_first_of("\\", pos);

				if (pos == string::npos)
				{
					break;
				}

				szValue.replace(pos, 1, "\\\\", 2);
				pos += 2;
			}

			// replace " with \"
			pos = 1;
			for (;; )
			{
				pos = szValue.find_first_of("\"", pos);

				if (pos == string::npos)
				{
					break;
				}

				szValue.replace(pos, 1, "\\\"", 2);
				pos += 2;
			}

			string szLine = pCVar->GetName();

			if (pCVar->GetType() == CVAR_STRING)
				szLine += " = \"" + szValue + "\"\r\n";
			else
				szLine += " = " + szValue + "\r\n";

			if (pCVar->GetFlags() & VF_WARNING_NOTUSED)
				fputs("-- REMARK: the following was not assigned to a console variable\r\n", m_pFile);

			fputs(szLine.c_str(), m_pFile);
		}
	}

private: // --------------------------------------------------------

	FILE* m_pFile;                  //
};

//////////////////////////////////////////////////////////////////////////
void CSystem::SaveConfiguration()
{
}

//////////////////////////////////////////////////////////////////////////
// system cfg
//////////////////////////////////////////////////////////////////////////
CSystemConfiguration::CSystemConfiguration(const string& strSysConfigFilePath, CSystem* pSystem, ILoadConfigurationEntrySink* pSink, ELoadConfigurationType configType)
	: m_strSysConfigFilePath(strSysConfigFilePath), m_bError(false), m_pSink(pSink), m_configType(configType)
{
	assert(pSink);

	m_pSystem = pSystem;
	m_bError = !ParseSystemConfig();
}

//////////////////////////////////////////////////////////////////////////
CSystemConfiguration::~CSystemConfiguration()
{
}

//////////////////////////////////////////////////////////////////////////
bool CSystemConfiguration::OpenFile(const string& filename, CCryFile& file, int flags)
{
	flags |= ICryPak::FOPEN_HINT_QUIET;

	// Absolute paths first
	if (gEnv->pCryPak->IsAbsPath(filename) || filename[0] == '%')
	{
		if (file.Open(filename, "rb", flags | ICryPak::FLAGS_PATH_REAL))
		{
			return true;
		}
		else
		{
			// the file is absolute and was not found, it is not useful to search further
			return false;
		}
	}

	// If the path is relative, search relevant folders in given order:
	// First search in current folder.
	if (file.Open(string("./") + filename, "rb", flags | ICryPak::FLAGS_PATH_REAL))
	{
		return true;
	}

	string gameFolder = PathUtil::RemoveSlash(gEnv->pCryPak->GetGameFolder());
	if (!filename.empty() && filename[0] == '%')
	{
		// When file name start with alias, not check game folder.
		gameFolder = "";
	}

	// Next search inside game folder (ex, game/game.cfg)
	if (!gameFolder.empty() && file.Open(gameFolder + "/" + filename, "rb", flags | ICryPak::FLAGS_PATH_REAL))
	{
		return true;
	}

	// Next Search in registered mod folders.
	if (file.Open(filename, "rb", flags))
	{
		return true;
	}

	// Next Search in game/config subfolder.
	if (!gameFolder.empty() && file.Open(gameFolder + "/config/" + filename, "rb", flags | ICryPak::FLAGS_PATH_REAL))
	{
		return true;
	}

	// Next Search in config subfolder.
	if (file.Open(string("config/") + filename, "rb", flags | ICryPak::FLAGS_PATH_REAL))
	{
		return true;
	}

	// Next Search in engine root.
	if (file.Open(string("%ENGINEROOT%/") + filename, "rb", flags))
	{
		return true;
	}

	// Next Search in engine config subfolder, in case loosely stored on drive
	if (file.Open(string("%ENGINEROOT%/Engine/config/") + filename, "rb", flags))
	{
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CSystemConfiguration::ParseSystemConfig()
{
	string filename = m_strSysConfigFilePath;
	if (strlen(PathUtil::GetExt(filename)) == 0)
	{
		filename = PathUtil::ReplaceExtension(filename, "cfg");
	}

	CCryFile file;

	{
		string filenameLog;

		int flags = ICryPak::FOPEN_HINT_QUIET | ICryPak::FOPEN_ONDISK;

		if (!OpenFile(filename, file, flags))
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Config file %s not found!", filename.c_str());
			return false;
		}
		filenameLog = file.GetAdjustedFilename();

		CryLog("Loading Config file %s (%s)", filename.c_str(), filenameLog.c_str());
	}

	INDENT_LOG_DURING_SCOPE();

	int nLen = file.GetLength();
	if (nLen == 0)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Couldn't get length for Config file %s", filename.c_str());
		return false;
	}
	char* sAllText = new char[nLen + 16];
	if (file.ReadRaw(sAllText, nLen) < (size_t)nLen)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Couldn't read Config file %s", filename.c_str());
		return false;
	}
	sAllText[nLen] = '\0';
	sAllText[nLen + 1] = '\0';

	string strGroup;      // current group e.g. "[General]"

	char* strLast = sAllText + nLen;
	char* str = sAllText;
	while (str < strLast)
	{
		char* s = str;
		while (str < strLast && *str != '\n' && *str != '\r')
			str++;
		*str = '\0';
		str++;
		while (str < strLast && (*str == '\n' || *str == '\r'))
			str++;

		string strLine = s;

		// detect groups e.g. "[General]"   should set strGroup="General"
		{
			string strTrimmedLine(RemoveWhiteSpaces(strLine));
			size_t size = strTrimmedLine.size();

			if (size >= 3)
				if (strTrimmedLine[0] == '[' && strTrimmedLine[size - 1] == ']') // currently no comments are allowed to be behind groups
				{
					strGroup = &strTrimmedLine[1];
					strGroup.resize(size - 2);                              // remove [ and ]
					continue;                                               // next line
				}
		}

		//trim all whitespace characters at the beginning and the end of the current line and store its size
		strLine.Trim();
		size_t strLineSize = strLine.size();

		//skip comments, comments start with ";" or "--" but may have preceding whitespace characters
		if (strLineSize > 0)
		{
			if (strLine[0] == ';')
				continue;
			else if (strLine.find("--") == 0)
				continue;
		}
		//skip empty lines
		else
			continue;

		//if line contains a '=' try to read and assign console variable
		string::size_type posEq(strLine.find("=", 0));
		if (string::npos != posEq)
		{
			string stemp(strLine, 0, posEq);
			string strKey(RemoveWhiteSpaces(stemp));

			//				if (!strKey.empty())
			{
				// extract value
				string::size_type posValueStart(strLine.find("\"", posEq + 1) + 1);
				// string::size_type posValueEnd( strLine.find( "\"", posValueStart ) );
				string::size_type posValueEnd(strLine.rfind('\"'));

				string strValue;

				if (string::npos != posValueStart && string::npos != posValueEnd)
					strValue = string(strLine, posValueStart, posValueEnd - posValueStart);
				else
				{
					string strTmp(strLine, posEq + 1, strLine.size() - (posEq + 1));
					strValue = RemoveWhiteSpaces(strTmp);
				}

				{
					// replace '\\\\' with '\\' and '\\\"' with '\"'
					strValue.replace("\\\\", "\\");
					strValue.replace("\\\"", "\"");

					//						m_pSystem->GetILog()->Log("Setting %s to %s",strKey.c_str(),strValue.c_str());
					m_pSink->OnLoadConfigurationEntry(strKey, strValue, strGroup);
				}
			}
		}
		else
		{
			gEnv->pLog->LogWithType(ILog::eWarning, "%s -> invalid configuration line: %s", filename.c_str(), strLine.c_str());
		}

	}

	delete[]sAllText;

	m_pSink->OnLoadConfigurationEntry_End();

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup)
{
	if (!gEnv->pConsole)
		return;

	if (*szKey != 0)
		gEnv->pConsole->LoadConfigVar(szKey, szValue);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::LoadConfiguration(const char* sFilename, ILoadConfigurationEntrySink* pSink, ELoadConfigurationType configType)
{
	ELoadConfigurationType lastType = m_env.pConsole->SetCurrentConfigType(configType);

	if (sFilename && strlen(sFilename) > 0)
	{
		if (!pSink)
			pSink = this;

		CSystemConfiguration tempConfig(sFilename, this, pSink, configType);
	}
	m_env.pConsole->SetCurrentConfigType(lastType);
}