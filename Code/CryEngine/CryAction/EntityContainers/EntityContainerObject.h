// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Description: Container Entity to be placed in the Editor that gets
// automatically registered with the Entity Container Manager
// - 10/02/2016 Created by Dario Sancho

#pragma once

#include <IGameObject.h>

class CEntityContainerObject : public CGameObjectExtensionHelper<CEntityContainerObject, IGameObjectExtension>
{
public:

	CEntityContainerObject() {};
	virtual ~CEntityContainerObject(){}

	// IGameObjectExtension
	virtual bool                 Init(IGameObject* pGameObject);
	virtual void                 InitClient(int channelId)                                                       {};
	virtual void                 PostInit(IGameObject* pGameObject);
	virtual void                 PostInitClient(int channelId)                                                   {};
	virtual bool                 ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params);
	virtual void                 PostReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params) {}
	virtual bool                 GetEntityPoolSignature(TSerialize signature);
	virtual void                 Release();
	virtual bool                 NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int pflags) { return true; }
	virtual void                 FullSerialize(TSerialize ser)                                                  {};
	virtual void                 PostSerialize()                                                                {}
	virtual void                 SerializeSpawnInfo(TSerialize ser)                                             {}
	virtual ISerializableInfoPtr GetSpawnInfo()                                                                 { return 0; }
	virtual void                 Update(SEntityUpdateContext& ctx, int updateSlot)                              {}
	virtual void                 PostUpdate(float frameTime)                                                    {};
	virtual void                 PostRemoteSpawn()                                                              {};
	virtual void                 HandleEvent(const SGameObjectEvent&)                                           {};
	virtual void                 ProcessEvent(const SEntityEvent&);
	virtual uint64               GetEventMask() const;
	virtual void                 SetChannelId(uint16 id)                                                        {};
	virtual void                 SetAuthority(bool auth)                                                        {};
	virtual void                 GetMemoryUsage(ICrySizer* pSizer) const                                        { pSizer->Add(*this); }
	//~IGameObjectExtension

private:
	bool ReadEditorProperties();
};
