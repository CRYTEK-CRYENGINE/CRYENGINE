// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LipSync_TransitionQueue.h"

//=============================================================================
//
// CLipSyncProvider_TransitionQueue
//
//=============================================================================

static const float LIPSYNC_START_TRANSITION_TIME = 0.1f;
static const float LIPSYNC_STOP_TRANSITION_TIME = 0.1f;

uint32 CLipSyncProvider_TransitionQueue::s_lastAnimationToken = 0;

static const char* GetSoundName(const CryAudio::ControlId soundId)
{
	CRY_ASSERT(gEnv && gEnv->pAudioSystem);

	REINST(was retrieving the filename of given soundId;
	       need something similar now)
	//_smart_ptr<ISound> pSound = gEnv->pAudioSystem->GetSound(soundId);
	//return pSound ? pSound->GetName() : NULL;
	return NULL;
}

static void SetAnimationTime(::CAnimation& activatedAnim, const float fSeconds)
{
	CRY_ASSERT(activatedAnim.IsActivated());
	CRY_ASSERT(fSeconds >= 0.0f);

	const float fAnimationDuration = activatedAnim.GetCurrentSegmentExpectedDurationSeconds();
	CRY_ASSERT(fAnimationDuration >= 0.0f);

	const bool isLooping = activatedAnim.HasStaticFlag(CA_LOOP_ANIMATION);

	float fNormalizedTime = 0.0f;
	if (fAnimationDuration > FLT_EPSILON)
	{
		const float fAnimTimeSeconds =
		  isLooping
		  ? fmodf(fSeconds, fAnimationDuration)
		  : std::min<float>(fSeconds, fAnimationDuration);

		fNormalizedTime = fAnimTimeSeconds / fAnimationDuration;
	}
	activatedAnim.SetCurrentSegmentNormalizedTime(fNormalizedTime);
}

CLipSyncProvider_TransitionQueue::CLipSyncProvider_TransitionQueue(EntityId entityId)
	: m_entityId(entityId)
	, m_nCharacterSlot(-1)
	, m_nAnimLayer(-1)
	, m_state(eS_Init)
	, m_isSynchronized(false)
	, m_requestedAnimId(-1)
	, m_nCurrentAnimationToken(0)
	, m_soundId(CryAudio::InvalidControlId)
{
	// read settings from script
	if (IEntity* pEntity = GetEntity())
	{
		if (SmartScriptTable pScriptTable = pEntity->GetScriptTable())
		{
			SmartScriptTable pPropertiesTable;
			if (pScriptTable->GetValue("Properties", pPropertiesTable))
			{
				SmartScriptTable pLipSyncTable;
				if (pPropertiesTable->GetValue("LipSync", pLipSyncTable))
				{
					SmartScriptTable pSettingsTable;
					if (pLipSyncTable->GetValue("TransitionQueueSettings", pSettingsTable))
					{
						pSettingsTable->GetValue("nCharacterSlot", m_nCharacterSlot);
						pSettingsTable->GetValue("nAnimLayer", m_nAnimLayer);
						pSettingsTable->GetValue("sDefaultAnimName", m_sDefaultAnimName);
					}
				}
			}
		}
	}
}

IEntity* CLipSyncProvider_TransitionQueue::GetEntity()
{
	return gEnv->pEntitySystem->GetEntity(m_entityId);
}

ICharacterInstance* CLipSyncProvider_TransitionQueue::GetCharacterInstance()
{
	if (IEntity* pEnt = GetEntity())
	{
		return pEnt->GetCharacter(m_nCharacterSlot);
	}
	else
	{
		return NULL;
	}
}

void CLipSyncProvider_TransitionQueue::FullSerialize(TSerialize ser)
{
	ser.BeginGroup("LipSyncProvider_TransitionQueue");

	ser.Value("m_entityId", m_entityId);
	ser.Value("m_nCharacterSlot", m_nCharacterSlot);
	ser.Value("m_nAnimLayer", m_nAnimLayer);
	ser.Value("m_sDefaultAnimName", m_sDefaultAnimName);
	ser.EnumValue("m_state", m_state, eS_Init, eS_Stopped);
	ser.Value("m_isSynchronized", m_isSynchronized);
	ser.Value("m_requestedAnimId", m_requestedAnimId);
	m_requestedAnimParams.Serialize(ser);
	m_cachedAnim.Serialize(ser);
	ser.Value("m_nCurrentAnimationToken", m_nCurrentAnimationToken);
	ser.Value("m_soundId", m_soundId);

	ser.EndGroup();

	if (ser.IsReading())
	{
		m_isSynchronized = false;
	}
}

void CLipSyncProvider_TransitionQueue::RequestLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod)
{
	CRY_ASSERT(pProxy);
	CRY_ASSERT(audioTriggerId != CryAudio::InvalidControlId);

	if (lipSyncMethod != eLSM_None)
	{
		if (ICharacterInstance* pChar = GetCharacterInstance())
		{
			FindMatchingAnim(audioTriggerId, lipSyncMethod, *pChar, &m_requestedAnimId, &m_requestedAnimParams);

			if (m_requestedAnimId >= 0)
			{
				const uint32 filePathCRC = pChar->GetIAnimationSet()->GetFilePathCRCByAnimID(m_requestedAnimId);
				m_cachedAnim = CAutoResourceCache_CAF(filePathCRC);
			}
			else
			{
				m_cachedAnim = CAutoResourceCache_CAF();
			}
		}
		else
		{
			m_cachedAnim = CAutoResourceCache_CAF();
		}
	}

	m_state = eS_Requested;
}

void CLipSyncProvider_TransitionQueue::StartLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod)
{
	CRY_ASSERT(pProxy);
	CRY_ASSERT(audioTriggerId != CryAudio::InvalidControlId);
	CRY_ASSERT((m_state == eS_Requested) || (m_state == eS_Unpaused));

	if (lipSyncMethod != eLSM_None)
	{
		m_soundId = audioTriggerId;
		m_isSynchronized = false;

		if (ICharacterInstance* pChar = GetCharacterInstance())
		{
			if (m_requestedAnimId >= 0)
			{
				ISkeletonAnim* skeletonAnimation = pChar->GetISkeletonAnim();
				const bool success = skeletonAnimation->StartAnimationById(m_requestedAnimId, m_requestedAnimParams);
				if (success)
				{
					m_nCurrentAnimationToken = m_requestedAnimParams.m_nUserToken;
					SynchronizeAnimationToSound(audioTriggerId);
				}
				else
				{
					m_nCurrentAnimationToken = -1;
				}
			}
		}
	}
	m_state = eS_Started;
}

void CLipSyncProvider_TransitionQueue::PauseLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod)
{
	CRY_ASSERT(pProxy);
	CRY_ASSERT(audioTriggerId != CryAudio::InvalidControlId);
	CRY_ASSERT(audioTriggerId == m_soundId);
	CRY_ASSERT((m_state == eS_Started) || (m_state == eS_Unpaused));

	m_state = eS_Paused;
}

void CLipSyncProvider_TransitionQueue::UnpauseLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod)
{
	CRY_ASSERT(pProxy);
	CRY_ASSERT(audioTriggerId != CryAudio::InvalidControlId);
	CRY_ASSERT(audioTriggerId == m_soundId);
	CRY_ASSERT((m_state == eS_Started) || (m_state == eS_Paused));

	if (lipSyncMethod != eLSM_None)
	{
		m_isSynchronized = false;
		SynchronizeAnimationToSound(audioTriggerId);
	}

	m_state = eS_Unpaused;
}

void CLipSyncProvider_TransitionQueue::StopLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod)
{
	CRY_ASSERT(pProxy);
	CRY_ASSERT(audioTriggerId != CryAudio::InvalidControlId);
	CRY_ASSERT((m_state == eS_Started) || (m_state == eS_Requested) || (m_state == eS_Unpaused) || (m_state == eS_Paused));

	if (lipSyncMethod != eLSM_None)
	{
		if (m_state == eS_Requested)
		{
			CRY_ASSERT(m_soundId == CryAudio::InvalidControlId);
		}
		else
		{
			CRY_ASSERT(audioTriggerId == m_soundId);

			if (ICharacterInstance* pChar = GetCharacterInstance())
			{
				if (m_requestedAnimId >= 0)
				{
					ISkeletonAnim* skeletonAnimation = pChar->GetISkeletonAnim();

					// NOTE: there is no simple way to just stop the exact animation we started, but this should do too:
					bool success = skeletonAnimation->StopAnimationInLayer(m_nAnimLayer, LIPSYNC_STOP_TRANSITION_TIME);
					CRY_ASSERT(success);
				}
			}

			m_soundId = CryAudio::InvalidControlId;
			m_isSynchronized = false;
		}

		m_cachedAnim = CAutoResourceCache_CAF();
	}
	m_state = eS_Stopped;
}

void CLipSyncProvider_TransitionQueue::UpdateLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod)
{
	CRY_ASSERT(pProxy);

	if (lipSyncMethod != eLSM_None)
	{
		if ((m_state == eS_Started) || (m_state == eS_Unpaused))
		{
			CRY_ASSERT(audioTriggerId == m_soundId);

			SynchronizeAnimationToSound(m_soundId);
		}
	}
}

void CLipSyncProvider_TransitionQueue::FillCharAnimationParams(const bool isDefaultAnim, CryCharAnimationParams* pParamsOut) const
{
	*pParamsOut = CryCharAnimationParams();

	pParamsOut->m_fTransTime = LIPSYNC_START_TRANSITION_TIME;
	pParamsOut->m_nLayerID = m_nAnimLayer;
	pParamsOut->m_nUserToken = ++s_lastAnimationToken;
	pParamsOut->m_nFlags = CA_ALLOW_ANIM_RESTART;

	if (isDefaultAnim)
	{
		pParamsOut->m_nFlags |= CA_LOOP_ANIMATION;
		pParamsOut->m_fPlaybackSpeed = 1.25f;
	}
}

void CLipSyncProvider_TransitionQueue::FindMatchingAnim(const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod, ICharacterInstance& character, int* pAnimIdOut, CryCharAnimationParams* pAnimParamsOut) const
{
	CRY_ASSERT(audioTriggerId != CryAudio::InvalidControlId);
	CRY_ASSERT(pAnimIdOut != NULL);
	CRY_ASSERT(pAnimParamsOut != NULL);

	const char* szSoundName = ::GetSoundName(audioTriggerId);
	CRY_ASSERT(szSoundName);

	// Look for an animation matching the sound name exactly

	string matchingAnimationName = PathUtil::GetFileName(szSoundName);
	const IAnimationSet* pAnimSet = character.GetIAnimationSet();
	int nAnimId = pAnimSet->GetAnimIDByName(matchingAnimationName.c_str());

	if (nAnimId < 0)
	{
		// First fallback: look for an animation matching the sound name without the index at the end

		int index = static_cast<int>(matchingAnimationName.length()) - 1;
		while ((index >= 0) && isdigit((unsigned char)matchingAnimationName[index]))
		{
			--index;
		}

		if ((index > 0) && (matchingAnimationName[index] == '_'))
		{
			matchingAnimationName = matchingAnimationName.Left(index);

			nAnimId = pAnimSet->GetAnimIDByName(matchingAnimationName.c_str());
		}
	}

	bool isDefaultAnim = false;

	if (nAnimId < 0)
	{
		// Second fallback: when requested use a default lip movement animation

		if (lipSyncMethod == eLSM_MatchAnimationToSoundName)
		{
			nAnimId = pAnimSet->GetAnimIDByName(m_sDefaultAnimName.c_str());
			if (nAnimId < 0)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "No '%s' default animation found for face '%s'. Automatic lip movement will not work.", m_sDefaultAnimName.c_str(), character.GetFilePath());
			}
			isDefaultAnim = true;
		}
	}

	*pAnimIdOut = nAnimId;
	FillCharAnimationParams(isDefaultAnim, pAnimParamsOut);
}

void CLipSyncProvider_TransitionQueue::SynchronizeAnimationToSound(const CryAudio::ControlId audioTriggerId)
{
	CRY_ASSERT(audioTriggerId != CryAudio::InvalidControlId);
	CRY_ASSERT(gEnv->pAudioSystem);

	if (m_isSynchronized)
		return;

	REINST(was retrieving the current playback position in milliseconds of given audioTriggerId)
	//_smart_ptr<ISound> pSound = gEnv->pSoundSystem ? gEnv->pSoundSystem->GetSound(nAudioTriggerId) : NULL;

	//// Workaround for crash TFS-301214.
	//// The crash happens because the sound already stopped but we didn't get the event yet.
	//// The early out here assumes we will still get that event later on.
	//if (!pSound)
	//	return;

	//ICharacterInstance* pChar = GetCharacterInstance();
	//if (!pChar)
	//	return;

	//const unsigned int nSoundMillis = pSound->GetInterfaceExtended()->GetCurrentSamplePos(true);
	//const float fSeconds = static_cast<float>(nSoundMillis)/1000.0f;

	//ISkeletonAnim* skeletonAnimation = pChar->GetISkeletonAnim();
	//::CAnimation* pAnim = skeletonAnimation->FindAnimInFIFO(m_nCurrentAnimationToken, m_nAnimLayer);
	//if (pAnim && pAnim->IsActivated())
	//{
	//	::SetAnimationTime(*pAnim, fSeconds);
	//	m_isSynchronized = true;
	//}
}

//=============================================================================
//
// CLipSync_TransitionQueue
//
//=============================================================================

void CLipSync_TransitionQueue::InjectLipSyncProvider()
{
	IEntity* pEntity = GetEntity();
	IEntityAudioComponent* pSoundProxy = pEntity->GetOrCreateComponent<IEntityAudioComponent>();
	CRY_ASSERT(pSoundProxy);
	m_pLipSyncProvider.reset(new CLipSyncProvider_TransitionQueue(pEntity->GetId()));
	REINST(add SetLipSyncProvider to interface)
	//pSoundProxy->SetLipSyncProvider(m_pLipSyncProvider);
}

void CLipSync_TransitionQueue::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
	if (m_pLipSyncProvider)
	{
		pSizer->Add(*m_pLipSyncProvider);
	}
}

bool CLipSync_TransitionQueue::Init(IGameObject* pGameObject)
{
	SetGameObject(pGameObject);
	return true;
}

void CLipSync_TransitionQueue::PostInit(IGameObject* pGameObject)
{
	InjectLipSyncProvider();
}

void CLipSync_TransitionQueue::InitClient(int channelId)
{
}

void CLipSync_TransitionQueue::PostInitClient(int channelId)
{
}

bool CLipSync_TransitionQueue::ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params)
{
	ResetGameObject();
	return true;
}

void CLipSync_TransitionQueue::PostReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params)
{
	InjectLipSyncProvider();
}

void CLipSync_TransitionQueue::FullSerialize(TSerialize ser)
{
	ser.BeginGroup("LipSync_TransitionQueue");

	bool bLipSyncProviderIsInjected = (m_pLipSyncProvider != NULL);
	ser.Value("bLipSyncProviderIsInjected", bLipSyncProviderIsInjected);
	if (bLipSyncProviderIsInjected && !m_pLipSyncProvider)
	{
		CRY_ASSERT(ser.IsReading());
		InjectLipSyncProvider();
	}
	if (m_pLipSyncProvider)
	{
		m_pLipSyncProvider->FullSerialize(ser);
	}

	ser.EndGroup();
}

bool CLipSync_TransitionQueue::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int pflags)
{
	return true;
}

void CLipSync_TransitionQueue::PostSerialize()
{
}

void CLipSync_TransitionQueue::SerializeSpawnInfo(TSerialize ser)
{
}

ISerializableInfoPtr CLipSync_TransitionQueue::GetSpawnInfo()
{
	return NULL;
}

void CLipSync_TransitionQueue::Update(SEntityUpdateContext& ctx, int updateSlot)
{
}

void CLipSync_TransitionQueue::HandleEvent(const SGameObjectEvent& event)
{
}

void CLipSync_TransitionQueue::ProcessEvent(const SEntityEvent& event)
{
}

void CLipSync_TransitionQueue::SetChannelId(uint16 id)
{
}

void CLipSync_TransitionQueue::PostUpdate(float frameTime)
{
}

void CLipSync_TransitionQueue::PostRemoteSpawn()
{
}

void CLipSync_TransitionQueue::OnShutDown()
{
	IEntity* pEntity = GetEntity();
	if (IEntityAudioComponent* pSoundProxy = pEntity->GetComponent<IEntityAudioComponent>())
	{
		REINST(add SetLipSyncProvider to interface)
		//pSoundProxy->SetLipSyncProvider(ILipSyncProviderPtr());
	}
}
