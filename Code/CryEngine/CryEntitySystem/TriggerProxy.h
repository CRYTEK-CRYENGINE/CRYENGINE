// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Entity.h"
#include "EntitySystem.h"

struct SProximityElement;

//////////////////////////////////////////////////////////////////////////
// Description:
//    Handles sounds in the entity.
//////////////////////////////////////////////////////////////////////////
class CEntityComponentTriggerBounds : public IEntityTriggerComponent
{
	CRY_ENTITY_COMPONENT_CLASS_GUID(CEntityComponentTriggerBounds, IEntityTriggerComponent, "CEntityComponentTriggerBounds", "1c58115a-a18e-446e-8e82-b3b4c6dd6f55"_cry_guid);

	CEntityComponentTriggerBounds();
	virtual ~CEntityComponentTriggerBounds();

public:
	//////////////////////////////////////////////////////////////////////////
	// IEntityComponent interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void   Initialize() final;
	virtual void   ProcessEvent(const SEntityEvent& event) final;
	virtual uint64 GetEventMask() const final { return ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM) | ENTITY_EVENT_BIT(ENTITY_EVENT_ENTERAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_LEAVEAREA); };
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEntityComponent interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetProxyType() const final { return ENTITY_PROXY_TRIGGER; }
	virtual void         Release() final            { delete this; };
	virtual void         GameSerialize(TSerialize ser) final;
	virtual bool         NeedGameSerialize() final;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEntityTriggerComponent
	//////////////////////////////////////////////////////////////////////////
	virtual void SetTriggerBounds(const AABB& bbox) final { SetAABB(bbox); };
	virtual void GetTriggerBounds(AABB& bbox) final       { bbox = m_aabb; };
	virtual void ForwardEventsTo(EntityId id) final       { m_forwardingEntity = id; }
	virtual void InvalidateTrigger() final;
	//////////////////////////////////////////////////////////////////////////

	const AABB&              GetAABB() const { return m_aabb; }
	void                     SetAABB(const AABB& aabb);

	CProximityTriggerSystem* GetTriggerSystem() { return static_cast<CEntitySystem*>(g_pIEntitySystem)->GetProximityTriggerSystem(); }

	virtual void             GetMemoryUsage(ICrySizer* pSizer) const final
	{
		pSizer->AddObject(this, sizeof(*this));
	}
private:
	void OnMove(bool invalidateAABB = false);
	void Reset();

private:
	AABB               m_aabb;
	SProximityElement* m_pProximityTrigger;
	EntityId           m_forwardingEntity;
};
