// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ===========================================================================
// ====
// ==== Vehicle seat action: Passive animation.
// ====
// ==== This is a simplified version of CVehicleSeatActionPassiveAnimation
// ==== that will only run the animation as long as someone is seated in
// ==== the vehicle.
// ====
// ==== It will also not ForceFinish() on the animation action but justs stops
// ==== it normally, so that we are guaranteed to trigger blend-transitions.
// ====
//

#ifndef __VEHICLESEATACTIONPASSIVEANIMATION_H__
#define __VEHICLESEATACTIONPASSIVEANIMATION_H__

class CVehicleSeatActionPassiveAnimation
	: public IVehicleSeatAction
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehicleSeatActionPassiveAnimation();
	~CVehicleSeatActionPassiveAnimation();

	// IVehicleSeatAction
	virtual bool Init(IVehicle* pVehicle, IVehicleSeat* pSeat, const CVehicleParams& table) override;
	virtual void Reset() override;
	virtual void Release() override { delete this; }

	virtual void StartUsing(EntityId passengerId) override;
	virtual void ForceUsage() override                                                               {}
	virtual void StopUsing() override;
	virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value) override {}

	virtual void Serialize(TSerialize ser, EEntityAspects aspects) override;
	virtual void PostSerialize() override                    {}
	virtual void Update(const float deltaTime) override      {}

	virtual void GetMemoryUsage(ICrySizer* s) const override { s->Add(*this); }
	// ~IVehicleSeatAction

	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override {}

private:

	// Animation control:
	void StartAnimation();
	void StopAnimation();

private:

	IVehicle*                   m_pVehicle;
	TAction<SAnimationContext>* m_pAnimationAction;
	FragmentID                  m_animationFragmentID;

	// The entity ID of the seated user (0 if none).
	EntityId m_userId;
};

#endif // __VEHICLESEATACTIONPASSIVEANIMATION_H__
