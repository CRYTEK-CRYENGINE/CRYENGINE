// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __DANGEROUS_RIGID_BODY_H__
#define __DANGEROUS_RIGID_BODY_H__

#include <IGameObject.h>

class CDangerousRigidBody : public CGameObjectExtensionHelper<CDangerousRigidBody, IGameObjectExtension, 1>
{
public:
	static const NetworkAspectType ASPECT_DAMAGE_STATUS	= eEA_GameServerC;
	
	static int sDangerousRigidBodyHitTypeId;

	CDangerousRigidBody();

	// IGameObjectExtension
	virtual bool Init(IGameObject *pGameObject);
	virtual void InitClient(int channelId);
	virtual void PostInit(IGameObject *pGameObject) {}
	virtual void PostInitClient(int channelId) {}
	virtual void Release();
	virtual void FullSerialize(TSerialize ser) {};
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags);
	virtual void PostSerialize() {}
	virtual void SerializeSpawnInfo(TSerialize ser) {}
	virtual ISerializableInfoPtr GetSpawnInfo() { return 0; }
	virtual void Update(SEntityUpdateContext &ctx, int updateSlot) {};
	virtual void PostUpdate(float frameTime) {}
	virtual void PostRemoteSpawn() {}
	virtual void HandleEvent(const SGameObjectEvent& event) {};
	virtual void ProcessEvent(const SEntityEvent& event);
	virtual uint64 GetEventMask() const;
	virtual void SetChannelId(uint16 id) {}
	virtual void GetMemoryUsage(ICrySizer *pSizer) const;
	virtual bool ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params );
	virtual void PostReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params ) {};
	virtual bool GetEntityPoolSignature( TSerialize signature );
	// IGameObjectExtension

	void SetIsDangerous(bool isDangerous, EntityId triggerPlayerId);

private:
	void Reset();

	float	m_damageDealt;
	float m_lastHitTime;
	float m_timeBetweenHits;
	bool m_dangerous;
	bool m_friendlyFireEnabled;
	uint8 m_activatorTeam;
};

#endif //__DANGEROUS_RIGID_BODY_H__
