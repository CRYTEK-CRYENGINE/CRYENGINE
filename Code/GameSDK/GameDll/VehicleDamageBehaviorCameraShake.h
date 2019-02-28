// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IVehicleSystem.h>


//! Shakes the passenger's camera on hit.
class CVehicleDamageBehaviorCameraShake
	: public IVehicleDamageBehavior
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehicleDamageBehaviorCameraShake();
	virtual ~CVehicleDamageBehaviorCameraShake();

	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table) override;
	virtual void Reset() override;
	virtual void Release() override { delete this; }

	virtual void Serialize(TSerialize ser, EEntityAspects aspects) override {}
	virtual void Update(const float deltaTime) override {}

	virtual void OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams) override;
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override;

	virtual void GetMemoryUsage(ICrySizer* s) const override { s->Add(*this); }

protected:

	void ShakeClient(float angle, float shift, float duration, float frequency);

	IVehicle* m_pVehicle;
	float m_damageMult;
};
