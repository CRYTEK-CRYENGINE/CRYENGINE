// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

	Plays Announcements based upon general game events

History:
- 17:11:2012		Created by Jim Bamford
*************************************************************************/

#ifndef __MISCANNOUNCER_H__
#define __MISCANNOUNCER_H__

#include "GameRulesModules/IGameRulesRevivedListener.h"
#include "GameRulesModules/IGameRulesRoundsListener.h"
#include "GameRulesTypes.h"
#include <CryCore/Containers/CryFixedArray.h>
#include "Audio/AudioSignalPlayer.h"
#include "IWeapon.h"
#include "IItemSystem.h"
#include <CryString/CryFixedString.h>
#include "AudioTypes.h"

class CWeapon;

class CMiscAnnouncer : public SGameRulesListener, public IGameRulesRoundsListener, public IItemSystemListener, public IWeaponEventListener
{
public:
	CMiscAnnouncer();
	~CMiscAnnouncer();

	void Init();
	void Reset();

	void Update(const float dt);
	
	// SGameRulesListener
	virtual void GameOver(EGameOverType localWinner, bool isClientSpectator) {}
	virtual void EnteredGame() {}
	virtual void EndGameNear(EntityId id) {}
	virtual void ClientEnteredGame( EntityId clientId ) {}
	virtual void ClientDisconnect( EntityId clientId );
	virtual void OnActorDeath( CActor* pActor ) {}
	virtual void SvOnTimeLimitExpired() {}
	virtual void ClTeamScoreFeedback(int teamId, int prevScore, int newScore) {}
	// ~SGameRulesListener

	// IGameRulesRoundsListener
	virtual void OnRoundStart();
	virtual void OnRoundEnd() {}
	virtual void OnSuddenDeath() {}
	virtual void ClRoundsNetSerializeReadState(int newState, int curState) {}
	virtual void OnRoundAboutToStart() {} 
	// ~IGameRulesRoundsListener

	//IItemSystemListener
	virtual void OnSetActorItem(IActor *pActor, IItem *pItem );
	virtual void OnDropActorItem(IActor *pActor, IItem *pItem ) {}
	virtual void OnSetActorAccessory(IActor *pActor, IItem *pItem ) {}
	virtual void OnDropActorAccessory(IActor *pActor, IItem *pItem ){}
	//~IItemSystemListener

	// IWeaponEventListener
	virtual void OnShoot(IWeapon *pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3 &vel);
	virtual void OnStartFire(IWeapon *pWeapon, EntityId shooterId) {}
	virtual void OnStopFire(IWeapon *pWeapon, EntityId shooterId) {}
	virtual void OnStartReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType) {}
	virtual void OnEndReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType) {}
	virtual void OnOutOfAmmo(IWeapon *pWeapon, IEntityClass* pAmmoType) {}
	virtual void OnSetAmmoCount(IWeapon *pWeapon, EntityId shooterId) {}
	virtual void OnReadyToFire(IWeapon *pWeapon) {}
	virtual void OnPickedUp(IWeapon *pWeapon, EntityId actorId, bool destroyed) {}
	virtual void OnDropped(IWeapon *pWeapon, EntityId actorId) {}
	virtual void OnMelee(IWeapon* pWeapon, EntityId shooterId) {}
	virtual void OnStartTargetting(IWeapon *pWeapon) {}
	virtual void OnStopTargetting(IWeapon *pWeapon) {}
	virtual void OnSelected(IWeapon *pWeapon, bool selected) {}
	virtual void OnEndBurst(IWeapon *pWeapon, EntityId shooterId) {}
	virtual void OnFireModeChanged(IWeapon *pWeapon, int currentFireMode) {}
	virtual void OnWeaponRippedOff(CWeapon *pWeapon) {}
	//~IWeaponEventListener

	void WeaponFired();

protected:

	void InitXML(XmlNodeRef root);
	void SetNewWeaponListener(IWeapon* pWeapon, EntityId weaponId, EntityId actorId);
	void RemoveWeaponListener(EntityId weaponId);
	void RemoveAllWeaponListeners();
	void RemoveWeaponListenerForActor(EntityId actorId);

	bool AnnouncerRequired();

	typedef std::map<EntityId, EntityId> ActorWeaponListenerMap;
	ActorWeaponListenerMap m_actorWeaponListener;

	struct SOnWeaponFired
	{
		IEntityClass *m_weaponEntityClass;
		CryFixedStringT<50> m_announcementName;
		EAnnouncementID m_announcementID;

		bool m_havePlayedFriendly;
		bool m_havePlayedEnemy;

		SOnWeaponFired() :
			m_weaponEntityClass(NULL),
			m_announcementID(0)
		{
			Reset();
		}

		SOnWeaponFired(IEntityClass *inWeaponEntityClass, const char *inAnnouncementName, EAnnouncementID inAnnouncementId) :
			m_weaponEntityClass(inWeaponEntityClass),
			m_announcementName(inAnnouncementName),
			m_announcementID(inAnnouncementId)
		{
			Reset();
		}

		void Reset()
		{
			m_havePlayedFriendly=false;
			m_havePlayedEnemy=false;
		}
	};

	typedef std::map<IEntityClass*, SOnWeaponFired> TWeaponFiredMap;
	TWeaponFiredMap m_weaponFiredMap;
};

#endif // __MISCANNOUNCER_H__
