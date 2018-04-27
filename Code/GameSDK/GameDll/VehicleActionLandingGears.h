// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a vehicle action for landing gears

-------------------------------------------------------------------------
History:
- 02:06:2006: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEACTIONLANDINGGEARS_H__
#define __VEHICLEACTIONLANDINGGEARS_H__

class CVehicleActionLandingGears
	: public IVehicleAction
{
	IMPLEMENT_VEHICLEOBJECT;

public:

	CVehicleActionLandingGears();
	virtual ~CVehicleActionLandingGears();

	// IVehicleAction
	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table) override;
	virtual void Reset() override;
	virtual void Release() override { delete this; }

	virtual int OnEvent(int eventType, SVehicleEventParams& eventParams) override;
	void GetMemoryStatistics(ICrySizer* s) { s->Add(*this); }
	// ~IVehicleAction

	// IVehicleObject
	virtual void Serialize(TSerialize ser, EEntityAspects aspects) override;
	virtual void Update(const float deltaTime) override;
  virtual void OnVehicleEvent(EVehicleEvent eventType, const SVehicleEventParams& params) override;
	// ~IVehicleObject

	bool AreLandingGearsOpen();
	void ExtractGears();
	void RetractGears();

protected:

	IVehicle* m_pVehicle;

	// Settings variables (do not change outside Init)
	float m_altitudeToRetractGears;
	float m_landingDamages;
	float m_velocityMax;
	float m_minTimeForChange;

	bool m_isOnlyAutoForPlayer;

	IVehicleAnimation* m_pLandingGearsAnim;
	TVehicleAnimStateId m_landingGearOpenedId;
	TVehicleAnimStateId m_landingGearClosedId;

	IVehiclePart* m_pPartToBlockRotation;

	// Status variables
	bool m_isDriverPlayer;
	bool m_isDestroyed;
	float m_damageReceived;

	float m_animTime;
	float m_animGoal;
	float m_timer;
};

#endif
