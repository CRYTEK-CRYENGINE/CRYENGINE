// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioXMLProcessor.h"
#include "InternalEntities.h"
#include "Common/Logger.h"
#include <IAudioImpl.h>
#include <CryString/CryPath.h>

namespace CryAudio
{
#define AUDIO_TRIGGER_IMPL_ID_NUM_RESERVED 100 // IDs below that value are used for the CATLTriggerImpl_Internal

//////////////////////////////////////////////////////////////////////////
CAudioXMLProcessor::CAudioXMLProcessor(
  AudioTriggerLookup& triggers,
  AudioParameterLookup& parameters,
  AudioSwitchLookup& switches,
  AudioEnvironmentLookup& environments,
  AudioPreloadRequestLookup& preloadRequests,
  CFileCacheManager& fileCacheMgr,
  SInternalControls const& internalControls)
	: m_triggers(triggers)
	, m_parameters(parameters)
	, m_switches(switches)
	, m_environments(environments)
	, m_preloadRequests(preloadRequests)
	, m_triggerImplIdCounter(AUDIO_TRIGGER_IMPL_ID_NUM_RESERVED)
	, m_fileCacheMgr(fileCacheMgr)
	, m_pIImpl(nullptr)
	, m_internalControls(internalControls)
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::Init(Impl::IImpl* const pIImpl)
{
	m_pIImpl = pIImpl;
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::Release()
{
	m_pIImpl = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::ParseControlsData(char const* const szFolderPath, EDataScope const dataScope)
{
	CryFixedStringT<MaxFilePathLength> sRootFolderPath(szFolderPath);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	CTimeValue const startTime(gEnv->pTimer->GetAsyncTime());
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	if (m_pIImpl != nullptr)
	{
		sRootFolderPath.TrimRight(R"(/\)");
		CryFixedStringT<MaxFilePathLength + MaxFileNameLength> sSearch(sRootFolderPath + "/*.xml");
		_finddata_t fd;
		intptr_t handle = gEnv->pCryPak->FindFirst(sSearch.c_str(), &fd);

		if (handle != -1)
		{
			CryFixedStringT<MaxFilePathLength + MaxFileNameLength> sFileName;

			do
			{
				sFileName = sRootFolderPath.c_str();
				sFileName += "/";
				sFileName += fd.name;

				XmlNodeRef const pRootNode(GetISystem()->LoadXmlFromFile(sFileName));

				if (pRootNode)
				{
					if (_stricmp(pRootNode->getTag(), s_szRootNodeTag) == 0)
					{
						LibraryId const libraryId = StringToId(pRootNode->getAttr(s_szNameAttribute));

						if (libraryId == DefaultLibraryId)
						{
							ParseDefaultControlsFile(pRootNode);
						}
						else
						{
							ParseControlsFile(pRootNode, dataScope);
						}
					}
				}
			}
			while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

			gEnv->pCryPak->FindClose(handle);
		}
	}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	char const* szDataScope = "unknown";

	switch (dataScope)
	{
	case EDataScope::Global:
		{
			szDataScope = "Global";

			break;
		}
	case EDataScope::LevelSpecific:
		{
			szDataScope = "Level Specific";

			break;
		}
	}

	float const duration = (gEnv->pTimer->GetAsyncTime() - startTime).GetMilliSeconds();
	Cry::Audio::Log(ELogType::Comment, R"(Parsed controls data in "%s" for data scope "%s" in %.3f ms!)", szFolderPath, szDataScope, duration);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::ParseControlsFile(XmlNodeRef const pRootNode, EDataScope const dataScope)
{
	int const rootChildCount = pRootNode->getChildCount();

	for (int i = 0; i < rootChildCount; ++i)
	{
		XmlNodeRef const pChildNode(pRootNode->getChild(i));

		if (pChildNode != nullptr)
		{
			char const* const szChildNodeTag = pChildNode->getTag();

			if (_stricmp(szChildNodeTag, s_szTriggersNodeTag) == 0)
			{
				ParseTriggers(pChildNode, dataScope);
			}
			else if (_stricmp(szChildNodeTag, s_szParametersNodeTag) == 0)
			{
				ParseParameters(pChildNode, dataScope);
			}
			else if (_stricmp(szChildNodeTag, s_szSwitchesNodeTag) == 0)
			{
				ParseSwitches(pChildNode, dataScope);
			}
			else if (_stricmp(szChildNodeTag, s_szEnvironmentsNodeTag) == 0)
			{
				ParseEnvironments(pChildNode, dataScope);
			}
			else if (_stricmp(szChildNodeTag, s_szPreloadsNodeTag) == 0 ||
			         _stricmp(szChildNodeTag, s_szEditorDataTag) == 0)
			{
				// This tag is valid but ignored here.
			}
			else
			{
				Cry::Audio::Log(ELogType::Warning, "Unknown AudioSystemData node: %s", szChildNodeTag);
				CRY_ASSERT(false);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::ParseDefaultControlsFile(XmlNodeRef const pRootNode)
{
	int const rootChildCount = pRootNode->getChildCount();

	for (int i = 0; i < rootChildCount; ++i)
	{
		XmlNodeRef const pChildNode(pRootNode->getChild(i));

		if (pChildNode != nullptr)
		{
			char const* const childNodeTag = pChildNode->getTag();

			if (_stricmp(childNodeTag, s_szTriggersNodeTag) == 0)
			{
				ParseDefaultTriggers(pChildNode);
			}
			else if (_stricmp(childNodeTag, s_szParametersNodeTag) == 0)
			{
				ParseDefaultParameters(pChildNode);
			}
			else if ((_stricmp(childNodeTag, s_szSwitchesNodeTag) == 0) ||
			         (_stricmp(childNodeTag, s_szEnvironmentsNodeTag) == 0) ||
			         (_stricmp(childNodeTag, s_szPreloadsNodeTag) == 0) ||
			         (_stricmp(childNodeTag, s_szEditorDataTag) == 0))
			{
				// These tags are valid but ignored here, beacuse no default controls of these type currently exist.
			}
			else
			{
				Cry::Audio::Log(ELogType::Warning, "Unknown AudioSystemData node: %s", childNodeTag);
				CRY_ASSERT(false);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::ParsePreloadsData(char const* const szFolderPath, EDataScope const dataScope)
{
	CryFixedStringT<MaxFilePathLength> rootFolderPath(szFolderPath);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	CTimeValue const startTime(gEnv->pTimer->GetAsyncTime());
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	if (m_pIImpl != nullptr)
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

				XmlNodeRef const pRootNode(GetISystem()->LoadXmlFromFile(fileName));

				if (pRootNode != nullptr)
				{
					if (_stricmp(pRootNode->getTag(), s_szRootNodeTag) == 0)
					{

						uint versionNumber = 1;
						pRootNode->getAttr(s_szVersionAttribute, versionNumber);
						int const numChildren = pRootNode->getChildCount();

						for (int i = 0; i < numChildren; ++i)
						{
							XmlNodeRef const pChildNode(pRootNode->getChild(i));

							if (pChildNode != nullptr)
							{
								char const* const szChildNodeTag = pChildNode->getTag();

								if (_stricmp(szChildNodeTag, s_szPreloadsNodeTag) == 0)
								{
									size_t const lastSlashIndex = rootFolderPath.rfind("/"[0]);

									if (rootFolderPath.npos != lastSlashIndex)
									{
										CryFixedStringT<MaxFilePathLength> const folderName(rootFolderPath.substr(lastSlashIndex + 1, rootFolderPath.size()));
										ParsePreloads(pChildNode, dataScope, folderName.c_str(), versionNumber);
									}
									else
									{
										ParsePreloads(pChildNode, dataScope, nullptr, versionNumber);
									}
								}
								else if (_stricmp(szChildNodeTag, s_szTriggersNodeTag) == 0 ||
								         _stricmp(szChildNodeTag, s_szParametersNodeTag) == 0 ||
								         _stricmp(szChildNodeTag, s_szSwitchesNodeTag) == 0 ||
								         _stricmp(szChildNodeTag, s_szEnvironmentsNodeTag) == 0 ||
								         _stricmp(szChildNodeTag, s_szEditorDataTag) == 0)
								{
									// These tags are valid but ignored here.
								}
								else
								{
									Cry::Audio::Log(ELogType::Warning, "Unknown AudioSystemData node: %s", szChildNodeTag);
								}
							}
						}
					}
				}
			}
			while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

			gEnv->pCryPak->FindClose(handle);
		}
	}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	char const* szDataScope = "unknown";

	switch (dataScope)
	{
	case EDataScope::Global:
		{
			szDataScope = "Global";

			break;
		}
	case EDataScope::LevelSpecific:
		{
			szDataScope = "Level Specific";

			break;
		}
	}

	float const duration = (gEnv->pTimer->GetAsyncTime() - startTime).GetMilliSeconds();
	Cry::Audio::Log(ELogType::Comment, R"(Parsed preloads data in "%s" for data scope "%s" in %.3f ms!)", szFolderPath, szDataScope, duration);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::ClearControlsData(EDataScope const dataScope)
{
	if (m_pIImpl != nullptr)
	{
		AudioTriggerLookup::iterator iterTriggers(m_triggers.begin());
		AudioTriggerLookup::const_iterator iterTriggersEnd(m_triggers.end());

		while (iterTriggers != iterTriggersEnd)
		{
			CATLTrigger const* const pTrigger = iterTriggers->second;

			if ((pTrigger->GetDataScope() == dataScope) || dataScope == EDataScope::All)
			{
				DeleteAudioTrigger(pTrigger);
				iterTriggers = m_triggers.erase(iterTriggers);
				iterTriggersEnd = m_triggers.end();
				continue;
			}

			++iterTriggers;
		}

		AudioParameterLookup::iterator iterParameters(m_parameters.begin());
		AudioParameterLookup::const_iterator iterParametersEnd(m_parameters.end());

		while (iterParameters != iterParametersEnd)
		{
			CParameter const* const pParameter = iterParameters->second;

			if ((pParameter->GetDataScope() == dataScope) || dataScope == EDataScope::All)
			{
				DeleteAudioParameter(pParameter);
				iterParameters = m_parameters.erase(iterParameters);
				iterParametersEnd = m_parameters.end();
				continue;
			}

			++iterParameters;
		}

		AudioSwitchLookup::iterator iterSwitches(m_switches.begin());
		AudioSwitchLookup::const_iterator iterSwitchesEnd(m_switches.end());

		while (iterSwitches != iterSwitchesEnd)
		{
			CATLSwitch const* const pSwitch = iterSwitches->second;

			if ((pSwitch->GetDataScope() == dataScope) || dataScope == EDataScope::All)
			{
				DeleteAudioSwitch(pSwitch);
				iterSwitches = m_switches.erase(iterSwitches);
				iterSwitchesEnd = m_switches.end();
				continue;
			}

			++iterSwitches;
		}

		AudioEnvironmentLookup::iterator iterEnvironments(m_environments.begin());
		AudioEnvironmentLookup::const_iterator iterEnvironmentsEnd(m_environments.end());

		while (iterEnvironments != iterEnvironmentsEnd)
		{
			CATLAudioEnvironment const* const pEnvironment = iterEnvironments->second;

			if ((pEnvironment->GetDataScope() == dataScope) || dataScope == EDataScope::All)
			{
				DeleteAudioEnvironment(pEnvironment);
				iterEnvironments = m_environments.erase(iterEnvironments);
				iterEnvironmentsEnd = m_environments.end();
				continue;
			}

			++iterEnvironments;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::ParsePreloads(XmlNodeRef const pPreloadDataRoot, EDataScope const dataScope, char const* const szFolderName, uint const version)
{
	LOADING_TIME_PROFILE_SECTION;

	int const numPreloadRequests = pPreloadDataRoot->getChildCount();

	for (int i = 0; i < numPreloadRequests; ++i)
	{
		XmlNodeRef const pPreloadRequestNode(pPreloadDataRoot->getChild(i));

		if (pPreloadRequestNode && _stricmp(pPreloadRequestNode->getTag(), s_szPreloadRequestTag) == 0)
		{
			PreloadRequestId preloadRequestId = GlobalPreloadRequestId;
			char const* szPreloadRequestName = s_szGlobalPreloadRequestName;
			bool const isAutoLoad = (_stricmp(pPreloadRequestNode->getAttr(s_szTypeAttribute), s_szDataLoadType) == 0);

			if (!isAutoLoad)
			{
				szPreloadRequestName = pPreloadRequestNode->getAttr(s_szNameAttribute);
				preloadRequestId = static_cast<PreloadRequestId>(StringToId(szPreloadRequestName));
			}
			else if (dataScope == EDataScope::LevelSpecific)
			{
				szPreloadRequestName = szFolderName;
				preloadRequestId = static_cast<PreloadRequestId>(StringToId(szPreloadRequestName));
			}

			if (preloadRequestId != InvalidPreloadRequestId)
			{
				XmlNodeRef pFileListParentNode = nullptr;
				int const platformCount = pPreloadRequestNode->getChildCount();

				for (int j = 0; j < platformCount; ++j)
				{
					XmlNodeRef const pPlatformNode(pPreloadRequestNode->getChild(j));

					if ((pPlatformNode != nullptr) && (_stricmp(pPlatformNode->getAttr(s_szNameAttribute), SATLXMLTags::szPlatform) == 0))
					{
						pFileListParentNode = pPlatformNode;
						break;
					}
				}

				if (pFileListParentNode != nullptr)
				{
					// Found the config group corresponding to the specified platform.
					int const fileCount = pFileListParentNode->getChildCount();

					CATLPreloadRequest::FileEntryIds cFileEntryIDs;
					cFileEntryIDs.reserve(fileCount);

					for (int k = 0; k < fileCount; ++k)
					{
						FileEntryId const id = m_fileCacheMgr.TryAddFileCacheEntry(pFileListParentNode->getChild(k), dataScope, isAutoLoad);

						if (id != InvalidFileEntryId)
						{
							cFileEntryIDs.push_back(id);
						}
						else
						{
							Cry::Audio::Log(ELogType::Warning, R"(Preload request "%s" could not create file entry from tag "%s"!)", szPreloadRequestName, pFileListParentNode->getChild(k)->getTag());
						}
					}

					CATLPreloadRequest* pPreloadRequest = stl::find_in_map(m_preloadRequests, preloadRequestId, nullptr);

					if (pPreloadRequest == nullptr)
					{
						pPreloadRequest = new CATLPreloadRequest(preloadRequestId, dataScope, isAutoLoad, cFileEntryIDs);

						if (pPreloadRequest != nullptr)
						{
							m_preloadRequests[preloadRequestId] = pPreloadRequest;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
							pPreloadRequest->m_name = szPreloadRequestName;
#endif          // INCLUDE_AUDIO_PRODUCTION_CODE
						}
						else
						{
							CryFatalError("<Audio>: Failed to allocate CATLPreloadRequest");
						}
					}
					else
					{
						// Add to existing preload request.
						pPreloadRequest->m_fileEntryIds.insert(pPreloadRequest->m_fileEntryIds.end(), cFileEntryIDs.begin(), cFileEntryIDs.end());
					}
				}
			}
			else
			{
				Cry::Audio::Log(ELogType::Error, R"(Preload request "%s" already exists! Skipping this entry!)", szPreloadRequestName);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::ClearPreloadsData(EDataScope const dataScope)
{
	if (m_pIImpl != nullptr)
	{
		AudioPreloadRequestLookup::iterator iRemover = m_preloadRequests.begin();
		AudioPreloadRequestLookup::const_iterator const iEnd = m_preloadRequests.end();

		while (iRemover != iEnd)
		{
			CATLPreloadRequest const* const pRequest = iRemover->second;

			if ((pRequest->GetDataScope() == dataScope) || dataScope == EDataScope::All)
			{
				DeleteAudioPreloadRequest(pRequest);
				m_preloadRequests.erase(iRemover++);
			}
			else
			{
				++iRemover;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::ParseEnvironments(XmlNodeRef const pAudioEnvironmentRoot, EDataScope const dataScope)
{
	int const numEnvironments = pAudioEnvironmentRoot->getChildCount();

	for (int i = 0; i < numEnvironments; ++i)
	{
		XmlNodeRef const pEnvironmentNode(pAudioEnvironmentRoot->getChild(i));

		if ((pEnvironmentNode != nullptr) && (_stricmp(pEnvironmentNode->getTag(), s_szEnvironmentTag) == 0))
		{
			char const* const szEnvironmentName = pEnvironmentNode->getAttr(s_szNameAttribute);
			EnvironmentId const environmentId = static_cast<EnvironmentId const>(StringToId(szEnvironmentName));

			if ((environmentId != InvalidControlId) && (stl::find_in_map(m_environments, environmentId, nullptr) == nullptr))
			{
				//there is no entry yet with this ID in the container
				int const numConnections = pEnvironmentNode->getChildCount();

				CATLAudioEnvironment::ImplPtrVec connections;
				connections.reserve(numConnections);

				for (int j = 0; j < numConnections; ++j)
				{
					XmlNodeRef const pEnvironmentImplNode(pEnvironmentNode->getChild(j));

					if (pEnvironmentImplNode != nullptr)
					{
						Impl::IEnvironment const* const pIEnvironment = m_pIImpl->ConstructEnvironment(pEnvironmentImplNode);

						if (pIEnvironment != nullptr)
						{
							CATLEnvironmentImpl const* const pEnvirnomentImpl = new CATLEnvironmentImpl(pIEnvironment);
							connections.push_back(pEnvirnomentImpl);
						}
					}
				}

				connections.shrink_to_fit();

				if (!connections.empty())
				{
					CATLAudioEnvironment* const pNewEnvironment = new CATLAudioEnvironment(environmentId, dataScope, connections);

					if (pNewEnvironment != nullptr)
					{
						m_environments[environmentId] = pNewEnvironment;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
						pNewEnvironment->m_name = szEnvironmentName;
#endif        // INCLUDE_AUDIO_PRODUCTION_CODE
					}
				}
			}
			else
			{
				Cry::Audio::Log(ELogType::Error, R"(Environment "%s" already exists!)", szEnvironmentName);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::ParseTriggers(XmlNodeRef const pXMLTriggerRoot, EDataScope const dataScope)
{
	int const numTriggers = pXMLTriggerRoot->getChildCount();

	for (int i = 0; i < numTriggers; ++i)
	{
		XmlNodeRef const pTriggerNode(pXMLTriggerRoot->getChild(i));

		if (pTriggerNode && _stricmp(pTriggerNode->getTag(), s_szTriggerTag) == 0)
		{
			char const* const szTriggerName = pTriggerNode->getAttr(s_szNameAttribute);
			ControlId const triggerId = static_cast<ControlId const>(StringToId(szTriggerName));

			if ((triggerId != InvalidControlId) && (stl::find_in_map(m_triggers, triggerId, nullptr) == nullptr))
			{
				int const numConnections = pTriggerNode->getChildCount();
				CATLTrigger::ImplPtrVec connections;
				connections.reserve(numConnections);

				float maxRadius = 0.0f;
				pTriggerNode->getAttr(s_szRadiusAttribute, maxRadius);

				for (int m = 0; m < numConnections; ++m)
				{
					XmlNodeRef const pTriggerImplNode(pTriggerNode->getChild(m));

					if (pTriggerImplNode)
					{
						Impl::ITrigger const* const pITrigger = m_pIImpl->ConstructTrigger(pTriggerImplNode);

						if (pITrigger != nullptr)
						{
							CATLTriggerImpl const* const pTriggerImpl = new CATLTriggerImpl(++m_triggerImplIdCounter, pITrigger);
							connections.push_back(pTriggerImpl);
						}
					}
				}

				connections.shrink_to_fit();

				CATLTrigger* const pNewTrigger = new CATLTrigger(triggerId, dataScope, connections, maxRadius);

				if (pNewTrigger != nullptr)
				{
					m_triggers[triggerId] = pNewTrigger;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
					pNewTrigger->m_name = szTriggerName;
#endif      // INCLUDE_AUDIO_PRODUCTION_CODE
				}
			}
			else
			{
				Cry::Audio::Log(ELogType::Error, R"(Trigger "%s" already exists!)", szTriggerName);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::ParseDefaultTriggers(XmlNodeRef const pXMLTriggerRoot)
{
	int const numTriggers = pXMLTriggerRoot->getChildCount();

	for (int i = 0; i < numTriggers; ++i)
	{
		XmlNodeRef const pTriggerNode(pXMLTriggerRoot->getChild(i));

		if (pTriggerNode && _stricmp(pTriggerNode->getTag(), s_szTriggerTag) == 0)
		{
			char const* const szTriggerName = pTriggerNode->getAttr(s_szNameAttribute);
			ControlId const triggerId = static_cast<ControlId const>(StringToId(szTriggerName));

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			if (stl::find_in_map(m_triggers, triggerId, nullptr) != nullptr)
			{
				Cry::Audio::Log(ELogType::Error, R"(Trigger "%s" already exists!)", szTriggerName);
			}
#endif      // INCLUDE_AUDIO_PRODUCTION_CODE

			int const numConnections = pTriggerNode->getChildCount();
			CATLTrigger::ImplPtrVec connections;

			if (numConnections == 0)
			{
				CATLTriggerImpl const* const pDefaultTrigger = stl::find_in_map(m_internalControls.m_triggerConnections, triggerId, nullptr);

				if (pDefaultTrigger != nullptr)
				{
					connections.push_back(pDefaultTrigger);
				}
				else
				{
					CRY_ASSERT_MESSAGE(false, R"(Default trigger "%s" does not exist.)", szTriggerName);
				}
			}
			else
			{
				connections.reserve(numConnections);

				for (int m = 0; m < numConnections; ++m)
				{
					XmlNodeRef const pConnectionNode(pTriggerNode->getChild(m));

					if (pConnectionNode != nullptr)
					{
						Impl::ITrigger const* const pITrigger = m_pIImpl->ConstructTrigger(pConnectionNode);

						if (pITrigger != nullptr)
						{
							CATLTriggerImpl const* const pTriggerImpl = new CATLTriggerImpl(++m_triggerImplIdCounter, pITrigger);
							connections.push_back(pTriggerImpl);
						}
					}
				}
			}

			connections.shrink_to_fit();

			CATLTrigger* const pNewTrigger = new CATLTrigger(triggerId, EDataScope::Global, connections, 0.0f);

			if (pNewTrigger != nullptr)
			{
				m_triggers[triggerId] = pNewTrigger;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
				pNewTrigger->m_name = szTriggerName;
#endif      // INCLUDE_AUDIO_PRODUCTION_CODE
			}
			else
			{
				CRY_ASSERT_MESSAGE(false, R"(Trigger "%s" could not get constructed.)", szTriggerName);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::ParseSwitches(XmlNodeRef const pXMLSwitchRoot, EDataScope const dataScope)
{
	int const numSwitches = pXMLSwitchRoot->getChildCount();

	for (int i = 0; i < numSwitches; ++i)
	{
		XmlNodeRef const pSwitchNode(pXMLSwitchRoot->getChild(i));

		if (pSwitchNode && _stricmp(pSwitchNode->getTag(), s_szSwitchTag) == 0)
		{
			char const* const szSwitchName = pSwitchNode->getAttr(s_szNameAttribute);
			ControlId const switchId = static_cast<ControlId const>(StringToId(szSwitchName));

			if ((switchId != InvalidControlId) && (stl::find_in_map(m_switches, switchId, nullptr) == nullptr))
			{
				CATLSwitch* const pNewSwitch = new CATLSwitch(switchId, dataScope);
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
				pNewSwitch->m_name = szSwitchName;
#endif    // INCLUDE_AUDIO_PRODUCTION_CODE

				int const numStates = pSwitchNode->getChildCount();

				for (int j = 0; j < numStates; ++j)
				{
					XmlNodeRef const pSwitchStateNode(pSwitchNode->getChild(j));

					if (pSwitchStateNode && _stricmp(pSwitchStateNode->getTag(), s_szStateTag) == 0)
					{
						char const* const szSwitchStateName = pSwitchStateNode->getAttr(s_szNameAttribute);
						SwitchStateId const switchStateId = static_cast<SwitchStateId const>(StringToId(szSwitchStateName));

						if (switchStateId != InvalidSwitchStateId)
						{
							int const numConnections = pSwitchStateNode->getChildCount();
							CATLSwitchState::ImplPtrVec connections;
							connections.reserve(numConnections);

							for (int k = 0; k < numConnections; ++k)
							{
								XmlNodeRef const pStateImplNode(pSwitchStateNode->getChild(k));

								if (pStateImplNode != nullptr)
								{
									Impl::ISwitchState const* const pISwitchState = m_pIImpl->ConstructSwitchState(pStateImplNode);

									if (pISwitchState != nullptr)
									{
										// Only add the connection if the middleware recognizes the control
										CExternalAudioSwitchStateImpl const* const pExternalSwitchStateImpl = new CExternalAudioSwitchStateImpl(pISwitchState);
										connections.push_back(pExternalSwitchStateImpl);
									}
								}
							}

							CATLSwitchState* const pNewState = new CATLSwitchState(switchId, switchStateId, connections);
							pNewSwitch->audioSwitchStates[switchStateId] = pNewState;
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
							pNewState->m_name = szSwitchStateName;
#endif          // INCLUDE_AUDIO_PRODUCTION_CODE
						}
					}
				}

				m_switches[switchId] = pNewSwitch;
			}
			else
			{
				Cry::Audio::Log(ELogType::Error, R"(Switch "%s" already exists!)", szSwitchName);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::ParseParameters(XmlNodeRef const pXMLParameterRoot, EDataScope const dataScope)
{
	int const numParameters = pXMLParameterRoot->getChildCount();

	for (int i = 0; i < numParameters; ++i)
	{
		XmlNodeRef const pParameterNode(pXMLParameterRoot->getChild(i));

		if (pParameterNode && _stricmp(pParameterNode->getTag(), s_szParameterTag) == 0)
		{
			char const* const szParameterName = pParameterNode->getAttr(s_szNameAttribute);
			ControlId const parameterId = static_cast<ControlId const>(StringToId(szParameterName));

			if ((parameterId != InvalidControlId) && (stl::find_in_map(m_parameters, parameterId, nullptr) == nullptr))
			{
				int const numConnections = pParameterNode->getChildCount();
				CParameter::ImplPtrVec connections;
				connections.reserve(numConnections);

				for (int j = 0; j < numConnections; ++j)
				{
					XmlNodeRef const pParameterImplNode(pParameterNode->getChild(j));

					if (pParameterImplNode != nullptr)
					{
						Impl::IParameter const* const pExternalParameterImpl = m_pIImpl->ConstructParameter(pParameterImplNode);

						if (pExternalParameterImpl != nullptr)
						{
							CParameterImpl const* const pParameterImpl = new CParameterImpl(pExternalParameterImpl);
							connections.push_back(pParameterImpl);
						}
					}
				}

				connections.shrink_to_fit();

				CParameter* const pParameter = new CParameter(parameterId, dataScope, connections);

				if (pParameter != nullptr)
				{
					m_parameters[parameterId] = pParameter;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
					pParameter->m_name = szParameterName;
#endif      // INCLUDE_AUDIO_PRODUCTION_CODE
				}
			}
			else
			{
				Cry::Audio::Log(ELogType::Error, R"(Parameter "%s" already exists!)", szParameterName);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::ParseDefaultParameters(XmlNodeRef const pXMLParameterRoot)
{
	int const numParameters = pXMLParameterRoot->getChildCount();

	for (int i = 0; i < numParameters; ++i)
	{
		XmlNodeRef const pParameterNode(pXMLParameterRoot->getChild(i));

		if (pParameterNode && _stricmp(pParameterNode->getTag(), s_szParameterTag) == 0)
		{
			char const* const szParameterName = pParameterNode->getAttr(s_szNameAttribute);
			ControlId const parameterId = static_cast<ControlId const>(StringToId(szParameterName));

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			if (stl::find_in_map(m_parameters, parameterId, nullptr) != nullptr)
			{
				Cry::Audio::Log(ELogType::Error, R"(Parameter "%s" already exists!)", szParameterName);
			}
#endif      // INCLUDE_AUDIO_PRODUCTION_CODE

			int const numConnections = pParameterNode->getChildCount();
			CParameter::ImplPtrVec connections;

			if (numConnections == 0)
			{
				IParameterImpl const* const pDefaultParamater = stl::find_in_map(m_internalControls.m_parameterConnections, parameterId, nullptr);

				if (pDefaultParamater != nullptr)
				{
					connections.push_back(pDefaultParamater);
				}
				else
				{
					CRY_ASSERT_MESSAGE(false, R"(Default Parameter "%s" does not exist.)", szParameterName);
				}
			}
			else
			{
				connections.reserve(numConnections);

				for (int j = 0; j < numConnections; ++j)
				{
					XmlNodeRef const pParameterImplNode(pParameterNode->getChild(j));

					if (pParameterImplNode != nullptr)
					{
						Impl::IParameter const* const pExternalParameterImpl = m_pIImpl->ConstructParameter(pParameterImplNode);

						if (pExternalParameterImpl != nullptr)
						{
							CParameterImpl const* const pParameterImpl = new CParameterImpl(pExternalParameterImpl);
							connections.push_back(pParameterImpl);
						}
					}
				}
			}

			connections.shrink_to_fit();

			CParameter* const pParameter = new CParameter(parameterId, EDataScope::Global, connections);

			if (pParameter != nullptr)
			{
				m_parameters[parameterId] = pParameter;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
				pParameter->m_name = szParameterName;
#endif      // INCLUDE_AUDIO_PRODUCTION_CODE
			}
			else
			{
				Cry::Audio::Log(ELogType::Error, R"(Parameter "%s" could not get constructed!)", szParameterName);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::DeleteAudioTrigger(CATLTrigger const* const pTrigger)
{
	if (pTrigger != nullptr)
	{
		for (auto const pTriggerImpl : pTrigger->m_implPtrs)
		{
			delete pTriggerImpl;
		}

		delete pTrigger;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::DeleteAudioParameter(CParameter const* const pParameter)
{
	if (pParameter != nullptr)
	{
		for (auto const pParameterImpl : pParameter->m_implPtrs)
		{
			delete pParameterImpl;
		}

		delete pParameter;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::DeleteAudioSwitch(CATLSwitch const* const pSwitch)
{
	if (pSwitch != nullptr)
	{
		for (auto const& statePair : pSwitch->audioSwitchStates)
		{
			CATLSwitchState const* const pSwitchState = statePair.second;

			if (pSwitchState != nullptr)
			{
				for (auto const pStateImpl : pSwitchState->m_implPtrs)
				{
					delete pStateImpl;
				}

				delete pSwitchState;
			}
		}

		delete pSwitch;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::DeleteAudioPreloadRequest(CATLPreloadRequest const* const pPreloadRequest)
{
	if (pPreloadRequest != nullptr)
	{
		EDataScope const dataScope = pPreloadRequest->GetDataScope();

		for (auto const fileId : pPreloadRequest->m_fileEntryIds)
		{
			m_fileCacheMgr.TryRemoveFileCacheEntry(fileId, dataScope);
		}

		delete pPreloadRequest;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::DeleteAudioEnvironment(CATLAudioEnvironment const* const pEnvironment)
{
	if (pEnvironment != nullptr)
	{
		for (auto const pEnvImpl : pEnvironment->m_implPtrs)
		{
			delete pEnvImpl;
		}

		delete pEnvironment;
	}
}
} // namespace CryAudio
