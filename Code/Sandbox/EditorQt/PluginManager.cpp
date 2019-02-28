// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IPlugin.h"
#include "PluginManager.h"
#include "Util/FileUtil.h"

typedef IPlugin* (* TPfnCreatePluginInstance)(PLUGIN_INIT_PARAM* pInitParam);
typedef void (*TPfnDeletePluginInstance)(IPlugin* pPlugin);

CPluginManager::CPluginManager()
{
}

CPluginManager::~CPluginManager()
{
	CleanUp();
}

void CPluginManager::UnloadAllPlugins()
{
	CleanUp();
}

namespace
{
int OnFactoryFilter(unsigned int code, struct _EXCEPTION_POINTERS* pExceptionPtrs)
{
	if (IsDebuggerPresent())
	{
		DebugBreak();
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

IPlugin* SafeCallFactory(
  TPfnCreatePluginInstance pfnFactory,
  PLUGIN_INIT_PARAM* pInitParam,
  const char* szFilePath)
{
	IPlugin* pIPlugin = 0;

	__try
	{
		pIPlugin = pfnFactory(pInitParam);
	}
	__except (OnFactoryFilter(GetExceptionCode(), GetExceptionInformation()))
	{
		CryLog("Can't initialize plugin '%s'! Possible binary version incompatibility. Please reinstall this plugin.", szFilePath);

		return 0;
	}

	return pIPlugin;
}
}

namespace
{
struct SPlugin
{
	string              m_path;
	string              m_name;
	std::vector<string> m_dependencies;
};

void ParseManifest(const string& manifestPath, SPlugin& plugin)
{
	if (!CFileUtil::FileExists(manifestPath))
	{
		return;
	}

	XmlNodeRef manifest = XmlHelpers::LoadXmlFromFile(manifestPath);

	if (manifest)
	{
		const uint numElements = manifest->getChildCount();
		for (uint i = 0; i < numElements; ++i)
		{
			XmlNodeRef element = manifest->getChild(i);
			if (stricmp(element->getTag(), "Dependency") == 0)
			{
				const char* dependencyName = element->getContent();
				stl::push_back_unique(plugin.m_dependencies, dependencyName);
			}
		}
	}
}

// This does a topological sort on the plugin list. It will also remove plugins that have
// missing dependencies or there is a cycle in the dependency tree.
void SortPluginsByDependency(std::list<SPlugin>& plugins)
{
	LOADING_TIME_PROFILE_SECTION;
	std::list<SPlugin> finalList;
	std::set<string, stl::less_stricmp<string>> loadedPlugins;

	while (!plugins.empty())
	{
		bool bCantReduce = true;

		for (auto iter = plugins.begin(); iter != plugins.end(); )
		{
			auto& dependencies = iter->m_dependencies;
			auto newEnd = std::remove_if(dependencies.begin(), dependencies.end(), [&](const string& dependency)
				{
					return (loadedPlugins.find(dependency) != loadedPlugins.end());
			  });

			dependencies.erase(newEnd, dependencies.end());

			if (dependencies.empty())
			{
				bCantReduce = false;
				finalList.push_back(*iter);
				loadedPlugins.insert(iter->m_name);
				iter = plugins.erase(iter);
			}
			else
			{
				++iter;
			}
		}

		if (bCantReduce)
		{
			for (auto iter = plugins.begin(); iter != plugins.end(); ++iter)
			{
				CryLog("Can't load plugin DLL '%s' because of missing or cyclic dependencies", iter->m_path.GetString());
			}
			break;
		}
	}

	plugins = finalList;
}
}

bool CPluginManager::LoadPlugins(const char* pPathWithMask)
{
	LOADING_TIME_PROFILE_SECTION;

	CString strPath = PathUtil::GetPathWithoutFilename(pPathWithMask).c_str();
	string strMask = PathUtil::GetFile(pPathWithMask);

	CLogFile::WriteLine("Loading plugins...");

	if (!PathFileExists(strPath))
	{
		CryLog("Cannot find plugin directory '%s'", strPath.GetBuffer(1));
		return false;
	}

	std::list<SPlugin> plugins;
	{
		CFileEnum cDLLFiles;
		__finddata64_t sFile;
		char szFilePath[_MAX_PATH];
		if (cDLLFiles.StartEnumeration(strPath.GetBuffer(1), strMask, &sFile))
		{
			do
			{
				// Construct the full filepath of the current file
				cry_strcpy(szFilePath, strPath.GetString());
				PathAddBackslash(szFilePath);
				cry_strcat(szFilePath, sFile.name);

				SPlugin plugin;
				plugin.m_path = szFilePath;
				plugin.m_name = PathUtil::GetFileName(szFilePath);
				plugins.push_back(plugin);
			}
			while (cDLLFiles.GetNextFile(&sFile));
		}
	}

	// Loop over found plugins and parse their manifests
	for (auto iter = plugins.begin(); iter != plugins.end(); ++iter)
	{
		string manifestPath = iter->m_path;
		const int extensionPos = manifestPath.ReverseFind('.');
		if (extensionPos != -1)
		{
			manifestPath = manifestPath.Mid(0, extensionPos);
		}
		manifestPath.Append(".mf");
		ParseManifest(manifestPath, *iter);
	}

	// Sort plugins by dependency
	SortPluginsByDependency(plugins);

	for (auto iter = plugins.begin(); iter != plugins.end(); ++iter)
	{
		LOADING_TIME_PROFILE_SECTION_NAMED_ARGS("Loading plugin", iter->m_path);

		// Load the plugin's DLL
		HMODULE hPlugin = LoadLibraryA(iter->m_path);

		if (!hPlugin)
		{
			const size_t kCharsInMessageBuffer = 32768;
			char szMessageBuffer[kCharsInMessageBuffer] = "";
			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, szMessageBuffer, kCharsInMessageBuffer, NULL);

			CryLog("Can't load plugin DLL '%s' message '%s' !", iter->m_path, szMessageBuffer);
			continue;
		}

		// Query the factory pointer
		TPfnCreatePluginInstance pfnFactory = (TPfnCreatePluginInstance) GetProcAddress(hPlugin, "CreatePluginInstance");

		if (!pfnFactory)
		{
			CryLog("Cannot query plugin DLL '%s' factory pointer (is it a Sandbox plugin?)", iter->m_path);
			FreeLibrary(hPlugin);
			continue;
		}

		IPlugin* pPlugin = NULL;
		PLUGIN_INIT_PARAM sInitParam =
		{
			GetIEditorImpl(),
			SANDBOX_PLUGIN_SYSTEM_VERSION,
			IPlugin::eError_None
		};

		pPlugin = SafeCallFactory(pfnFactory, &sInitParam, iter->m_path);

		if (!pPlugin)
		{
			if (sInitParam.outErrorCode == IPlugin::eError_VersionMismatch)
			{
				CryLog("Cannot create instance of plugin DLL '%s'! Version mismatch. Please update the plugin.", iter->m_path);
			}
			else
			{
				CryLog("Cannot create instance of plugin DLL '%s'! Error code %u.", iter->m_path, sInitParam.outErrorCode);
			}

			ASSERT(hPlugin);
			FreeLibrary(hPlugin);

			continue;
		}

		RegisterPlugin(hPlugin, pPlugin);

		// Write log string about plugin
		CryLog("Successfully loaded plugin '%s', version '%i'",
		                     pPlugin->GetPluginName(), pPlugin->GetPluginVersion());
	}

	return true;
}

void CPluginManager::RegisterPlugin(HMODULE dllHandle, IPlugin* pPlugin)
{
	SPluginEntry entry;

	entry.hLibrary = dllHandle;
	entry.pPlugin = pPlugin;
	m_plugins.push_back(entry);
}

void CPluginManager::CleanUp()
{
	CLogFile::WriteLine("Unloading all previous plugins");

	for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it)
	{
		CRY_ASSERT(it->hLibrary);
		TPfnDeletePluginInstance pfnDelete = (TPfnDeletePluginInstance) GetProcAddress(it->hLibrary, "DeletePluginInstance");
		CRY_ASSERT(pfnDelete);
		//Note : maybe needs a safe call here ?
		pfnDelete(it->pPlugin);
		FreeLibrary(it->hLibrary);
	}

	m_plugins.clear();
}


