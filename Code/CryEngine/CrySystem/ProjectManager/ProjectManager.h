#pragma once

#include <CrySystem/ISystem.h>
#include <CrySystem/IProjectManager.h>
#include <CryExtension/ICryPluginManager.h>
#include <CrySerialization/yasli/STL.h>

struct SPluginDefinition
{
	SPluginDefinition() {}
	SPluginDefinition(ICryPluginManager::EPluginType pluginType, const char* szPath)
		: type(pluginType)
		, path(szPath) {}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(type, "type", "type");
		ar(path, "path", "path");
	}

	bool operator==(const SPluginDefinition& rhs) const
	{
		return type == rhs.type && path == rhs.path;
	}
	bool operator!=(const SPluginDefinition& rhs) const
	{
		return !(*this == rhs);
	}

	ICryPluginManager::EPluginType type;
	string path;
};

namespace Cry
{
	namespace ProjectManagerInternals
	{
		struct SProject
		{
			// Serialize the project file
			bool Serialize(Serialization::IArchive& ar);

			int version = 0;
			// why do we need this?
			string type;
			// Project name
			string name;
			// Path to the .cryproject file
			string filePath;
			string engineVersionId;

			// Directory containing the .cryproject file
			string rootDirectory;
			// Directory game will search for assets in (relative to project directory)
			string assetDirectory;
			// Full path to the asset directory
			string assetDirectoryFullPath;

			// Directory containing native and managed code (relative to project directory)
			string codeDirectory;

			// List of plug-ins to load
			std::vector<SPluginDefinition> plugins;

			std::unordered_map<string, string, stl::hash_strcmp<string>> legacyGameDllPaths;

			struct SConsoleInstruction
			{
				SConsoleInstruction() {}
				SConsoleInstruction(const char* szKey, const char* szValue)
					: key(szKey)
					, value(szValue) {}

				void Serialize(Serialization::IArchive& ar)
				{
					ar(key, "name", "name");
					ar(value, "value", "value");
				}

				string key;
				string value;
			};

			// Specialized CVar values for the project
			std::vector<SConsoleInstruction> consoleVariables;
			// Specialized console commands for the project
			std::vector<SConsoleInstruction> consoleCommands;
		};

		template<int version> struct SProjectFileParser {};

		constexpr int LatestProjectFileVersion = 1;

		template<>
		struct SProjectFileParser<LatestProjectFileVersion>
		{
			void Serialize(Serialization::IArchive& ar, SProject& project)
			{
				struct SRequire
				{
					SRequire(SProject& _project) : project(_project) {}

					void Serialize(Serialization::IArchive& ar)
					{
						ar(project.engineVersionId, "engine", "engine");
						ar(project.plugins, "plugins", "plugins");
					}

					SProject& project;
				};

				struct SInfo
				{
					SInfo(SProject& _project) : project(_project) {}

					void Serialize(Serialization::IArchive& ar)
					{
						ar(project.name, "name", "name");
					}

					SProject& project;
				};

				struct SContent
				{
					struct SLibrary
					{
						struct SShared
						{
							void Serialize(Serialization::IArchive& ar)
							{
								ar(libPathAny, "any", "any");
								ar(libPathWin64, "win_x64", "win_x64");
								ar(libPathWin32, "win_x86", "win_x86");
							}

							string libPathAny;
							string libPathWin64;
							string libPathWin32;
						};

						void Serialize(Serialization::IArchive& ar)
						{
							ar(name, "name", "name");
							ar(shared, "shared", "shared");
						}

						string name;
						SShared shared;
					};

					SContent(SProject& _project) : project(_project) {}

					void Serialize(Serialization::IArchive& ar)
					{
						if (ar.isOutput())
						{
							assetDirectories = { project.assetDirectory };
							codeDirectories = { project.codeDirectory };

							libraries.resize(1);
							libraries[0].name = project.name;
							libraries[0].shared.libPathAny = project.legacyGameDllPaths["any"];
							libraries[0].shared.libPathWin64 = project.legacyGameDllPaths["win_x64"];
							libraries[0].shared.libPathWin32 = project.legacyGameDllPaths["win_x86"];
						}

						ar(assetDirectories, "assets", "assets");
						ar(codeDirectories, "code", "code");
						ar(libraries, "libs", "libs");

						if (ar.isInput())
						{
							project.assetDirectory = assetDirectories.size() > 0 ? assetDirectories.front() : "";
							project.codeDirectory = codeDirectories.size() > 0 ? codeDirectories.front() : "";

							if (libraries.size() > 0)
							{
								project.legacyGameDllPaths["any"] = libraries[0].shared.libPathAny;
								project.legacyGameDllPaths["win_x64"] = libraries[0].shared.libPathWin64;
								project.legacyGameDllPaths["win_x86"] = libraries[0].shared.libPathWin32;
							}
						}
					}

					std::vector<string> assetDirectories;
					std::vector<string> codeDirectories;
					std::vector<SLibrary> libraries;

					SProject& project;
				};

				ar(project.type, "type", "type");
				ar(SInfo(project), "info", "info");
				ar(SContent(project), "content", "content");
				ar(SRequire(project), "require", "require");
				ar(project.consoleVariables, "console_variables", "console_variables");
				ar(project.consoleCommands, "console_commands", "console_commands");
			}
		};
	}
}

class CProjectManager final
	: public IProjectManager
	, public ILoadConfigurationEntrySink
{
public:
	CProjectManager();
	virtual ~CProjectManager() {}

	bool ParseProjectFile();
	void MigrateFromLegacyWorkflowIfNecessary();

	// IProjectManager
	virtual const char* GetCurrentProjectName() override;

	virtual const char* GetCurrentProjectDirectoryAbsolute() override;

	virtual const char* GetCurrentAssetDirectoryRelative() override;
	virtual const char* GetCurrentAssetDirectoryAbsolute() override;

	virtual void        StoreConsoleVariable(const char* szCVarName, const char* szValue) override;
	virtual void        SaveProjectChanges() override;
	// ~IProjectManager

	// ILoadConfigurationEntrySink
	virtual void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup) override;
	// ~ILoadConfigurationEntrySink

	const std::vector<SPluginDefinition>& GetPluginDefinitions() const { return m_project.plugins; }

protected:
	void RegisterCVars();

	void LoadLegacyPluginCSV();
	void LoadLegacyGameCfg();
	void AddDefaultPlugins();

	void AddPlugin(ICryPluginManager::EPluginType type, const char* szFileName);

	bool CanMigrateFromLegacyWorkflow() const { return m_project.version == 0 && m_sys_game_folder->GetString()[0] != '\0'; }

protected:
	Cry::ProjectManagerInternals::SProject m_project;

	ICVar* m_sys_project;

	// Legacy CVars
	ICVar* m_sys_game_name;
	ICVar* m_sys_dll_game;
	ICVar* m_sys_game_folder;
};