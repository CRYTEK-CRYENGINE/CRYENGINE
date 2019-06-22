// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryExtension/CryGUID.h>
#include "ICryPluginManager.h"

#include <CrySerialization/yasli/STL.h>

namespace Cry
{

YASLI_ENUM_BEGIN(EPlatform, "Platform")
YASLI_ENUM(EPlatform::Windows, "Windows", "Windows")
YASLI_ENUM(EPlatform::Linux, "Linux", "Linux")
YASLI_ENUM(EPlatform::MacOS, "MacOS", "MacOS")
YASLI_ENUM(EPlatform::XboxOne, "XboxOne", "XboxOne")
YASLI_ENUM(EPlatform::PS4, "PS4", "PS4")
YASLI_ENUM(EPlatform::Android, "Android", "Android")
YASLI_ENUM(EPlatform::iOS, "iOS", "iOS")
YASLI_ENUM_END()

struct SPluginDefinition
{
	SPluginDefinition() = default;

	void Serialize(Serialization::IArchive& ar)
	{
		ar(guid, "guid", "guid");
		const bool success = ar(type, "type", "type");
		if (!success && ar.isInput())
		{
			Cry::IPluginManager::EPluginType pluginType = IPluginManager::EPluginType::Native;
			ar(pluginType, "type", "type");

			const int32 typeValue = static_cast<int32>(pluginType);
			type = static_cast<Cry::IPluginManager::EType>(typeValue);
		}
		ar(path, "path", "path");

		if (ar.isInput() || !platforms.empty())
		{
			ar(platforms, "platforms", "platforms");
		}
	}

	bool operator==(const SPluginDefinition& rhs) const
	{
		return type == rhs.type && path == rhs.path;
	}
	bool operator!=(const SPluginDefinition& rhs) const
	{
		return !(*this == rhs);
	}

	Cry::IPluginManager::EType type;
	string                     path;
	//! Determines the platforms for which this plug-in should be loaded
	//! An empty vector indicates that we should always load
	std::vector<EPlatform> platforms;
	CryGUID                guid;
};

//! Latest version of the project syntax
//! Bump this when syntax changes, or default plug-ins are added
//! This allows us to automatically migrate and support older versions
//! Version 0 = pre-project system, allows for migrating from legacy (game.cfg etc) to .cryproject
constexpr unsigned int LatestProjectFileVersion = 4;

struct SProject
{
	// Serialize the project file
	bool Serialize(Serialization::IArchive& ar);

	unsigned int version = 0;
	// why do we need this?
	string       type;
	// Project name
	string       name;
	CryGUID      guid;
	// Path to the .cryproject file
	string       filePath;
	string       engineVersionId;

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

	// Specialized CVar values for the project
	std::map<string, string> consoleVariables;
	// Specialized console commands for the project
	std::map<string, string> consoleCommands;
};

template<unsigned int version> struct SProjectFileParser {};

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
				ar(project.guid, "guid", "guid");
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

				string  name;
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
					project.assetDirectory = !assetDirectories.empty() ? assetDirectories.front() : string("");
					project.codeDirectory = !codeDirectories.empty() ? codeDirectories.front() : string("");

					if (!libraries.empty())
					{
						project.legacyGameDllPaths["any"] = libraries[0].shared.libPathAny;
						project.legacyGameDllPaths["win_x64"] = libraries[0].shared.libPathWin64;
						project.legacyGameDllPaths["win_x86"] = libraries[0].shared.libPathWin32;
					}
				}
			}

			std::vector<string>   assetDirectories;
			std::vector<string>   codeDirectories;
			std::vector<SLibrary> libraries;

			SProject&             project;
		};

		ar(project.type, "type", "type");
		ar(SInfo(project), "info", "info");
		ar(SContent(project), "content", "content");
		ar(SRequire(project), "require", "require");

		ar(project.consoleVariables, "console_variables", "console_variables");
		ar(project.consoleCommands, "console_commands", "console_commands");
	}
};

inline bool SProject::Serialize(Serialization::IArchive& ar)
{
	// Only save to the latest format
	if (ar.isOutput())
	{
		version = LatestProjectFileVersion;
	}

	ar(version, "version", "version");

	SProjectFileParser<LatestProjectFileVersion> parser;
	parser.Serialize(ar, *this);
	return true;
}

//! Main interface used to manage the currently run project (known by the .cryproject extension).
struct IProjectManager
{
	virtual ~IProjectManager() {}

	//! Gets the human readable name of the game, for example used for updating the window title on desktop
	virtual const char* GetCurrentProjectName() const = 0;
	//! Gets the globally unique identifier for this project, used to uniquely identify certain assets with projects
	virtual CryGUID     GetCurrentProjectGUID() const = 0;
	//! Gets the identifier for the engine used by this project
	virtual const char* GetCurrentEngineID() const = 0;

	//! Gets the absolute path to the root of the project directory, where the .cryproject resides.
	//! \return Path without trailing separator.
	virtual const char* GetCurrentProjectDirectoryAbsolute() const = 0;

	//! Gets the path to the assets directory, relative to project root
	virtual const char* GetCurrentAssetDirectoryRelative() const = 0;
	//! Gets the absolute path to the asset directory
	virtual const char* GetCurrentAssetDirectoryAbsolute() const = 0;

	virtual const char* GetProjectFilePath() const = 0;

	//! Adds or updates the value of a CVar in the project configuration
	virtual void StoreConsoleVariable(const char* szCVarName, const char* szValue) = 0;

	//! Saves the .cryproject file with new values from StoreConsoleVariable
	virtual void SaveProjectChanges() = 0;

	//! Gets the number of plug-ins for the current project
	virtual const uint16 GetPluginCount() const = 0;
	//! Gets details on a specific plug-in by index
	//! \see GetPluginCount
	virtual void   GetPluginInfo(uint16 index, Cry::IPluginManager::EType& typeOut, string& pathOut, DynArray<EPlatform>& platformsOut) const = 0;
	//! Loads a project template, allowing substitution of aliases prefixed by '$' in the provided lambda
	virtual string LoadTemplateFile(const char* szPath, std::function<string(const char* szAlias)> aliasReplacementFunc) const = 0;
};

}
