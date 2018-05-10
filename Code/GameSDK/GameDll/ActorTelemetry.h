// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Helper class to record telemetry for actors

-------------------------------------------------------------------------
History:
- 02-3-2010		Steve Humphreys

*************************************************************************/

#pragma once

#ifndef __ACTORTELEMETRY_H__
#define __ACTORTELEMETRY_H__

#include <IWeapon.h>
#include <IItemSystem.h>
#include "StatHelpers.h"		// need CPositionStats
#include "GameRulesTypes.h"	// need EXPReason

class CActor;
class CItem;
DECLARE_SHARED_POINTERS(CActor);

#define USE_POSITION_COMPRESSION			1

struct IStatsTracker;

class CLerpTrack
{
	protected:
		struct SDatum
		{
			CPositionStats	data;
			CTimeValue		time;
			
			SDatum() {}
			SDatum(const CPositionStats *inStats,const CTimeValue *inTime) :
				data(inStats),
				time(inTime->GetValue())
			{
			}
		};

		std::deque<SDatum>	m_recentData;			// FIXME need a CryFixedDeque
		int					m_maxRecentDataLen;
		SDatum				m_lastOutput;
		size_t				m_trackId;

		void				Record(
								IStatsTracker			*inTracker,
								const CPositionStats	*inValue,
								const CTimeValue		*inTime);

		bool				ValueWillLerp(
								const CPositionStats	*inFrom,
								const CTimeValue		*inFromTime,
								const CPositionStats	*inMid,
								const CTimeValue		*inMidTime,
								const CPositionStats	*inTo,
								const CTimeValue		*inToTime);

	public:
#if defined(_DEBUG)
		int					m_numAdded;
		int					m_numOutput;
#endif
		
							CLerpTrack(
								size_t			inTrack,
								int				inWindowBufferSize);

		void				TryRecord(
								IStatsTracker			*inTracker,
								const CPositionStats	*inValue,
								const CTimeValue		*inTime);

		void				Flush(
								IStatsTracker			*inTracker);
};

class CActorTelemetry : public IItemSystemListener, public IWeaponEventListener
{
public:
	CActorTelemetry();
	~CActorTelemetry();

	//IItemSystemListener
	virtual void OnSetActorItem(IActor *pActor, IItem *pItem );
	virtual void OnDropActorItem(IActor *pActor, IItem *pItem ) {}
	virtual void OnSetActorAccessory(IActor *pActor, IItem *pItem ) {}
	virtual void OnDropActorAccessory(IActor *pActor, IItem *pItem ){}
	//~IItemSystemListener

	// IWeaponEventListener
	virtual void OnShoot(IWeapon *pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3 &vel);
	virtual void OnStartFire(IWeapon *pWeapon, EntityId shooterId) {};
	virtual void OnStopFire(IWeapon *pWeapon, EntityId shooterId) {};
	virtual void OnFireModeChanged(IWeapon *pWeapon, int currentFireMode);
	virtual void OnStartReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType) {};
	virtual void OnEndReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType);
	virtual void OnSetAmmoCount(IWeapon *pWeapon, EntityId shooterId) {};
	virtual void OnOutOfAmmo(IWeapon *pWeapon, IEntityClass* pAmmoType) {};
	virtual void OnReadyToFire(IWeapon *pWeapon) {};
	virtual void OnPickedUp(IWeapon *pWeapon, EntityId actorId, bool destroyed) {};
	virtual void OnDropped(IWeapon *pWeapon, EntityId actorId);
	virtual void OnMelee(IWeapon* pWeapon, EntityId shooterId);
	virtual void OnStartTargetting(IWeapon *pWeapon) {};
	virtual void OnStopTargetting(IWeapon *pWeapon) {};
	virtual void OnSelected(IWeapon *pWeapon, bool selected) {};
	virtual void OnEndBurst(IWeapon *pWeapon, EntityId shooterId) {};
	// ~IWeaponEventListener

	void OnXPChanged(int inXPDelta, EXPReason inReason);
	void OnIncreasePoints(int score, EGameRulesScoreType type);
	void OnFlashed(float flashDuration, EntityId shooterId);
	void OnTagged(EntityId shooter, float time, CGameRules::ERadarTagReason reason);
	void OnExchangeItem(CItem* pCurrentItem, CItem* pNewItem);

	void SubscribeToWeapon(EntityId weaponId);
	void UnsubscribeFromWeapon();
	void RecordCurrentHealth();
	void SetOwner(const CActor* pActor);
	inline IStatsTracker* GetStatsTracker()		{ return m_statsTracker; }
	void SetStatsTracker(IStatsTracker *inTracker);
	void Update();

	//Save/Load
	virtual void Serialize(TSerialize ser);
	virtual void PostSerialize();

	void GetMemoryUsage( ICrySizer *pSizer ) const {}
	// Monitor player actions.

	void OnPlayerAction(const ActionId &action, int activationMode, float value);


private:

	// Register player action filters.

	void RegisterPlayerActionFilters();

	void RecordInitialState();

	CActorWeakPtr m_pOwner;

	// previous values and timestamps
	//	(to prevent saving unchanged data)
	CTimeValue m_lastPositionTime;
	CTimeValue m_lastHealthTime;
	CTimeValue m_lastLookDirTime;
	CTimeValue m_lastLookRotationTime;
	
	Vec3 m_lastPosition;
	Vec3 m_lastLookDir;
	float m_lastHealth;
	float m_lastLookRotation;
	
	EntityId m_currentWeaponId;

	bool m_wasSprinting;

	// Player action filters.

	typedef std::set<ActionId> TActionIdSet;

	TActionIdSet	m_playerActionFilters;

#if USE_POSITION_COMPRESSION
	CLerpTrack	m_positionTrack;
#endif

	IStatsTracker	*m_statsTracker;

	void		Flush();
};

#endif	// __ACTORTELEMETRY_H__
