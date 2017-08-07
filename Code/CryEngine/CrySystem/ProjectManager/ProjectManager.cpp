#include "StdAfx.h"
#include "ProjectManager.h"

#include "System.h"

#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>
#include <CrySerialization/Enum.h>

using namespace Cry::ProjectManagerInternals;

#if CRY_PLATFORM_WINDOWS
#include <Shlwapi.h>
#endif

YASLI_ENUM_BEGIN_NESTED(ICryPluginManager, EPluginType, "PluginType")
YASLI_ENUM_VALUE_NESTED(ICryPluginManager, EPluginType::Native, "Native")
YASLI_ENUM_VALUE_NESTED(ICryPluginManager, EPluginType::Managed, "Managed")
YASLI_ENUM_END()

CProjectManager::CProjectManager()
	: m_sys_project(nullptr)
	, m_sys_game_name(nullptr)
	, m_sys_dll_game(nullptr)
	, m_sys_game_folder(nullptr)
{
	RegisterCVars();

	CryFindRootFolderAndSetAsCurrentWorkingDirectory();
}

const char* CProjectManager::GetCurrentProjectName()
{
	return m_sys_game_name->GetString();
}

const char* CProjectManager::GetCurrentProjectDirectoryAbsolute()
{
	return m_project.rootDirectory;
}

const char* CProjectManager::GetCurrentAssetDirectoryRelative()
{
	return gEnv->pCryPak->GetGameFolder();
}

const char* CProjectManager::GetCurrentAssetDirectoryAbsolute()
{ 
	return m_project.assetDirectoryFullPath;
}

void CProjectManager::StoreConsoleVariable(const char* szCVarName, const char* szValue)
{
	for (auto it = m_project.consoleVariables.begin(); it != m_project.consoleVariables.end(); ++it)
	{
		if (!stricmp(it->key, szCVarName))
		{
			it->value = szValue;

			return;
		}
	}

	m_project.consoleVariables.emplace_back(szCVarName, szValue);
}

void CProjectManager::SaveProjectChanges()
{
	gEnv->pSystem->GetArchiveHost()->SaveJsonFile(m_project.filePath, Serialization::SStruct(m_project));
}

void SProject::Serialize(Serialization::IArchive& ar)
{
	// Only save to the latest format
	if (ar.isOutput())
	{
		version = LatestProjectFileVersion;
	}

	ar(version, "version", "version");
	if (version == 0 || version == 1)
	{
		SProjectFileParser<1> parser;
		parser.Serialize(ar, *this);
	}
}

void CProjectManager::ParseProjectFile()
{
	const ICmdLineArg* arg = gEnv->pSystem->GetICmdLine()->FindArg(eCLAT_Pre, "project");
	string projectFile = arg != nullptr ? arg->GetValue() : m_sys_project->GetString();
	projectFile = PathUtil::ReplaceExtension(projectFile, "cryproject");

	char szEngineRootDirectoryBuffer[_MAX_PATH];
	CryFindEngineRootFolder(CRY_ARRAY_COUNT(szEngineRootDirectoryBuffer), szEngineRootDirectoryBuffer);

#if CRY_PLATFORM_DURANGO
	if(true)
#elif CRY_PLATFORM_WINAPI
	if (PathIsRelative(projectFile.c_str()))
#elif CRY_PLATFORM_POSIX
	if (projectFile[0] != '/')
#endif
	{
		m_project.filePath = PathUtil::Make(szEngineRootDirectoryBuffer, projectFile.c_str());
	}
	else
	{
		m_project.filePath = projectFile.c_str();
	}

	if (gEnv->pSystem->GetArchiveHost()->LoadJsonFile(Serialization::SStruct(m_project), m_project.filePath, true))
	{
		m_project.rootDirectory = PathUtil::RemoveSlash(PathUtil::ToUnixPath(PathUtil::GetPathWithoutFilename(m_project.filePath)));

		// Create the full path to the asset directory
		m_project.assetDirectoryFullPath = PathUtil::Make(m_project.rootDirectory, m_project.assetDirectory);

		// Set the legacy game folder and name
		m_sys_game_folder->Set(m_project.assetDirectory);
		m_sys_game_name->Set(m_project.name);

		for (SProject::SConsoleVariable& consoleVariable : m_project.consoleVariables)
		{
			gEnv->pConsole->LoadConfigVar(consoleVariable.key, consoleVariable.value);
		}

		auto gameDllIt = m_project.legacyGameDllPaths.find("any");

#if (CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT)
		if (gameDllIt == m_project.legacyGameDllPaths.end())
		{
			gameDllIt = m_project.legacyGameDllPaths.find("win_x64");
		}
#elif (CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT)
		if (gameDllIt == m_project.legacyGameDllPaths.end())
		{
			gameDllIt = m_project.legacyGameDllPaths.find("win_x86");
		}
#endif
		string legacyGameDllPath;

		// Set legacy Game DLL
		if (gameDllIt != m_project.legacyGameDllPaths.end())
		{
			legacyGameDllPath = gameDllIt->second;

			m_sys_dll_game->Set(legacyGameDllPath);
		}

		gEnv->pConsole->LoadConfigVar("sys_dll_game", legacyGameDllPath);

#ifdef CRY_PLATFORM_WINDOWS
		SetDllDirectoryW(CryStringUtils::UTF8ToWStr(m_project.rootDirectory));
#endif

#ifndef CRY_PLATFORM_ORBIS
		CrySetCurrentWorkingDirectory(m_project.rootDirectory);
#endif

		// Update the project file if the loaded version was outdated
		if (m_project.version != LatestProjectFileVersion)
		{
			// Add default plug-ins since they were hardcoded before version 1
			if (m_project.version == 0)
			{
				AddDefaultPlugins();
				LoadLegacyPluginCSV();
				LoadLegacyGameCfg();
			}

			m_project.version = LatestProjectFileVersion;

			SaveProjectChanges();
		}
	}


	CryLogAlways("\nProject %s", GetCurrentProjectName());
	CryLogAlways("	Using Project Folder %s", GetCurrentProjectDirectoryAbsolute());
	CryLogAlways("	Using Asset Folder %s", GetCurrentAssetDirectoryAbsolute());
	CryLogAlways("	Using Engine Folder %s", szEngineRootDirectoryBuffer);
}

void CProjectManager::MigrateFromLegacyWorkflowIfNecessary()
{
	// Populate project data and save .cryproject if no project was used
	// This is done by assuming legacy game folder setup.
	if (m_project.version == 0 && strlen(GetCurrentAssetDirectoryRelative()) > 0)
	{
		m_project.version = LatestProjectFileVersion;
		m_project.type = "CRYENGINE Project";
		m_project.name = m_sys_game_name->GetString();

		// TODO: Detect latest using CrySelect?
		m_project.engineVersionId = "engine-5.4";

		char buffer[MAX_PATH];
		CryGetCurrentDirectory(MAX_PATH, buffer);
		
		m_project.rootDirectory = PathUtil::RemoveSlash(PathUtil::ToUnixPath(buffer));
		m_project.assetDirectory = GetCurrentAssetDirectoryRelative();

		// Create the full path to the asset directory
		m_project.assetDirectoryFullPath = PathUtil::Make(m_project.rootDirectory, m_project.assetDirectory);

		// Make sure we include default plug-ins
		AddDefaultPlugins();
		LoadLegacyPluginCSV();
		LoadLegacyGameCfg();

		const char* legacyDllName = m_sys_dll_game->GetString();
		if (strlen(legacyDllName) > 0)
		{
			m_project.legacyGameDllPaths["any"] = PathUtil::RemoveExtension(legacyDllName);
		}

		string sProjectFile = PathUtil::Make(m_project.rootDirectory, m_sys_project->GetString());
		// Make sure we have the .cryproject extension
		sProjectFile = PathUtil::ReplaceExtension(sProjectFile, ".cryproject");
		gEnv->pSystem->GetArchiveHost()->SaveJsonFile(sProjectFile, Serialization::SStruct(m_project));
	}
}

void CProjectManager::RegisterCVars()
{
	m_sys_project = REGISTER_STRING("sys_project", "game.cryproject", VF_NULL, "Specifies which project to load.\nLoads from the engine root if relative path, otherwise full paths are allowed to allow out-of-engine projects\nHas no effect if -project switch is used!");

	// Legacy
	m_sys_game_name = REGISTER_STRING("sys_game_name", "CRYENGINE", VF_DUMPTODISK, "Specifies the name to be displayed in the Launcher window title bar");
	m_sys_dll_game = REGISTER_STRING("sys_dll_game", "", VF_NULL, "Specifies the game DLL to load");
	m_sys_game_folder = REGISTER_STRING("sys_game_folder", "", VF_NULL, "Specifies the game folder to read all data from. Can be fully pathed for external folders or relative path for folders inside the root.");
}

//--- UTF8 parse helper routines

static const char* Parser_NextChar(const char* pStart, const char* pEnd)
{
	CRY_ASSERT(pStart != nullptr && pEnd != nullptr);

	if (pStart < pEnd)
	{
		CRY_ASSERT(0 <= *pStart && *pStart <= SCHAR_MAX);
		pStart++;
	}

	CRY_ASSERT(pStart <= pEnd);
	return pStart;
}

static const char* Parser_StrChr(const char* pStart, const char* pEnd, int c)
{
	CRY_ASSERT(pStart != nullptr && pEnd != nullptr);
	CRY_ASSERT(pStart <= pEnd);
	CRY_ASSERT(0 <= c && c <= SCHAR_MAX);
	const char* it = (const char*)memchr(pStart, c, pEnd - pStart);
	return (it != nullptr) ? it : pEnd;
}

static bool Parser_StrEquals(const char* pStart, const char* pEnd, const char* szKey)
{
	size_t klen = strlen(szKey);
	return (klen == pEnd - pStart) && memcmp(pStart, szKey, klen) == 0;
}

void CProjectManager::LoadLegacyPluginCSV()
{
	FILE* pFile = gEnv->pCryPak->FOpen("cryplugin.csv", "rb", ICryPak::FLAGS_PATH_REAL);
	if (pFile == nullptr)
		return;

	std::vector<char> buffer;
	buffer.resize(gEnv->pCryPak->FGetSize(pFile));

	gEnv->pCryPak->FReadRawAll(buffer.data(), buffer.size(), pFile);
	gEnv->pCryPak->FClose(pFile);

	const char* pTokenStart = buffer.data();
	const char* pBufferEnd = buffer.data() + buffer.size();
	while (pTokenStart != pBufferEnd)
	{
		const char* pNewline = Parser_StrChr(pTokenStart, pBufferEnd, '\n');
		const char* pSemicolon = Parser_StrChr(pTokenStart, pNewline, ';');

		ICryPluginManager::EPluginType pluginType = ICryPluginManager::EPluginType::Native;
		if (Parser_StrEquals(pTokenStart, pSemicolon, "C#"))
			pluginType = ICryPluginManager::EPluginType::Managed;

		// Parsing of plugin name
		pTokenStart = Parser_NextChar(pSemicolon, pNewline);
		pSemicolon = Parser_StrChr(pTokenStart, pNewline, ';');

		pTokenStart = Parser_NextChar(pSemicolon, pNewline);
		pSemicolon = Parser_StrChr(pTokenStart, pNewline, ';');

		string pluginClassName;
		pluginClassName.assign(pTokenStart, pSemicolon - pTokenStart);
		pluginClassName.Trim();

		pTokenStart = Parser_NextChar(pSemicolon, pNewline);
		pSemicolon = Parser_StrChr(pTokenStart, pNewline, ';');

		string pluginBinaryPath;
		pluginBinaryPath.assign(pTokenStart, pSemicolon - pTokenStart);
		pluginBinaryPath.Trim();

		pTokenStart = Parser_NextChar(pSemicolon, pNewline);
		pSemicolon = Parser_StrChr(pTokenStart, pNewline, ';');

		string pluginAssetDirectory;
		pluginAssetDirectory.assign(pTokenStart, pSemicolon - pTokenStart);
		pluginAssetDirectory.Trim();

		pTokenStart = Parser_NextChar(pNewline, pBufferEnd);

		AddPlugin(pluginType, pluginBinaryPath);
	}
}

void CProjectManager::LoadLegacyGameCfg()
{
	string cfgPath = PathUtil::Make(m_project.assetDirectoryFullPath, "game.cfg");
	gEnv->pSystem->LoadConfiguration(cfgPath, this);
}

void CProjectManager::OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup)
{
	// Make sure we set the value in the engine as well
	gEnv->pConsole->LoadConfigVar(szKey, szValue);
	
	// Store in our cryproject json, unless the CVar is one that we have in other properties
	if (strcmp(szKey, m_sys_dll_game->GetName()) != 0 && strcmp(szKey, m_sys_game_name->GetName()) != 0)
	{
		StoreConsoleVariable(szKey, szValue);
	}
}

void CProjectManager::AddDefaultPlugins()
{
	for (const char* szDefaultPlugin : CCryPluginManager::GetDefaultPlugins())
	{
		AddPlugin(ICryPluginManager::EPluginType::Native, szDefaultPlugin);
	}
}

void CProjectManager::AddPlugin(ICryPluginManager::EPluginType type, const char* szFileName)
{
	// Make sure duplicates aren't added
	for(const SPluginDefinition& pluginDefinition : m_project.plugins)
	{
		if (!stricmp(pluginDefinition.path, szFileName))
		{
			return;
		}
	}

	m_project.plugins.emplace_back(type, szFileName);
}