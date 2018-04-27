// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VEHICLEDAMAGEBEHAVIORDISABLESEATACTION_H__
#define __VEHICLEDAMAGEBEHAVIORDISABLESEATACTION_H__

class CVehicle;

//! Disables all actions or a specific action on a seat.
class CVehicleDamageBehaviorDisableSeatAction : public IVehicleDamageBehavior
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehicleDamageBehaviorDisableSeatAction();
	virtual ~CVehicleDamageBehaviorDisableSeatAction() {}

	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table) override;
	virtual void Reset() override;
	virtual void Release() override                                         { delete this; }

	virtual void Serialize(TSerialize ser, EEntityAspects aspects) override {}
	virtual void Update(const float deltaTime) override                     {}

	virtual void OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams) override;
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override {}

	virtual void GetMemoryUsage(ICrySizer* s) const override;

protected:

	CVehicle* m_pVehicle;

	string    m_seatName;
	string    m_seatActionName;
};

#endif
