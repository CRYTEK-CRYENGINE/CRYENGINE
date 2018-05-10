// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a third person view for vehicles

   -------------------------------------------------------------------------
   History:
   - 02:05:2005: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEVIEWTHIRDPERSON_H__
#define __VEHICLEVIEWTHIRDPERSON_H__

#include "VehicleViewBase.h"

class CVehicleViewThirdPerson
	: public CVehicleViewBase
{
	IMPLEMENT_VEHICLEOBJECT;
public:

	CVehicleViewThirdPerson();
	~CVehicleViewThirdPerson();

	// IVehicleView
	virtual bool Init(IVehicleSeat* pSeat, const CVehicleParams& table) override;
	virtual void Reset() override;
	virtual void ResetPosition() override
	{
		m_position = m_pVehicle->GetEntity()->GetWorldPos();
	}

	virtual const char* GetName() override           { return m_name; }
	virtual bool        IsThirdPerson() override     { return true; }
	virtual bool        IsPassengerHidden() override { return false; }

	virtual void        OnAction(const TVehicleActionId actionId, int activationMode, float value) override;
	virtual void        UpdateView(SViewParams& viewParams, EntityId playerId) override;

	virtual void        OnStartUsing(EntityId passengerId) override;

	virtual void        Update(const float frameTime) override;
	virtual void Serialize(TSerialize serialize, EEntityAspects) override;

	virtual bool ShootToCrosshair() override { return false; }

	virtual void OffsetPosition(const Vec3& delta) override;
	// ~IVehicleView

	bool Init(CVehicleSeat* pSeat);

	//! sets default view distance. if 0, the distance from the vehicle is used
	static void SetDefaultDistance(float dist);

	//! sets default height offset. if 0, the heightOffset from the vehicle is used
	static void SetDefaultHeight(float height);

	void        GetMemoryUsage(ICrySizer* s) const override { s->Add(*this); }

protected:

	I3DEngine*         m_p3DEngine;
	IEntitySystem*     m_pEntitySystem;

	EntityId           m_targetEntityId;
	IVehiclePart*      m_pAimPart;

	float              m_distance;
	float              m_heightOffset;

	float              m_height;
	Vec3               m_vehicleCenter;

	static const char* m_name;

	Vec3               m_position;
	Vec3               m_worldPos;
	float              m_rot;

	float              m_interpolationSpeed;
	float              m_boundSwitchAngle;
	float              m_zoom;
	float              m_zoomMult;

	static float       m_defaultDistance;
	static float       m_defaultHeight;

	float              m_actionZoom;
	float              m_actionZoomSpeed;

	bool               m_isUpdatingPos;
};

#endif
