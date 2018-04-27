// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "PerceptionComponentHelpers.h"

#include <CrySerialization/Decorators/BitFlags.h>
#include <CryAISystem/VisionMapTypes.h>
#include <CryAISystem/Components/IEntityObserverComponent.h>

namespace Schematyc
{
struct IEnvRegistrar;
}

namespace ObserverProperties
{
struct SVisionBlockingProperties
{
	static void ReflectType(Schematyc::CTypeDesc<SVisionBlockingProperties>& typeInfo)
	{
		typeInfo.SetGUID("37c2c87d-97ba-42d2-90a6-bcdc8217bd0a"_cry_guid);
	}

	void Serialize(Serialization::IArchive& archive)
	{
		archive(bBlockedBySolids, "blockedBySolids", "Blocked By Solids");
		archive.doc("If enabled, vision ray-casts cannot pass through colliders that are part of solid objects (sometimes also denoted as 'hard cover').");
		archive(bBlockedBySoftCover, "blockedBySoftCover", "Blocked By Soft Cover");
		archive.doc(
		  "If enabled, vision ray-casts cannot pass through colliders marked as 'soft cover'. "
		  "These are colliders that only block vision but you can still sit 'inside' them (examples are: bushes, tall grass, etc). ");
	}

	bool operator==(const SVisionBlockingProperties& other) const
	{
		return bBlockedBySolids == other.bBlockedBySolids && bBlockedBySoftCover == other.bBlockedBySoftCover;
	}

	bool bBlockedBySolids = true;
	bool bBlockedBySoftCover = true;
};

struct SVisionProperties
{
	static void ReflectType(Schematyc::CTypeDesc<SVisionProperties>& typeInfo)
	{
		typeInfo.SetGUID("aa29f1ae-d479-432a-b638-c2d420b0a6e6"_cry_guid);
	}

	void Serialize(Serialization::IArchive& archive)
	{
		archive(fov, "fov", "Field of View");
		archive.doc("Field of view in degrees");

		archive(range, "range", "Sight Range");
		archive.doc("The maximum sight distance from the 'eye' point to an observable location on an entity.");

		archive(location, "location", "Location");
		archive.doc("The location of the sensor.");

		archive(blockingProperties, "blockingProps", "Vision Blocking");
		archive.doc("Vision blocking properties.");
	}

	bool operator==(const SVisionProperties& other) const
	{
		return location == other.location
		       && fov == other.fov
		       && range == other.range
		       && blockingProperties == other.blockingProperties;
	}

	Perception::ComponentHelpers::SLocation location;
	CryTransform::CClampedAngle<0, 360>     fov = 60.0_degrees;
	Schematyc::Range<0, 1000, 0, 1000>      range = 20.0f;
	SVisionBlockingProperties               blockingProperties;
};

}

class CEntityAIObserverComponent : public IEntityObserverComponent
{
public:
	static void ReflectType(Schematyc::CTypeDesc<CEntityAIObserverComponent>& desc);
	static void Register(Schematyc::IEnvRegistrar& registrar);

	CEntityAIObserverComponent();
	virtual ~CEntityAIObserverComponent();

protected:

	// IEntityComponent
	virtual void   OnShutDown() override;

	virtual void   ProcessEvent(const SEntityEvent& event) override;
	virtual uint64 GetEventMask() const override { return m_entityEventMask; };
	// ~IEntityComponent

	// IEntityObserverComponent
	virtual bool CanSee(const EntityId entityId) const override;

	virtual void SetUserConditionCallback(const UserConditionCallback callback) override;
	virtual void SetTypeMask(const uint32 typeMask) override;
	virtual void SetTypesToObserveMask(const uint32 typesToObserverMask) override;
	virtual void SetFactionsToObserveMask(const uint32 factionsToObserveMask) override;
	virtual void SetFOV(const float fovInRad) override;
	virtual void SetRange(const float range) override;
	virtual void SetPivotOffset(const Vec3& offsetFromPivot) override;
	virtual void SetBoneOffset(const Vec3& offsetFromBone, const char* szBoneName) override;

	virtual uint32 GetTypeMask() const override;
	virtual uint32 GetTypesToObserveMask() const override;
	virtual uint32 GetFactionsToObserveMask() const override;
	virtual float GetFOV() const override;
	virtual float GetRange() const override;
	// ~IEntityObserverComponent

private:
	void   Update();
	void   Reset(EEntitySimulationMode simulationMode);

	void   RegisterToVisionMap();
	void   UnregisterFromVisionMap();
	bool   IsRegistered() const { return m_observerId != 0; }

	void   SyncWithEntity();
	void   UpdateChange();
	void   UpdateChange(uint32 changeHintFlags);

	bool   CanSeeSchematyc(Schematyc::ExplicitEntityId entityId) const;

	bool   OnObserverUserCondition(const VisionID& observerId, const ObserverParams& observerParams, const VisionID& observableId, const ObservableParams& observableParams);
	void   ObserverUserConditionResult(bool bResult);

	void   OnObserverVisionChanged(const VisionID& observerId, const ObserverParams& observerParams, const VisionID& observableId, const ObservableParams& observableParams, bool visible);

	uint32 GetRaycastFlags() const;

	ObserverID                   m_observerId;
	ObserverParams               m_params;
	EChangeHint                  m_changeHintFlags;
	bool                         m_bUserConditionResult = false;

	std::unordered_set<EntityId> m_visibleEntitiesSet;
	uint64                       m_entityEventMask;

	// Properties

	Perception::ComponentHelpers::SVisionMapType m_visionMapType;
	ObserverProperties::SVisionProperties        m_visionProperties;
	Perception::ComponentHelpers::SVisionMapType m_typesToObserve;
	SFactionFlagsMask                            m_factionsToObserve;
	UserConditionCallback                        m_userConditionCallback;
	bool                                         m_bUseUserCustomCondition = false;
};
