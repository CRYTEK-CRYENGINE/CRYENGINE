// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a third person view for vehicles tweaked for
   action gameplay

   -------------------------------------------------------------------------
   History:
   - 18:08:2006: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEVIEWACTIONTHIRDPERSON_H__
#define __VEHICLEVIEWACTIONTHIRDPERSON_H__

#include "VehicleViewBase.h"

class CVehicleViewActionThirdPerson
	: public CVehicleViewBase
{
	IMPLEMENT_VEHICLEOBJECT;
public:

	CVehicleViewActionThirdPerson();
	~CVehicleViewActionThirdPerson();

	// IVehicleView
	virtual bool Init(IVehicleSeat* pSeat, const CVehicleParams& table) override;
	virtual void Reset() override;
	virtual void ResetPosition() override
	{
		m_worldCameraPos = m_pVehicle->GetEntity()->GetWorldPos();
		m_worldViewPos = m_worldCameraPos;
	}

	virtual const char* GetName() override           { return m_name; }
	virtual bool        IsThirdPerson() override     { return true; }
	virtual bool        IsPassengerHidden() override { return false; }

	virtual void        OnStartUsing(EntityId passengerId) override;

	virtual void        OnAction(const TVehicleActionId actionId, int activationMode, float value) override;
	virtual void        UpdateView(SViewParams& viewParams, EntityId playerId) override;

	virtual void        Update(const float frameTime) override;
	virtual void Serialize(TSerialize serialize, EEntityAspects) override;

	virtual void GetMemoryUsage(ICrySizer* s) const override { s->Add(*this); }

	virtual void OffsetPosition(const Vec3& delta) override;
	// ~IVehicleView

protected:

	Matrix34 GetCameraTM(const Matrix34& worldTM, const Vec3& aimPos);
	Vec3     GetAimPos(const Matrix34& worldTM);

	Vec3               m_worldViewPos;
	Vec3               m_worldCameraPos;
	Vec3               m_worldCameraAim;

	IVehiclePart*      m_pAimPart;

	Vec3               m_localCameraPos;
	Vec3               m_localCameraAim;
	Vec3               m_velocityMult;
	float              m_lagSpeed;

	Vec3               m_extraLag;

	float              m_zoom;
	float              m_zoomTarget;
	float              m_actionZoom;
	float              m_verticalFilter;
	float              m_verticalFilterOffset;

	float              m_heightAboveWater;

	Vec3               m_cameraOffset;

	static const char* m_name;
};

#endif
