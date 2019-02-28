// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VEHICLEDAMAGEBEHAVIORBURN_H__
#define __VEHICLEDAMAGEBEHAVIORBURN_H__

class CVehicle;

//! Generates a controlled explosion during 60sec that will apply damage to nearby actors.
class CVehicleDamageBehaviorBurn
	: public IVehicleDamageBehavior
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehicleDamageBehaviorBurn();
	virtual ~CVehicleDamageBehaviorBurn();

	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table) override;
	virtual void Reset() override;
	virtual void Release() override { delete this; }

	virtual void Serialize(TSerialize ser, EEntityAspects aspects) override;
	virtual void Update(const float deltaTime) override;

	virtual void OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams) override;
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override;

	virtual void GetMemoryUsage(ICrySizer* s) const override { s->Add(*this); }

protected:

	void Activate(bool activate);

	IVehicle* m_pVehicle;
	IVehicleHelper* m_pHelper;

	float m_damageRatioMin;
	float m_damage;
  float m_selfDamage;
	float m_interval;
	float m_radius;

	bool m_isActive;
	float m_timeCounter;

	EntityId m_shooterId;
	int m_timerId;
};

#endif
