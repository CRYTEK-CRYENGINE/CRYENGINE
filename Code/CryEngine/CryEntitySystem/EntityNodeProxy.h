// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EntitySystem.h"
#include <CryNetwork/ISerialize.h>

class CEntityComponentTrackViewNode :
	public IEntityComponent
{
public:
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CEntityComponentTrackViewNode, "CEntityComponentTrackViewNode", "60f18291-c2a1-46f7-ba7f-02f390c85bb2"_cry_guid);

	CEntityComponentTrackViewNode();
	virtual ~CEntityComponentTrackViewNode() {}

	//////////////////////////////////////////////////////////////////////////
	// IEntityComponent interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void   Initialize() final;
	virtual void   ProcessEvent(const SEntityEvent& event) final;
	virtual uint64 GetEventMask() const final;
	//////////////////////////////////////////////////////////////////////////

	virtual void         GetMemoryUsage(ICrySizer* pSizer) const final {};
	virtual EEntityProxy GetProxyType() const final                    { return ENTITY_PROXY_ENTITYNODE; };

	virtual void         Release() final                               { delete this; }
};
