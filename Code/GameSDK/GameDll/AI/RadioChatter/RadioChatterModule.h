// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../GameAIHelpers.h"

class RadioChatterInstance : public CGameAIInstanceBase
{
};

// Plays radio chatter through a character's intercom every now and then.
// It grabs a couple of the characters closest to the camera and randomly
// picks one of them to play the sound. Sweet.
class RadioChatterModule : public AIModule<RadioChatterModule, RadioChatterInstance, 12, 1>
{
public:
	RadioChatterModule() : m_enabled(false)
	{
		SetDefaultSilenceDuration();
		SetDefaultSound();
	}

	virtual const char* GetName() const override
	{
		return "RadioChatter";
	}

	virtual void Update(float updateTime) override;
	virtual void Reset(bool bUnload) override;
	virtual void Serialize(TSerialize ser) override;

	void SetDefaultSilenceDuration();
	void SetSilenceDuration(float min, float max);

	void SetDefaultSound();
	void SetSound(const char* name);

	void SetEnabled(bool enabled);

private:
	typedef float SquaredDistanceToCamera;
	typedef std::pair<IEntity*, SquaredDistanceToCamera> EntityAndDistance;
	typedef std::vector<EntityAndDistance> ChatterCandidates;

	static bool CloserToCameraPred(const EntityAndDistance& left, const EntityAndDistance& right);

	void PlayChatterOnRandomNearbyAgent();
	void PlayChatterOnEntity(IEntity& entity);
	void GatherCandidates(ChatterCandidates& candidates);
	void RefreshNextChatterTime();

	string m_chatterSoundName;
	CTimeValue m_nextChatterTime;
	float m_minimumSilenceDurationBetweenChatter;
	float m_maximumSilenceDurationBetweenChatter;
	bool m_enabled;
};