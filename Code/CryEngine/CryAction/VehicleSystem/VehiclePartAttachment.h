// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a attachment socket for vehicle entity attachments

   -------------------------------------------------------------------------
   History:
   - 09:06:2006: Created by MichaelR

*************************************************************************/
#ifndef __VEHICLEPARTATTACHMENT_H__
#define __VEHICLEPARTATTACHMENT_H__

#include "VehiclePartBase.h"

class CVehicle;

class CVehiclePartEntityAttachment
	: public CVehiclePartBase
{
	IMPLEMENT_VEHICLEOBJECT
public:

	// IVehiclePartAttachment
	virtual bool        Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType) override;
	//virtual void InitGeometry();
	virtual void        PostInit() override;

	virtual void        Update(const float frameTime) override;

	virtual Matrix34    GetLocalTM(bool relativeToParentPart, bool forced = false) override;
	virtual Matrix34    GetWorldTM() override;

	virtual void        SetLocalTM(const Matrix34& localTM) override;
	virtual const AABB& GetLocalBounds() override;

	virtual void        GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
		CVehiclePartBase::GetMemoryUsageInternal(s);
	}
	// ~IVehiclePartAttachment

	void     SetAttachmentEntity(EntityId entityId);
	IEntity* GetAttachmentEntity();

protected:
	void UpdateAttachment();
	void DetachAttachment();

	EntityId m_attachmentId;

	Matrix34 m_localTM;
	Matrix34 m_worldTM;
};

#endif
