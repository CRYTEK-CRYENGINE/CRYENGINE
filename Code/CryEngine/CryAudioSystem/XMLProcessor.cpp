// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "XMLProcessor.h"
#include "CVars.h"
#include "Managers.h"
#include "FileCacheManager.h"
#include "LoseFocusTrigger.h"
#include "GetFocusTrigger.h"
#include "MuteAllTrigger.h"
#include "UnmuteAllTrigger.h"
#include "PauseAllTrigger.h"
#include "ResumeAllTrigger.h"
#include "Environment.h"
#include "Parameter.h"
#include "PreloadRequest.h"
#include "Setting.h"
#include "Switch.h"
#include "SwitchState.h"
#include "Trigger.h"
#include <IImpl.h>
#include <CryString/CryPath.h>

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	#include "PreviewTrigger.h"
	#include "Common/Logger.h"
#endif // CRY_AUDIO_USE_DEBUG_CODE

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
bool ParseSystemDataFile(char const* const szFolderPath, SPoolSizes& poolSizes, ContextId const contextId)
{
	bool parsedValidLibrary = false;

	CryFixedStringT<MaxFilePathLength> rootFolderPath(szFolderPath);
	rootFolderPath.TrimRight(R"(/\)");
	CryFixedStringT<MaxFilePathLength + MaxFileNameLength> search(rootFolderPath + "/*.xml");
	_finddata_t fd;
	intptr_t const handle = gEnv->pCryPak->FindFirst(search.c_str(), &fd);

	if (handle != -1)
	{
		CryFixedStringT<MaxFilePathLength + MaxFileNameLength> fileName;

		do
		{
			fileName = rootFolderPath.c_str();
			fileName += "/";
			fileName += fd.name;

			XmlNodeRef const rootNode(GetISystem()->LoadXmlFromFile(fileName));

			if (rootNode.isValid())
			{
				if (_stricmp(rootNode->getTag(), g_szRootNodeTag) == 0)
				{
					uint16 numTriggers = 0;
					rootNode->getAttr(g_szNumTriggersAttribute, numTriggers);
					poolSizes.triggers += numTriggers;

					uint16 numParameters = 0;
					rootNode->getAttr(g_szNumParametersAttribute, numParameters);
					poolSizes.parameters += numParameters;

					uint16 numSwitches = 0;
					rootNode->getAttr(g_szNumSwitchesAttribute, numSwitches);
					poolSizes.switches += numSwitches;

					uint16 numStates = 0;
					rootNode->getAttr(g_szNumStatesAttribute, numStates);
					poolSizes.states += numStates;

					uint16 numEnvironments = 0;
					rootNode->getAttr(g_szNumEnvironmentsAttribute, numEnvironments);
					poolSizes.environments += numEnvironments;

					uint16 numPreloads = 0;
					rootNode->getAttr(g_szNumPreloadsAttribute, numPreloads);
					poolSizes.preloads += numPreloads;

					uint16 numSettings = 0;
					rootNode->getAttr(g_szNumSettingsAttribute, numSettings);
					poolSizes.settings += numSettings;

					uint16 numFiles = 0;
					rootNode->getAttr(g_szNumFilesAttribute, numFiles);
					poolSizes.files += numFiles;

					XmlNodeRef const implDataNode = rootNode->findChild(g_szImplDataNodeTag);

					if (implDataNode.isValid())
					{
						g_pIImpl->SetLibraryData(implDataNode, contextId);
					}

					parsedValidLibrary = true;
				}
			}
		}
		while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

		gEnv->pCryPak->FindClose(handle);
	}

	return parsedValidLibrary;
}

//////////////////////////////////////////////////////////////////////////
void ParseContextSystemData(char const* const szFolderPath, SPoolSizes& poolSizes)
{
	CryFixedStringT<MaxFilePathLength> contextFolderPath(szFolderPath);
	contextFolderPath += g_szContextsFolderName;
	contextFolderPath += "/";

	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(contextFolderPath + "*", &fd);

	if (handle != -1)
	{
		do
		{
			if (fd.attrib & _A_SUBDIR)
			{
				char const* const szName = fd.name;

				if ((_stricmp(szName, ".") != 0) && (_stricmp(szName, "..") != 0))
				{
					char const* const szSubFolderName = contextFolderPath + szName;
					ContextId const contextId = StringToId(szName);
					SPoolSizes contextPoolSizes;

					if (ParseSystemDataFile(szSubFolderName, contextPoolSizes, contextId))
					{
						g_contextLookup[contextId] = szName;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
						auto contextInfo = g_contextInfo.find(szName);

						if (contextInfo != g_contextInfo.cend())
						{
							contextInfo->second.isRegistered = true;
						}
						else
						{
							g_contextInfo.emplace(
								std::piecewise_construct,
								std::forward_as_tuple(szName),
								std::forward_as_tuple(SContextInfo(contextId, true, false)));
						}
#endif        // CRY_AUDIO_USE_DEBUG_CODE
					}

					if (g_cvars.m_poolAllocationMode <= 0)
					{
						poolSizes.triggers += contextPoolSizes.triggers;
						poolSizes.parameters += contextPoolSizes.parameters;
						poolSizes.switches += contextPoolSizes.switches;
						poolSizes.states += contextPoolSizes.states;
						poolSizes.environments += contextPoolSizes.environments;
						poolSizes.preloads += contextPoolSizes.preloads;
						poolSizes.settings += contextPoolSizes.settings;
						poolSizes.files += contextPoolSizes.files;
					}
					else
					{
						poolSizes.triggers = std::max(poolSizes.triggers, contextPoolSizes.triggers);
						poolSizes.parameters = std::max(poolSizes.parameters, contextPoolSizes.parameters);
						poolSizes.switches = std::max(poolSizes.switches, contextPoolSizes.switches);
						poolSizes.states = std::max(poolSizes.states, contextPoolSizes.states);
						poolSizes.environments = std::max(poolSizes.environments, contextPoolSizes.environments);
						poolSizes.preloads = std::max(poolSizes.preloads, contextPoolSizes.preloads);
						poolSizes.settings = std::max(poolSizes.settings, contextPoolSizes.settings);
						poolSizes.files = std::max(poolSizes.files, contextPoolSizes.files);
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CXMLProcessor::ParseSystemData()
{
	ZeroStruct(g_poolSizes);
	g_contextLookup.clear();

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	for (auto& contextPair : g_contextInfo)
	{
		if (contextPair.second.contextId != GlobalContextId)
		{
			contextPair.second.isRegistered = false;
		}
	}
#endif // CRY_AUDIO_USE_DEBUG_CODE

	g_pIImpl->OnBeforeLibraryDataChanged();
	g_pIImpl->GetInfo(g_implInfo);
	g_configPath = CRY_AUDIO_DATA_ROOT "/";
	g_configPath += (g_implInfo.folderName + "/" + g_szConfigFolderName + "/").c_str();

	ParseSystemDataFile(g_configPath.c_str(), g_poolSizes, GlobalContextId);

	SPoolSizes contextPoolSizes;
	ParseContextSystemData(g_configPath.c_str(), contextPoolSizes);

	g_poolSizes.triggers += contextPoolSizes.triggers;
	g_poolSizes.parameters += contextPoolSizes.parameters;
	g_poolSizes.switches += contextPoolSizes.switches;
	g_poolSizes.states += contextPoolSizes.states;
	g_poolSizes.environments += contextPoolSizes.environments;
	g_poolSizes.preloads += contextPoolSizes.preloads;
	g_poolSizes.settings += contextPoolSizes.settings;
	g_poolSizes.files += contextPoolSizes.files;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	// Used to hide pools without allocations in debug draw.
	g_debugPoolSizes = g_poolSizes;
#endif // CRY_AUDIO_USE_DEBUG_CODE

	// Need to set pool sizes to at least 1, because there could be files that don't contain the
	// counts yet, which could result in asserts of the pool object.
	g_poolSizes.triggers = std::max<uint16>(1, g_poolSizes.triggers);
	g_poolSizes.parameters = std::max<uint16>(1, g_poolSizes.parameters);
	g_poolSizes.switches = std::max<uint16>(1, g_poolSizes.switches);
	g_poolSizes.states = std::max<uint16>(1, g_poolSizes.states);
	g_poolSizes.environments = std::max<uint16>(1, g_poolSizes.environments);
	g_poolSizes.preloads = std::max<uint16>(1, g_poolSizes.preloads);
	g_poolSizes.settings = std::max<uint16>(1, g_poolSizes.settings);
	g_poolSizes.files = std::max<uint16>(1, g_poolSizes.files);

	g_pIImpl->OnAfterLibraryDataChanged(g_cvars.m_poolAllocationMode);
}

//////////////////////////////////////////////////////////////////////////
bool CXMLProcessor::ParseControlsData(char const* const szFolderPath, ContextId const contextId, char const* const szContextName)
{
	bool contextExists = false;

	CryFixedStringT<MaxFilePathLength> sRootFolderPath(szFolderPath);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	CTimeValue const startTime(gEnv->pTimer->GetAsyncTime());
#endif // CRY_AUDIO_USE_DEBUG_CODE

	if (g_pIImpl != nullptr)
	{
		sRootFolderPath.TrimRight(R"(/\)");
		CryFixedStringT<MaxFilePathLength + MaxFileNameLength> sSearch(sRootFolderPath + "/*.xml");
		_finddata_t fd;
		intptr_t const handle = gEnv->pCryPak->FindFirst(sSearch.c_str(), &fd);

		if (handle != -1)
		{
			CryFixedStringT<MaxFilePathLength + MaxFileNameLength> sFileName;

			do
			{
				sFileName = sRootFolderPath.c_str();
				sFileName += "/";
				sFileName += fd.name;

				XmlNodeRef const rootNode(GetISystem()->LoadXmlFromFile(sFileName));

				if (rootNode.isValid())
				{
					if (_stricmp(rootNode->getTag(), g_szRootNodeTag) == 0)
					{
						LibraryId const libraryId = StringToId(rootNode->getAttr(g_szNameAttribute));

						if (libraryId == DefaultLibraryId)
						{
							ParseDefaultControlsFile(rootNode);
						}
						else
						{
							ParseControlsFile(rootNode, contextId);
						}

						contextExists = true;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
						for (auto& contextPair : g_contextInfo)
						{
							if (contextPair.second.contextId == contextId)
							{
								contextPair.second.isActive = true;
								break;
							}
						}
#endif        // CRY_AUDIO_USE_DEBUG_CODE
					}
				}
			}
			while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

			gEnv->pCryPak->FindClose(handle);
		}
	}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	float const duration = (gEnv->pTimer->GetAsyncTime() - startTime).GetMilliSeconds();
	Cry::Audio::Log(ELogType::Comment, R"(Parsed controls data in "%s" for context "%s" in %.3f ms!)", szFolderPath, szContextName, duration);
#endif // CRY_AUDIO_USE_DEBUG_CODE

	return contextExists;
}

//////////////////////////////////////////////////////////////////////////
void CXMLProcessor::ParseControlsFile(XmlNodeRef const& rootNode, ContextId const contextId)
{
	int const rootChildCount = rootNode->getChildCount();

	for (int i = 0; i < rootChildCount; ++i)
	{
		XmlNodeRef const childNode(rootNode->getChild(i));

		if (childNode.isValid())
		{
			char const* const szChildNodeTag = childNode->getTag();

			if (_stricmp(szChildNodeTag, g_szTriggersNodeTag) == 0)
			{
				ParseTriggers(childNode, contextId);
			}
			else if (_stricmp(szChildNodeTag, g_szParametersNodeTag) == 0)
			{
				ParseParameters(childNode, contextId);
			}
			else if (_stricmp(szChildNodeTag, g_szSwitchesNodeTag) == 0)
			{
				ParseSwitches(childNode, contextId);
			}
			else if (_stricmp(szChildNodeTag, g_szEnvironmentsNodeTag) == 0)
			{
				ParseEnvironments(childNode, contextId);
			}
			else if (_stricmp(szChildNodeTag, g_szSettingsNodeTag) == 0)
			{
				ParseSettings(childNode, contextId);
			}
			else if ((_stricmp(szChildNodeTag, g_szImplDataNodeTag) == 0) ||
			         (_stricmp(szChildNodeTag, g_szPreloadsNodeTag) == 0) ||
			         (_stricmp(szChildNodeTag, g_szEditorDataTag) == 0))
			{
				// These tags are valid but ignored here.
			}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			else
			{
				Cry::Audio::Log(ELogType::Warning, "Unknown AudioSystemData node: %s", szChildNodeTag);
			}
#endif  // CRY_AUDIO_USE_DEBUG_CODE
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CXMLProcessor::ParseDefaultControlsFile(XmlNodeRef const& rootNode)
{
	int const rootChildCount = rootNode->getChildCount();

	for (int i = 0; i < rootChildCount; ++i)
	{
		XmlNodeRef const childNode(rootNode->getChild(i));

		if (childNode.isValid())
		{
			char const* const childNodeTag = childNode->getTag();

			if (_stricmp(childNodeTag, g_szTriggersNodeTag) == 0)
			{
				ParseDefaultTriggers(childNode);
			}
			else if ((_stricmp(childNodeTag, g_szImplDataNodeTag) == 0) ||
			         (_stricmp(childNodeTag, g_szParametersNodeTag) == 0) ||
			         (_stricmp(childNodeTag, g_szSwitchesNodeTag) == 0) ||
			         (_stricmp(childNodeTag, g_szEnvironmentsNodeTag) == 0) ||
			         (_stricmp(childNodeTag, g_szPreloadsNodeTag) == 0) ||
			         (_stricmp(childNodeTag, g_szSettingsNodeTag) == 0) ||
			         (_stricmp(childNodeTag, g_szEditorDataTag) == 0))
			{
				// These tags are valid but ignored here, because no default controls of these type currently exist.
			}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			else
			{
				Cry::Audio::Log(ELogType::Warning, "Unknown AudioSystemData node: %s", childNodeTag);
			}
#endif  // CRY_AUDIO_USE_DEBUG_CODE
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CXMLProcessor::ParsePreloadsData(char const* const szFolderPath, ContextId const contextId)
{
	CryFixedStringT<MaxFilePathLength> rootFolderPath(szFolderPath);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	CTimeValue const startTime(gEnv->pTimer->GetAsyncTime());
#endif // CRY_AUDIO_USE_DEBUG_CODE

	if (g_pIImpl != nullptr)
	{
		rootFolderPath.TrimRight(R"(/\)");
		CryFixedStringT<MaxFilePathLength + MaxFileNameLength> search(rootFolderPath + "/*.xml");
		_finddata_t fd;
		intptr_t const handle = gEnv->pCryPak->FindFirst(search.c_str(), &fd);

		if (handle != -1)
		{
			CryFixedStringT<MaxFilePathLength + MaxFileNameLength> fileName;

			do
			{
				fileName = rootFolderPath.c_str();
				fileName += "/";
				fileName += fd.name;

				XmlNodeRef const rootNode(GetISystem()->LoadXmlFromFile(fileName));

				if (rootNode.isValid())
				{
					if (_stricmp(rootNode->getTag(), g_szRootNodeTag) == 0)
					{

						uint versionNumber = 1;
						rootNode->getAttr(g_szVersionAttribute, versionNumber);
						int const numChildren = rootNode->getChildCount();

						for (int i = 0; i < numChildren; ++i)
						{
							XmlNodeRef const childNode(rootNode->getChild(i));

							if (childNode.isValid())
							{
								char const* const szChildNodeTag = childNode->getTag();

								if (_stricmp(szChildNodeTag, g_szPreloadsNodeTag) == 0)
								{
									size_t const lastSlashIndex = rootFolderPath.rfind("/"[0]);

									if (rootFolderPath.npos != lastSlashIndex)
									{
										CryFixedStringT<MaxFilePathLength> const folderName(rootFolderPath.substr(lastSlashIndex + 1, rootFolderPath.size()));
										ParsePreloads(childNode, contextId, folderName.c_str(), versionNumber);
									}
									else
									{
										ParsePreloads(childNode, contextId, nullptr, versionNumber);
									}
								}
								else if (_stricmp(szChildNodeTag, g_szImplDataNodeTag) == 0 ||
								         _stricmp(szChildNodeTag, g_szTriggersNodeTag) == 0 ||
								         _stricmp(szChildNodeTag, g_szParametersNodeTag) == 0 ||
								         _stricmp(szChildNodeTag, g_szSwitchesNodeTag) == 0 ||
								         _stricmp(szChildNodeTag, g_szEnvironmentsNodeTag) == 0 ||
								         _stricmp(szChildNodeTag, g_szSettingsNodeTag) == 0 ||
								         _stricmp(szChildNodeTag, g_szEditorDataTag) == 0)
								{
									// These tags are valid but ignored here.
								}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
								else
								{
									Cry::Audio::Log(ELogType::Warning, "Unknown AudioSystemData node: %s", szChildNodeTag);
								}
#endif            // CRY_AUDIO_USE_DEBUG_CODE
							}
						}
					}
				}
			}
			while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

			gEnv->pCryPak->FindClose(handle);
		}
	}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	char const* const szContextName = stl::find_in_map(g_contextLookup, contextId, "unknown");
	float const duration = (gEnv->pTimer->GetAsyncTime() - startTime).GetMilliSeconds();
	Cry::Audio::Log(ELogType::Comment, R"(Parsed preloads data in "%s" for context "%s" in %.3f ms!)", szFolderPath, szContextName, duration);
#endif // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CXMLProcessor::ClearControlsData(ContextId const contextId, bool const clearAll)
{
	if (g_pIImpl != nullptr)
	{
		if (clearAll || (contextId == GlobalContextId))
		{
			g_loseFocusTrigger.Clear();
			g_getFocusTrigger.Clear();
			g_muteAllTrigger.Clear();
			g_unmuteAllTrigger.Clear();
			g_pauseAllTrigger.Clear();
			g_resumeAllTrigger.Clear();

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			g_previewTrigger.Clear();
#endif  // CRY_AUDIO_USE_DEBUG_CODE
		}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		if (clearAll)
		{
			for (auto& contextPair : g_contextInfo)
			{
				contextPair.second.isActive = false;
			}
		}
		else
		{
			for (auto& contextPair : g_contextInfo)
			{
				if (contextPair.second.contextId == contextId)
				{
					contextPair.second.isActive = false;
					break;
				}
			}
		}
#endif  // CRY_AUDIO_USE_DEBUG_CODE

		TriggerLookup::iterator iterTriggers(g_triggerLookup.begin());
		TriggerLookup::const_iterator iterTriggersEnd(g_triggerLookup.end());

		while (iterTriggers != iterTriggersEnd)
		{
			CTrigger const* const pTrigger = iterTriggers->second;

			if (clearAll || (pTrigger->GetContextId() == contextId))
			{
				delete pTrigger;
				iterTriggers = g_triggerLookup.erase(iterTriggers);
				iterTriggersEnd = g_triggerLookup.end();
				continue;
			}

			++iterTriggers;
		}

		ParameterLookup::iterator iterParameters(g_parameterLookup.begin());
		ParameterLookup::const_iterator iterParametersEnd(g_parameterLookup.end());

		while (iterParameters != iterParametersEnd)
		{
			CParameter const* const pParameter = iterParameters->second;

			if (clearAll || (pParameter->GetContextId() == contextId))
			{
				delete pParameter;
				iterParameters = g_parameterLookup.erase(iterParameters);
				iterParametersEnd = g_parameterLookup.end();
				continue;
			}

			++iterParameters;
		}

		SwitchLookup::iterator iterSwitches(g_switchLookup.begin());
		SwitchLookup::const_iterator iterSwitchesEnd(g_switchLookup.end());

		while (iterSwitches != iterSwitchesEnd)
		{
			CSwitch const* const pSwitch = iterSwitches->second;

			if (clearAll || (pSwitch->GetContextId() == contextId))
			{
				delete pSwitch;
				iterSwitches = g_switchLookup.erase(iterSwitches);
				iterSwitchesEnd = g_switchLookup.end();
				continue;
			}

			++iterSwitches;
		}

		EnvironmentLookup::iterator iterEnvironments(g_environmentLookup.begin());
		EnvironmentLookup::const_iterator iterEnvironmentsEnd(g_environmentLookup.end());

		while (iterEnvironments != iterEnvironmentsEnd)
		{
			CEnvironment const* const pEnvironment = iterEnvironments->second;

			if (clearAll || (pEnvironment->GetContextId() == contextId))
			{
				delete pEnvironment;
				iterEnvironments = g_environmentLookup.erase(iterEnvironments);
				iterEnvironmentsEnd = g_environmentLookup.end();
				continue;
			}

			++iterEnvironments;
		}

		SettingLookup::iterator iterSettings(g_settingLookup.begin());
		SettingLookup::const_iterator iterSettingsEnd(g_settingLookup.end());

		while (iterSettings != iterSettingsEnd)
		{
			CSetting const* const pSetting = iterSettings->second;

			if (clearAll || (pSetting->GetContextId() == contextId))
			{
				delete pSetting;
				iterSettings = g_settingLookup.erase(iterSettings);
				iterSettingsEnd = g_settingLookup.end();
				continue;
			}

			++iterSettings;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CXMLProcessor::ParsePreloads(XmlNodeRef const& rootNode, ContextId const contextId, char const* const szFolderName, uint const version)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	int const numPreloadRequests = rootNode->getChildCount();

	for (int i = 0; i < numPreloadRequests; ++i)
	{
		XmlNodeRef const preloadRequestNode(rootNode->getChild(i));

		if (preloadRequestNode.isValid() && (_stricmp(preloadRequestNode->getTag(), g_szPreloadRequestTag) == 0))
		{
			PreloadRequestId preloadRequestId = GlobalPreloadRequestId;
			char const* szPreloadRequestName = g_szGlobalPreloadRequestName;
			bool const isAutoLoad = (_stricmp(preloadRequestNode->getAttr(g_szTypeAttribute), g_szDataLoadType) == 0);

			if (!isAutoLoad)
			{
				szPreloadRequestName = preloadRequestNode->getAttr(g_szNameAttribute);
				preloadRequestId = static_cast<PreloadRequestId>(StringToId(szPreloadRequestName));
			}
			else if (contextId != GlobalContextId)
			{
				szPreloadRequestName = szFolderName;
				preloadRequestId = static_cast<PreloadRequestId>(StringToId(szPreloadRequestName));
			}

			if (preloadRequestId != InvalidPreloadRequestId)
			{
				int const numFiles = preloadRequestNode->getChildCount();

				CPreloadRequest::FileIds fileIds;
				fileIds.reserve(numFiles);

				for (int j = 0; j < numFiles; ++j)
				{
					XmlNodeRef const fileNode(preloadRequestNode->getChild(j));

					if (fileNode.isValid())
					{
						FileId const id = g_fileCacheManager.TryAddFileCacheEntry(fileNode, contextId, isAutoLoad);

						if (id != InvalidFileId)
						{
							fileIds.push_back(id);
						}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
						else
						{
							Cry::Audio::Log(ELogType::Warning, R"(Preload request "%s" could not create file entry from tag "%s"!)", szPreloadRequestName, fileNode->getTag());
						}
#endif        // CRY_AUDIO_USE_DEBUG_CODE
					}
				}

				CPreloadRequest* pPreloadRequest = stl::find_in_map(g_preloadRequests, preloadRequestId, nullptr);

				if (pPreloadRequest == nullptr)
				{
					MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CryAudio::CPreloadRequest");
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
					pPreloadRequest = new CPreloadRequest(preloadRequestId, contextId, isAutoLoad, fileIds, szPreloadRequestName);
#else
					pPreloadRequest = new CPreloadRequest(preloadRequestId, contextId, isAutoLoad, fileIds);
#endif        // CRY_AUDIO_USE_DEBUG_CODE

					if (pPreloadRequest != nullptr)
					{
						g_preloadRequests[preloadRequestId] = pPreloadRequest;
					}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
					else
					{
						Cry::Audio::Log(ELogType::Error, "Failed to allocate CPreloadRequest");
					}
#endif        // CRY_AUDIO_USE_DEBUG_CODE
				}
				else
				{
					// Add to existing preload request.
					pPreloadRequest->m_fileIds.insert(pPreloadRequest->m_fileIds.end(), fileIds.begin(), fileIds.end());
				}
			}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			else
			{
				Cry::Audio::Log(ELogType::Error, R"(Preload request "%s" already exists! Skipping this entry!)", szPreloadRequestName);
			}
#endif        // CRY_AUDIO_USE_DEBUG_CODE
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CXMLProcessor::ClearPreloadsData(ContextId const contextId, bool const clearAll)
{
	if (g_pIImpl != nullptr)
	{
		PreloadRequestLookup::iterator iter = g_preloadRequests.begin();
		PreloadRequestLookup::const_iterator const iterEnd = g_preloadRequests.end();

		while (iter != iterEnd)
		{
			CPreloadRequest const* const pRequest = iter->second;

			if (clearAll || (pRequest->GetContextId() == contextId))
			{
				DeletePreloadRequest(pRequest);
				g_preloadRequests.erase(iter++);
			}
			else
			{
				++iter;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CXMLProcessor::ParseEnvironments(XmlNodeRef const& rootNode, ContextId const contextId)
{
	int const numEnvironments = rootNode->getChildCount();

	for (int i = 0; i < numEnvironments; ++i)
	{
		XmlNodeRef const environmentNode(rootNode->getChild(i));

		if (environmentNode.isValid() && (_stricmp(environmentNode->getTag(), g_szEnvironmentTag) == 0))
		{
			char const* const szEnvironmentName = environmentNode->getAttr(g_szNameAttribute);
			auto const environmentId = static_cast<EnvironmentId const>(StringToId(szEnvironmentName));

			if ((environmentId != InvalidControlId) && (stl::find_in_map(g_environmentLookup, environmentId, nullptr) == nullptr))
			{
				//there is no entry yet with this ID in the container
				int const numConnections = environmentNode->getChildCount();

				EnvironmentConnections connections;
				connections.reserve(numConnections);

				for (int j = 0; j < numConnections; ++j)
				{
					XmlNodeRef const environmentConnectionNode(environmentNode->getChild(j));

					if (environmentConnectionNode.isValid())
					{
						Impl::IEnvironmentConnection* const pConnection = g_pIImpl->ConstructEnvironmentConnection(environmentConnectionNode);

						if (pConnection != nullptr)
						{
							connections.push_back(pConnection);
						}
					}
				}

				if (!connections.empty())
				{
					if (connections.size() < numConnections)
					{
						connections.shrink_to_fit();
					}

					MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CryAudio::CEnvironment");
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
					auto const pNewEnvironment = new CEnvironment(environmentId, contextId, connections, szEnvironmentName);
#else
					auto const pNewEnvironment = new CEnvironment(environmentId, contextId, connections);
#endif      // CRY_AUDIO_USE_DEBUG_CODE

					if (pNewEnvironment != nullptr)
					{
						g_environmentLookup[environmentId] = pNewEnvironment;
					}
				}
			}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			else
			{
				Cry::Audio::Log(ELogType::Error, R"(Environment "%s" already exists!)", szEnvironmentName);
			}
#endif      // CRY_AUDIO_USE_DEBUG_CODE
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CXMLProcessor::ParseSettings(XmlNodeRef const& rootNode, ContextId const contextId)
{
	int const numSettings = rootNode->getChildCount();

	for (int i = 0; i < numSettings; ++i)
	{
		XmlNodeRef const settingNode(rootNode->getChild(i));

		if (settingNode.isValid() && (_stricmp(settingNode->getTag(), g_szSettingTag) == 0))
		{
			char const* const szSettingName = settingNode->getAttr(g_szNameAttribute);
			auto const settingId = static_cast<ControlId>(StringToId(szSettingName));

			if ((settingId != InvalidControlId) && (stl::find_in_map(g_settingLookup, settingId, nullptr) == nullptr))
			{
				int const numConnections = settingNode->getChildCount();

				SettingConnections connections;
				connections.reserve(numConnections);

				for (int j = 0; j < numConnections; ++j)
				{
					XmlNodeRef const settingConnectionNode(settingNode->getChild(j));

					if (settingConnectionNode.isValid())
					{
						Impl::ISettingConnection* const pConnection = g_pIImpl->ConstructSettingConnection(settingConnectionNode);

						if (pConnection != nullptr)
						{
							connections.push_back(pConnection);
						}
					}
				}

				if (!connections.empty())
				{
					if (connections.size() < numConnections)
					{
						connections.shrink_to_fit();
					}

					bool const isAutoLoad = (_stricmp(settingNode->getAttr(g_szTypeAttribute), g_szDataLoadType) == 0);

					MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CryAudio::CSetting");
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
					auto const pNewSetting = new CSetting(settingId, contextId, isAutoLoad, connections, szSettingName);
#else
					auto const pNewSetting = new CSetting(settingId, contextId, isAutoLoad, connections);
#endif        // CRY_AUDIO_USE_DEBUG_CODE

					if (pNewSetting != nullptr)
					{
						g_settingLookup[settingId] = pNewSetting;
					}
				}
			}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			else
			{
				Cry::Audio::Log(ELogType::Error, R"(Setting "%s" already exists!)", szSettingName);
			}
#endif      // CRY_AUDIO_USE_DEBUG_CODE
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CXMLProcessor::ParseTriggers(XmlNodeRef const& rootNode, ContextId const contextId)
{
	int const numTriggers = rootNode->getChildCount();

	for (int i = 0; i < numTriggers; ++i)
	{
		XmlNodeRef const triggerNode(rootNode->getChild(i));

		if (triggerNode.isValid() && (_stricmp(triggerNode->getTag(), g_szTriggerTag) == 0))
		{
			char const* const szTriggerName = triggerNode->getAttr(g_szNameAttribute);
			auto const triggerId = static_cast<ControlId const>(StringToId(szTriggerName));

			if ((triggerId != InvalidControlId) && (stl::find_in_map(g_triggerLookup, triggerId, nullptr) == nullptr))
			{
				int const numConnections = triggerNode->getChildCount();
				TriggerConnections connections;
				connections.reserve(numConnections);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
				float maxRadius = 0.0f;
#endif        // CRY_AUDIO_USE_DEBUG_CODE

				for (int m = 0; m < numConnections; ++m)
				{
					XmlNodeRef const triggerConnectionNode(triggerNode->getChild(m));

					if (triggerConnectionNode.isValid())
					{
						float radius = 0.0f;
						Impl::ITriggerConnection* const pConnection = g_pIImpl->ConstructTriggerConnection(triggerConnectionNode, radius);

						if (pConnection != nullptr)
						{
							connections.push_back(pConnection);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
							maxRadius = std::max(radius, maxRadius);
#endif          // CRY_AUDIO_USE_DEBUG_CODE
						}
					}
				}

				if (!connections.empty())
				{
					if (connections.size() < numConnections)
					{
						connections.shrink_to_fit();
					}

					MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CryAudio::CTrigger");
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
					auto const pNewTrigger = new CTrigger(triggerId, contextId, connections, maxRadius, szTriggerName);
#else
					auto const pNewTrigger = new CTrigger(triggerId, contextId, connections);
#endif        // CRY_AUDIO_USE_DEBUG_CODE

					if (pNewTrigger != nullptr)
					{
						g_triggerLookup[triggerId] = pNewTrigger;
					}
				}
			}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			else
			{
				Cry::Audio::Log(ELogType::Error, R"(Trigger "%s" already exists!)", szTriggerName);
			}
#endif        // CRY_AUDIO_USE_DEBUG_CODE
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CXMLProcessor::ParseDefaultTriggers(XmlNodeRef const& rootNode)
{
	int const numTriggers = rootNode->getChildCount();

	for (int i = 0; i < numTriggers; ++i)
	{
		XmlNodeRef const triggerNode(rootNode->getChild(i));

		if (triggerNode.isValid() && (_stricmp(triggerNode->getTag(), g_szTriggerTag) == 0))
		{
			char const* const szTriggerName = triggerNode->getAttr(g_szNameAttribute);
			auto const triggerId = static_cast<ControlId const>(StringToId(szTriggerName));
			int const numConnections = triggerNode->getChildCount();
			TriggerConnections connections;
			connections.reserve(numConnections);

			for (int j = 0; j < numConnections; ++j)
			{
				XmlNodeRef const connectionNode(triggerNode->getChild(j));

				if (connectionNode.isValid())
				{
					float radius = 0.0f;
					Impl::ITriggerConnection* const pConnection = g_pIImpl->ConstructTriggerConnection(connectionNode, radius);

					if (pConnection != nullptr)
					{
						connections.push_back(pConnection);
					}
				}
			}

			if (!connections.empty())
			{
				if (connections.size() < numConnections)
				{
					connections.shrink_to_fit();
				}

				switch (triggerId)
				{
				case g_loseFocusTriggerId:
					{
						g_loseFocusTrigger.AddConnections(connections);
						break;
					}
				case g_getFocusTriggerId:
					{
						g_getFocusTrigger.AddConnections(connections);
						break;
					}
				case g_muteAllTriggerId:
					{
						g_muteAllTrigger.AddConnections(connections);
						break;
					}
				case g_unmuteAllTriggerId:
					{
						g_unmuteAllTrigger.AddConnections(connections);
						break;
					}
				case g_pauseAllTriggerId:
					{
						g_pauseAllTrigger.AddConnections(connections);
						break;
					}
				case g_resumeAllTriggerId:
					{
						g_resumeAllTrigger.AddConnections(connections);
						break;
					}
				default:
					{
						CRY_ASSERT_MESSAGE(false, R"(The default trigger "%s" does not exist during %s)", szTriggerName, __FUNCTION__);
						break;
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CXMLProcessor::ParseSwitches(XmlNodeRef const& rootNode, ContextId const contextId)
{
	int const numSwitches = rootNode->getChildCount();

	for (int i = 0; i < numSwitches; ++i)
	{
		XmlNodeRef const switchNode(rootNode->getChild(i));

		if (switchNode.isValid() && (_stricmp(switchNode->getTag(), g_szSwitchTag) == 0))
		{
			char const* const szSwitchName = switchNode->getAttr(g_szNameAttribute);
			auto const switchId = static_cast<ControlId const>(StringToId(szSwitchName));

			if ((switchId != InvalidControlId) && (stl::find_in_map(g_switchLookup, switchId, nullptr) == nullptr))
			{
				MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CryAudio::CSwitch");
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
				auto const pNewSwitch = new CSwitch(switchId, contextId, szSwitchName);
#else
				auto const pNewSwitch = new CSwitch(switchId, contextId);
#endif    // CRY_AUDIO_USE_DEBUG_CODE

				int const numStates = switchNode->getChildCount();

				for (int j = 0; j < numStates; ++j)
				{
					XmlNodeRef const switchStateNode(switchNode->getChild(j));

					if (switchStateNode.isValid() && (_stricmp(switchStateNode->getTag(), g_szStateTag) == 0))
					{
						char const* const szSwitchStateName = switchStateNode->getAttr(g_szNameAttribute);
						auto const switchStateId = static_cast<SwitchStateId const>(StringToId(szSwitchStateName));

						if (switchStateId != InvalidSwitchStateId)
						{
							int const numConnections = switchStateNode->getChildCount();
							SwitchStateConnections connections;
							connections.reserve(numConnections);

							for (int k = 0; k < numConnections; ++k)
							{
								XmlNodeRef const stateConnectionNode(switchStateNode->getChild(k));

								if (stateConnectionNode.isValid())
								{
									Impl::ISwitchStateConnection* const pConnection = g_pIImpl->ConstructSwitchStateConnection(stateConnectionNode);

									if (pConnection != nullptr)
									{
										connections.push_back(pConnection);
									}
								}
							}

							if (!connections.empty())
							{
								if (connections.size() < numConnections)
								{
									connections.shrink_to_fit();
								}

								MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CryAudio::CSwitchState");
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
								auto const pNewState = new CSwitchState(switchId, switchStateId, connections, szSwitchStateName);
#else
								auto const pNewState = new CSwitchState(switchId, switchStateId, connections);
#endif            // CRY_AUDIO_USE_DEBUG_CODE

								pNewSwitch->AddState(switchStateId, pNewState);
							}
						}
					}
				}

				g_switchLookup[switchId] = pNewSwitch;
			}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			else
			{
				Cry::Audio::Log(ELogType::Error, R"(Switch "%s" already exists!)", szSwitchName);
			}
#endif        // CRY_AUDIO_USE_DEBUG_CODE
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CXMLProcessor::ParseParameters(XmlNodeRef const& rootNode, ContextId const contextId)
{
	int const numParameters = rootNode->getChildCount();

	for (int i = 0; i < numParameters; ++i)
	{
		XmlNodeRef const parameterNode(rootNode->getChild(i));

		if (parameterNode.isValid() && (_stricmp(parameterNode->getTag(), g_szParameterTag) == 0))
		{
			char const* const szParameterName = parameterNode->getAttr(g_szNameAttribute);
			auto const parameterId = static_cast<ControlId const>(StringToId(szParameterName));

			if ((parameterId != InvalidControlId) && (stl::find_in_map(g_parameterLookup, parameterId, nullptr) == nullptr))
			{
				int const numConnections = parameterNode->getChildCount();
				ParameterConnections connections;
				connections.reserve(numConnections);

				for (int j = 0; j < numConnections; ++j)
				{
					XmlNodeRef const parameterConnectionNode(parameterNode->getChild(j));

					if (parameterConnectionNode.isValid())
					{
						Impl::IParameterConnection* const pConnection = g_pIImpl->ConstructParameterConnection(parameterConnectionNode);

						if (pConnection != nullptr)
						{
							connections.push_back(pConnection);
						}
					}
				}

				if (!connections.empty())
				{
					if (connections.size() < numConnections)
					{
						connections.shrink_to_fit();
					}

					MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CryAudio::CParameter");
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
					auto const pParameter = new CParameter(parameterId, contextId, connections, szParameterName);
#else
					auto const pParameter = new CParameter(parameterId, contextId, connections);
#endif        // CRY_AUDIO_USE_DEBUG_CODE

					if (pParameter != nullptr)
					{
						g_parameterLookup[parameterId] = pParameter;
					}
				}
			}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			else
			{
				Cry::Audio::Log(ELogType::Error, R"(Parameter "%s" already exists!)", szParameterName);
			}
#endif        // CRY_AUDIO_USE_DEBUG_CODE
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CXMLProcessor::DeletePreloadRequest(CPreloadRequest const* const pPreloadRequest)
{
	if (pPreloadRequest != nullptr)
	{
		ContextId const contextId = pPreloadRequest->GetContextId();

		for (auto const fileId : pPreloadRequest->m_fileIds)
		{
			g_fileCacheManager.TryRemoveFileCacheEntry(fileId, contextId);
		}

		delete pPreloadRequest;
	}
}
} // namespace CryAudio
