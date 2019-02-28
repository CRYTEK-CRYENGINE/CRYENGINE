// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements water ripple generation for boats and similar vehicles

   -------------------------------------------------------------------------
   History:
   - 04:06:2012: Created by Benito G.R.

*************************************************************************/
#ifndef __VEHICLEPAR_RIPPLEGENERATOR_H__
#define __VEHICLEPAR_RIPPLEGENERATOR_H__

#include "VehiclePartBase.h"

class CVehicle;

class CVehiclePartWaterRipplesGenerator
	: public CVehiclePartBase
{
	IMPLEMENT_VEHICLEOBJECT

public:

	CVehiclePartWaterRipplesGenerator();
	virtual ~CVehiclePartWaterRipplesGenerator();

	// IVehiclePart
	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType) override;
	virtual void PostInit() override;
	virtual void Update(const float frameTime) override;

	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
		CVehiclePartBase::GetMemoryUsageInternal(s);
	}
	// ~IVehiclePart

private:

	Vec3      m_localOffset;
	float     m_waterRipplesScale;
	float     m_waterRipplesStrength;
	float     m_minMovementSpeed;
	bool      m_onlyMovingForward;
};

#endif //__VEHICLEPAR_RIPPLEGENERATOR_H__
