// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "PerceptionComponentHelpers.h"
#include <CryAISystem/IAuditionMap.h>

namespace Schematyc
{
	struct IEnvRegistrar;
}

class CEntityAIListenerComponent
	: public IEntityComponent
{
public:
	static const CryGUID& IID()
	{
		static CryGUID id = "B8A61764-31CC-4B76-A6CF-05F754E62024"_cry_guid;
		return id;
	}

	static void ReflectType(Schematyc::CTypeDesc<CEntityAIListenerComponent>& desc);
	static void Register(Schematyc::IEnvRegistrar& registrar);

	CEntityAIListenerComponent();
	virtual ~CEntityAIListenerComponent();

	// IEntityComponent
	virtual void OnShutDown() override;

	virtual void ProcessEvent(const SEntityEvent& event) override;
	virtual uint64 GetEventMask() const override { return m_entityEventMask; };
	// ~IEntityComponent

private:
	void Update();
	void Reset(EEntitySimulationMode simulationMode);

	void RegisterToAuditionMap();
	void UnregisterFromAuditionMap();
	bool IsRegistered() const { return m_registeredEntityId != INVALID_ENTITYID; }

	void SyncWithEntity();
	void UpdateChange();

	void ListenerUserConditionResult(bool bResult);
	bool OnListenerUserCondition(const Perception::SSoundStimulusParams& soundParams);
	void OnSoundHeard(const Perception::SSoundStimulusParams& soundParams);

	bool IsUsingBones() const;

	EntityId m_registeredEntityId = INVALID_ENTITYID;
	bool     m_userConditionResult = false;

	SFactionFlagsMask      m_factionsToListen = { 0 };
	float                  m_distanceScale = 1.0f;
	Perception::ComponentHelpers::SLocationsArray m_earLocations;
	bool m_bUserCustomCondition = false;

	Perception::SListenerParams m_params;
	Perception::ListenerParamsChangeOptions::Value m_changeHintFlags;
	uint64 m_entityEventMask;
};
