// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Vehicle part class that spawns a particle effect and attaches it to the vehicle.

   -------------------------------------------------------------------------
   History:
   - 01:09:2010: Created by Paul Slinger

*************************************************************************/

#ifndef __VEHICLEPARTPARTICLEEFFECT_H__
#define __VEHICLEPARTPARTICLEEFFECT_H__

#include "IGameObject.h"
#include "VehiclePartBase.h"

class CVehiclePartParticleEffect : public CVehiclePartBase
{
public:

	IMPLEMENT_VEHICLEOBJECT

	CVehiclePartParticleEffect();

	~CVehiclePartParticleEffect();

	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType) override;
	virtual void PostInit() override;
	virtual void Reset() override;
	virtual void Update(const float frameTime) override;
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override;
	// PS - May need to implement SetLocalTM(), GetLocalTM(), GetWorldTM() and GetLocalBounds here, but that would require quite a lot of extra storage and calculations.
	virtual void GetMemoryUsage(ICrySizer* pSizer) const override;

	IEntity*     GetPartEntity() const;

private:

	void ActivateParticleEffect(bool activate);

	int              m_id;

	string           m_particleEffectName, m_helperName;

	IParticleEffect* m_pParticleEffect;

	IVehicleHelper*  m_pHelper;
};

#endif //__VEHICLEPARTPARTICLEEFFECT_H__
