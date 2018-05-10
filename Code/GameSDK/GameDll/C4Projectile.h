// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  C4 projectile specific stuff
-------------------------------------------------------------------------
History:
- 08:06:2007   : Created by Benito G.R.

*************************************************************************/

#ifndef __C4PROJECTILE_H__
#define __C4PROJECTILE_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Projectile.h"
#include "StickyProjectile.h"
#include "GameRulesModules/IGameRulesTeamChangedListener.h"
#include "GameRulesModules/IGameRulesClientConnectionListener.h"

struct IAttachmentManager;

class CC4Projectile : public CProjectile
										, public IGameRulesTeamChangedListener
										,	public IGameRulesClientConnectionListener
{
private:
	typedef CProjectile BaseClass;

	static const NetworkAspectType ASPECT_C4_STATUS	= eEA_GameServerA;


public:
	CC4Projectile();
	virtual ~CC4Projectile();

	virtual bool Init(IGameObject *pGameObject);
	virtual void HandleEvent(const SGameObjectEvent &event);
	virtual void Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale);
	virtual bool CanDetonate();
	virtual bool Detonate();
	virtual void ProcessEvent(const SEntityEvent& event);
	virtual void FullSerialize(TSerialize ser);
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int pflags);
	virtual NetworkAspectType GetNetSerializeAspects();
	virtual void Update(SEntityUpdateContext &ctx, int updateSlot);
	virtual void PostRemoteSpawn();
	virtual void Explode(const CProjectile::SExplodeDesc& explodeDesc);

	virtual void OnHit(const HitInfo&);
	virtual void OnServerExplosion(const ExplosionInfo&);
	virtual void OnExplosion(const ExplosionInfo&);
	virtual EntityId GetStuckToEntityId() const;

	virtual void SerializeSpawnInfo( TSerialize ser );
	virtual ISerializableInfoPtr GetSpawnInfo();

	// IGameRulesTeamChangedListener
	virtual void OnChangedTeam(EntityId entityId, int oldTeamId, int newTeamId);
	// ~IGameRulesTeamChangedListener

	// IGameRulesClientConnectionListener
	virtual void OnClientConnect(int channelId, bool isReset, EntityId playerId) {};
	virtual void OnClientDisconnect(int channelId, EntityId playerId) {};
	virtual void OnClientEnteredGame(int channelId, bool isReset, EntityId playerId) {};
	virtual void OnOwnClientEnteredGame();
	// ~IGameRulesClientConnectionListener

protected:

	struct SC4Info : public SInfo
	{
		int team;

		void SerializeWith( TSerialize ser )
		{
			SInfo::SerializeWith(ser);

			ser.Value("team", team, 'team');
		}
	};

private:

	virtual void SetParams(const SProjectileDesc& projectileDesc);
	void Arm(bool arm);
	void CreateLight();
	void UpdateLight(float fFrameTime, bool forceColorChange);
	void SetLightParams();
	void RemoveLight();
	void SetupUIIcon();

	CStickyProjectile	m_stickyProjectile;
	int					m_teamId;
	bool				m_armed;
	bool				m_OnSameTeam;
	bool				m_isShowingUIIcon;
	float				m_disarmTimer;
	float				m_pulseTimer;
	
	IStatObj*		m_pStatObj;
	IMaterial*	m_pArmedMaterial;
	IMaterial*	m_pDisarmedMaterial;
	ILightSource*	m_pLightSource;
};

#endif
