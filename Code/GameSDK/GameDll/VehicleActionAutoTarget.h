// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Makes the vehicle a target for auto asist

*************************************************************************/
#ifndef __VEHICLEACTIONAUTOTARGET_H__
#define __VEHICLEACTIONAUTOTARGET_H__

#include "IVehicleSystem.h"
#include "AutoAimManager.h"

class CVehicleActionAutoTarget
	: public IVehicleAction
{
	IMPLEMENT_VEHICLEOBJECT;

public:
	CVehicleActionAutoTarget();
	virtual ~CVehicleActionAutoTarget();

	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table) override;
	virtual void Reset() override;
	virtual void Release() override;

	virtual int OnEvent(int eventType, SVehicleEventParams& eventParams) override;
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override;

	// IVehicleObject
	virtual void Serialize(TSerialize ser, EEntityAspects aspects) override {}
	virtual void Update(const float deltaTime) override {}
	// ~IVehicleObject

protected:
	SAutoaimTargetRegisterParams m_autoAimParams;

	IVehicle* m_pVehicle;
	bool m_RegisteredWithAutoAimManager;

private:
	static const char * m_name;
};

#endif //__VEHICLEACTIONAUTOTARGET_H__
