// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __HUDEVENTTRANSLATOR_H__
#define __HUDEVENTTRANSLATOR_H__

#include "IGameRulesSystem.h"
#include "Player.h"
#include "UI/HUD/HUDEventDispatcher.h"

#include "IPlayerEventListener.h"
#include "GameRulesModules/IGameRulesKillListener.h"
#include "IVehicleSystem.h"

//////////////////////////////////////////////////////////////////////////


class CHUDEventTranslator :
				IHUDEventListener,
				IItemSystemListener,
				IHitListener,
				IWeaponEventListener,
				IPlayerEventListener,
				IInventoryListener,
				IGameRulesKillListener,
				IVehicleUsageEventListener,
				IVehicleEventListener
{

public:

	CHUDEventTranslator();
	virtual ~CHUDEventTranslator();

	void Init( void );
	void Clear( void );

	//IItemSystemListener
	virtual void OnSetActorItem(IActor *pActor, IItem *pItem );

	virtual void OnDropActorItem(IActor *pActor, IItem *pItem ) {}
	virtual void OnSetActorAccessory(IActor *pActor, IItem *pItem ) {}
	virtual void OnDropActorAccessory(IActor *pActor, IItem *pItem ) {}
	//~IItemSystemListener


	//IInventoryListener
	virtual void OnAddItem(EntityId entityId);
	virtual void OnSetAmmoCount(IEntityClass* pAmmoType, int count);
	virtual void OnAddAccessory(IEntityClass* pAccessoryClass);
	//~IInventoryListener


	//IHitListener
	virtual void OnHit(const HitInfo& hitInfo);

	virtual void OnExplosion(const ExplosionInfo&) {}
	virtual void OnServerExplosion(const ExplosionInfo&) {}
	//~IHitListener


	//IWeaponEventListener
	virtual void		OnStartReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType);
	virtual void		OnEndReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType);
	virtual void		OnSetAmmoCount(IWeapon *pWeapon, EntityId shooterId);
	virtual void		OnFireModeChanged(IWeapon *pWeapon, int currentFireMode);

	virtual void		OnShoot(IWeapon *pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3 &vel) {}
	virtual void		OnStartFire(IWeapon *pWeapon, EntityId shooterId) {}
	virtual void		OnStopFire(IWeapon *pWeapon, EntityId shooterId) {}
	virtual void		OnOutOfAmmo(IWeapon *pWeapon, IEntityClass* pAmmoType) {}
	virtual void		OnReadyToFire(IWeapon *pWeapon) {}
	virtual void		OnPickedUp(IWeapon *pWeapon, EntityId actorId, bool destroyed) {}
	virtual void		OnDropped(IWeapon *pWeapon, EntityId actorId) {}
	virtual void		OnMelee(IWeapon* pWeapon, EntityId shooterId) {}
	virtual void		OnStartTargetting(IWeapon *pWeapon) {}
	virtual void		OnStopTargetting(IWeapon *pWeapon) {}
	virtual void		OnSelected(IWeapon *pWeapon, bool selected) {}
	virtual void		OnEndBurst(IWeapon *pWeapon, EntityId shooterId) {}
	//~IWeaponEventListener

	//IPlayerEventListener
	virtual void OnDeath(IActor* pActor, bool bIsGod);
	virtual void OnHealthChanged(IActor* pActor, float newHealth);
	virtual void OnItemPickedUp(IActor* pActor, EntityId itemId);
	virtual void OnPickedUpPickableAmmo(IActor* pActor, IEntityClass* pAmmoType, int count);
	//~IPlayerEventListener

	//IHUDEventListener
	virtual void OnHUDEvent( const SHUDEvent& hudEvent );
	//~IHUDEventListener

	//IGameRulesKillListener
	virtual void OnEntityKilledEarly(const HitInfo &hitInfo) {};
	virtual void OnEntityKilled(const HitInfo &hitInfo);
	//~IGameRulesKillListener

	//IVehicleUsageEventListener
	virtual void OnStartUse( const EntityId playerId, IVehicle* pVehicle );
	virtual void OnEndUse( const EntityId playerId, IVehicle* pVehicle );
	//~IVehicleUsageEventListener

	//IVehicleEventListener
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params);
	//~IVehicleEventListener

	void      OnInitGameRules( void );
	void      OnInitLocalPlayer( void );

	void GetMemoryUsage(ICrySizer *pSizer ) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}
private:

	void			SubscribeWeapon(EntityId weapon);
	void			UnsubscribeWeapon(EntityId weapon);

	EntityId	m_currentWeapon;
	EntityId	m_currentVehicle;
	EntityId	m_currentActor;

	bool			m_checkForVehicleTransition;
};


//////////////////////////////////////////////////////////////////////////

#endif

