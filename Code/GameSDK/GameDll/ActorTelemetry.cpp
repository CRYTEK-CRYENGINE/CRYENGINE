// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Helper class to record telemetry for actors

-------------------------------------------------------------------------
History:
- 02-3-2010		Steve Humphreys

*************************************************************************/

#include "StdAfx.h"
#include "ActorTelemetry.h"

#include "Actor.h"
#include "Player.h"
#include "StatsRecordingMgr.h"
#include "StatHelpers.h"
#include "Weapon.h"
#include "GameActions.h"

static const float POSITION_DUMP_INTERVAL = 1.0f;
static const float POSITION_THRESHOLD = 0.5f;
static const float HEALTH_DUMP_INTERVAL =  2.0f;
static const float LOOKDIR_DUMP_INTERVAL = 0.33f;
static const float LOOKROT_DUMP_INTERVAL = 0.5f;
static const float LOOKDIR_THRESHOLD = 0.1f;
static const float LOOKROT_THRESHOLD = 1.0f;


CActorTelemetry::CActorTelemetry()
: m_lastPosition(ZERO)
, m_lastHealth(-1.0f)
, m_lastLookDir(ZERO)
, m_lastLookRotation(0.f)
, m_currentWeaponId(0)
, m_wasSprinting(false)
#if USE_POSITION_COMPRESSION
, m_positionTrack(eSE_Position,10)
#endif
, m_statsTracker(NULL)
{
	IItemSystem		*is=g_pGame->GetIGameFramework()->GetIItemSystem();

	if(is && gEnv->bServer && !gEnv->IsEditor())
	{
		is->RegisterListener(this);
	}
}

CActorTelemetry::~CActorTelemetry()
{
	Flush();

	UnsubscribeFromWeapon();

	IItemSystem		*is=g_pGame->GetIGameFramework()->GetIItemSystem();

	if (is && !gEnv->IsEditor())
	{
		// Unregister from item system - note: don't check for gEnv->bServer here since we might have been
		// a server once but not anymore due to a host migration
		is->UnregisterListener(this);
	}
}

void CActorTelemetry::SetOwner(const CActor* pActor)
{
	CRY_ASSERT(pActor);
	m_pOwner = pActor->GetWeakPtr();

	if(pActor->IsPlayer())
	{
		RegisterPlayerActionFilters();
	}
}

void CActorTelemetry::Flush()
{
	if (m_statsTracker)
	{
#if USE_POSITION_COMPRESSION
// #if defined(_DEBUG)
// 		CryLog("Flushing actor telemetry, added %d points, wrote %d points, reduction of %.2f%%",
// 			m_positionTrack.m_numAdded,m_positionTrack.m_numOutput,100.0-(100.0*(float(m_positionTrack.m_numOutput)/float(m_positionTrack.m_numAdded))));
// #endif
		m_positionTrack.Flush(m_statsTracker);
#endif
	}
}

void CActorTelemetry::SetStatsTracker(IStatsTracker *inTracker)
{
	Flush();

	// on stopping stats recording, we can stop listening to the weapon
	if(!inTracker && m_statsTracker)
	{
		UnsubscribeFromWeapon();
	}

	m_statsTracker=inTracker;

	if(!gEnv->bMultiplayer && inTracker)
	{
		RecordInitialState();
	}

	m_lastPosition = ZERO;
	m_lastHealth = -1.0f;
	m_lastLookDir = ZERO;
	m_wasSprinting = false;
}

void CActorTelemetry::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

#ifndef _RELEASE
	if (m_statsTracker)
	{
		static_cast<CCircularBufferStatsContainer*>(m_statsTracker->GetStatsContainer())->Validate();
	}
#endif

	CActorPtr pOwner = m_pOwner.lock();
	CRY_ASSERT(pOwner.get());

	CActor* pOwnerRaw = pOwner.get();

	IStatsTracker *tracker = GetStatsTracker();
	CStatsRecordingMgr* pMgr = g_pGame->GetStatsRecorder();
	if(tracker && pOwner && pMgr)
	{
		IGameStatistics		*gs=g_pGame->GetIGameFramework()->GetIGameStatistics();
		CTimeValue time = gEnv->pTimer->GetFrameStartTime(ITimer::ETIMER_UI);
		assert(gs);

		if (TimePassedCheck(m_lastPositionTime, time, POSITION_DUMP_INTERVAL) && pMgr->ShouldRecordEvent(eSE_Position, pOwnerRaw))
		{
			Vec3			pos=pOwner->GetEntity()->GetWorldPos();

#if USE_POSITION_COMPRESSION
			{
				CPositionStats	pstat(pos, gEnv->p3DEngine->GetTerrainElevation(pos.x, pos.y));
				m_positionTrack.TryRecord(tracker,&pstat,&time);
			}
#else
			if(!pos.IsEquivalent(m_lastPosition, POSITION_THRESHOLD))
			{
				m_lastPosition = pos;
				tracker->Event(eSE_Position, new CPositionStats(pos, gEnv->p3DEngine->GetTerrainElevation(pos.x, pos.y) ));
			}
#endif
		}

		if (TimePassedCheck(m_lastHealthTime, time, HEALTH_DUMP_INTERVAL))
		{
			RecordCurrentHealth();
		}
		
		const bool isSprinting = pOwnerRaw->IsSprinting();
		if (isSprinting != m_wasSprinting)
		{
			tracker->Event(eGSE_Sprint, isSprinting);
			m_wasSprinting = isSprinting;
		}

		std::shared_ptr<CPlayer> pPlayerOwner = std::static_pointer_cast<CPlayer>(pOwner);
		IPlayerInput* pInput = pPlayerOwner->GetPlayerInput();

		if (pMgr->ShouldRecordEvent(eSE_LookDir, pOwnerRaw))
		{
			if ( pInput && TimePassedCheck(m_lastLookDirTime, time, LOOKDIR_DUMP_INTERVAL))
			{
				SSerializedPlayerInput serInp;
				pInput->GetState(serInp);
				const Vec3& look = serInp.lookDirection;

				if(!look.IsEquivalent(m_lastLookDir, LOOKDIR_THRESHOLD))
				{
					m_lastLookDir = look;

					tracker->Event(eSE_LookDir, new CLookDirStats(look));
				}
			}
		}

		if (pMgr->ShouldRecordEvent(eSE_LookRotation, pOwnerRaw))
		{
			if ( pInput && TimePassedCheck(m_lastLookRotationTime, time, LOOKROT_DUMP_INTERVAL))
			{
				SSerializedPlayerInput serInp;
				pInput->GetState(serInp);
				const Vec3& look = serInp.lookDirection;
				float angle=RAD2DEG(-atan2f(look.x, look.y)); // synced with camera matrix GetAnglesXYZ() as used by r_displayInfo

				//CryLogAlways("CActorTelemetry::Update() look=%f, %f, %f; angle=%f (RAD2DEG:%f)", look.x, look.y, look.z, angle, RAD2DEG(angle));

				if (fabs_tpl(angle-m_lastLookRotation) > LOOKROT_THRESHOLD)
				{
					m_lastLookRotation = angle;
					tracker->Event(eSE_LookRotation, angle);
				}
			}
		}
	}

	assert(pOwner.get());

#ifndef _RELEASE
	if (m_statsTracker)
	{
		static_cast<CCircularBufferStatsContainer*>(m_statsTracker->GetStatsContainer())->Validate();
	}
#endif
}

#pragma warning(push)
#pragma warning(disable: 6319)
void CActorTelemetry::SubscribeToWeapon(EntityId weaponId)
{
	CStatsRecordingMgr* pMgr = g_pGame->GetStatsRecorder();
	IStatsTracker* pTracker = GetStatsTracker();
	if(pTracker && pMgr)
	{
		IWeapon				*pWeapon=NULL;
		CItem* pItem = NULL;
		if (weaponId)
		{
			pItem = (CItem*)gEnv->pGameFramework->GetIItemSystem()->GetItem(weaponId);
			if(pItem)
			{
				pWeapon=pItem->GetIWeapon();

				pWeapon->AddEventListener(this, "CActorTelemetry");
				m_currentWeaponId = weaponId;
			}
		}

		CActorPtr pOwner = m_pOwner.lock();
		CActor* pOwnerRaw = pOwner.get();
		if (pWeapon)
		{
			if(pMgr->ShouldRecordEvent(eSE_Weapon, pOwnerRaw))
			{
				const char* bottomAttachment = "";
				const char* barrelAttachment = "";
				const char* scopeAttachment = "";
				const CItem::TAccessoryArray& accessories = pItem->GetAccessories();
				CItem::TAccessoryArray::const_iterator itAccessory;
				for (itAccessory = accessories.begin(); itAccessory != accessories.end(); ++itAccessory)
				{
					const SAccessoryParams* pParams = pItem->GetAccessoryParams(itAccessory->pClass);
					if (pParams)
					{
						if (pParams->category == g_pItemStrings->bottom || pParams->category == g_pItemStrings->ammo) // Ammo is only used on Bow which should'nt have any other attachments.
							bottomAttachment = itAccessory->pClass->GetName();
						else if (pParams->category == g_pItemStrings->barrel)
							barrelAttachment = itAccessory->pClass->GetName();
						else if (pParams->category == g_pItemStrings->scope)
							scopeAttachment = itAccessory->pClass->GetName();
						else
							CRY_ASSERT_MESSAGE(pParams->category.empty(), string().Format("Unrecognised attachment category %s", pParams->category.c_str()));
					}
					else
					{
						CRY_ASSERT_MESSAGE(0, "Unable to find accessory params");
					}
				}

				pTracker->Event(eSE_Weapon,new CWeaponChangeEvent(static_cast<CWeapon*>(pWeapon)->GetName(), bottomAttachment, barrelAttachment, scopeAttachment));
			}

			if(pMgr->ShouldRecordEvent(eGSE_Firemode, pOwnerRaw))
			{
				IFireMode		*fireMode=pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());
				if (fireMode)
				{
					pTracker->Event(eGSE_Firemode,fireMode->GetName() );
				}
				else
				{
					pTracker->Event(eGSE_Firemode,"unknown firemode");
				}
			}
		}
		else if(pMgr->ShouldRecordEvent(eSE_Weapon, pOwnerRaw))
		{
			pTracker->Event(eSE_Weapon, new CWeaponChangeEvent("unarmed"));
			// until we know how we want the stats to be handled, leave the firemode timeline as it is when switching to no weapon
		}
	}
}
#pragma warning(pop)

void CActorTelemetry::UnsubscribeFromWeapon()
{
	IItem* pItem = gEnv->pGameFramework->GetIItemSystem()->GetItem(m_currentWeaponId);
	if(pItem)
	{
		IWeapon *pWeapon = pItem->GetIWeapon();
		if(pWeapon)
		{
			pWeapon->RemoveEventListener(this);
		}
	}
	m_currentWeaponId = 0;
}

void CActorTelemetry::RecordCurrentHealth()
{
	CStatsRecordingMgr* pMgr = g_pGame->GetStatsRecorder();
	CActorPtr pActor = m_pOwner.lock();

	if (pMgr && pMgr->ShouldRecordEvent(eSE_Health, pActor.get()))
	{
		float health = pActor->GetHealth();
		if(health != m_lastHealth)
		{
			IStatsTracker *tracker = GetStatsTracker();
			if (tracker)
			{
				m_lastHealth = health;
				tracker->Event(eSE_Health, health);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// IItemSystemListener functions
//////////////////////////////////////////////////////////////////////////

void CActorTelemetry::OnSetActorItem(IActor *pActor, IItem *pItem )
{
	if(pActor == m_pOwner.lock().get())
	{
		// stop listening to old weapon
		UnsubscribeFromWeapon();
		
		// listen to new weapon
		if(pItem)
		{
			SubscribeToWeapon(pItem->GetEntityId());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// IWeaponEventListener functions
//////////////////////////////////////////////////////////////////////////

void CActorTelemetry::OnEndReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType)
{
	CActorPtr pActor = m_pOwner.lock();

	CRY_ASSERT(pActor && shooterId == pActor->GetEntityId());

	CStatsRecordingMgr* pMgr = g_pGame->GetStatsRecorder();
	if(!pMgr || !pMgr->ShouldRecordEvent(eSE_Reload, pActor.get()))
		return;

	if ( IStatsTracker *tracker = GetStatsTracker() )
	{
		tracker->Event(eSE_Reload); // TODO kiev differentiated between auto reloads and manual reloads
	}
}

void CActorTelemetry::OnMelee(IWeapon* pWeapon, EntityId shooterId)
{
	CActorPtr pActor = m_pOwner.lock();

	CRY_ASSERT(pActor && shooterId == pActor->GetEntityId());

	CStatsRecordingMgr* pMgr = g_pGame->GetStatsRecorder();
	if(!pMgr || !pMgr->ShouldRecordEvent(eGSE_Melee, pActor.get()))
		return;

	if ( IStatsTracker *tracker = GetStatsTracker() )
	{
		tracker->Event(eGSE_Melee);	// TODO kiev had a melee id here, which was a rolling id stored in CPlayer. was used to link melee events to melee damage
	}
}

void CActorTelemetry::OnShoot(IWeapon *pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType,
																 const Vec3 &pos, const Vec3 &dir, const Vec3 &vel)
{
	CActorPtr pActor = m_pOwner.lock();

	CRY_ASSERT(pActor && shooterId == pActor->GetEntityId());

	CStatsRecordingMgr* pMgr = g_pGame->GetStatsRecorder();
	if(!pMgr || !pMgr->ShouldRecordEvent(eSE_Shot, pActor.get()))
		return;

	if ( IStatsTracker *tracker = GetStatsTracker() )
	{
		int				fmidx=pWeapon->GetCurrentFireMode();
		IFireMode		*fm=pWeapon->GetFireMode(fmidx);
		IEntityClass *ammoType=fm ? fm->GetAmmoType() : NULL;
		// TODO add entity id that is being aimed at so we can calculate the % of shots that hit the intended target
		//	(SNH: should be possible to record hits, and match the projectile ids in StatsTool)
		tracker->Event( eSE_Shot, new CShotFiredStats(fm ? fm->GetProjectileId() : 0, pWeapon->GetAmmoCount(ammoType), ammoType ? ammoType->GetName() : "unknown ammo"));
	}
}

void CActorTelemetry::OnFireModeChanged(IWeapon *pWeapon, int currentFireMode)
{
	CActorPtr pActor = m_pOwner.lock();

	CRY_ASSERT(pActor && static_cast<CWeapon*>(pWeapon)->GetOwnerId() == pActor->GetEntityId());		// is this cast always safe?

	CStatsRecordingMgr* pMgr = g_pGame->GetStatsRecorder();
	if(!pMgr || !pMgr->ShouldRecordEvent(eGSE_Firemode, pActor.get()))
		return;

	if ( IStatsTracker *tracker = GetStatsTracker() )
	{
		tracker->Event( eGSE_Firemode, pWeapon->GetFireMode(currentFireMode)->GetName() );
	}
}

void CActorTelemetry::OnDropped(IWeapon *pWeapon, EntityId actorId)
{
#ifdef DEBUG
	CActorPtr pActor = m_pOwner.lock();
	CRY_ASSERT(pActor && actorId == pActor->GetEntityId());
#endif

	if(static_cast<CWeapon*>(pWeapon)->GetEntityId() == m_currentWeaponId)
	{
		UnsubscribeFromWeapon();
	}
}

void CActorTelemetry::Serialize( TSerialize ser )
{
	CActorPtr pActor = m_pOwner.lock();
	EntityId ownerId = pActor ? pActor->GetEntityId() : 0;
	ser.Value("OwnerId", ownerId);
	ser.Value("WeaponId", m_currentWeaponId);

	if(ser.IsReading())
	{
		CActor *pOwnerActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(ownerId));
		CRY_ASSERT(pOwnerActor);
		//m_pOwner = pOwnerActor->GetWeakPtr();
		SetOwner(pOwnerActor);
	}

	// Record 'Save' telemetry stats.

	if(ser.IsWriting())
	{
		CStatsRecordingMgr::TryTrackEvent(m_pOwner.lock().get(), eGSE_Save);
	}
}

void CActorTelemetry::PostSerialize()
{
	CActorPtr pActor = m_pOwner.lock();

	IItem* pItem = gEnv->pGameFramework->GetIItemSystem()->GetItem(m_currentWeaponId);
	if(pItem && pActor && pItem->GetOwnerId() != pActor->GetEntityId())
		UnsubscribeFromWeapon();
}

void CActorTelemetry::OnPlayerAction(const ActionId &action, int activationMode, float value)
{
	// Record 'PlayerAction' telemetry stats.

	if(activationMode != eIS_Down && m_playerActionFilters.find(action) != m_playerActionFilters.end())
	{
		CStatsRecordingMgr::TryTrackEvent(m_pOwner.lock().get(), eGSE_PlayerAction, action.c_str());
	}
}


void CActorTelemetry::OnXPChanged(int inXPDelta, EXPReason inReason)
{
	CStatsRecordingMgr *pStatsRecordingMgr = g_pGame->GetStatsRecorder();
	CActorPtr pActor = m_pOwner.lock();

	if(pStatsRecordingMgr && pStatsRecordingMgr->ShouldRecordEvent(eGSE_XPChanged, pActor.get()))
	{
		if (inReason == k_XPRsn_MatchBonus)
		{
			pStatsRecordingMgr->OnMatchBonusXp(pActor.get(), inXPDelta);
		}
		else if(m_statsTracker)
		{
			SStatAnyValue		value(new CXPIncEvent(inXPDelta,inReason));
			m_statsTracker->Event(eGSE_XPChanged,value);
		}
	}
}

void CActorTelemetry::OnIncreasePoints(int score, EGameRulesScoreType type)
{
	CStatsRecordingMgr *pStatsRecordingMgr = g_pGame->GetStatsRecorder();
	CActorPtr pActor = m_pOwner.lock();

	if(pStatsRecordingMgr && pStatsRecordingMgr->ShouldRecordEvent(eSE_Score, pActor.get()))
	{
		if(m_statsTracker)
		{
			SStatAnyValue		value(new CScoreIncEvent(score, type));
			m_statsTracker->Event(eSE_Score,value);
		}
	}
}

void CActorTelemetry::OnFlashed(float flashDuration, EntityId shooterId)
{
	CStatsRecordingMgr *pStatsRecordingMgr = g_pGame->GetStatsRecorder();
	CActorPtr pActor = m_pOwner.lock();

	if(pStatsRecordingMgr && pStatsRecordingMgr->ShouldRecordEvent(eGSE_Flashed, pActor.get()))
	{
		if(m_statsTracker)
		{
			SStatAnyValue value(new CFlashedEvent(flashDuration, shooterId));
			m_statsTracker->Event(eGSE_Flashed,value);
		}
	}
}

void CActorTelemetry::OnTagged(EntityId shooter, float time, CGameRules::ERadarTagReason reason)
{
	CStatsRecordingMgr *pStatsRecordingMgr = g_pGame->GetStatsRecorder();
	CActorPtr pActor = m_pOwner.lock();

	if(pStatsRecordingMgr && pStatsRecordingMgr->ShouldRecordEvent(eGSE_Tagged, pActor.get()))
	{
		if(m_statsTracker)
		{
			m_statsTracker->Event(eGSE_Tagged, new CTaggedEvent(shooter, time, reason));
		}
	}
}

void CActorTelemetry::OnExchangeItem(CItem* pCurrentItem, CItem* pNewItem)
{
	if (pCurrentItem && pNewItem)
	{
		CStatsRecordingMgr *pStatsRecordingMgr = g_pGame->GetStatsRecorder();
		CActorPtr pActor = m_pOwner.lock();

		if(pStatsRecordingMgr && pStatsRecordingMgr->ShouldRecordEvent(eGSE_ExchangeItem, pActor.get()))
		{
			if(m_statsTracker)
			{
				m_statsTracker->Event(eGSE_ExchangeItem, new CExchangeItemEvent(pCurrentItem->GetEntity()->GetClass()->GetName(), pNewItem->GetEntity()->GetClass()->GetName()));
			}
		}
	}
}

void CActorTelemetry::RegisterPlayerActionFilters()
{
	const CGameActions	&actions = g_pGame->Actions();

	m_playerActionFilters.insert(actions.jump);
	m_playerActionFilters.insert(actions.crouch);
	m_playerActionFilters.insert(actions.sprint);
	m_playerActionFilters.insert(actions.special);
	m_playerActionFilters.insert(actions.attack1);
	m_playerActionFilters.insert(actions.attack1_xi);
	m_playerActionFilters.insert(actions.attack2_xi);
	m_playerActionFilters.insert(actions.reload);
	m_playerActionFilters.insert(actions.modify);
	m_playerActionFilters.insert(actions.nextitem);
	m_playerActionFilters.insert(actions.previtem);
	m_playerActionFilters.insert(actions.toggle_explosive);
	m_playerActionFilters.insert(actions.toggle_weapon);
	m_playerActionFilters.insert(actions.toggle_grenade);
	m_playerActionFilters.insert(actions.handgrenade);
	m_playerActionFilters.insert(actions.xi_handgrenade);
	m_playerActionFilters.insert(actions.debug);
	m_playerActionFilters.insert(actions.zoom);
	m_playerActionFilters.insert(actions.firemode);
	m_playerActionFilters.insert(actions.weapon_change_firemode);
	m_playerActionFilters.insert(actions.binoculars);
	m_playerActionFilters.insert(actions.objectives);
	m_playerActionFilters.insert(actions.grenade);
	m_playerActionFilters.insert(actions.xi_grenade);
	m_playerActionFilters.insert(actions.zoom_in);
	m_playerActionFilters.insert(actions.zoom_out);
	m_playerActionFilters.insert(actions.use);
	m_playerActionFilters.insert(actions.itemPickup);
	m_playerActionFilters.insert(actions.voice_chat_talk);
	m_playerActionFilters.insert(actions.save);
	m_playerActionFilters.insert(actions.load);
	m_playerActionFilters.insert(actions.xi_zoom);
	m_playerActionFilters.insert(actions.xi_disconnect);
	m_playerActionFilters.insert(actions.scores);
	m_playerActionFilters.insert(actions.v_changeseat1);
	m_playerActionFilters.insert(actions.v_changeseat2);
	m_playerActionFilters.insert(actions.v_changeseat3);
	m_playerActionFilters.insert(actions.v_changeseat4);
	m_playerActionFilters.insert(actions.v_changeseat5);
	m_playerActionFilters.insert(actions.v_changeview);
	m_playerActionFilters.insert(actions.v_viewoption);
	m_playerActionFilters.insert(actions.v_zoom_in);
	m_playerActionFilters.insert(actions.v_zoom_out);
	m_playerActionFilters.insert(actions.v_attack1);
	m_playerActionFilters.insert(actions.v_attack2);
	m_playerActionFilters.insert(actions.v_lights);
	m_playerActionFilters.insert(actions.v_horn);
	m_playerActionFilters.insert(actions.v_exit);
	m_playerActionFilters.insert(actions.v_afterburner);
	m_playerActionFilters.insert(actions.v_boost);
	m_playerActionFilters.insert(actions.v_changeseat);
}

void CActorTelemetry::RecordInitialState()
{
	// called when a valid tracker is set, to make sure the new 
	// one has valid data for weapon etc 
	//	(was missing when going through checkpoints)

	RecordCurrentHealth();

	CActorPtr pOwner = m_pOwner.lock();
	CRY_ASSERT(pOwner.get());

	CActor* pOwnerRaw = pOwner.get();

	OnSetActorItem(pOwnerRaw, pOwner->GetCurrentItem());
}

//////////////////////////////////////////////////////////////////////////
// CLerpTrack functions
//////////////////////////////////////////////////////////////////////////

// generally, the more data that is tracked, the better the path simplification can be, but it is dependent on how interpolatable the
// data is, so check a bigger number actually yields better compression before committing to it
CLerpTrack::CLerpTrack(
	size_t			inTrack,
	int				inMaxRecentDataToTrack) :
	m_maxRecentDataLen(inMaxRecentDataToTrack),
	m_trackId(inTrack)
#if defined(_DEBUG)
	,m_numAdded(0),
	m_numOutput(0)
#endif
{
	m_lastOutput.time=CTimeValue(-1.0f);			// marks last output as invalid
	//m_recentData.reserve(inMaxRecentDataToTrack);	// FIXME need a CryFixedDeque so we can actually do a reserve and manage this in a contiguous block
}

// inValue is copied and not adopted
void CLerpTrack::Record(
	IStatsTracker			*inTracker,
	const CPositionStats	*inValue,
	const CTimeValue		*inTime)
{
	m_lastOutput.data=*inValue;
	m_lastOutput.time=*inTime;
#if defined(_DEBUG)
	m_numOutput++;
#endif

#if STATS_MODE_CVAR
	if(CCryActionCVars::Get().g_statisticsMode != 2)
		return;
#endif

	SStatAnyValue	value(new CPositionStats(inValue));	

	inTracker->GetStatsContainer()->AddEvent(m_trackId,*inTime,value);
	// FIXME - no interface to call OnTrackedEvent() nor to add an event with an explicit time stamp from game code
	// FIXME - not calling OnTrackedEvent() for now
	// m_gameStats->OnTrackedEvent(m_tracker->GetLocator(),m_eventId,*inTime,value);
}

// returns true if the mid value can be produced by a lerp from the from point to the to point
bool CLerpTrack::ValueWillLerp(
	const CPositionStats	*inFrom,
	const CTimeValue		*inFromTime,
	const CPositionStats	*inMid,
	const CTimeValue		*inMidTime,
	const CPositionStats	*inTo,
	const CTimeValue		*inToTime)
{
#define lerp(a,b,t)	((a)*(1.0f-t)+(b)*(t))	// FIXME can't find this in the cry code base
	static const float	k_lerpPosThresholdSq=sqr(0.5f);		// bigger the values, more aggressive the compression and less precise the data
	static const float	k_lerpElevationThreshold=0.5f;

	CTimeValue	range=(*inToTime)-(*inFromTime);
	float		rangef=range.GetMilliSeconds();
	bool		result;

	if (rangef>0.0f)
	{
		CTimeValue	diff=(*inMidTime)-(*inFromTime);
		float		difff=diff.GetMilliSeconds();
		float		frac=difff/rangef;
		bool		canInterpolatePos,canInterpolateElev;

		CRY_ASSERT(frac>=0.0f && frac<=1.0f);

		Vec3		dir(inTo->m_pos);

		dir-=inFrom->m_pos;
		dir*=frac;

		Vec3		interpolated(inFrom->m_pos);

		interpolated+=dir;

		float		distSq=interpolated.GetSquaredDistance(inMid->m_pos);

		canInterpolatePos=(distSq<k_lerpPosThresholdSq);

		float		interpolatedElev=lerp(inFrom->m_elev,inTo->m_elev,frac);

		canInterpolateElev=(fabsf(interpolatedElev-inMid->m_elev)<k_lerpElevationThreshold);

		result=canInterpolateElev && canInterpolatePos;
	}
	else
	{
		result=true;		// no time difference - trivially will lerp
	}

	return result;
#undef lerp
}

// inValue is copied and not adopted
void CLerpTrack::TryRecord(
	IStatsTracker			*inTracker,
	const CPositionStats	*inValue,
	const CTimeValue		*inTime)
{
#if defined(_DEBUG)
	m_numAdded++;
#endif

	if (m_lastOutput.time==CTimeValue(-1.0f))
	{
		Record(inTracker,inValue,inTime);
	}
	else
	{
		bool	interpolated=false;

		if ((int)m_recentData.size()<m_maxRecentDataLen)
		{
			interpolated=true;
			for (std::deque<SDatum>::const_iterator iter=m_recentData.begin(), end=m_recentData.end(); iter!=end; ++iter)
			{
				const SDatum	&data=*iter;

				if (!ValueWillLerp(&m_lastOutput.data,&m_lastOutput.time,&data.data,&data.time,inValue,inTime))
				{
					interpolated=false;
					break;
				}
			}
		}

		if (!interpolated)
		{
			Flush(inTracker);
		}

		m_recentData.push_back(SDatum(inValue,inTime));
	}
}

void CLerpTrack::Flush(
	IStatsTracker			*inTracker)
{
	if (m_recentData.size()>0)
	{
		const SDatum	&data=m_recentData.back();
		Record(inTracker,&data.data,&data.time);
		m_recentData.clear();
	}
}
