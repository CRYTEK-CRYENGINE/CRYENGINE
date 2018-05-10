// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RadioChatterModule.h"

void RadioChatterModule::Update(float updateTime)
{
	IF_UNLIKELY (!m_enabled)
		return;

	const CTimeValue& now = gEnv->pTimer->GetFrameStartTime();

	if (now > m_nextChatterTime)
	{
		PlayChatterOnRandomNearbyAgent();
		RefreshNextChatterTime();
	}
}

void RadioChatterModule::Reset(bool bUnload)
{
	BaseClass::Reset(bUnload);

	m_enabled = true;

	SetDefaultSilenceDuration();
	SetDefaultSound();
	RefreshNextChatterTime();
}

void RadioChatterModule::Serialize(TSerialize ser)
{
	ser.Value("minSilence", m_minimumSilenceDurationBetweenChatter);
	ser.Value("maxSilence", m_maximumSilenceDurationBetweenChatter);
	ser.Value("soundName", m_chatterSoundName);
	ser.Value("enabled", m_enabled);

	if (ser.IsReading())
	{
		RefreshNextChatterTime();
	}
}

void RadioChatterModule::SetDefaultSilenceDuration()
{
	SetSilenceDuration(4.0f, 12.0f);
}

void RadioChatterModule::SetSilenceDuration(float min, float max)
{
	min = std::max(1.0f, min);
	max = std::max(1.0f, max);

	min = std::min(min, max);
	max = std::max(min, max);

	m_minimumSilenceDurationBetweenChatter = min;
	m_maximumSilenceDurationBetweenChatter = max;

	RefreshNextChatterTime();
}

void RadioChatterModule::SetDefaultSound()
{
	SetSound("Sounds/dialog:dialog:walkie_talkie");
}

void RadioChatterModule::SetSound(const char* name)
{
	m_chatterSoundName = name;
}

void RadioChatterModule::SetEnabled(bool enabled)
{
	m_enabled = enabled;

	if (enabled)
	{
		RefreshNextChatterTime();
	}
}

bool RadioChatterModule::CloserToCameraPred(const EntityAndDistance& left, const EntityAndDistance& right)
{
	return left.second < right.second;
}

void RadioChatterModule::PlayChatterOnRandomNearbyAgent()
{
	ChatterCandidates candidates;
	GatherCandidates(candidates);
	if (candidates.empty())
		return;

	std::sort(candidates.begin(), candidates.end(), CloserToCameraPred);

	if (candidates.size() > 3)
		candidates.resize(3);

	ChatterCandidates::size_type randomCandidateIndex = cry_random((size_t)0, candidates.size() - 1);

	IEntity* entity = candidates[randomCandidateIndex].first;
	assert(entity);
	PlayChatterOnEntity(*entity);
}

void RadioChatterModule::PlayChatterOnEntity(IEntity& entity)
{
	/*IEntity* pEnt = &entity;

	IEntityAudioComponent *pIEntityAudioComponent = pEnt->GetOrCreateComponent<IEntityAudioComponent*>();
	ISound* pSound = gEnv->pSoundSystem->CreateSound(m_chatterSoundName, FLAG_SOUND_DEFAULT_3D | FLAG_SOUND_VOICE);
	if (pSound && pIEntityAudioComponent)
	{
		pSound->SetSemantic(eSoundSemantic_FlowGraph);
		pIEntityAudioComponent->PlaySound(pSound, Vec3(ZERO), FORWARD_DIRECTION, 1.0f);
	}*/
}

void RadioChatterModule::GatherCandidates(ChatterCandidates& candidates)
{
	const Vec3 cameraPosition = GetISystem()->GetViewCamera().GetMatrix().GetTranslation();

	Instances::const_iterator it = m_running->begin();
	Instances::const_iterator end = m_running->end();

	for ( ; it != end; ++it)
	{
		InstanceID instanceID = it->second;
		const RadioChatterInstance* instance = GetInstanceFromID(instanceID);
		assert(instance);
		if (instance)
		{
			if (IEntity* entity = instance->GetEntity())
			{
				const float distanceToCamera = entity->GetWorldPos().GetSquaredDistance(cameraPosition);
				candidates.push_back(std::make_pair(entity, distanceToCamera));
			}
		}
	}
}

void RadioChatterModule::RefreshNextChatterTime()
{
	const CTimeValue& now = gEnv->pTimer->GetFrameStartTime();

	const float silenceDuration = cry_random(
		m_minimumSilenceDurationBetweenChatter,
		m_maximumSilenceDurationBetweenChatter);

	m_nextChatterTime = now + CTimeValue(silenceDuration);
}
