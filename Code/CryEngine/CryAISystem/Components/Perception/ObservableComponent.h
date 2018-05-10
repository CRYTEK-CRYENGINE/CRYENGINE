// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "PerceptionComponentHelpers.h"

namespace Schematyc
{
	struct IEnvRegistrar;
}

class CEntityAIObservableComponent
	: public IEntityComponent
{
public:
	static const CryGUID& IID()
	{
		static CryGUID id = "5A32746A-9CEA-4877-B9E4-5C4E06EEE90C"_cry_guid;
		return id;
	}

	static void ReflectType(Schematyc::CTypeDesc<CEntityAIObservableComponent>& desc);
	static void Register(Schematyc::IEnvRegistrar& registrar);

	CEntityAIObservableComponent();
	virtual ~CEntityAIObservableComponent();

protected:

	// IEntityComponent
	virtual void OnShutDown() override;

	virtual void ProcessEvent(const SEntityEvent& event) override;
	virtual uint64 GetEventMask() const override { return m_entityEventMask; };
	// ~IEntityComponent

private:
	void Update();
	void Reset(EEntitySimulationMode simulationMode);

	void RegisterToVisionMap();
	void UnregisterFromVisionMap();
	bool IsRegistered() const { return m_observableId != 0; }

	void SyncWithEntity();
	void UpdateChange();

	void OnObservableVisionChanged(const VisionID& observerId, const ObserverParams& observerParams, const VisionID& observableId, const ObservableParams& observableParams, bool visible);

	bool IsUsingBones() const
	{
		for (const Perception::ComponentHelpers::SLocation& location : m_observableLocations.locations)
		{
			if (location.type == Perception::ComponentHelpers::SLocation::EType::Bone)
				return true;
		}
		return false;
	}

	uint64 m_entityEventMask;

	ObservableID m_observableId;
	ObservableParams m_params;
	EChangeHint m_changeHintFlags;

	Perception::ComponentHelpers::SVisionMapType m_visionMapType;
	Perception::ComponentHelpers::SLocationsArray m_observableLocations;
};
