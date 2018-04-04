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

bool SProject::Serialize(Serialization::IArchive& ar)
{
	// Only save to the latest format
	if (ar.isOutput())
	{
		version = LatestProjectFileVersion;
	}

	ar(version, "version", "version");

	SProjectFileParser<1> parser;
	parser.Serialize(ar, *this);
	return true;
}

bool CProjectManager::ParseProjectFile()
{
	const ICmdLineArg* arg = gEnv->pSystem->GetICmdLine()->FindArg(eCLAT_Pre, "project");
	string projectFile = arg != nullptr ? arg->GetValue() : m_sys_project->GetString();

	string extension = PathUtil::GetExt(projectFile);
	if (extension.length() == 0)
	{
		projectFile = PathUtil::ReplaceExtension(projectFile, "cryproject");
	}

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

	CCryFile file;
	file.Open(m_project.filePath.c_str(), "rb", ICryPak::FOPEN_ONDISK);
	std::vector<char> projectFileJson;
	if (file.GetHandle() != nullptr)
	{
		projectFileJson.resize(file.GetLength());
	}

	if (projectFileJson.size() > 0 &&
		file.ReadRaw(projectFileJson.data(), projectFileJson.size()) == projectFileJson.size() &&
		gEnv->pSystem->GetArchiveHost()->LoadJsonBuffer(Serialization::SStruct(m_project), projectFileJson.data(), projectFileJson.size()))
	{
		if (m_project.version > LatestProjectFileVersion)
		{
			EQuestionResult result = CryMessageBox("Attempting to start the engine with a potentially unsupported .cryproject made with a newer version of the engine!\nDo you want to continue?", "Loading unknown .cryproject version", eMB_YesCancel);
			if (result == eQR_Cancel)
			{
				CryLogAlways("Unknown .cryproject version %i detected, user opted to quit", m_project.version);
				return false;
			}

			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Loading a potentially unsupported .cryproject made with a newer version of the engine!");
		}

		m_project.rootDirectory = PathUtil::RemoveSlash(PathUtil::ToUnixPath(PathUtil::GetPathWithoutFilename(m_project.filePath)));

		// Create the full path to the asset directory
		m_project.assetDirectoryFullPath = PathUtil::Make(m_project.rootDirectory, m_project.assetDirectory);

		if (!gEnv->pCryPak->IsFileExist(m_project.assetDirectoryFullPath))
		{
			EQuestionResult result = CryMessageBox(string().Format("Attempting to start the engine with non-existent asset directory %s!\nDo you want to create it?", m_project.assetDirectoryFullPath.c_str()), "Non-existent asset directory", eMB_YesCancel);
			if (result == eQR_Cancel)
			{
				CryLogAlways("\tNon-existent asset directory %s detected, user opted to quit", m_project.assetDirectoryFullPath.c_str());
				return false;
			}
			
			CryCreateDirectory(m_project.assetDirectoryFullPath);
		}

		// Set the legacy game folder and name
		m_sys_game_folder->Set(m_project.assetDirectory);
		m_sys_game_name->Set(m_project.name);

		for (SProject::SConsoleInstruction& consoleVariable : m_project.consoleVariables)
		{
			gEnv->pConsole->LoadConfigVar(consoleVariable.key, consoleVariable.value);
		}

		for (SProject::SConsoleInstruction& consoleCommand : m_project.consoleCommands)
		{
			stack_string command = consoleCommand.key;
			command.append(" ");
			command.append(consoleCommand.value.c_str());
			
			gEnv->pConsole->ExecuteString(command.c_str(), false, true);
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
	else if(m_sys_game_folder->GetString()[0] == '\0')
	{
		// No project folder found, and no legacy context to migrate from.
		m_project.filePath.clear();
	}

	// Check if we are migrating from legacy workflow
	if (CanMigrateFromLegacyWorkflow())
	{
		CryLogAlways("\nMigrating from legacy project workflow to new %s file", m_project.filePath.c_str());

		// Migration will occur once MigrateFromLegacyWorkflowIfNecessary is called
		return true;
	} 
	// Detect running engine without project directory
	else if (m_project.filePath.empty())
	{
		if (gEnv->bTesting)
		{
			CryLogAlways("\nRunning engine in unit testing mode without project");

			// Create a temporary asset directory, as some systems rely on an assets directory existing.
			m_project.assetDirectory = "NoAssetFolder";
			m_project.assetDirectoryFullPath = PathUtil::Make(szEngineRootDirectoryBuffer, m_project.assetDirectory);
			if (!gEnv->pCryPak->IsFileExist(m_project.assetDirectoryFullPath))
			{
				CryCreateDirectory(m_project.assetDirectoryFullPath);
			}

			m_sys_game_folder->Set(m_project.assetDirectory);

			return true;
		}
		else
		{
			CryMessageBox("Attempting to start the engine without a project!\nPlease use a .cryproject file!", "Engine initialization failed", eMB_Error);
			return false;
		}
	}
	// Detect running without asset directory
	else if (m_project.assetDirectory.empty())
	{
		if (!gEnv->bTesting)
		{
			EQuestionResult result = CryMessageBox("Attempting to start the engine without an asset directory!\nContinuing will put the engine into a readonly state where changes can't be saved, do you want to continue?", "No Assets directory", eMB_YesCancel);
			if (result == eQR_Cancel)
			{
				CryLogAlways("\tNo asset directory detected, user opted to quit");
				return false;
			}
		}

		// Engine started without asset directory, we have to create a temporary directory in this case
		// This is done as many systems rely on checking for files in the asset directory, without one they will search the root or even the entire drive.
		m_project.assetDirectory = "NoAssetFolder";
		CryLogAlways("\tSkipped use of assets directory");
	}

	CryLogAlways("\nProject %s", GetCurrentProjectName());
	CryLogAlways("\tUsing Project Folder %s", GetCurrentProjectDirectoryAbsolute());
	CryLogAlways("\tUsing Engine Folder %s", szEngineRootDirectoryBuffer);
	CryLogAlways("\tUsing Asset Folder %s", GetCurrentAssetDirectoryAbsolute());

	return true;
}

void CProjectManager::MigrateFromLegacyWorkflowIfNecessary()
{
	// Populate project data and save .cryproject if no project was used
	// This is done by assuming legacy game folder setup.
	if (CanMigrateFromLegacyWorkflow())
	{
		m_project.version = LatestProjectFileVersion;
		m_project.type = "CRYENGINE Project";
		m_project.name = m_sys_game_name->GetString();
		// Specify that cryproject file is in engine root
		m_project.engineVersionId = ".";

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