// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioProxy.h"
#include "SoundCVars.h"
#include "AudioSystem.h"
#include <AudioLogger.h>
#include <CrySystem/ISystem.h>
#include <CryEntitySystem/IEntitySystem.h>

AudioControlId CAudioProxy::s_occlusionTypeSwitchId = INVALID_AUDIO_CONTROL_ID;
AudioSwitchStateId CAudioProxy::s_occlusionTypeStateIds[eAudioOcclusionType_Count] = {
	INVALID_AUDIO_SWITCH_STATE_ID,
	INVALID_AUDIO_SWITCH_STATE_ID,
	INVALID_AUDIO_SWITCH_STATE_ID,
	INVALID_AUDIO_SWITCH_STATE_ID
};

//////////////////////////////////////////////////////////////////////////
CAudioProxy::CAudioProxy()
	: m_audioObjectId(INVALID_AUDIO_OBJECT_ID)
	, m_flags(eAudioProxyFlags_None)
{
}

//////////////////////////////////////////////////////////////////////////
CAudioProxy::~CAudioProxy()
{
	CRY_ASSERT(m_audioObjectId == INVALID_AUDIO_OBJECT_ID);
	stl::free_container(m_queuedAudioCommands);
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::Initialize(char const* const szObjectName, bool const bInitAsync /*= true*/)
{
	if ((bInitAsync && g_audioCVars.m_audioProxiesInitType == 0) || g_audioCVars.m_audioProxiesInitType == 2)
	{
		if ((m_flags & eAudioProxyFlags_WaitingForId) == 0)
		{
			m_flags |= eAudioProxyFlags_WaitingForId;

			SAudioRequest request;
			SAudioManagerRequestData<eAudioManagerRequestType_ReserveAudioObjectId> requestData(&m_audioObjectId, szObjectName);
			request.flags = eAudioRequestFlags_PriorityHigh | eAudioRequestFlags_SyncCallback;
			request.pOwner = this;
			request.pData = &requestData;

			gEnv->pAudioSystem->PushRequest(request);
		}
		else
		{
			SQueuedAudioCommand queuedCommand = SQueuedAudioCommand(eQueuedAudioCommandType_Initialize);
			queuedCommand.stringValue = szObjectName;
			TryAddQueuedCommand(queuedCommand);
		}
	}
	else
	{
		SAudioRequest request;
		SAudioManagerRequestData<eAudioManagerRequestType_ReserveAudioObjectId> requestData(&m_audioObjectId, szObjectName);
		request.flags = eAudioRequestFlags_PriorityHigh | eAudioRequestFlags_ExecuteBlocking;
		request.pData = &requestData;

		gEnv->pAudioSystem->PushRequest(request);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		if (m_audioObjectId == INVALID_AUDIO_OBJECT_ID)
		{
			CryFatalError("<Audio> Failed to reserve audio object ID on AudioProxy (%s)!", szObjectName);
		}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::ExecuteTrigger(
  AudioControlId const audioTriggerId,
  SAudioCallBackInfo const& callBackInfo /*= SAudioCallBackInfo::GetEmptyObject()*/)
{
	if ((m_flags & eAudioProxyFlags_WaitingForId) == 0)
	{
		CRY_ASSERT(m_audioObjectId != INVALID_AUDIO_OBJECT_ID);

		SAudioRequest request;
		request.audioObjectId = m_audioObjectId;
		request.flags = callBackInfo.requestFlags;

		SAudioObjectRequestData<eAudioObjectRequestType_ExecuteTrigger> requestData(audioTriggerId, 0.0f);
		request.pOwner = (callBackInfo.pObjectToNotify != nullptr) ? callBackInfo.pObjectToNotify : this;
		request.pUserData = callBackInfo.pUserData;
		request.pUserDataOwner = callBackInfo.pUserDataOwner;
		request.pData = &requestData;

		gEnv->pAudioSystem->PushRequest(request);
	}
	else
	{
		SQueuedAudioCommand queuedCommand = SQueuedAudioCommand(eQueuedAudioCommandType_ExecuteTrigger);
		queuedCommand.audioTriggerId = audioTriggerId;
		queuedCommand.pOwnerOverride = callBackInfo.pObjectToNotify;
		queuedCommand.pUserData = callBackInfo.pUserData;
		queuedCommand.pUserDataOwner = callBackInfo.pUserDataOwner;
		queuedCommand.requestFlags = callBackInfo.requestFlags;
		TryAddQueuedCommand(queuedCommand);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::StopTrigger(AudioControlId const audioTriggerId)
{
	if ((m_flags & eAudioProxyFlags_WaitingForId) == 0)
	{
		SAudioRequest request;
		SAudioObjectRequestData<eAudioObjectRequestType_StopTrigger> requestData(audioTriggerId);
		request.audioObjectId = m_audioObjectId;
		request.flags = eAudioRequestFlags_PriorityNormal;
		request.pData = &requestData;
		request.pOwner = this;

		gEnv->pAudioSystem->PushRequest(request);
	}
	else
	{
		SQueuedAudioCommand queuedCommand = SQueuedAudioCommand(eQueuedAudioCommandType_StopTrigger);
		queuedCommand.audioTriggerId = audioTriggerId;
		TryAddQueuedCommand(queuedCommand);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::SetSwitchState(AudioControlId const audioSwitchId, AudioSwitchStateId const audioStateId)
{
	if ((m_flags & eAudioProxyFlags_WaitingForId) == 0)
	{
		SAudioRequest request;
		SAudioObjectRequestData<eAudioObjectRequestType_SetSwitchState> requestData(audioSwitchId, audioStateId);
		request.audioObjectId = m_audioObjectId;
		request.flags = eAudioRequestFlags_PriorityNormal;
		request.pData = &requestData;
		request.pOwner = this;

		gEnv->pAudioSystem->PushRequest(request);
	}
	else
	{
		SQueuedAudioCommand queuedCommand = SQueuedAudioCommand(eQueuedAudioCommandType_SetSwitchState);
		queuedCommand.audioSwitchId = audioSwitchId;
		queuedCommand.audioSwitchStateId = audioStateId;
		TryAddQueuedCommand(queuedCommand);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::SetRtpcValue(AudioControlId const audioRtpcId, float const value)
{
	if ((m_flags & eAudioProxyFlags_WaitingForId) == 0)
	{
		SAudioRequest request;
		SAudioObjectRequestData<eAudioObjectRequestType_SetRtpcValue> requestData(audioRtpcId, value);
		request.audioObjectId = m_audioObjectId;
		request.flags = eAudioRequestFlags_PriorityNormal;
		request.pData = &requestData;
		request.pOwner = this;

		gEnv->pAudioSystem->PushRequest(request);
	}
	else
	{
		SQueuedAudioCommand queuedCommand = SQueuedAudioCommand(eQueuedAudioCommandType_SetRtpcValue);
		queuedCommand.audioRtpcId = audioRtpcId;
		queuedCommand.floatValue = value;
		TryAddQueuedCommand(queuedCommand);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::SetOcclusionType(EAudioOcclusionType const occlusionType)
{
	AudioEnumFlagsType const index = static_cast<AudioEnumFlagsType>(occlusionType);

	if (index < eAudioOcclusionType_Count)
	{
		SetSwitchState(s_occlusionTypeSwitchId, s_occlusionTypeStateIds[index]);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::SetTransformation(Matrix34 const& transformation)
{
	// Update position only if the delta exceeds a given value.
	// Ideally this should be done on the caller's side!
	if (g_audioCVars.m_positionUpdateThreshold == 0.0f ||
	    !m_transformation.IsEquivalent(transformation, g_audioCVars.m_positionUpdateThreshold))
	{
		SetTransformationInternal(transformation);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::SetPosition(Vec3 const& position)
{
	// Update position only if the delta exceeds a given value.
	// Ideally this should be done on the caller's side!
	if (g_audioCVars.m_positionUpdateThreshold == 0.0f ||
	    !position.IsEquivalent(m_transformation.GetPosition(), g_audioCVars.m_positionUpdateThreshold))
	{
		SetTransformationInternal(Matrix34(IDENTITY, position));
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::SetTransformationInternal(Matrix34 const& transformation)
{
	if ((m_flags & eAudioProxyFlags_WaitingForId) == 0)
	{
		m_transformation = transformation;

		SAudioRequest request;
		SAudioObjectRequestData<eAudioObjectRequestType_SetTransformation> requestData(transformation);
		request.audioObjectId = m_audioObjectId;
		request.flags = eAudioRequestFlags_PriorityNormal;
		request.pData = &requestData;
		request.pOwner = this;

		gEnv->pAudioSystem->PushRequest(request);
	}
	else
	{
		SQueuedAudioCommand queuedCommand = SQueuedAudioCommand(eQueuedAudioCommandType_SetPosition);
		queuedCommand.transformation = transformation;
		TryAddQueuedCommand(queuedCommand);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::SetEnvironmentAmount(AudioEnvironmentId const audioEnvironmentId, float const amount)
{
	if ((m_flags & eAudioProxyFlags_WaitingForId) == 0)
	{
		SAudioRequest request;
		SAudioObjectRequestData<eAudioObjectRequestType_SetEnvironmentAmount> requestData(audioEnvironmentId, amount);
		request.audioObjectId = m_audioObjectId;
		request.flags = eAudioRequestFlags_PriorityNormal;
		request.pData = &requestData;
		request.pOwner = this;

		gEnv->pAudioSystem->PushRequest(request);
	}
	else
	{
		SQueuedAudioCommand queuedCommand = SQueuedAudioCommand(eQueuedAudioCommandType_SetEnvironmentAmount);
		queuedCommand.audioEnvironmentId = audioEnvironmentId;
		queuedCommand.floatValue = amount;
		TryAddQueuedCommand(queuedCommand);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::SetCurrentEnvironments(EntityId const entityToIgnore /*= 0*/)
{
	if ((m_flags & eAudioProxyFlags_WaitingForId) == 0)
	{
		ClearEnvironments();
		IAreaManager* const pAreaManager = gEnv->pEntitySystem->GetAreaManager();
		size_t numAreas = 0;
		static SAudioAreaInfo s_areaInfos[s_maxAreas];

		if (pAreaManager->QueryAudioAreas(m_transformation.GetPosition(), s_areaInfos, s_maxAreas, numAreas))
		{
			for (size_t i = 0; i < numAreas; ++i)
			{
				SAudioAreaInfo const& areaInfo = s_areaInfos[i];

				if (entityToIgnore == INVALID_ENTITYID || entityToIgnore != areaInfo.envProvidingEntityId)
				{
					SetEnvironmentAmount(areaInfo.audioEnvironmentId, areaInfo.amount);
				}
			}
		}
	}
	else
	{
		SQueuedAudioCommand queuedCommand = SQueuedAudioCommand(eQueuedAudioCommandType_SetCurrentEnvironments);
		queuedCommand.entityId = entityToIgnore;
		TryAddQueuedCommand(queuedCommand);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::ClearEnvironments()
{
	if ((m_flags & eAudioProxyFlags_WaitingForId) == 0)
	{
		SAudioRequest request;
		SAudioObjectRequestData<eAudioObjectRequestType_ResetEnvironments> requestData;
		request.audioObjectId = m_audioObjectId;
		request.flags = eAudioRequestFlags_PriorityNormal;
		request.pData = &requestData;
		request.pOwner = this;

		gEnv->pAudioSystem->PushRequest(request);
	}
	else
	{
		TryAddQueuedCommand(SQueuedAudioCommand(eQueuedAudioCommandType_ClearEnvironments));
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::Release()
{
	if ((m_flags & eAudioProxyFlags_WaitingForId) == 0)
	{
		Reset();
		static_cast<CAudioSystem*>(gEnv->pAudioSystem)->FreeAudioProxy(this);
	}
	else
	{
		TryAddQueuedCommand(SQueuedAudioCommand(eQueuedAudioCommandType_Release));
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::Reset()
{
	if ((m_flags & eAudioProxyFlags_WaitingForId) == 0)
	{
		if (m_audioObjectId != INVALID_AUDIO_OBJECT_ID)
		{
			// Request must be asynchronous and lowest priority!
			SAudioRequest request;
			SAudioObjectRequestData<eAudioObjectRequestType_ReleaseObject> requestData;
			request.audioObjectId = m_audioObjectId;
			request.pData = &requestData;

			gEnv->pAudioSystem->PushRequest(request);

			m_audioObjectId = INVALID_AUDIO_OBJECT_ID;
		}

		m_transformation = CAudioObjectTransformation();
	}
	else
	{
		TryAddQueuedCommand(SQueuedAudioCommand(eQueuedAudioCommandType_Reset));
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::PlayFile(SAudioPlayFileInfo const& playFileInfo, SAudioCallBackInfo const& callBackInfo /* = SAudioCallBackInfo::GetEmptyObject() */)
{
	if ((m_flags & eAudioProxyFlags_WaitingForId) == 0)
	{
		CRY_ASSERT(m_audioObjectId != INVALID_AUDIO_OBJECT_ID);

		SAudioRequest request;
		request.audioObjectId = m_audioObjectId;
		request.flags = callBackInfo.requestFlags;
		SAudioObjectRequestData<eAudioObjectRequestType_PlayFile> requestData(playFileInfo.szFile, playFileInfo.bLocalized, playFileInfo.usedTriggerForPlayback);
		request.pOwner = (callBackInfo.pObjectToNotify != nullptr) ? callBackInfo.pObjectToNotify : this;
		request.pUserData = callBackInfo.pUserData;
		request.pUserDataOwner = callBackInfo.pUserDataOwner;
		request.pData = &requestData;

		gEnv->pAudioSystem->PushRequest(request);
	}
	else
	{
		SQueuedAudioCommand queuedCommand = SQueuedAudioCommand(eQueuedAudioCommandType_PlayFile);
		queuedCommand.pOwnerOverride = callBackInfo.pObjectToNotify;
		queuedCommand.pUserData = callBackInfo.pUserData;
		queuedCommand.pUserDataOwner = callBackInfo.pUserDataOwner;
		queuedCommand.requestFlags = callBackInfo.requestFlags;
		queuedCommand.stringValue = playFileInfo.szFile;
		queuedCommand.audioTriggerId = playFileInfo.usedTriggerForPlayback;
		queuedCommand.floatValue = (playFileInfo.bLocalized) ? 1.0f : 0.0f;
		TryAddQueuedCommand(queuedCommand);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::StopFile(char const* const _szFile)
{
	if ((m_flags & eAudioProxyFlags_WaitingForId) == 0)
	{
		CRY_ASSERT(m_audioObjectId != INVALID_AUDIO_OBJECT_ID);

		SAudioRequest request;
		request.audioObjectId = m_audioObjectId;
		request.flags = eAudioRequestFlags_PriorityNormal;

		SAudioObjectRequestData<eAudioObjectRequestType_StopFile> requestData(_szFile);
		request.pData = &requestData;

		gEnv->pAudioSystem->PushRequest(request);
	}
	else
	{
		SQueuedAudioCommand queuedCommand = SQueuedAudioCommand(eQueuedAudioCommandType_StopFile);
		queuedCommand.stringValue = _szFile;
		TryAddQueuedCommand(queuedCommand);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::ExecuteQueuedCommands()
{
	m_flags &= ~eAudioProxyFlags_WaitingForId;

	if (!m_queuedAudioCommands.empty())
	{
		for (auto const& command : m_queuedAudioCommands)
		{
			switch (command.type)
			{
			case eQueuedAudioCommandType_PlayFile:
				{
					SAudioCallBackInfo const callbackInfo(command.pOwnerOverride, command.pUserData, command.pUserDataOwner, command.requestFlags);
					SAudioPlayFileInfo const playbackInfo(command.stringValue.c_str(), (command.floatValue != 0.0f) ? true : false, command.audioTriggerId);
					PlayFile(playbackInfo, callbackInfo);
				}
				break;
			case eQueuedAudioCommandType_StopFile:
				StopFile(command.stringValue.c_str());
				break;
			case eQueuedAudioCommandType_ExecuteTrigger:
				{
					SAudioCallBackInfo const callbackInfo(command.pOwnerOverride, command.pUserData, command.pUserDataOwner, command.requestFlags);
					ExecuteTrigger(command.audioTriggerId, callbackInfo);
				}
				break;
			case eQueuedAudioCommandType_StopTrigger:
				StopTrigger(command.audioTriggerId);
				break;
			case eQueuedAudioCommandType_SetSwitchState:
				SetSwitchState(command.audioSwitchId, command.audioSwitchStateId);
				break;
			case eQueuedAudioCommandType_SetRtpcValue:
				SetRtpcValue(command.audioRtpcId, command.floatValue);
				break;
			case eQueuedAudioCommandType_SetEnvironmentAmount:
				SetEnvironmentAmount(command.audioEnvironmentId, command.floatValue);
				break;
			case eQueuedAudioCommandType_SetCurrentEnvironments:
				SetCurrentEnvironments(command.entityId);
				break;
			case eQueuedAudioCommandType_ClearEnvironments:
				ClearEnvironments();
				break;
			case eQueuedAudioCommandType_SetPosition:
				SetTransformationInternal(command.transformation);
				break;
			case eQueuedAudioCommandType_Reset:
				Reset();
				break;
			case eQueuedAudioCommandType_Release:
				Release();
				break;
			case eQueuedAudioCommandType_Initialize:
				Initialize(command.stringValue.c_str(), true);
				break;
			default:
				CryFatalError("Unknown command type in CAudioProxy::ExecuteQueuedCommands!");
				break;
			}

			if ((m_flags & eAudioProxyFlags_WaitingForId) > 0)
			{
				// An Initialize command was queued up.
				// Here we need to keep all commands after the Initialize.
				break;
			}
		}

		if ((m_flags & eAudioProxyFlags_WaitingForId) == 0)
		{
			m_queuedAudioCommands.clear();
		}
		else
		{
			// An Initialize command was queued up.
			// Here we need to keep queued commands except for Reset and Initialize.
			TQueuedAudioCommands::iterator iter(m_queuedAudioCommands.begin());
			TQueuedAudioCommands::const_iterator iterEnd(m_queuedAudioCommands.cend());

			while (iter != iterEnd)
			{
				SQueuedAudioCommand const& refCommand = (*iter);

				if (refCommand.type == eQueuedAudioCommandType_Reset || refCommand.type == eQueuedAudioCommandType_Initialize)
				{
					iter = m_queuedAudioCommands.erase(iter);
					iterEnd = m_queuedAudioCommands.cend();
					continue;
				}

				++iter;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioProxy::TryAddQueuedCommand(SQueuedAudioCommand const& command)
{
	bool bAdd = true;

	switch (command.type)
	{
	case eQueuedAudioCommandType_PlayFile:
	case eQueuedAudioCommandType_StopFile:
	case eQueuedAudioCommandType_ExecuteTrigger:
	case eQueuedAudioCommandType_StopTrigger:
		{
			// These type of commands get always pushed back!
			break;
		}
	case eQueuedAudioCommandType_SetSwitchState:
		{
			if (!m_queuedAudioCommands.empty())
			{
				bAdd = (std::find_if(m_queuedAudioCommands.begin(), m_queuedAudioCommands.end(), SFindSetSwitchState(command.audioSwitchId, command.audioSwitchStateId)) == m_queuedAudioCommands.end());
			}

			break;
		}
	case eQueuedAudioCommandType_SetRtpcValue:
		{
			if (!m_queuedAudioCommands.empty())
			{
				bAdd = (std::find_if(m_queuedAudioCommands.begin(), m_queuedAudioCommands.end(), SFindSetRtpcValue(command.audioRtpcId, command.floatValue)) == m_queuedAudioCommands.end());
			}

			break;
		}
	case eQueuedAudioCommandType_SetPosition:
		{
			if (!m_queuedAudioCommands.empty())
			{
				bAdd = (std::find_if(m_queuedAudioCommands.begin(), m_queuedAudioCommands.end(), SFindSetPosition(command.transformation)) == m_queuedAudioCommands.end());
			}

			break;
		}
	case eQueuedAudioCommandType_SetEnvironmentAmount:
		{
			if (!m_queuedAudioCommands.empty())
			{
				bAdd = (std::find_if(m_queuedAudioCommands.begin(), m_queuedAudioCommands.end(), SFindSetEnvironmentAmount(command.audioEnvironmentId, command.floatValue)) == m_queuedAudioCommands.end());
			}

			break;
		}
	case eQueuedAudioCommandType_SetCurrentEnvironments:
	case eQueuedAudioCommandType_ClearEnvironments:
	case eQueuedAudioCommandType_Release:
		{
			// These type of commands don't need another instance!
			if (!m_queuedAudioCommands.empty())
			{
				bAdd = (std::find_if(m_queuedAudioCommands.begin(), m_queuedAudioCommands.end(), SFindCommand(command.type)) == m_queuedAudioCommands.end());
			}

			break;
		}
	case eQueuedAudioCommandType_Reset:
		{
			for (auto const& localCommand : m_queuedAudioCommands)
			{
				if (localCommand.type == eQueuedAudioCommandType_Reset || localCommand.type == eQueuedAudioCommandType_Release)
				{
					// If either eQACT_RESET or eQACT_RELEASE are already queued up then there is no need for adding a eQACT_RESET command.
					bAdd = false;

					break;
				}
			}

			break;
		}
	case eQueuedAudioCommandType_Initialize:
		{
			// There must be only 1 Initialize command be queued up.
			m_queuedAudioCommands.clear();

			// Precede the Initialization with a Reset command to release the pending audio object.
			m_queuedAudioCommands.push_back(SQueuedAudioCommand(eQueuedAudioCommandType_Reset));

			break;
		}
	default:
		{
			CRY_ASSERT(false);
			g_audioLogger.Log(eAudioLogType_Error, "Unknown queued command type in CAudioProxy::TryAddQueuedCommand!");
			bAdd = false;

			break;
		}
	}

	if (bAdd)
	{
		if (command.type != eQueuedAudioCommandType_SetPosition)
		{
			m_queuedAudioCommands.push_back(command);
		}
		else
		{
			// Make sure we set position first!
			m_queuedAudioCommands.push_front(command);
		}
	}
}
