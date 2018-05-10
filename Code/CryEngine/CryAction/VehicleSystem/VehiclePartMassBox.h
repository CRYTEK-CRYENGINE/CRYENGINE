// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a simple box-shaped part for mass distribution

   -------------------------------------------------------------------------
   History:
   - 17:10:2005: Created by MichaelR

*************************************************************************/
#ifndef __VEHICLEPARTMASSBOX_H__
#define __VEHICLEPARTMASSBOX_H__

#include "VehiclePartBase.h"

class CVehicle;

class CVehiclePartMassBox
	: public CVehiclePartBase
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehiclePartMassBox();
	~CVehiclePartMassBox();

	// IVehiclePart
	virtual bool        Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType) override;
	virtual void        Reset() override;

	virtual void        Physicalize() override;

	virtual Matrix34    GetLocalTM(bool relativeToParentPart, bool forced = false) override;
	virtual Matrix34    GetWorldTM() override;
	virtual const AABB& GetLocalBounds() override;

	virtual void        Update(const float frameTime) override;
	virtual void        Serialize(TSerialize ser, EEntityAspects aspects) override;

	virtual void        GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
		CVehiclePartBase::GetMemoryUsageInternal(s);
	}
	// ~IVehiclePart

protected:

	Matrix34 m_localTM;
	Matrix34 m_worldTM;
	Vec3     m_size;
	float    m_drivingOffset;

private:
	enum EMassBoxDrivingType
	{
		eMASSBOXDRIVING_DEFAULT    = 1 << 0,
		eMASSBOXDRIVING_NORMAL     = 1 << 1,
		eMASSBOXDRIVING_INTHEAIR   = 1 << 2,
		eMASSBOXDRIVING_INTHEWATER = 1 << 3,
		eMASSBOXDRIVING_DESTROYED  = 1 << 4,
	};
	void SetupDriving(EMassBoxDrivingType drive);
	EMassBoxDrivingType m_Driving; //0 default 1 driving 2 in the air
};

#endif
