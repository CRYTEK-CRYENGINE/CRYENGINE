// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef		__STATHELPERS_H__
#define		__STATHELPERS_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <CryGame/IGameStatistics.h>
#include "CircularStatsStorage.h"
#include "GameRulesTypes.h"
#include "GameRules.h"

#define USE_STATS_CIRCULAR_BUFFER			1

#if USE_STATS_CIRCULAR_BUFFER
#define	SERIALIZABLE_STATS_BASE			CCircularBufferTimelineEntry
#else
#define SERIALIZABLE_STATS_BASE			CXMLSerializableBase
#endif

struct IStatsTracker;
struct IActor;
class CActor;

class CStatHelpers
{
public:
	static bool SaveActorArmor( CActor* pActor );
	static bool SaveActorStats( CActor* pActor, XmlNodeRef stats );
	static bool SaveActorWeapons( CActor* pActor );
	static bool SaveActorAttachments( CActor* pActor );
	static bool SaveActorAmmos( CActor* pActor );
	static int GetProfileId( int channelId );
	static int GetChannelId( int profileId );
	static EntityId GetEntityId( int profileId );

private:
	static IActor* GetProfileActor( int profileId );
};

//////////////////////////////////////////////////////////////////////////

class CPositionStats : public SERIALIZABLE_STATS_BASE
{
public:

	CPositionStats();
	CPositionStats(const Vec3& pos, float elevation);
	CPositionStats(const CPositionStats *inCopy);
	CPositionStats &operator=(const CPositionStats &inFrom);
	virtual XmlNodeRef GetXML(IGameStatistics* pGS);
	virtual void GetMemoryStatistics(ICrySizer* pSizer) const;

	Vec3 m_pos;
	float m_elev;
};

//////////////////////////////////////////////////////////////////////////

class CLookDirStats : public SERIALIZABLE_STATS_BASE
{
public:

	CLookDirStats(const Vec3& dir);
	virtual XmlNodeRef GetXML(IGameStatistics* pGS);
	virtual void GetMemoryStatistics(ICrySizer* pSizer) const;

	Vec3 m_dir;
};

//////////////////////////////////////////////////////////////////////////

class CShotFiredStats : public SERIALIZABLE_STATS_BASE
{
public:

	CShotFiredStats(EntityId projectileId, int ammo_left, const char* ammo_type);
	virtual XmlNodeRef GetXML(IGameStatistics* pGS);
	virtual void GetMemoryStatistics(ICrySizer* pSizer) const;

	EntityId m_projectileId;
	int m_ammoLeft;
	CCryName m_ammoType;
};

//////////////////////////////////////////////////////////////////////////

class CKillStats : public SERIALIZABLE_STATS_BASE
{
public:

	CKillStats(EntityId projectileId, EntityId targetId, const char* hit_type, const char * weapon_class, const char* projectile_class);
	virtual XmlNodeRef GetXML(IGameStatistics* pGS);
	virtual void GetMemoryStatistics(ICrySizer* pSizer) const;

	EntityId m_projectileId;
	EntityId target_id;
	CCryName m_hitType;
	CCryName m_weaponClass;
	CCryName m_projectileClass;
};

//////////////////////////////////////////////////////////////////////////

class CDeathStats : public SERIALIZABLE_STATS_BASE
{
public:

	CDeathStats(EntityId projectileId, EntityId killerId, const char* hit_type, const char* weapon_class, const char* projectile_class);
	virtual XmlNodeRef GetXML(IGameStatistics* pGS);
	virtual void GetMemoryStatistics(ICrySizer* pSizer) const;

	EntityId m_projectileId;
	EntityId m_killerId;
	CCryName m_hitType;
	CCryName m_weaponClass;
	CCryName m_projectileClass;
};

//////////////////////////////////////////////////////////////////////////

class CHitStats : public SERIALIZABLE_STATS_BASE
{
public:
	CHitStats(EntityId projectileId, EntityId shooterId, float damage, const char* hit_type, const char* weapon_class, const char* projectile_class, const char* hit_part);
	virtual XmlNodeRef GetXML(IGameStatistics* pGS);
	virtual void GetMemoryStatistics(ICrySizer* pSizer) const;

	EntityId m_projectileId;
	EntityId m_shooterId;
	float m_damage;
	CCryName m_hitType;
	CCryName m_weaponClass;
	CCryName m_projectileClass;
	CCryName m_hitPart;
};

//////////////////////////////////////////////////////////////////////////

class CXPIncEvent : public SERIALIZABLE_STATS_BASE
{
public:
	CXPIncEvent(int inDelta, EXPReason inReason);
	virtual XmlNodeRef GetXML(IGameStatistics* pGS);
	virtual void GetMemoryStatistics(ICrySizer* pSizer) const;

	int					m_delta;
	EXPReason	m_reason;
};

//////////////////////////////////////////////////////////////////////////

class CScoreIncEvent : public SERIALIZABLE_STATS_BASE
{
public:
	CScoreIncEvent(int score, EGameRulesScoreType type);
	virtual XmlNodeRef GetXML(IGameStatistics* pGS);
	virtual void GetMemoryStatistics(ICrySizer* pSizer) const;

	int	m_score;
	EGameRulesScoreType	m_type;
};

//////////////////////////////////////////////////////////////////////////

class CWeaponChangeEvent : public SERIALIZABLE_STATS_BASE
{
public:
	CWeaponChangeEvent(const char* weapon_class, const char* bottom_attachment_class = "", const char* barrel_attachment_class = "", const char* scope_attachment_class = "");
	virtual XmlNodeRef GetXML(IGameStatistics* pGS);
	virtual void GetMemoryStatistics(ICrySizer* pSizer) const;

	CCryName m_weaponClass;
	CCryName m_bottomAttachmentClass;
	CCryName m_barrelAttachmentClass;
	CCryName m_scopeAttachmentClass;
};

//////////////////////////////////////////////////////////////////////////

class CFlashedEvent : public SERIALIZABLE_STATS_BASE
{
public:
	CFlashedEvent(float flashDuration, EntityId shooterId);
	virtual XmlNodeRef GetXML(IGameStatistics* pGS);
	virtual void GetMemoryStatistics(ICrySizer* pSizer) const;

	float	m_flashDuration;
	EntityId	m_shooterId;
};

//////////////////////////////////////////////////////////////////////////

class CTaggedEvent : public SERIALIZABLE_STATS_BASE
{
public:
	CTaggedEvent(EntityId shooter, float time, CGameRules::ERadarTagReason reason);
	virtual XmlNodeRef GetXML(IGameStatistics* pGS);
	virtual void GetMemoryStatistics(ICrySizer* pSizer) const;

	EntityId m_shooter;
	CGameRules::ERadarTagReason m_reason;
};

//////////////////////////////////////////////////////////////////////////

class CExchangeItemEvent : public SERIALIZABLE_STATS_BASE
{
public:
	CExchangeItemEvent(const char* old_item_class, const char* new_item_class);
	virtual XmlNodeRef GetXML(IGameStatistics* pGS);
	virtual void GetMemoryStatistics(ICrySizer* pSizer) const;

	CCryName m_oldItemClass;
	CCryName m_newItemClass;
};

///////////////////////////////////////////////////////////////////////////
enum 
{
	eEVE_Ripup,
	eEVE_Pickup,
	eEVE_Drop,
	eEVE_MeleeAttack,
	eEVE_MeleeKill,
	eEVE_ThrowAttack,
	eEVE_ThrowKill,
};


class CEnvironmentalWeaponEvent : public SERIALIZABLE_STATS_BASE
{
public:
	CEnvironmentalWeaponEvent( EntityId weaponId, int16 action, int16 iParam);
	virtual XmlNodeRef GetXML( IGameStatistics* pGS );
	virtual void GetMemoryStatistics( ICrySizer* pSizer ) const;


	EntityId	m_weaponId;
	int16			m_action;
	int16			m_param;
};


///////////////////////////////////////////////////////////////////////////
enum
{
	eSSE_neutral,
	eSSE_capturing_from_neutral,
	eSSE_captured,
	eSSE_capturing_from_capture,
	eSSE_contested,
	eSSE_failed_capture,
};

class CSpearStateEvent : public SERIALIZABLE_STATS_BASE
{
// owning team currently owns this spear and is scoring points on it
// capturing team is the only team with players round this spear and is currently moving it into their control

public:
	CSpearStateEvent( uint8 spearId, uint8 state, uint8 team1count, uint8 team2count, uint8 owningTeam, uint8 capturingTeam );
	virtual XmlNodeRef GetXML( IGameStatistics* pGS );
	virtual void GetMemoryStatistics( ICrySizer* pSizer ) const;

	uint8 m_spearId;
	uint8 m_state;
	uint8 m_team1count; 
	uint8 m_team2count; 
	uint8 m_owningTeam; 
	uint8 m_capturingTeam;
};


#endif
