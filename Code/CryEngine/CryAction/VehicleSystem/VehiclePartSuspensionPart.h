// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a suspension part

   -------------------------------------------------------------------------
   History:
   - 16:11:2011: Created by Stan Fichele

*************************************************************************/
#ifndef __VEHICLEPARTSUSPENSIONPART_H__
#define __VEHICLEPARTSUSPENSIONPART_H__

#include "VehicleSystem/VehiclePartSubPart.h"

class CVehicle;
class CVehiclePartAnimated;

class CVehiclePartSuspensionPart : public CVehiclePartSubPart
{
	IMPLEMENT_VEHICLEOBJECT

private:
	typedef CVehiclePartSubPart inherited;

public:
	CVehiclePartSuspensionPart();
	virtual ~CVehiclePartSuspensionPart();

	// IVehiclePart
	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* pParent, CVehicle::SPartInitInfo& initInfo, int partType) override;
	virtual void PostInit() override;
	virtual void Reset() override;
	virtual void Release() override;
	virtual void OnEvent(const SVehiclePartEvent& event) override;
	virtual bool ChangeState(EVehiclePartState state, int flags = 0) override;
	virtual void Physicalize() override;
	virtual void Update(const float frameTime) override;
	// ~IVehiclePart

protected:
	enum
	{
		k_modeRotate = 0,       // Rotate towards the target, but dont translate or stretch the joint
		k_modeStretch,          // Rotate towards EF and stretch so that the end-effector hits the target
		k_modeSnapToEF,         // Rotate and translate so that the end-effector hits the target
	};
	enum
	{
		k_flagTargetHelper         = 0x1,      // Target was specified using a helper
		k_flagIgnoreTargetRotation = 0x2,      // Dont use the target's rotation (i.e. for wheels)
	};
	CVehiclePartAnimated* m_animatedRoot;
	CVehiclePartBase*     m_targetPart;          // The part the target lies on
	Quat                  m_initialRot;          // This is not always ID, so need to store it
	Vec3                  m_targetOffset;        // target position on target part
	Vec3                  m_pos0;                // Initial position, relative to parent
	Vec3                  m_direction0;          // The initial direction vector
	float                 m_invLength0;          // Inverse length of direction0
	int16                 m_jointId;             // Joint ID of cga statobj that was overridden
	int8                  m_mode;                // IK mode
	uint8                 m_ikFlags;
};

#endif
