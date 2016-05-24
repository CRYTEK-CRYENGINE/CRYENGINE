// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ATLEntities.h"

#if CRY_PLATFORM_WINDOWS
char const* const SATLXMLTags::szPlatform = "pc";
#elif CRY_PLATFORM_DURANGO
char const* const SATLXMLTags::szPlatform = "xboxone";
#elif CRY_PLATFORM_ORBIS
char const* const SATLXMLTags::szPlatform = "ps4";
#elif CRY_PLATFORM_MAC
char const* const SATLXMLTags::szPlatform = "mac";
#elif CRY_PLATFORM_LINUX
char const* const SATLXMLTags::szPlatform = "linux";
#elif CRY_PLATFORM_IOS
char const* const SATLXMLTags::szPlatform = "ios";
#elif CRY_PLATFORM_ANDROID
char const* const SATLXMLTags::szPlatform = "linux";
#else
	#error "Undefined platform."
#endif

char const* const SATLXMLTags::szRootNodeTag = "ATLConfig";
char const* const SATLXMLTags::szEditorDataTag = "EditorData";
char const* const SATLXMLTags::szTriggersNodeTag = "AudioTriggers";
char const* const SATLXMLTags::szRtpcsNodeTag = "AudioRtpcs";
char const* const SATLXMLTags::szSwitchesNodeTag = "AudioSwitches";
char const* const SATLXMLTags::szPreloadsNodeTag = "AudioPreloads";
char const* const SATLXMLTags::szEnvironmentsNodeTag = "AudioEnvironments";

char const* const SATLXMLTags::szATLTriggerTag = "ATLTrigger";
char const* const SATLXMLTags::szATLSwitchTag = "ATLSwitch";
char const* const SATLXMLTags::szATLRtpcTag = "ATLRtpc";
char const* const SATLXMLTags::szATLSwitchStateTag = "ATLSwitchState";
char const* const SATLXMLTags::szATLEnvironmentTag = "ATLEnvironment";
char const* const SATLXMLTags::szATLPlatformsTag = "ATLPlatforms";
char const* const SATLXMLTags::szATLConfigGroupTag = "ATLConfigGroup";

char const* const SATLXMLTags::szATLTriggerRequestTag = "ATLTriggerRequest";
char const* const SATLXMLTags::szATLSwitchRequestTag = "ATLSwitchRequest";
char const* const SATLXMLTags::szATLValueTag = "ATLValue";
char const* const SATLXMLTags::szATLRtpcRequestTag = "ATLRtpcRequest";
char const* const SATLXMLTags::szATLPreloadRequestTag = "ATLPreloadRequest";
char const* const SATLXMLTags::szATLEnvironmentRequestTag = "ATLEnvironmentRequest";

char const* const SATLXMLTags::szATLNameAttribute = "atl_name";
char const* const SATLXMLTags::szATLVersionAttribute = "atl_version";
char const* const SATLXMLTags::szATLInternalNameAttribute = "atl_internal_name";
char const* const SATLXMLTags::szATLTypeAttribute = "atl_type";
char const* const SATLXMLTags::szATLConfigGroupAttribute = "atl_config_group_name";

char const* const SATLXMLTags::szATLDataLoadType = "AutoLoad";

AudioControlId SATLInternalControlIDs::obstructionOcclusionCalcSwitchId = INVALID_AUDIO_CONTROL_ID;
AudioControlId SATLInternalControlIDs::objectDopplerTrackingSwitchId = INVALID_AUDIO_CONTROL_ID;
AudioControlId SATLInternalControlIDs::objectVelocityTrackingSwitchId = INVALID_AUDIO_CONTROL_ID;
AudioControlId SATLInternalControlIDs::loseFocusTriggerId = INVALID_AUDIO_CONTROL_ID;
AudioControlId SATLInternalControlIDs::getFocusTriggerId = INVALID_AUDIO_CONTROL_ID;
AudioControlId SATLInternalControlIDs::muteAllTriggerId = INVALID_AUDIO_CONTROL_ID;
AudioControlId SATLInternalControlIDs::unmuteAllTriggerId = INVALID_AUDIO_CONTROL_ID;
AudioControlId SATLInternalControlIDs::objectDopplerRtpcId = INVALID_AUDIO_CONTROL_ID;
AudioControlId SATLInternalControlIDs::objectVelocityRtpcId = INVALID_AUDIO_CONTROL_ID;
AudioSwitchStateId SATLInternalControlIDs::ignoreStateId = INVALID_AUDIO_SWITCH_STATE_ID;
AudioSwitchStateId SATLInternalControlIDs::singleRayStateId = INVALID_AUDIO_SWITCH_STATE_ID;
AudioSwitchStateId SATLInternalControlIDs::multiRayStateId = INVALID_AUDIO_SWITCH_STATE_ID;
AudioSwitchStateId SATLInternalControlIDs::onStateId = INVALID_AUDIO_SWITCH_STATE_ID;
AudioSwitchStateId SATLInternalControlIDs::offStateId = INVALID_AUDIO_SWITCH_STATE_ID;
AudioPreloadRequestId SATLInternalControlIDs::globalPreloadRequestId = INVALID_AUDIO_PRELOAD_REQUEST_ID;

using namespace CryAudio::Impl;

//////////////////////////////////////////////////////////////////////////
void InitATLControlIDs()
{
	using namespace CryAudio::Impl;
	SATLInternalControlIDs::loseFocusTriggerId = AudioStringToId("lose_focus");
	SATLInternalControlIDs::getFocusTriggerId = AudioStringToId("get_focus");
	SATLInternalControlIDs::muteAllTriggerId = AudioStringToId("mute_all");
	SATLInternalControlIDs::unmuteAllTriggerId = AudioStringToId("unmute_all");
	SATLInternalControlIDs::objectDopplerRtpcId = AudioStringToId("object_doppler");
	SATLInternalControlIDs::objectVelocityRtpcId = AudioStringToId("object_speed");
	SATLInternalControlIDs::obstructionOcclusionCalcSwitchId = AudioStringToId("ObstructionOcclusionCalculationType");
	SATLInternalControlIDs::ignoreStateId = AudioStringToId("ignore");
	SATLInternalControlIDs::singleRayStateId = AudioStringToId("single_ray");
	SATLInternalControlIDs::multiRayStateId = AudioStringToId("multi_ray");
	SATLInternalControlIDs::objectDopplerTrackingSwitchId = AudioStringToId("object_doppler_tracking");
	SATLInternalControlIDs::objectVelocityTrackingSwitchId = AudioStringToId("object_velocity_tracking");
	SATLInternalControlIDs::onStateId = AudioStringToId("on");
	SATLInternalControlIDs::offStateId = AudioStringToId("off");
	SATLInternalControlIDs::globalPreloadRequestId = AudioStringToId("global_atl_preloads");
}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
//////////////////////////////////////////////////////////////////////////
CATLDebugNameStore::CATLDebugNameStore()
	: m_bAudioObjectsChanged(false)
	, m_bAudioTriggersChanged(false)
	, m_bAudioRtpcsChanged(false)
	, m_bAudioSwitchesChanged(false)
	, m_bAudioPreloadsChanged(false)
	, m_bAudioEnvironmentsChanged(false)
	, m_bAudioStandaloneFilesChanged(false)
{
}

//////////////////////////////////////////////////////////////////////////
CATLDebugNameStore::~CATLDebugNameStore()
{
	// the containers only hold numbers and strings, no ATL specific objects,
	// so there is no need to call the implementation to do the cleanup
	stl::free_container(m_audioObjectNames);
	stl::free_container(m_audioTriggerNames);
	stl::free_container(m_audioSwitchNames);
	stl::free_container(m_audioRtpcNames);
	stl::free_container(m_audioPreloadRequestNames);
	stl::free_container(m_audioEnvironmentNames);
	stl::free_container(m_audioStandaloneFileNames);
}

//////////////////////////////////////////////////////////////////////////
void CATLDebugNameStore::SyncChanges(CATLDebugNameStore const& otherNameStore)
{
	if (otherNameStore.m_bAudioObjectsChanged)
	{
		m_audioObjectNames.clear();
		m_audioObjectNames.insert(otherNameStore.m_audioObjectNames.begin(), otherNameStore.m_audioObjectNames.end());
		otherNameStore.m_bAudioObjectsChanged = false;
	}

	if (otherNameStore.m_bAudioTriggersChanged)
	{
		m_audioTriggerNames.clear();
		m_audioTriggerNames.insert(otherNameStore.m_audioTriggerNames.begin(), otherNameStore.m_audioTriggerNames.end());
		otherNameStore.m_bAudioTriggersChanged = false;
	}

	if (otherNameStore.m_bAudioRtpcsChanged)
	{
		m_audioRtpcNames.clear();
		m_audioRtpcNames.insert(otherNameStore.m_audioRtpcNames.begin(), otherNameStore.m_audioRtpcNames.end());
		otherNameStore.m_bAudioRtpcsChanged = false;
	}

	if (otherNameStore.m_bAudioSwitchesChanged)
	{
		m_audioSwitchNames.clear();
		m_audioSwitchNames.insert(otherNameStore.m_audioSwitchNames.begin(), otherNameStore.m_audioSwitchNames.end());
		otherNameStore.m_bAudioSwitchesChanged = false;
	}

	if (otherNameStore.m_bAudioPreloadsChanged)
	{
		m_audioPreloadRequestNames.clear();
		m_audioPreloadRequestNames.insert(otherNameStore.m_audioPreloadRequestNames.begin(), otherNameStore.m_audioPreloadRequestNames.end());
		otherNameStore.m_bAudioPreloadsChanged = false;
	}

	if (otherNameStore.m_bAudioEnvironmentsChanged)
	{
		m_audioEnvironmentNames.clear();
		m_audioEnvironmentNames.insert(otherNameStore.m_audioEnvironmentNames.begin(), otherNameStore.m_audioEnvironmentNames.end());
		otherNameStore.m_bAudioEnvironmentsChanged = false;
	}

	if (otherNameStore.m_bAudioStandaloneFilesChanged)
	{
		m_audioStandaloneFileNames.clear();
		m_audioStandaloneFileNames.insert(otherNameStore.m_audioStandaloneFileNames.begin(), otherNameStore.m_audioStandaloneFileNames.end());
		otherNameStore.m_bAudioStandaloneFilesChanged = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLDebugNameStore::AddAudioObject(AudioObjectId const audioObjectId, char const* const szName)
{
	m_audioObjectNames[audioObjectId] = szName;
	m_bAudioObjectsChanged = true;
}

//////////////////////////////////////////////////////////////////////////
void CATLDebugNameStore::AddAudioTrigger(AudioControlId const audioTriggerId, char const* const szName)
{
	m_audioTriggerNames[audioTriggerId] = szName;
	m_bAudioTriggersChanged = true;
}

//////////////////////////////////////////////////////////////////////////
void CATLDebugNameStore::AddAudioRtpc(AudioControlId const audioRtpcId, char const* const szName)
{
	m_audioRtpcNames[audioRtpcId] = szName;
	m_bAudioRtpcsChanged = true;
}

//////////////////////////////////////////////////////////////////////////
void CATLDebugNameStore::AddAudioSwitch(AudioControlId const audioSwitchId, char const* const szName)
{
	m_audioSwitchNames[audioSwitchId] = std::make_pair(szName, AudioSwitchStateMap());
	m_bAudioSwitchesChanged = true;
}

//////////////////////////////////////////////////////////////////////////
void CATLDebugNameStore::AddAudioSwitchState(AudioControlId const audioSwitchId, AudioSwitchStateId const audioSwitchStateId, char const* const szName)
{
	m_audioSwitchNames[audioSwitchId].second[audioSwitchStateId] = szName;
	m_bAudioSwitchesChanged = true;
}

//////////////////////////////////////////////////////////////////////////
void CATLDebugNameStore::AddAudioPreloadRequest(AudioPreloadRequestId const audioPreloadRequestId, char const* const szName)
{
	m_audioPreloadRequestNames[audioPreloadRequestId] = szName;
	m_bAudioPreloadsChanged = true;
}

//////////////////////////////////////////////////////////////////////////
void CATLDebugNameStore::AddAudioEnvironment(AudioEnvironmentId const audioEnvironmentId, char const* const szName)
{
	m_audioEnvironmentNames[audioEnvironmentId] = szName;
	m_bAudioEnvironmentsChanged = true;
}

//////////////////////////////////////////////////////////////////////////
void CATLDebugNameStore::AddAudioStandaloneFile(AudioStandaloneFileId const audioStandaloneFileId, char const* const szName)
{
	m_audioStandaloneFileNames[audioStandaloneFileId].first = szName;
	++(m_audioStandaloneFileNames[audioStandaloneFileId].second);
	m_bAudioStandaloneFilesChanged = true;
}

//////////////////////////////////////////////////////////////////////////
void CATLDebugNameStore::RemoveAudioObject(AudioObjectId const audioObjectId)
{
	m_audioObjectNames.erase(audioObjectId);
	m_bAudioObjectsChanged = true;
}

//////////////////////////////////////////////////////////////////////////
void CATLDebugNameStore::RemoveAudioTrigger(AudioControlId const audioTriggerId)
{
	m_audioTriggerNames.erase(audioTriggerId);
	m_bAudioTriggersChanged = true;
}

//////////////////////////////////////////////////////////////////////////
void CATLDebugNameStore::RemoveAudioRtpc(AudioControlId const audioRtpcId)
{
	m_audioRtpcNames.erase(audioRtpcId);
	m_bAudioRtpcsChanged = true;
}

//////////////////////////////////////////////////////////////////////////
void CATLDebugNameStore::RemoveAudioSwitch(AudioControlId const audioSwitchId)
{
	m_audioSwitchNames.erase(audioSwitchId);
	m_bAudioSwitchesChanged = true;
}

//////////////////////////////////////////////////////////////////////////
void CATLDebugNameStore::RemoveAudioPreloadRequest(AudioPreloadRequestId const audioPreloadRequestId)
{
	m_audioPreloadRequestNames.erase(audioPreloadRequestId);
	m_bAudioPreloadsChanged = true;
}

//////////////////////////////////////////////////////////////////////////
void CATLDebugNameStore::RemoveAudioEnvironment(AudioEnvironmentId const audioEnvironmentId)
{
	m_audioEnvironmentNames.erase(audioEnvironmentId);
	m_bAudioEnvironmentsChanged = true;
}

//////////////////////////////////////////////////////////////////////////
void CATLDebugNameStore::RemoveAudioStandaloneFile(AudioStandaloneFileId const audioStandaloneFileId)
{
	if (m_audioStandaloneFileNames[audioStandaloneFileId].second == 0)
	{
		CryFatalError("<Audio>: Negative ref count on \"%s\" during CATLDebugNameStore::RemoveAudioStandaloneFile!", m_audioStandaloneFileNames[audioStandaloneFileId].first.c_str());
	}

	if (--(m_audioStandaloneFileNames[audioStandaloneFileId].second) == 0)
	{
		m_audioStandaloneFileNames.erase(audioStandaloneFileId);
	}

	m_bAudioStandaloneFilesChanged = true;
}

//////////////////////////////////////////////////////////////////////////
char const* CATLDebugNameStore::LookupAudioObjectName(AudioObjectId const audioObjectId) const
{
	AudioObjectMap::const_iterator iter(m_audioObjectNames.cbegin());
	char const* szResult = nullptr;

	if (FindPlaceConst(m_audioObjectNames, audioObjectId, iter))
	{
		szResult = iter->second.c_str();
	}
	else if (audioObjectId == GLOBAL_AUDIO_OBJECT_ID)
	{
		szResult = "GlobalAudioObject";
	}

	return szResult;
}

//////////////////////////////////////////////////////////////////////////
char const* CATLDebugNameStore::LookupAudioTriggerName(AudioControlId const audioTriggerId) const
{
	AudioControlMap::const_iterator iter(m_audioTriggerNames.cbegin());
	char const* szResult = nullptr;

	if (FindPlaceConst(m_audioTriggerNames, audioTriggerId, iter))
	{
		szResult = iter->second.c_str();
	}

	return szResult;
}

//////////////////////////////////////////////////////////////////////////
char const* CATLDebugNameStore::LookupAudioRtpcName(AudioControlId const audioRtpcId) const
{
	AudioControlMap::const_iterator iter(m_audioRtpcNames.cbegin());
	char const* szResult = nullptr;

	if (FindPlaceConst(m_audioRtpcNames, audioRtpcId, iter))
	{
		szResult = iter->second.c_str();
	}

	return szResult;
}

//////////////////////////////////////////////////////////////////////////
char const* CATLDebugNameStore::LookupAudioSwitchName(AudioControlId const audioSwitchId) const
{
	AudioSwitchMap::const_iterator iter(m_audioSwitchNames.cbegin());
	char const* szResult = nullptr;

	if (FindPlaceConst(m_audioSwitchNames, audioSwitchId, iter))
	{
		szResult = iter->second.first.c_str();
	}

	return szResult;
}

//////////////////////////////////////////////////////////////////////////
char const* CATLDebugNameStore::LookupAudioSwitchStateName(AudioControlId const audioSwitchId, AudioSwitchStateId const audioSwitchStateId) const
{
	AudioSwitchMap::const_iterator iter(m_audioSwitchNames.cbegin());
	char const* szResult = nullptr;

	if (FindPlaceConst(m_audioSwitchNames, audioSwitchId, iter))
	{
		AudioSwitchStateMap const& states(iter->second.second);
		AudioSwitchStateMap::const_iterator iterState(states.cbegin());

		if (FindPlaceConst(states, audioSwitchStateId, iterState))
		{
			szResult = iterState->second.c_str();
		}
	}

	return szResult;
}

//////////////////////////////////////////////////////////////////////////
char const* CATLDebugNameStore::LookupAudioPreloadRequestName(AudioPreloadRequestId const audioPreloadRequestId) const
{
	AudioPreloadRequestsMap::const_iterator iter(m_audioPreloadRequestNames.cbegin());
	char const* szResult = nullptr;

	if (FindPlaceConst(m_audioPreloadRequestNames, audioPreloadRequestId, iter))
	{
		szResult = iter->second.c_str();
	}

	return szResult;
}

//////////////////////////////////////////////////////////////////////////
char const* CATLDebugNameStore::LookupAudioEnvironmentName(AudioEnvironmentId const audioEnvironmentId) const
{
	AudioEnvironmentMap::const_iterator iter(m_audioEnvironmentNames.cbegin());
	char const* szResult = nullptr;

	if (FindPlaceConst(m_audioEnvironmentNames, audioEnvironmentId, iter))
	{
		szResult = iter->second.c_str();
	}

	return szResult;
}

//////////////////////////////////////////////////////////////////////////
char const* CATLDebugNameStore::LookupAudioStandaloneFileName(AudioStandaloneFileId const audioStandaloneFileId) const
{
	AudioStandaloneFileMap::const_iterator const iter(m_audioStandaloneFileNames.find(audioStandaloneFileId));
	char const* szResult = nullptr;

	if (iter != m_audioStandaloneFileNames.end())
	{
		szResult = iter->second.first.c_str();
	}

	return szResult;
}

//////////////////////////////////////////////////////////////////////////
void CATLDebugNameStore::GetAudioDebugData(SAudioDebugData& audioDebugData) const
{
	for (auto const& name : m_audioObjectNames)
	{
		audioDebugData.audioObjectNames.push_back(name.second);
	}
}

#endif // INCLUDE_AUDIO_PRODUCTION_CODE
