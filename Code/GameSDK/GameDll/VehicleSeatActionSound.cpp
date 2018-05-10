// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IVehicleSystem.h"
#include "VehicleSeatActionSound.h"

#include <IPerceptionManager.h>

const NetworkAspectType ASPECT_SEAT_ACTION = eEA_GameClientDynamic;

CVehicleSeatActionSound::CVehicleSeatActionSound()
	: m_pVehicle(nullptr)
	, m_pHelper(nullptr)
	, m_pSeat(nullptr)
	, m_enabled(false)
	, m_audioTriggerStartId(CryAudio::InvalidControlId)
	, m_audioTriggerStopId(CryAudio::InvalidControlId)
{
}

//------------------------------------------------------------------------
bool CVehicleSeatActionSound::Init(IVehicle* pVehicle, IVehicleSeat* pSeat, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;
	m_pSeat = pSeat;

	CVehicleParams soundTable = table.findChild("Audio");
	if (!soundTable)
		return false;

	m_audioTriggerStartId = CryAudio::StringToId(soundTable.getAttr("startTrigger"));
	m_audioTriggerStopId = CryAudio::StringToId(soundTable.getAttr("stopTrigger"));

	if (soundTable.haveAttr("helper"))
		m_pHelper = m_pVehicle->GetHelper(soundTable.getAttr("helper"));

	if (!m_pHelper)
		return false;

	m_enabled = false;
	return true;
}

//------------------------------------------------------------------------
void CVehicleSeatActionSound::Serialize(TSerialize ser, EEntityAspects aspects)
{
	if (aspects & ASPECT_SEAT_ACTION)
	{
		NET_PROFILE_SCOPE("SeatAction_Sound", ser.IsReading());

		bool enabled = m_enabled;

		ser.Value("enabled", enabled, 'bool');

		if (ser.IsReading())
		{
			if (m_enabled != enabled)
			{
				if (enabled)
					ExecuteTrigger(m_audioTriggerStartId);
				else
					StopTrigger();

				m_enabled = enabled;
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionSound::StopUsing()
{
	if (m_enabled)
		StopTrigger();
}

//------------------------------------------------------------------------
void CVehicleSeatActionSound::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	if (actionId == eVAI_Horn && activationMode == eAAM_OnPress)
	{
		ExecuteTrigger(m_audioTriggerStartId);
	}
	else if (actionId == eVAI_Horn && activationMode == eAAM_OnRelease)
	{
		StopTrigger();
	}
}

void CVehicleSeatActionSound::GetMemoryUsage(ICrySizer* s) const
{
	s->AddObject(this, sizeof(*this));
}

void CVehicleSeatActionSound::ExecuteTrigger(const CryAudio::ControlId& controlID)
{
	if (controlID == CryAudio::InvalidControlId)
		return;

	if (m_pSeat)
		m_pSeat->ChangedNetworkState(ASPECT_SEAT_ACTION);

	IEntityAudioComponent* pIEntityAudioComponent = m_pVehicle->GetEntity()->GetOrCreateComponent<IEntityAudioComponent>();
	assert(pIEntityAudioComponent);

	pIEntityAudioComponent->ExecuteTrigger(controlID);

	// Report the Perception system about the vehicle movement sound.
	if (!gEnv->bMultiplayer && gEnv->pAISystem)
	{
		if (IPerceptionManager::GetInstance())
		{
			SAIStimulus stim(AISTIM_SOUND, AISOUND_MOVEMENT_LOUD, m_pVehicle->GetEntityId(), 0, m_pHelper->GetVehicleSpaceTranslation(), ZERO, 200.0f);
			IPerceptionManager::GetInstance()->RegisterStimulus(stim);
		}
	}
	m_enabled = true;
}

void CVehicleSeatActionSound::StopTrigger()
{
	if (m_pSeat)
		m_pSeat->ChangedNetworkState(ASPECT_SEAT_ACTION);

	if (m_audioTriggerStopId != CryAudio::InvalidControlId)
	{
		ExecuteTrigger(m_audioTriggerStopId);
	}
	else if (m_audioTriggerStartId != CryAudio::InvalidControlId)
	{
		IEntityAudioComponent* pIEntityAudioComponent = m_pVehicle->GetEntity()->GetOrCreateComponent<IEntityAudioComponent>();
		assert(pIEntityAudioComponent);
		pIEntityAudioComponent->StopTrigger(m_audioTriggerStartId);
	}

	m_enabled = false;
}

DEFINE_VEHICLEOBJECT(CVehicleSeatActionSound);
