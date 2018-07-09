// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "FileCacheManager.h"

namespace CryAudio
{
struct SInternalControls;

class CAudioXMLProcessor final
{
public:

	explicit CAudioXMLProcessor(
	  AudioTriggerLookup& triggers,
	  AudioParameterLookup& parameters,
	  AudioSwitchLookup& switches,
	  AudioEnvironmentLookup& environments,
	  AudioPreloadRequestLookup& preloadRequests,
	  CFileCacheManager& fileCacheMgr,
	  SInternalControls const& internalControls);

	CAudioXMLProcessor(CAudioXMLProcessor const&) = delete;
	CAudioXMLProcessor(CAudioXMLProcessor&&) = delete;
	CAudioXMLProcessor& operator=(CAudioXMLProcessor const&) = delete;
	CAudioXMLProcessor& operator=(CAudioXMLProcessor&&) = delete;

	void                Init(Impl::IImpl* const pIImpl);
	void                Release();

	void                ParseControlsData(char const* const szFolderPath, EDataScope const dataScope);
	void                ClearControlsData(EDataScope const dataScope);
	void                ParsePreloadsData(char const* const szFolderPath, EDataScope const dataScope);
	void                ClearPreloadsData(EDataScope const dataScope);

	void                ParseControlsFile(XmlNodeRef const pRootNode, EDataScope const dataScope);
	void                ParseDefaultControlsFile(XmlNodeRef const pRootNode);

private:

	void ParseTriggers(XmlNodeRef const pXMLTriggerRoot, EDataScope const dataScope);
	void ParseDefaultTriggers(XmlNodeRef const pXMLTriggerRoot);
	void ParseSwitches(XmlNodeRef const pXMLSwitchRoot, EDataScope const dataScope);
	void ParseParameters(XmlNodeRef const pXMLParameterRoot, EDataScope const dataScope);
	void ParseDefaultParameters(XmlNodeRef const pXMLParameterRoot);
	void ParsePreloads(XmlNodeRef const pPreloadDataRoot, EDataScope const dataScope, char const* const szFolderName, uint const version);
	void ParseEnvironments(XmlNodeRef const pAudioEnvironmentRoot, EDataScope const dataScope);

	void DeleteAudioTrigger(CATLTrigger const* const pTrigger);
	void DeleteAudioParameter(CParameter const* const pParameter);
	void DeleteAudioSwitch(CATLSwitch const* const pSwitch);
	void DeleteAudioPreloadRequest(CATLPreloadRequest const* const pPreloadRequest);
	void DeleteAudioEnvironment(CATLAudioEnvironment const* const pEnvironment);

	AudioTriggerLookup&        m_triggers;
	AudioParameterLookup&      m_parameters;
	AudioSwitchLookup&         m_switches;
	AudioEnvironmentLookup&    m_environments;
	AudioPreloadRequestLookup& m_preloadRequests;
	TriggerImplId              m_triggerImplIdCounter;
	CFileCacheManager&         m_fileCacheMgr;
	SInternalControls const&   m_internalControls;
	Impl::IImpl*               m_pIImpl;
};
} // namespace CryAudio
