// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioXMLProcessor.h"
#include "InternalEntities.h"
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

				XmlNodeRef const pATLConfigRoot(GetISystem()->LoadXmlFromFile(sFileName));

				if (pATLConfigRoot)
				{
					if (_stricmp(pATLConfigRoot->getTag(), SATLXMLTags::szRootNodeTag) == 0)
					{
						size_t const nATLConfigChildrenCount = static_cast<size_t>(pATLConfigRoot->getChildCount());

						for (size_t i = 0; i < nATLConfigChildrenCount; ++i)
						{
							XmlNodeRef const pAudioConfigNode(pATLConfigRoot->getChild(i));

							if (pAudioConfigNode)
							{
								char const* const sAudioConfigNodeTag = pAudioConfigNode->getTag();

								if (_stricmp(sAudioConfigNodeTag, SATLXMLTags::szTriggersNodeTag) == 0)
								{
									ParseAudioTriggers(pAudioConfigNode, dataScope);
								}
								else if (_stricmp(sAudioConfigNodeTag, SATLXMLTags::szParametersNodeTag) == 0)
								{
									ParseAudioParameters(pAudioConfigNode, dataScope);
								}
								else if (_stricmp(sAudioConfigNodeTag, SATLXMLTags::szSwitchesNodeTag) == 0)
								{
									ParseAudioSwitches(pAudioConfigNode, dataScope);
								}
								else if (_stricmp(sAudioConfigNodeTag, SATLXMLTags::szEnvironmentsNodeTag) == 0)
								{
									ParseAudioEnvironments(pAudioConfigNode, dataScope);
								}
								else if (_stricmp(sAudioConfigNodeTag, SATLXMLTags::szPreloadsNodeTag) == 0 ||
								         _stricmp(sAudioConfigNodeTag, SATLXMLTags::szEditorDataTag) == 0)
								{
									// This tag is valid but ignored here.
								}
								else
								{
									g_logger.Log(ELogType::Warning, "Unknown AudioConfig node: %s", sAudioConfigNodeTag);
									CRY_ASSERT(false);
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
	g_logger.Log(ELogType::Warning, R"(Parsed controls data in "%s" for data scope "%s" in %.3f ms!)", szFolderPath, szDataScope, duration);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
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
		CryFixedStringT<MaxFilePathLength + MaxFileNameLength> search(rootFolderPath + CRY_NATIVE_PATH_SEPSTR "*.xml");
		_finddata_t fd;
		intptr_t const handle = gEnv->pCryPak->FindFirst(search.c_str(), &fd);

		if (handle != -1)
		{
			CryFixedStringT<MaxFilePathLength + MaxFileNameLength> fileName;

			do
			{
				fileName = rootFolderPath.c_str();
				fileName += CRY_NATIVE_PATH_SEPSTR;
				fileName += fd.name;

				XmlNodeRef const pATLConfigRoot(GetISystem()->LoadXmlFromFile(fileName));

				if (pATLConfigRoot)
				{
					if (_stricmp(pATLConfigRoot->getTag(), SATLXMLTags::szRootNodeTag) == 0)
					{

						uint versionNumber = 1;
						pATLConfigRoot->getAttr(SATLXMLTags::szATLVersionAttribute, versionNumber);
						size_t const numChildren = static_cast<size_t>(pATLConfigRoot->getChildCount());

						for (size_t i = 0; i < numChildren; ++i)
						{
							XmlNodeRef const pAudioConfigNode(pATLConfigRoot->getChild(i));

							if (pAudioConfigNode)
							{
								char const* const szAudioConfigNodeTag = pAudioConfigNode->getTag();

								if (_stricmp(szAudioConfigNodeTag, SATLXMLTags::szPreloadsNodeTag) == 0)
								{
									size_t const lastSlashIndex = rootFolderPath.rfind(CRY_NATIVE_PATH_SEPSTR[0]);

									if (rootFolderPath.npos != lastSlashIndex)
									{
										CryFixedStringT<MaxFilePathLength> const folderName(rootFolderPath.substr(lastSlashIndex + 1, rootFolderPath.size()));
										ParseAudioPreloads(pAudioConfigNode, dataScope, folderName.c_str(), versionNumber);
									}
									else
									{
										ParseAudioPreloads(pAudioConfigNode, dataScope, nullptr, versionNumber);
									}
								}
								else if (_stricmp(szAudioConfigNodeTag, SATLXMLTags::szTriggersNodeTag) == 0 ||
								         _stricmp(szAudioConfigNodeTag, SATLXMLTags::szParametersNodeTag) == 0 ||
								         _stricmp(szAudioConfigNodeTag, SATLXMLTags::szSwitchesNodeTag) == 0 ||
								         _stricmp(szAudioConfigNodeTag, SATLXMLTags::szEnvironmentsNodeTag) == 0 ||
								         _stricmp(szAudioConfigNodeTag, SATLXMLTags::szEditorDataTag) == 0)
								{
									// These tags are valid but ignored here.
								}
								else
								{
									g_logger.Log(ELogType::Warning, "Unknown AudioConfig node: %s", szAudioConfigNodeTag);
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
	g_logger.Log(ELogType::Warning, R"(Parsed preloads data in "%s" for data scope "%s" in %.3f ms!)", szFolderPath, szDataScope, duration);
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
void CAudioXMLProcessor::ParseAudioPreloads(XmlNodeRef const pPreloadDataRoot, EDataScope const dataScope, char const* const szFolderName, uint const version)
{
	LOADING_TIME_PROFILE_SECTION;

	size_t const preloadRequestCount = static_cast<size_t>(pPreloadDataRoot->getChildCount());

	for (size_t i = 0; i < preloadRequestCount; ++i)
	{
		XmlNodeRef const pPreloadRequestNode(pPreloadDataRoot->getChild(i));

		if (pPreloadRequestNode && _stricmp(pPreloadRequestNode->getTag(), SATLXMLTags::szATLPreloadRequestTag) == 0)
		{
			PreloadRequestId audioPreloadRequestId = GlobalPreloadRequestId;
			char const* szAudioPreloadRequestName = GlobalPreloadRequestName;
			bool const bAutoLoad = (_stricmp(pPreloadRequestNode->getAttr(SATLXMLTags::szATLTypeAttribute), SATLXMLTags::szATLDataLoadType) == 0);

			if (!bAutoLoad)
			{
				szAudioPreloadRequestName = pPreloadRequestNode->getAttr(SATLXMLTags::szATLNameAttribute);
				audioPreloadRequestId = static_cast<PreloadRequestId>(StringToId_RunTime(szAudioPreloadRequestName));
			}
			else if (dataScope == EDataScope::LevelSpecific)
			{
				szAudioPreloadRequestName = szFolderName;
				audioPreloadRequestId = static_cast<PreloadRequestId>(StringToId_RunTime(szAudioPreloadRequestName));
			}

			if (audioPreloadRequestId != CryAudio::InvalidPreloadRequestId)
			{
				XmlNodeRef pFileListParentNode = nullptr;
				if (version >= 2)
				{
					size_t const platformCount = pPreloadRequestNode->getChildCount();

					for (size_t j = 0; j < platformCount; ++j)
					{
						XmlNodeRef const pPlatformNode(pPreloadRequestNode->getChild(j));

						if (pPlatformNode && _stricmp(pPlatformNode->getAttr(SATLXMLTags::szATLNameAttribute), SATLXMLTags::szPlatform) == 0)
						{
							pFileListParentNode = pPlatformNode;
							break;
						}
					}
				}
				else
				{
					size_t const preloadRequestChidrenCount = static_cast<size_t>(pPreloadRequestNode->getChildCount());

					if (preloadRequestChidrenCount > 1)
					{
						// We need to have at least two children: ATLPlatforms and at least one ATLConfigGroup
						XmlNodeRef const pPlatformsNode(pPreloadRequestNode->getChild(0));
						char const* szATLConfigGroupName = nullptr;

						if (pPlatformsNode && _stricmp(pPlatformsNode->getTag(), SATLXMLTags::szATLPlatformsTag) == 0)
						{
							size_t const platformCount = pPlatformsNode->getChildCount();

							for (size_t j = 0; j < platformCount; ++j)
							{
								XmlNodeRef const pPlatformNode(pPlatformsNode->getChild(j));

								if (pPlatformNode && _stricmp(pPlatformNode->getAttr(SATLXMLTags::szATLNameAttribute), SATLXMLTags::szPlatform) == 0)
								{
									szATLConfigGroupName = pPlatformNode->getAttr(SATLXMLTags::szATLConfigGroupAttribute);
									break;
								}
							}
						}

						if (szATLConfigGroupName != nullptr)
						{
							for (size_t j = 1; j < preloadRequestChidrenCount; ++j)
							{
								XmlNodeRef const pConfigGroupNode(pPreloadRequestNode->getChild(j));
								if (_stricmp(pConfigGroupNode->getAttr(SATLXMLTags::szATLNameAttribute), szATLConfigGroupName) == 0)
								{
									pFileListParentNode = pConfigGroupNode;
									break;
								}
							}
						}
					}
				}

				if (pFileListParentNode)
				{
					// Found the config group corresponding to the specified platform.
					size_t const fileCount = static_cast<size_t>(pFileListParentNode->getChildCount());

					CATLPreloadRequest::FileEntryIds cFileEntryIDs;
					cFileEntryIDs.reserve(fileCount);

					for (size_t k = 0; k < fileCount; ++k)
					{
						FileEntryId const id = m_fileCacheMgr.TryAddFileCacheEntry(pFileListParentNode->getChild(k), dataScope, bAutoLoad);

						if (id != CryAudio::InvalidFileEntryId)
						{
							cFileEntryIDs.push_back(id);
						}
						else
						{
							g_logger.Log(ELogType::Warning, R"(Preload request "%s" could not create file entry from tag "%s"!)", szAudioPreloadRequestName, pFileListParentNode->getChild(k)->getTag());
						}
					}

					CATLPreloadRequest* pPreloadRequest = stl::find_in_map(m_preloadRequests, audioPreloadRequestId, nullptr);

					if (pPreloadRequest == nullptr)
					{
						pPreloadRequest = new CATLPreloadRequest(audioPreloadRequestId, dataScope, bAutoLoad, cFileEntryIDs);

						if (pPreloadRequest != nullptr)
						{
							m_preloadRequests[audioPreloadRequestId] = pPreloadRequest;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
							pPreloadRequest->m_name = szAudioPreloadRequestName;
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
				g_logger.Log(ELogType::Error, R"(Preload request "%s" already exists! Skipping this entry!)", szAudioPreloadRequestName);
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
void CAudioXMLProcessor::ParseAudioEnvironments(XmlNodeRef const pAudioEnvironmentRoot, EDataScope const dataScope)
{
	size_t const numAudioEnvironments = static_cast<size_t>(pAudioEnvironmentRoot->getChildCount());

	for (size_t i = 0; i < numAudioEnvironments; ++i)
	{
		XmlNodeRef const pAudioEnvironmentNode(pAudioEnvironmentRoot->getChild(i));

		if (pAudioEnvironmentNode && _stricmp(pAudioEnvironmentNode->getTag(), SATLXMLTags::szATLEnvironmentTag) == 0)
		{
			char const* const szAudioEnvironmentName = pAudioEnvironmentNode->getAttr(SATLXMLTags::szATLNameAttribute);
			EnvironmentId const audioEnvironmentId = static_cast<EnvironmentId const>(StringToId_RunTime(szAudioEnvironmentName));

			if ((audioEnvironmentId != CryAudio::InvalidControlId) && (stl::find_in_map(m_environments, audioEnvironmentId, nullptr) == nullptr))
			{
				//there is no entry yet with this ID in the container
				size_t const numAudioEnvironmentChildren = static_cast<size_t>(pAudioEnvironmentNode->getChildCount());

				CATLAudioEnvironment::ImplPtrVec implPtrs;
				implPtrs.reserve(numAudioEnvironmentChildren);

				for (size_t j = 0; j < numAudioEnvironmentChildren; ++j)
				{
					XmlNodeRef const pEnvironmentImplNode(pAudioEnvironmentNode->getChild(j));

					if (pEnvironmentImplNode)
					{
						Impl::IEnvironment const* pIEnvironment = nullptr;

						if (_stricmp(pEnvironmentImplNode->getTag(), SATLXMLTags::szATLEnvironmentRequestTag) == 0)
						{
							pIEnvironment = NewInternalAudioEnvironment(pEnvironmentImplNode);
						}
						else
						{
							pIEnvironment = m_pIImpl->ConstructEnvironment(pEnvironmentImplNode);
						}

						if (pIEnvironment != nullptr)
						{
							CATLEnvironmentImpl* pEnvirnomentImpl = new CATLEnvironmentImpl(pIEnvironment);
							implPtrs.push_back(pEnvirnomentImpl);
						}
						else
						{
							g_logger.Log(ELogType::Warning, "Could not parse an Environment Implementation with XML tag %s", pEnvironmentImplNode->getTag());
						}
					}
				}

				implPtrs.shrink_to_fit();

				if (!implPtrs.empty())
				{
					CATLAudioEnvironment* pNewEnvironment = new CATLAudioEnvironment(audioEnvironmentId, dataScope, implPtrs);

					if (pNewEnvironment != nullptr)
					{
						m_environments[audioEnvironmentId] = pNewEnvironment;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
						pNewEnvironment->m_name = szAudioEnvironmentName;
#endif        // INCLUDE_AUDIO_PRODUCTION_CODE
					}
				}
			}
			else
			{
				g_logger.Log(ELogType::Error, R"(AudioEnvironment "%s" already exists!)", szAudioEnvironmentName);
				CRY_ASSERT(false);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::ParseAudioTriggers(XmlNodeRef const pXMLTriggerRoot, EDataScope const dataScope)
{
	size_t const numAudioTriggersChildren = static_cast<size_t>(pXMLTriggerRoot->getChildCount());

	for (size_t i = 0; i < numAudioTriggersChildren; ++i)
	{
		XmlNodeRef const pAudioTriggerNode(pXMLTriggerRoot->getChild(i));

		if (pAudioTriggerNode && _stricmp(pAudioTriggerNode->getTag(), SATLXMLTags::szATLTriggerTag) == 0)
		{
			char const* const szAudioTriggerName = pAudioTriggerNode->getAttr(SATLXMLTags::szATLNameAttribute);
			ControlId const audioTriggerId = static_cast<ControlId const>(StringToId_RunTime(szAudioTriggerName));

			if ((audioTriggerId != CryAudio::InvalidControlId) && (stl::find_in_map(m_triggers, audioTriggerId, nullptr) == nullptr))
			{
				size_t const numAudioTriggerChildren = static_cast<size_t>(pAudioTriggerNode->getChildCount());
				CATLTrigger::ImplPtrVec implPtrs;
				implPtrs.reserve(numAudioTriggerChildren);

				float maxRadius = 0.0f;
				pAudioTriggerNode->getAttr(SATLXMLTags::szATLRadiusAttribute, maxRadius);

				float occlusionFadeOutDistance = 0.0f;
				pAudioTriggerNode->getAttr(SATLXMLTags::szATLOcclusionFadeOutDistanceAttribute, occlusionFadeOutDistance);

				for (size_t m = 0; m < numAudioTriggerChildren; ++m)
				{
					XmlNodeRef const pTriggerImplNode(pAudioTriggerNode->getChild(m));

					if (pTriggerImplNode)
					{
						Impl::ITrigger const* pITrigger = nullptr;

						if (_stricmp(pTriggerImplNode->getTag(), SATLXMLTags::szATLTriggerRequestTag) == 0)
						{
							pITrigger = NewInternalAudioTrigger(pTriggerImplNode);
						}
						else
						{
							pITrigger = m_pIImpl->ConstructTrigger(pTriggerImplNode);
						}

						if (pITrigger != nullptr)
						{
							CATLTriggerImpl* pTriggerImpl = new CATLTriggerImpl(++m_triggerImplIdCounter, pITrigger);
							implPtrs.push_back(pTriggerImpl);
						}
						else
						{
							g_logger.Log(ELogType::Warning, "Could not parse a Trigger Implementation with XML tag %s", pTriggerImplNode->getTag());
						}
					}
				}

				implPtrs.shrink_to_fit();

				CATLTrigger* pNewTrigger = new CATLTrigger(audioTriggerId, dataScope, implPtrs, maxRadius, occlusionFadeOutDistance);

				if (pNewTrigger != nullptr)
				{
					m_triggers[audioTriggerId] = pNewTrigger;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
					pNewTrigger->m_name = szAudioTriggerName;
#endif      // INCLUDE_AUDIO_PRODUCTION_CODE
				}
			}
			else
			{
				g_logger.Log(ELogType::Error, R"(trigger "%s" already exists!)", szAudioTriggerName);
				CRY_ASSERT(false);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::ParseAudioSwitches(XmlNodeRef const pXMLSwitchRoot, EDataScope const dataScope)
{
	size_t const numAudioSwitchChildren = static_cast<size_t>(pXMLSwitchRoot->getChildCount());

	for (size_t i = 0; i < numAudioSwitchChildren; ++i)
	{
		XmlNodeRef const pATLSwitchNode(pXMLSwitchRoot->getChild(i));

		if (pATLSwitchNode && _stricmp(pATLSwitchNode->getTag(), SATLXMLTags::szATLSwitchTag) == 0)
		{
			char const* const szAudioSwitchName = pATLSwitchNode->getAttr(SATLXMLTags::szATLNameAttribute);
			ControlId const audioSwitchId = static_cast<ControlId const>(StringToId_RunTime(szAudioSwitchName));

			if ((audioSwitchId != CryAudio::InvalidControlId) && (stl::find_in_map(m_switches, audioSwitchId, nullptr) == nullptr))
			{
				CATLSwitch* pNewSwitch = new CATLSwitch(audioSwitchId, dataScope);
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
				pNewSwitch->m_name = szAudioSwitchName;
#endif    // INCLUDE_AUDIO_PRODUCTION_CODE

				size_t const numAudioSwitchStates = static_cast<size_t>(pATLSwitchNode->getChildCount());

				for (size_t j = 0; j < numAudioSwitchStates; ++j)
				{
					XmlNodeRef const pATLSwitchStateNode(pATLSwitchNode->getChild(j));

					if (pATLSwitchStateNode && _stricmp(pATLSwitchStateNode->getTag(), SATLXMLTags::szATLSwitchStateTag) == 0)
					{
						char const* const szAudioSwitchStateName = pATLSwitchStateNode->getAttr(SATLXMLTags::szATLNameAttribute);
						SwitchStateId const audioSwitchStateId = static_cast<SwitchStateId const>(StringToId_RunTime(szAudioSwitchStateName));

						if (audioSwitchStateId != CryAudio::InvalidSwitchStateId)
						{
							size_t const numAudioSwitchStateImpl = static_cast<size_t>(pATLSwitchStateNode->getChildCount());

							CATLSwitchState::ImplPtrVec switchStateImplVec;
							switchStateImplVec.reserve(numAudioSwitchStateImpl);

							for (size_t k = 0; k < numAudioSwitchStateImpl; ++k)
							{
								XmlNodeRef const pStateImplNode(pATLSwitchStateNode->getChild(k));

								if (pStateImplNode)
								{
									char const* const szStateImplNodeTag = pStateImplNode->getTag();
									if (_stricmp(szStateImplNodeTag, SATLXMLTags::szATLSwitchRequestTag) == 0)
									{
										IAudioSwitchStateImpl const* pSwitchStateImpl = NewInternalAudioSwitchState(pStateImplNode);
										if (pSwitchStateImpl != nullptr)
										{
											switchStateImplVec.push_back(pSwitchStateImpl);
										}
									}
									else
									{
										Impl::ISwitchState const* const pISwitchState = m_pIImpl->ConstructSwitchState(pStateImplNode);

										if (pISwitchState != nullptr)
										{
											// Only add the connection if the middleware recognizes the control
											CExternalAudioSwitchStateImpl* pExternalSwitchStateImpl = new CExternalAudioSwitchStateImpl(pISwitchState);
											switchStateImplVec.push_back(pExternalSwitchStateImpl);
										}
									}
								}
							}

							CATLSwitchState* pNewState = new CATLSwitchState(audioSwitchId, audioSwitchStateId, switchStateImplVec);
							pNewSwitch->audioSwitchStates[audioSwitchStateId] = pNewState;
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
							pNewState->m_name = szAudioSwitchStateName;
#endif          // INCLUDE_AUDIO_PRODUCTION_CODE
						}
					}
				}

				m_switches[audioSwitchId] = pNewSwitch;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioXMLProcessor::ParseAudioParameters(XmlNodeRef const pXMLParameterRoot, EDataScope const dataScope)
{
	size_t const numAudioParameterChildren = static_cast<size_t>(pXMLParameterRoot->getChildCount());

	for (size_t i = 0; i < numAudioParameterChildren; ++i)
	{
		XmlNodeRef const pAudioParameterNode(pXMLParameterRoot->getChild(i));

		if (pAudioParameterNode && _stricmp(pAudioParameterNode->getTag(), SATLXMLTags::szATLParametersTag) == 0)
		{
			char const* const szAudioParameterName = pAudioParameterNode->getAttr(SATLXMLTags::szATLNameAttribute);
			ControlId const audioParameterId = static_cast<ControlId const>(StringToId_RunTime(szAudioParameterName));

			if ((audioParameterId != CryAudio::InvalidControlId) && (stl::find_in_map(m_parameters, audioParameterId, nullptr) == nullptr))
			{
				size_t const numParameterNodeChildren = static_cast<size_t>(pAudioParameterNode->getChildCount());
				CParameter::ImplPtrVec implPtrs;
				implPtrs.reserve(numParameterNodeChildren);

				for (size_t j = 0; j < numParameterNodeChildren; ++j)
				{
					XmlNodeRef const pParameterImplNode(pAudioParameterNode->getChild(j));

					if (pParameterImplNode)
					{
						char const* const szParameterImplNodeTag = pParameterImplNode->getTag();

						if (_stricmp(szParameterImplNodeTag, SATLXMLTags::szATLParametersRequestTag) == 0)
						{
							IParameterImpl const* pParameterImpl = NewInternalAudioParameter(pParameterImplNode);
							if (pParameterImpl != nullptr)
							{
								implPtrs.push_back(pParameterImpl);
							}
						}
						else
						{
							Impl::IParameter const* pExternalParameterImpl = m_pIImpl->ConstructParameter(pParameterImplNode);

							if (pExternalParameterImpl != nullptr)
							{
								CParameterImpl* pParameterImpl = new CParameterImpl(pExternalParameterImpl);
								implPtrs.push_back(pParameterImpl);
							}
						}
					}
				}

				implPtrs.shrink_to_fit();

				CParameter* pParameter = new CParameter(audioParameterId, dataScope, implPtrs);

				if (pParameter != nullptr)
				{
					m_parameters[audioParameterId] = pParameter;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
					pParameter->m_name = szAudioParameterName;
#endif      // INCLUDE_AUDIO_PRODUCTION_CODE
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
Impl::ITrigger const* CAudioXMLProcessor::NewInternalAudioTrigger(XmlNodeRef const pXMLTriggerRoot)
{
	//TODO: implement
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
IParameterImpl const* CAudioXMLProcessor::NewInternalAudioParameter(XmlNodeRef const pXMLParameterRoot)
{
	//TODO: implement
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
IAudioSwitchStateImpl const* CAudioXMLProcessor::NewInternalAudioSwitchState(XmlNodeRef const pXMLSwitchRoot)
{
	IAudioSwitchStateImpl const* pSwitchStateImpl = nullptr;

	char const* const szInternalSwitchNodeName = pXMLSwitchRoot->getAttr(SATLXMLTags::szATLNameAttribute);

	if ((szInternalSwitchNodeName != nullptr) && (szInternalSwitchNodeName[0] != 0) && (pXMLSwitchRoot->getChildCount() == 1))
	{
		XmlNodeRef const pValueNode(pXMLSwitchRoot->getChild(0));

		if (pValueNode && _stricmp(pValueNode->getTag(), SATLXMLTags::szATLValueTag) == 0)
		{
			char const* const szInternalSwitchStateName = pValueNode->getAttr(SATLXMLTags::szATLNameAttribute);

			if ((szInternalSwitchStateName != nullptr) && (szInternalSwitchStateName[0] != 0))
			{
				ControlId const switchId = static_cast<ControlId>(StringToId_RunTime(szInternalSwitchNodeName));
				SwitchStateId const stateId = static_cast<SwitchStateId>(StringToId_RunTime(szInternalSwitchStateName));
				pSwitchStateImpl = stl::find_in_map(m_internalControls.m_switchStates, std::make_pair(switchId, stateId), nullptr);
			}
		}
	}
	else
	{
		g_logger.Log(
		  ELogType::Warning,
		  "An ATLSwitchRequest %s inside ATLSwitchState needs to have exactly one ATLValue.",
		  szInternalSwitchNodeName);
	}

	return pSwitchStateImpl;
}

//////////////////////////////////////////////////////////////////////////
Impl::IEnvironment const* CAudioXMLProcessor::NewInternalAudioEnvironment(XmlNodeRef const pXMLEnvironmentRoot)
{
	// TODO: implement
	return nullptr;
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
