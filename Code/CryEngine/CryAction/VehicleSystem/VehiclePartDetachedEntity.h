// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements an entity class for detached parts

   -------------------------------------------------------------------------
   History:
   - 26:10:2005: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEPARTDETACHEDENTITY_H__
#define __VEHICLEPARTDETACHEDENTITY_H__

#include "IGameObject.h"

class CVehiclePartDetachedEntity
	: public CGameObjectExtensionHelper<CVehiclePartDetachedEntity, IGameObjectExtension>
{
public:
	~CVehiclePartDetachedEntity();

	virtual bool                 Init(IGameObject* pGameObject);
	virtual void                 InitClient(int channelId)                                                       {};
	virtual void                 PostInit(IGameObject*);
	virtual void                 PostInitClient(int channelId)                                                   {};
	virtual bool                 ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params);
	virtual void                 PostReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params) {}
	virtual void                 Release()                                                                       { delete this; }

	virtual void                 FullSerialize(TSerialize ser);
	virtual bool                 NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) { return true; }
	virtual void                 PostSerialize()                                                               {}
	virtual void                 SerializeSpawnInfo(TSerialize ser)                                            {}
	virtual ISerializableInfoPtr GetSpawnInfo()                                                                { return 0; }
	virtual void                 Update(SEntityUpdateContext& ctx, int slot);
	virtual void                 HandleEvent(const SGameObjectEvent& event);
	virtual void                 ProcessEvent(const SEntityEvent& event);
	virtual uint64               GetEventMask() const;
	virtual void                 SetChannelId(uint16 id)                 {};
	virtual void                 PostUpdate(float frameTime)             { CRY_ASSERT(false); }
	virtual void                 PostRemoteSpawn()                       {};
	virtual void                 GetMemoryUsage(ICrySizer* pSizer) const { pSizer->Add(*this); }

protected:
	void InitVehiclePart(IGameObject* pGameObject);

	float m_timeUntilStartIsOver;
};

#endif
