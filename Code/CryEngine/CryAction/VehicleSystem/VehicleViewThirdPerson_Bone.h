// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VEHICLEVIEWTHIRDPERSON_BONE_H
#define __VEHICLEVIEWTHIRDPERSON_BONE_H

#include "VehicleViewBase.h"

class CVehicleViewThirdPersonBone : public CVehicleViewBase
{
	IMPLEMENT_VEHICLEOBJECT;
public:
	CVehicleViewThirdPersonBone();
	~CVehicleViewThirdPersonBone();

	// IVehicleView
	virtual bool        Init(IVehicleSeat* pSeat, const CVehicleParams& table);

	virtual const char* GetName()           { return m_name; }
	virtual bool        IsThirdPerson()     { return true; }
	virtual bool        IsPassengerHidden() { return false; }

	virtual void        OnAction(const TVehicleActionId actionId, int activationMode, float value);
	virtual void        UpdateView(SViewParams& viewParams, EntityId playerId);

	virtual void        OnStartUsing(EntityId passengerId);

	virtual void        Update(const float frameTime);

	virtual bool        ShootToCrosshair() { return true; }

	virtual void        OffsetPosition(const Vec3& delta);
	// ~IVehicleView

	void GetMemoryUsage(ICrySizer* s) const { s->Add(*this); }

protected:
	static const char* m_name;

	int                m_directionBoneId;
	Vec3               m_offset;
	float              m_distance;
	Vec3               m_position;
	Quat               m_baseRotation;
	Quat               m_additionalRotation;

};

#endif
