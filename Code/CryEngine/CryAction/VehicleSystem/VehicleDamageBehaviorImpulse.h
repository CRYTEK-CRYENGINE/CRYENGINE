// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VEHICLEDAMAGEBEHAVIORIMPULSE_H__
#define __VEHICLEDAMAGEBEHAVIORIMPULSE_H__

class CVehicle;

//! Applies a physics impulse to the vehicle.
class CVehicleDamageBehaviorImpulse
	: public IVehicleDamageBehavior
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehicleDamageBehaviorImpulse();
	virtual ~CVehicleDamageBehaviorImpulse() {}

	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table) override;
	virtual void Reset() override;
	virtual void Release() override                                         { delete this; }

	virtual void Serialize(TSerialize ser, EEntityAspects aspects) override {}
	virtual void Update(const float deltaTime) override                     {}

	virtual void OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams) override;
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override {}

	virtual void GetMemoryUsage(ICrySizer* s) const override                                     { s->Add(*this); }

protected:

	CVehicle*       m_pVehicle;

	float           m_forceMin;
	float           m_forceMax;
	Vec3            m_impulseDir;
	Vec3            m_angImpulse;

	IVehicleHelper* m_pImpulseLocation;
	bool            m_worldSpace;
};

#endif
