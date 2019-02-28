// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Entity that can change it's physicalisation state during a
		game.
  
 -------------------------------------------------------------------------
  History:
  - 19:09:2010: Created by Colin Gulliver

*************************************************************************/

#ifndef __NETWORKEDPHYSICSENTITY_H__
#define __NETWORKEDPHYSICSENTITY_H__

class CNetworkedPhysicsEntity :	public CGameObjectExtensionHelper<CNetworkedPhysicsEntity, IGameObjectExtension, 2>,
																public IGameObjectProfileManager
{
public:
	enum ePhysicalization
	{
		ePhys_NotPhysicalized,
		ePhys_PhysicalizedRigid,
		ePhys_PhysicalizedStatic,
	};

	CNetworkedPhysicsEntity();
	virtual ~CNetworkedPhysicsEntity();

	// IGameObjectExtension
	virtual bool Init(IGameObject *pGameObject);
	virtual void InitClient(int channelId) {}
	virtual void PostInit(IGameObject *pGameObject) {}
	virtual void PostInitClient(int channelId) {}
	virtual bool ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params );
	virtual void PostReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params ) {}
	virtual bool GetEntityPoolSignature( TSerialize signature );
	virtual void Release();
	virtual void FullSerialize(TSerialize ser) {};
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags);
	virtual void PostSerialize() {}
	virtual void SerializeSpawnInfo(TSerialize ser) {}
	virtual ISerializableInfoPtr GetSpawnInfo() { return 0; }
	virtual void Update(SEntityUpdateContext &ctx, int updateSlot) {};
	virtual void PostUpdate(float frameTime) {}
	virtual void PostRemoteSpawn() {}
	virtual void HandleEvent(const SGameObjectEvent& event) {}
	virtual void ProcessEvent(const SEntityEvent& event) {}
	virtual void SetChannelId(uint16 id) {}
	virtual void SetAuthority(bool auth);
	virtual void GetMemoryUsage(ICrySizer *pSizer) const;
	// ~IGameObjectExtension

	// IGameObjectProfileManager
	virtual bool SetAspectProfile( EEntityAspects aspect, uint8 profile );
	virtual uint8 GetDefaultProfile( EEntityAspects aspect );
	// ~IGameObjectProfileManager

	void Physicalize(ePhysicalization physicsType);

private:
	void ReadPhysicsParams();

	SEntityPhysicalizeParams m_physicsParams;
	ePhysicalization m_physicsType;
	ePhysicalization m_requestedPhysicsType;
};

#endif //__NETWORKEDPHYSICSENTITY_H__
