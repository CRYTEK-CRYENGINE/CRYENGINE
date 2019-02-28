// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VEHICLEDAMAGEBEHAVIORDESTROY_H__
#define __VEHICLEDAMAGEBEHAVIORDESTROY_H__

class CVehicle;

//! Notifies all vehicle systems and components of a vehicle destruction event.
//! This behaviour triggers other damage behaviours that react to vehicle destruction and hooks with the lua 'OnVehicleDestroyed' function.
//! Destroy also adds a 'DetachPart' damage behaviour to all vehicle parts of type 'AnimatedJoint' (in CVehicleDamagesGroup::ParseDamagesGroup).
class CVehicleDamageBehaviorDestroy
	: public IVehicleDamageBehavior
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehicleDamageBehaviorDestroy();
	virtual ~CVehicleDamageBehaviorDestroy() {}

	virtual bool          Init(IVehicle* pVehicle, const CVehicleParams& table) override;
	virtual void          Reset() override;
	virtual void          Release() override { delete this; }

	virtual void          Serialize(TSerialize ser, EEntityAspects aspects) override;
	virtual void          Update(const float deltaTime) override {}

	virtual void          OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams) override;
	virtual void          OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override {}

	virtual void          GetMemoryUsage(ICrySizer* s) const override                                     { s->Add(*this); }

	virtual const string& GetEffectName() const                                                           { return m_effectName; }

protected:

	void SetDestroyed(bool isDestroyed, EntityId shooterId);

	CVehicle* m_pVehicle;

	string    m_effectName;
};

#endif
