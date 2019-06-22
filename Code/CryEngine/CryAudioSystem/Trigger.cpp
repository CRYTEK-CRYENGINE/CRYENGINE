// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Trigger.h"
#include "Managers.h"
#include "Object.h"
#include "TriggerInstance.h"
#include "TriggerUtils.h"
#include "Common/IImpl.h"
#include "Common/IObject.h"
#include "Common/ITriggerConnection.h"

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	#include "Common/Logger.h"
#endif  // CRY_AUDIO_USE_DEBUG_CODE

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
CTrigger::~CTrigger()
{
	CRY_ASSERT_MESSAGE(g_pIImpl != nullptr, "g_pIImpl mustn't be nullptr during %s", __FUNCTION__);

	for (auto const pConnection : m_connections)
	{
		g_pIImpl->DestructTriggerConnection(pConnection);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTrigger::Execute(
	CObject& object,
	void* const pOwner /* = nullptr */,
	void* const pUserData /* = nullptr */,
	void* const pUserDataOwner /* = nullptr */,
	ERequestFlags const flags /* = ERequestFlags::None */,
	EntityId const entityId /*= INVALID_ENTITYID*/) const
{
	Impl::IObject* const pIObject = object.GetImplData();

	bool isPlaying = false;
	uint16 numPlayingInstances = 0;
	uint16 numPendingInstances = 0;

	for (auto const pConnection : m_connections)
	{
		ETriggerResult const result = pConnection->Execute(pIObject, g_triggerInstanceIdCounter);

		if ((result == ETriggerResult::Playing) || (result == ETriggerResult::Virtual) || (result == ETriggerResult::Pending))
		{
			if (result == ETriggerResult::Playing)
			{
				++numPlayingInstances;
				isPlaying = true;
			}
			else if (result == ETriggerResult::Virtual)
			{
				++numPlayingInstances;
			}
			else
			{
				++numPendingInstances;
			}
		}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		else if (result != ETriggerResult::DoNotTrack)
		{
			Cry::Audio::Log(ELogType::Warning, R"(Trigger "%s" failed on object "%s" during %s)", GetName(), object.GetName(), __FUNCTION__);
		}
#endif  // CRY_AUDIO_USE_DEBUG_CODE
	}

	if ((numPlayingInstances > 0) || (numPendingInstances > 0))
	{
		if (isPlaying)
		{
			object.RemoveFlag(EObjectFlags::Virtual);
		}
		else if ((object.GetFlags() & EObjectFlags::Active) == 0)
		{
			// Only when no trigger was active before, the object can get set to virtual here.
			object.SetFlag(EObjectFlags::Virtual);
		}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		object.ConstructTriggerInstance(m_id, entityId, numPlayingInstances, numPendingInstances, flags, pOwner, pUserData, pUserDataOwner, m_radius);
#else
		object.ConstructTriggerInstance(m_id, entityId, numPlayingInstances, numPendingInstances, flags, pOwner, pUserData, pUserDataOwner);
#endif    // CRY_AUDIO_USE_DEBUG_CODE
	}
	else
	{
		// All of the events have either finished before we got here or never started, immediately inform the user that the trigger has finished.
		SendFinishedTriggerInstanceRequest(m_id, entityId, flags, pOwner, pUserData, pUserDataOwner);
	}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	if (m_connections.empty())
	{
		Cry::Audio::Log(ELogType::Warning, R"(Trigger "%s" executed on object "%s" without connections)", GetName(), object.GetName());
	}
#endif // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CTrigger::Execute(
	void* const pOwner /* = nullptr */,
	void* const pUserData /* = nullptr */,
	void* const pUserDataOwner /* = nullptr */,
	ERequestFlags const flags /* = ERequestFlags::None */) const
{
	uint16 numPlayingInstances = 0;
	uint16 numPendingInstances = 0;

	for (auto const pConnection : m_connections)
	{
		ETriggerResult const result = pConnection->Execute(g_pIObject, g_triggerInstanceIdCounter);

		if ((result == ETriggerResult::Playing) || (result == ETriggerResult::Virtual) || (result == ETriggerResult::Pending))
		{
			if (result != ETriggerResult::Pending)
			{
				++numPlayingInstances;
			}
			else
			{
				++numPendingInstances;
			}
		}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		else if (result != ETriggerResult::DoNotTrack)
		{
			Cry::Audio::Log(ELogType::Warning, R"(Trigger "%s" failed during %s)", GetName(), __FUNCTION__);
		}
#endif  // CRY_AUDIO_USE_DEBUG_CODE
	}

	if ((numPlayingInstances > 0) || (numPendingInstances > 0))
	{
		ConstructGlobalTriggerInstance(m_id, numPlayingInstances, numPendingInstances, flags, pOwner, pUserData, pUserDataOwner);
	}
	else
	{
		// All of the events have either finished before we got here or never started, immediately inform the user that the trigger has finished.
		SendFinishedTriggerInstanceRequest(m_id, INVALID_ENTITYID, flags, pOwner, pUserData, pUserDataOwner);
	}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	if (m_connections.empty())
	{
		Cry::Audio::Log(ELogType::Warning, R"(Trigger "%s" executed without connections)", GetName());
	}
#endif // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CTrigger::Stop(Impl::IObject* const pIObject) const
{
	for (auto const pConnection : m_connections)
	{
		pConnection->Stop(pIObject);
	}
}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
void CTrigger::Execute(
	CObject& object,
	TriggerInstanceId const triggerInstanceId,
	CTriggerInstance* const pTriggerInstance,
	uint16 const triggerCounter) const
{
	bool isPlaying = false;

	Impl::IObject* const pIObject = object.GetImplData();

	for (auto const pConnection : m_connections)
	{
		ETriggerResult const result = pConnection->Execute(pIObject, triggerInstanceId);

		if ((result == ETriggerResult::Playing) || (result == ETriggerResult::Virtual) || (result == ETriggerResult::Pending))
		{
	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			pTriggerInstance->SetRadius(m_radius);
			object.UpdateMaxRadius(m_radius);
	#endif  // CRY_AUDIO_USE_DEBUG_CODE

			if (result == ETriggerResult::Playing)
			{
				pTriggerInstance->IncrementNumPlayingConnectionInstances();
				isPlaying = true;
			}
			else if (result == ETriggerResult::Virtual)
			{
				pTriggerInstance->IncrementNumPlayingConnectionInstances();
			}
			else
			{
				pTriggerInstance->IncrementNumPendingConnectionInstances();
			}
		}
	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		else if (result != ETriggerResult::DoNotTrack)
		{
			Cry::Audio::Log(ELogType::Warning, R"(Trigger "%s" failed on object "%s" during %s)", GetName(), object.GetName(), __FUNCTION__);
		}
	#endif // CRY_AUDIO_USE_DEBUG_CODE
	}

	if (isPlaying)
	{
		object.RemoveFlag(EObjectFlags::Virtual);
	}
	else if (triggerCounter == 0)
	{
		// If the first trigger is not virtual, the object cannot get set to virtual here afterwards.
		object.SetFlag(EObjectFlags::Virtual);
	}

	if (m_connections.empty())
	{
		Cry::Audio::Log(ELogType::Warning, R"(Trigger "%s" executed on object "%s" without connections)", GetName(), object.GetName());
	}
}

//////////////////////////////////////////////////////////////////////////
void CTrigger::Execute(
	TriggerInstanceId const triggerInstanceId,
	CTriggerInstance* const pTriggerInstance,
	uint16 const triggerCounter) const
{
	for (auto const pConnection : m_connections)
	{
		ETriggerResult const result = pConnection->Execute(g_pIObject, triggerInstanceId);

		if ((result == ETriggerResult::Playing) || (result == ETriggerResult::Virtual) || (result == ETriggerResult::Pending))
		{
			if (result == ETriggerResult::Playing)
			{
				pTriggerInstance->IncrementNumPlayingConnectionInstances();

			}
			else if (result == ETriggerResult::Virtual)
			{
				pTriggerInstance->IncrementNumPlayingConnectionInstances();
			}
			else
			{
				pTriggerInstance->IncrementNumPendingConnectionInstances();
			}
		}
	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		else if (result != ETriggerResult::DoNotTrack)
		{
			Cry::Audio::Log(ELogType::Warning, R"(Trigger "%s" failed during %s)", GetName(), __FUNCTION__);
		}
	#endif // CRY_AUDIO_USE_DEBUG_CODE
	}

	if (m_connections.empty())
	{
		Cry::Audio::Log(ELogType::Warning, R"(Trigger "%s" executed without connections)", GetName());
	}
}
#endif // CRY_AUDIO_USE_DEBUG_CODE
}      // namespace CryAudio
