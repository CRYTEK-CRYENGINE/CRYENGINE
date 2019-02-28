// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a vehicle action for an automatic door

-------------------------------------------------------------------------
History:
- 02:06:2006: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEACTIONAUTOMATICDOOR_H__
#define __VEHICLEACTIONAUTOMATICDOOR_H__

class CVehicleActionAutomaticDoor
: public IVehicleAction
{
	IMPLEMENT_VEHICLEOBJECT;

public:

	CVehicleActionAutomaticDoor();
	virtual ~CVehicleActionAutomaticDoor();

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
  virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override;
	// ~IVehicleObject

	void OpenDoor(bool value);
	void BlockDoor(bool value);

	bool IsOpened();

protected:

	IVehicle* m_pVehicle;
	
	IVehicleAnimation* m_pDoorAnim;
	TVehicleAnimStateId m_doorOpenedStateId;
	TVehicleAnimStateId m_doorClosedStateId;

	float m_timeMax;
	float m_eventSamplingTime;
	bool m_isTouchingGroundBase;

	float m_timeOnTheGround;
	float m_timeInTheAir;
	bool m_isTouchingGround;
	bool m_isOpenRequested;
	bool m_isBlocked;
	bool m_isDisabled;

	float m_animGoal;
	float m_animTime;

	friend class CScriptBind_AutomaticDoor;
};

#endif
