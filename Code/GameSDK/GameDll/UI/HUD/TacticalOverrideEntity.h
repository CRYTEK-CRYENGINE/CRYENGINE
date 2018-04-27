// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: HUD Tactical override entity
  
 -------------------------------------------------------------------------
  History:
  - 13:12:2012: Created by Dean Claassen

*************************************************************************/

#ifndef __HUD_TACTICALOVERRIDE_ENTITY_H__
#define __HUD_TACTICALOVERRIDE_ENTITY_H__

// CTacticalOverrideEntity

class CTacticalOverrideEntity : public CGameObjectExtensionHelper<CTacticalOverrideEntity, IGameObjectExtension>
{
public:
	CTacticalOverrideEntity();
	virtual ~CTacticalOverrideEntity();

	// IGameObjectExtension
	virtual bool Init(IGameObject *pGameObject);
	virtual void InitClient(int channelId);
	virtual void PostInit(IGameObject *pGameObject);
	virtual void PostInitClient(int channelId) {}
	virtual void Release();
	virtual void FullSerialize(TSerialize ser) {};
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) { return true; }
	virtual void PostSerialize() {}
	virtual void SerializeSpawnInfo(TSerialize ser) {}
	virtual ISerializableInfoPtr GetSpawnInfo() { return 0; }
	virtual void Update(SEntityUpdateContext &ctx, int updateSlot) {}
	virtual void PostUpdate(float frameTime) {}
	virtual void PostRemoteSpawn() {}
	virtual void HandleEvent(const SGameObjectEvent& details) {}
	virtual void ProcessEvent(const SEntityEvent& details);
	virtual void SetChannelId(uint16 id) {}
	virtual void GetMemoryUsage(ICrySizer *pSizer) const;
	virtual bool ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params );
	virtual void PostReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params ) {}
	virtual bool GetEntityPoolSignature( TSerialize signature );
	//~IGameObjectExtension

protected:
	bool m_bMappedToParent;
};

#endif // __HUD_TACTICALOVERRIDE_ENTITY_H__

