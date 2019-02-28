// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MP_PATH_H__
#define __MP_PATH_H__

#include <IGameObject.h>

class CMPPath : public CGameObjectExtensionHelper<CMPPath, IGameObjectExtension, 1>
{
public:
	// IGameObjectExtension
	virtual bool Init(IGameObject *pGameObject);
	virtual void InitClient(int channelId) {};
	virtual void PostInit(IGameObject *pGameObject) {};
	virtual void PostInitClient(int channelId) {}
	virtual void Release();
	virtual void FullSerialize(TSerialize ser) {};
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) { return true; }
	virtual void PostSerialize() {}
	virtual void SerializeSpawnInfo(TSerialize ser) {}
	virtual ISerializableInfoPtr GetSpawnInfo() { return 0; }
	virtual void Update(SEntityUpdateContext &ctx, int updateSlot) {};
	virtual void PostUpdate(float frameTime) {}
	virtual void PostRemoteSpawn() {}
	virtual void HandleEvent(const SGameObjectEvent& details) {}
	virtual void ProcessEvent(const SEntityEvent& details);
	virtual uint64 GetEventMask() const;
	virtual void SetChannelId(uint16 id) {}
	virtual void GetMemoryUsage(ICrySizer *pSizer) const;
	virtual bool ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params );
	virtual void PostReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params ) {}
	virtual bool GetEntityPoolSignature( TSerialize signature );
	//~IGameObjectExtension
};

#endif // __MP_PATH_H__