// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "PerceptionComponentHelpers.h"
#include <CryAISystem/Components/IEntityObservableComponent.h>

namespace Schematyc
{
	struct IEnvRegistrar;
}

class CEntityAIObservableComponent : public IEntityObservableComponent
{
public:
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

	// IEntityObservableComponent
	virtual void SetTypeMask(const uint32 typeMask) override;
	virtual void AddObservableLocationOffsetFromPivot(const Vec3& offsetFromPivot) override;
	virtual void AddObservableLocationOffsetFromBone(const Vec3& offsetFromBone, const char* szBoneName) override;
	virtual void SetObservableLocationOffsetFromPivot(const size_t index, const Vec3& offsetFromPivot) override;
	virtual void SetObservableLocationOffsetFromBone(const size_t index, const Vec3& offsetFromBone, const char* szBoneName) override;
	// ~IEntityObservableComponent

private:
	void Update();
	void Reset(EEntitySimulationMode simulationMode);

	void RegisterToVisionMap();
	void UnregisterFromVisionMap();
	bool IsRegistered() const { return m_observableId != 0; }

	void SyncWithEntity();
	void UpdateChange();
	void UpdateChange(const uint32 changeHintFlags);

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

	// Properties

	Perception::ComponentHelpers::SVisionMapType m_visionMapType;
	Perception::ComponentHelpers::SLocationsArray m_observableLocations;
};
