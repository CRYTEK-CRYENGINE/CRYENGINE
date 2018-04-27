// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Custom nodes relating to general engine features for this game.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>
#include <CrySandbox/IEditorGame.h>
#include <CryString/CryPath.h>
#include <CrySystem/File/ICryPak.h>
#include <ILevelSystem.h>

#define LAYERSWITCHNODE_UICONFIG_NAME "LayerSet"
#define LAYERSWITCHNODE_UICONFIG      "enum_global_def:" LAYERSWITCHNODE_UICONFIG_NAME
#define LAYERSWITCHNODE_SEP           CRY_NATIVE_PATH_SEPSTR
#define LAYERSWITCHNODE_ROOT_FOLDER   "libs" LAYERSWITCHNODE_SEP "LayerSets" // Designers would have preferred this to be next to each level, but it wouldn't be automatically be picked up by the pak system in that case

class CLayerSetSwitchNode
	: public CFlowBaseNode<eNCT_Singleton>
	, ILevelSystemListener
	, IEntityLayerSetUpdateListener
{
	enum INPUTS
	{
		eIP_Set = 0,
		eIP_LayerSetName,
		eIP_Serialize,
	};

	enum OUTPUTS
	{
		eOP_Done = 0,
		eOP_Failed,
	};

	typedef std::vector<string>        TStringNames;
	typedef std::vector<const char*>   TSzNames;
	typedef CryStackStringT<char, 256> TPathString;

public:
	CLayerSetSwitchNode(SActivationInfo* pActInfo)
		: m_bRegistered(false)
	{
		TryRegister();
	}

	virtual ~CLayerSetSwitchNode()
	{
		if (gEnv->IsEditor() && m_bRegistered)
		{
			if (CCryAction* pCryAction = CCryAction::GetCryAction())
			{
				ILevelSystem* pLevelSystem = pCryAction->GetILevelSystem();
				pLevelSystem->RemoveListener(this);
			}
		}
	}

private:
	virtual void GetMemoryUsage(ICrySizer * s) const override
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig &config) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void   ("Set",              _HELP("Apply the selected configuration.")),
			InputPortConfig<string>("LayerSetName", "", _HELP("Slot in which the character is loaded or -1 for all loaded characters."), nullptr, _UICONFIG(LAYERSWITCHNODE_UICONFIG)),
			InputPortConfig<bool>  ("Serialize", false, _HELP("Enables objects in this layer being saved/loaded.")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Done",   _HELP("Triggered when the layer switching has been done.")),
			OutputPortConfig_AnyType("Failed", _HELP("Triggered when something went wrong (couldn't find the specified layer set ?).")),
			{0}
		};
		config.sDescription = _HELP( "This node lets you easily switch to a different set of layers instead of having to manually maintain each one individually." );
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void UpdateOutput(SActivationInfo* pActInfo, const bool bSuccess)
	{
		if (bSuccess)
		{
			ActivateOutput(pActInfo, eOP_Done, true);
		}
		else
		{
			ActivateOutput(pActInfo, eOP_Failed, true);
		}
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				if (!m_bRegistered)
				{
					TryRegister();
				}
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIP_Set))
				{
					CRY_PROFILE_REGION(PROFILE_GAME, __FUNCTION__);

					bool bSuccess = false;
					const string& layerSetName = GetPortString(pActInfo, eIP_LayerSetName);
					if (!layerSetName.empty())
					{
						const bool bIsSerialized = GetPortBool(pActInfo, eIP_Serialize);

						TLayerSetNameToContent::const_iterator cacheIt = m_layerSetCache.find(layerSetName);
						if (cacheIt != m_layerSetCache.end())
						{
							const SCacheEntry& cacheEntry = cacheIt->second;
							gEnv->pEntitySystem->EnableLayerSet(cacheEntry.szNames.data(), cacheEntry.szNames.size(), bIsSerialized, this);
							bSuccess = true;
						}
						else
						{
							if (!gEnv->IsEditor())
							{
								GameWarning("[LayerSetSwitchNode] layer set %s wasn't found in the cache. Trying to parse the XML at runtime !", layerSetName.c_str());
							}

							TStringNames layerStringNames;
							if (ParseXml(layerStringNames, layerSetName))
							{
								if (!layerStringNames.empty())
								{
									TSzNames layerSzNames;
									UpdateSzNamesFromStringNames(layerSzNames, layerStringNames);

									gEnv->pEntitySystem->EnableLayerSet(&layerSzNames[0], layerSzNames.size(), bIsSerialized, this);
									bSuccess = true;
								}
								else
								{
									GameWarning(string().Format("!layerStringNames.empty() : Selected layer \"%s\" set definition was read properly but seems to contain no visible layer. Are you sure this file is still valid ?", layerSetName.c_str()).c_str());
								}
							}
							else
							{
								GameWarning(string().Format("ParseXml(layerSetName) : Failed to read the selected layer set definition \"%s\". Are you sure this file still exists ?", layerSetName.c_str()).c_str());
							}
						}
					}
					else
					{
						GameWarning("!layerSetName.empty() : Trying to select an invalid layer set (empty name).");
					}
					UpdateOutput(pActInfo, bSuccess);
				}
			}
		}
	}

	// ILevelSystemListener
	virtual void OnLevelNotFound(const char* levelName) override {};
	virtual void OnLoadingStart(ILevelInfo* pLevel) override {};
	virtual void OnLoadingLevelEntitiesStart(ILevelInfo* pLevel) override {};
	virtual void OnLoadingComplete(ILevelInfo* pLevel) override { m_currentLevelName = pLevel->GetName(); RefreshLayerSetsUIEnum(); Preload(); };
	virtual void OnLoadingError(ILevelInfo* pLevel, const char* error) override {};
	virtual void OnLoadingProgress(ILevelInfo* pLevel, int progressAmount) override {};
	virtual void OnUnloadComplete(ILevelInfo* pLevel) override {};
	// ~ILevelSystemListener

	// IEntityLayerSetUpdateListener
	virtual void LayerEnablingEvent(const char* szLayerName, bool bEnabled, bool bSerialized) override
	{
		if (bEnabled && gEnv->pAISystem)
		{
			gEnv->pAISystem->LayerEnabled(szLayerName, bEnabled, bSerialized);
		}
	}
	// ~IEntityLayerSetUpdateListener


	void TryRegister()
	{
		if (CCryAction* pCryAction = CCryAction::GetCryAction())
		{
			ILevelSystem* pLevelSystem = pCryAction->GetILevelSystem();
			pLevelSystem->AddListener(this);
			if (ILevelInfo* pCurrentLevelInfo = pLevelSystem->GetCurrentLevel())
			{
				m_currentLevelName = pCurrentLevelInfo->GetName();
			}

			m_bRegistered = true;
		}
	}

	bool ParseXml(TStringNames& outStringNames, const string& layerSetName) const
	{
		bool bSuccess = false;
		outStringNames.clear();

		TPathString fileName;
		MakeLayerSetPathFromLayerSetName(fileName, layerSetName);

		//We'll need to rethink how to cache these files. Maybe we could store them in this singleton FG node directly ?
		//CGameCache& gameCache = g_pGame->GetGameCache();
		//XmlNodeRef xmlRoot = gameCache.LoadXml(fileName.c_str());

		XmlNodeRef xmlRoot = gEnv->pSystem->LoadXmlFromFile(fileName.c_str());
		if (xmlRoot)
		{
			const int childCount = xmlRoot->getChildCount();
			for (int i = 0; i < childCount; ++i)
			{
				XmlNodeRef childNode = xmlRoot->getChild(i);
				bool bVisible = false;
				if(childNode->getAttr("Visible", bVisible) && bVisible)
				{
					const char* const szName = childNode->getAttr("Name");
					if (szName && *szName)
					{
						outStringNames.push_back(szName);
					}
				}
			}
			bSuccess = true;
		}
		else
		{
			GameWarning(string().Format("Failed to load the selected XML (\"%s\"). Does the file still exist ? Has it been tampered with since last export ?", fileName.c_str()).c_str());
		}

		return bSuccess;
	}

	static void UpdateSzNamesFromStringNames(TSzNames& outSzNames, const TStringNames& stringNames)
	{
		outSzNames.clear();
		outSzNames.reserve(stringNames.size());
		for (const string& layerName : stringNames)
		{
			outSzNames.push_back(layerName.c_str());
		}
	}

	static void FindAllFilesInFolder(TStringNames& outNames, const char* szFolderName)
	{
		TPathString search = szFolderName;
		search += LAYERSWITCHNODE_SEP "*.lvp";

		ICryPak *pPak = gEnv->pCryPak;

		_finddata_t fd;
		intptr_t handle = pPak->FindFirst(search.c_str(), &fd);

		if (handle > -1)
		{
			do
			{
				if (!strcmp(fd.name, ".") || !strcmp(fd.name, ".."))
					continue;

				if (fd.attrib & _A_SUBDIR)
					continue;

				outNames.push_back(fd.name);

			} while (pPak->FindNext(handle, &fd) >= 0);
		}
	}

	void RefreshLayerSetsUIEnum() const
	{
		if (CCryAction* pCryAction = CCryAction::GetCryAction())
		{
			if (gEnv->IsEditor() && !m_currentLevelName.empty())
			{
				if (IGameToEditorInterface* pGameToEditor = pCryAction->GetIGameToEditor())
				{
					TPathString folderName;
					GetLayerSetFolder(folderName);

					TStringNames fileNames;
					fileNames.reserve(16);
					fileNames.push_back("");
					FindAllFilesInFolder(fileNames, folderName.c_str());

					// remove extensions
					for(string& fileName : fileNames)
					{
						fileName = PathUtil::GetFileName(fileName);
					}

					TSzNames fileSzNames;
					UpdateSzNamesFromStringNames(fileSzNames, fileNames);

					pGameToEditor->SetUIEnums(LAYERSWITCHNODE_UICONFIG_NAME, &fileSzNames[0], static_cast<int>(fileSzNames.size()));
				}
			}
		}
	}

	/* Somehow, we'll need to find a way to listen to these sort of events later on. Still, this node is usable without them.
	void OnEditorReloadScripts(const SEditorReloadScriptsEvent& evt) const
	{
		RefreshLayerSetsUIEnum();
	}

	void OnGameCachePreloadEvent(const SGameCachePreloadEvent& evt)
	{
	}
	*/

	void Preload()
	{
		if (!gEnv->IsEditor())
		{
			m_layerSetCache.clear();

			TPathString folderName;
			GetLayerSetFolder(folderName);

			TStringNames fileNames;
			fileNames.reserve(16);
			FindAllFilesInFolder(fileNames, folderName.c_str());

			for(const string& fileName : fileNames)
			{
				const string& fileNameNoExtension = PathUtil::RemoveExtension(fileName);
				SCacheEntry& cacheEntry = m_layerSetCache[fileNameNoExtension];
				ParseXml(cacheEntry.stringNames, fileNameNoExtension);
				UpdateSzNamesFromStringNames(cacheEntry.szNames, cacheEntry.stringNames);
			}
		}
	}

	void GetLayerSetFolder(TPathString& out) const
	{
		out.Format("%s" LAYERSWITCHNODE_SEP "%s", LAYERSWITCHNODE_ROOT_FOLDER, m_currentLevelName.c_str());
	}

	void MakeLayerSetPathFromLayerSetName(TPathString& out, const string& layerSetName) const
	{
		TPathString layerSetFolder;
		GetLayerSetFolder(layerSetFolder);
		out.Format("%s" LAYERSWITCHNODE_SEP "%s.lvp", layerSetFolder.c_str(), layerSetName.c_str());
	}

	struct SCacheEntry
	{
		TStringNames stringNames;
		TSzNames     szNames;
	};

	typedef std::map<string, SCacheEntry> TLayerSetNameToContent;

	TLayerSetNameToContent              m_layerSetCache;
	TPathString                         m_currentLevelName;
	bool                                m_bRegistered;
};

#undef LAYERSWITCHNODE_ROOT_FOLDER
#undef LAYERSWITCHNODE_SEP
#undef LAYERSWITCHNODE_UICONFIG
#undef LAYERSWITCHNODE_UICONFIG_NAME

REGISTER_FLOW_NODE("Engine:LayerSetSwitch", CLayerSetSwitchNode);

