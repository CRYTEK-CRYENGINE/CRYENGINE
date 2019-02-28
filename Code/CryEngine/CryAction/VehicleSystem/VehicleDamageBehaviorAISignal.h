// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VEHICLEDAMAGEBEHAVIORAISIGNAL_H__
#define __VEHICLEDAMAGEBEHAVIORAISIGNAL_H__

class CVehicle;

//! Sends a signal to the AI system and a free signal to the vehicle's entity AI on hit.
class CVehicleDamageBehaviorAISignal
	: public IVehicleDamageBehavior
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehicleDamageBehaviorAISignal();
	virtual ~CVehicleDamageBehaviorAISignal() {}

	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table) override;
	virtual void Reset() override;
	virtual void Release() override                                         { delete this; }

	virtual void Serialize(TSerialize ser, EEntityAspects aspects) override {}
	virtual void Update(const float deltaTime) override;

	virtual void OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams) override;
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override {}

	virtual void GetMemoryUsage(ICrySizer* s) const override;

protected:

	void ActivateUpdate(bool activate);

	IVehicle* m_pVehicle;

	bool      m_freeSignalRepeat;
	bool      m_isActive;
	int       m_signalId;
	float     m_freeSignalRadius;
	float     m_timeCounter;
	string    m_signalText;
	string    m_freeSignalText;
};

#endif
