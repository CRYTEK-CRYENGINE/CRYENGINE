// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a seat action to handle IK on the passenger body

   -------------------------------------------------------------------------
   History:
   - 10:01:2006: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLESEATACTIONPASSENGERIK_H__
#define __VEHICLESEATACTIONPASSENGERIK_H__

class CVehicleSeatActionPassengerIK
	: public IVehicleSeatAction
{
	IMPLEMENT_VEHICLEOBJECT
public:

	virtual bool Init(IVehicle* pVehicle, IVehicleSeat* pSeat, const CVehicleParams& table) override;
	virtual void Reset() override;
	virtual void Release() override { delete this; }

	virtual void StartUsing(EntityId passengerId) override;
	virtual void ForceUsage() override                                                               {}
	virtual void StopUsing() override;
	virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value) override {}

	virtual void Serialize(TSerialize ser, EEntityAspects aspects) override                          {}
	virtual void PostSerialize() override                                                            {}
	virtual void Update(const float deltaTime) override;

	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override {}

	virtual void GetMemoryUsage(ICrySizer* s) const override;

protected:

	IVehicle* m_pVehicle;
	EntityId  m_passengerId;

	struct SIKLimb
	{
		string          limbName;
		IVehicleHelper* pHelper;
		void            GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(limbName);
		}
	};

	typedef std::vector<SIKLimb> TIKLimbVector;
	TIKLimbVector m_ikLimbs;

	bool          m_waitShortlyBeforeStarting;
};

#endif
