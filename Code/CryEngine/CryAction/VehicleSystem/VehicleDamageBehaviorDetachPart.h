// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "VehicleSystem/VehiclePartBase.h"

class CVehicle;
class CVehiclePartBase;
struct IParticleEffect;

//! Detaches this part and its children from the vehicle with an impulse.
//! The behaviour is applicable to any part type as long as it has static geometry and is subject to some randomness for variability.
class CVehicleDamageBehaviorDetachPart
	: public IVehicleDamageBehavior
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehicleDamageBehaviorDetachPart();
	virtual ~CVehicleDamageBehaviorDetachPart();

	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table) override;
	virtual bool Init(IVehicle* pVehicle, const string& partName, const string& effectName);
	virtual void Reset() override;
	virtual void Release() override { delete this; }

	virtual void Serialize(TSerialize ser, EEntityAspects aspects) override;
	virtual void Update(const float deltaTime) override {}

	virtual void OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams) override;
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override;

	virtual void GetMemoryUsage(ICrySizer* s) const override;

protected:

	IEntity* SpawnDetachedEntity();
	bool     MovePartToTheNewEntity(IEntity* pTargetEntity, CVehiclePartBase* pPart);
	void     AttachParticleEffect(IEntity* pDetachedEntity, IParticleEffect* pEffect);

	CVehicle* m_pVehicle;
	string    m_partName;

	typedef std::pair<CVehiclePartBase*, IStatObj*> TPartObjectPair;
	typedef std::vector<TPartObjectPair>            TDetachedStatObjs;
	TDetachedStatObjs m_detachedStatObjs;

	EntityId          m_detachedEntityId;

	IParticleEffect*  m_pEffect;
	bool              m_pickableDebris;
	bool              m_notifyMovement;
};
