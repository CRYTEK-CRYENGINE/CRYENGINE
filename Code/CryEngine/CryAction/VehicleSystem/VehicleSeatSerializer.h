// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements an entity class which can serialize vehicle parts

   -------------------------------------------------------------------------
   History:
   - 16:09:2005: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLESEATSERIALIZER_H__
#define __VEHICLESEATSERIALIZER_H__

#include "IGameObject.h"

class CVehicle;
class CVehicleSeat;

class CVehicleSeatSerializer
	: public CGameObjectExtensionHelper<CVehicleSeatSerializer, IGameObjectExtension>
{
public:

	CVehicleSeatSerializer();
	virtual ~CVehicleSeatSerializer();

	virtual bool                 Init(IGameObject* pGameObject);
	virtual void                 InitClient(int channelId);
	virtual void                 PostInit(IGameObject*)                                                          {}
	virtual void                 PostInitClient(int channelId)                                                   {};
	virtual bool                 ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params);
	virtual void                 PostReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params) {}
	virtual void                 Release()                                                                       { delete this; }

	virtual void                 FullSerialize(TSerialize ser);
	virtual bool                 NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags);
	virtual void                 PostSerialize() {}
	virtual void                 SerializeSpawnInfo(TSerialize ser);
	virtual ISerializableInfoPtr GetSpawnInfo();
	virtual void                 Update(SEntityUpdateContext& ctx, int);
	virtual void                 HandleEvent(const SGameObjectEvent& event);
	virtual void                 ProcessEvent(const SEntityEvent& event)  {};
	virtual uint64               GetEventMask() const { return 0; }
	virtual void                 SetChannelId(uint16 id)            {};
	virtual void                 PostUpdate(float frameTime)        { CRY_ASSERT(false); };
	virtual void                 PostRemoteSpawn()                  {};
	virtual void                 GetMemoryUsage(ICrySizer* s) const { s->Add(*this); }

	void                         SetVehicle(CVehicle* pVehicle);
	void                         SetSeat(CVehicleSeat* pSeat);

protected:
	CVehicle*     m_pVehicle;
	CVehicleSeat* m_pSeat;
};

#endif
