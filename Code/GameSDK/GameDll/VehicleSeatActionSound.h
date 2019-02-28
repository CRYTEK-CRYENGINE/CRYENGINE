// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a seat action for sounds (ie: honk on trucks)

   -------------------------------------------------------------------------
   History:
   - 16:11:2005: Created by Mathieu Pinard

*************************************************************************/

#ifndef __VEHICLESEATACTIONSOUND_H__
#define __VEHICLESEATACTIONSOUND_H__

class CVehicleSeatActionSound
	: public IVehicleSeatAction
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehicleSeatActionSound();

	virtual bool Init(IVehicle* pVehicle, IVehicleSeat* pSeat, const CVehicleParams& table) override;
	virtual void Reset() override                          {}
	virtual void Release() override                        { delete this; }

	virtual void StartUsing(EntityId passengerId) override {}
	virtual void ForceUsage() override                     {}
	virtual void StopUsing() override;
	virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value) override;

	virtual void Serialize(TSerialize ser, EEntityAspects aspects) override;
	virtual void PostSerialize() override               {}
	virtual void Update(const float deltaTime) override {}

	virtual void ExecuteTrigger(const CryAudio::ControlId& controlID);
	virtual void StopTrigger();

	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override {}

	virtual void GetMemoryUsage(ICrySizer* s) const override;

protected:

	IVehicle*           m_pVehicle;
	IVehicleHelper*     m_pHelper;
	IVehicleSeat*       m_pSeat;

	bool                m_enabled;

	CryAudio::ControlId m_audioTriggerStartId;
	CryAudio::ControlId m_audioTriggerStopId;
};

#endif
