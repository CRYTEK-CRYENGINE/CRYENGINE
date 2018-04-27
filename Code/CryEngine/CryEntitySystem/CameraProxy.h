// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Entity.h"
#include "EntitySystem.h"

struct SProximityElement;

//////////////////////////////////////////////////////////////////////////
// Description:
//    Handles sounds in the entity.
//////////////////////////////////////////////////////////////////////////
class CEntityComponentCamera : public IEntityCameraComponent
{
	CRY_ENTITY_COMPONENT_CLASS_GUID(CEntityComponentCamera, IEntityCameraComponent, "CEntityComponentCamera", "0f8eee88-f3aa-49b2-a20d-2747b5e33df9"_cry_guid);

	CEntityComponentCamera();
	virtual ~CEntityComponentCamera() {}

public:
	//////////////////////////////////////////////////////////////////////////
	// IEntityComponent interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void   Initialize() final;
	virtual void   ProcessEvent(const SEntityEvent& event) final;
	virtual uint64 GetEventMask() const final;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEntityComponent interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetProxyType() const final { return ENTITY_PROXY_CAMERA; }
	virtual void         Release() final            { delete this; };
	virtual void         GameSerialize(TSerialize ser) final;
	virtual bool         NeedGameSerialize() final  { return false; };
	//////////////////////////////////////////////////////////////////////////

	virtual void     SetCamera(CCamera& cam) final;
	virtual CCamera& GetCamera() final { return m_camera; };
	//////////////////////////////////////////////////////////////////////////

	void         UpdateMaterialCamera();

	virtual void GetMemoryUsage(ICrySizer* pSizer) const final
	{
		pSizer->AddObject(this, sizeof(*this));
	}
private:
	CCamera m_camera;
};
