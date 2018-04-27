// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IVehicleSystem.h>


//! Generate an explosion on vehicle destruction
class CVehicleDamageBehaviorExplosion
	: public IVehicleDamageBehavior
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehicleDamageBehaviorExplosion();
	virtual ~CVehicleDamageBehaviorExplosion() {}

	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table) override;
	virtual void Reset() override;
	virtual void Release() override { delete this; }

	virtual void Serialize(TSerialize ser, EEntityAspects aspects) override;
	virtual void Update(const float deltaTime) override {}

	virtual void OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams) override;
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override {}

	virtual void GetMemoryUsage(ICrySizer* s) const override { s->Add(*this); }

protected:

	IVehicle* m_pVehicle;

	float m_damage;
	float m_minRadius;
	float m_radius;
	float m_minPhysRadius;
	float m_physRadius;
	float m_pressure;
	float m_soundRadius;
	IVehicleHelper* m_pHelper;

	bool m_exploded;
};
