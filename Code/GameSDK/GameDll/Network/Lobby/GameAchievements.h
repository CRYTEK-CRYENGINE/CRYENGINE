// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 14:05:2010		Created by Steve Humphreys
*************************************************************************/

#pragma once

#ifndef __GAME_ACHIEVEMENTS_H__
#define __GAME_ACHIEVEMENTS_H__

#include <CryGame/IGameFramework.h>
#include "GameRulesTypes.h"
#include "GameRulesModules/IGameRulesKillListener.h"
#include "../../DownloadMgr.h"
#include "UI/HUD//HUDEventDispatcher.h"
#include "Network/Lobby/GameLobbyManager.h"


//////////////////////////////////////////////////////////////////////////
// Achievements / Trophies
//	For now we assume that achievements are numbered sequentially on
//	both platforms, though potentially starting at different indices.
//	Since this depends on xlast etc it may be necessary to assign
//	an id per achievement for each platform later.
//////////////////////////////////////////////////////////////////////////

//Currently PC achievements are stubbed out in CryNetwork
static const int ACHIEVEMENT_STARTINDEX = 0;

#define C3Achievement(f) \
	f(eC3A_Tutorial) \
	f(eC3A_Jailbreak) \
	f(eC3A_Fields) \
	f(eC3A_Canyon) \
	f(eC3A_Swamp) \
	f(eC3A_River) \
	f(eC3A_Island) \
	f(eC3A_Cave) \
	f(eC3A_SPProg_Half_Veteran) \
	f(eC3A_SPProg_Half_Supersoldier) \
	f(eC3A_Complete_Campaign) \
	f(eC3A_SPProg_Veteran) \
	f(eC3A_SPProg_Supersoldier) \
	f(eC3A_SPFeature_Geared_Up) \
	f(eC3A_SPFeature_Suited_Up) \
	f(eC3A_SPFeature_Pro_Bow) \
	f(eC3A_SPFeature_Maximum_Strength) \
	f(eC3A_SPFeature_Hunter_Gatherer) \
	f(eC3A_SPFeature_Ill_Have_That) \
	f(eC3A_SPFeature_Supercharge) \
	f(eC3A_SPFeature_Collector) \
	f(eC3A_SPLevel_BangForTheBuck) \
	f(eC3A_SPLevel_CanYouHearMe) \
	f(eC3A_SPLevel_Jailbreak_Rockets) \
	f(eC3A_SPLevel_Donut_Surf) \
	f(eC3A_SPLevel_Roadkill) \
	f(eC3A_SPSkill_InsideJob) \
	f(eC3A_SPSkill_Post_Human) \
	f(eC3A_SPSkill_Improviser) \
	f(eC3A_SPSkill_From_Behind) \
	f(eC3A_SPSkill_Stick_Around) \
	f(eC3A_SPSkill_Clever_Girl) \
	f(eC3A_SPSkill_Poltergeist) \
	f(eC3A_MP_Rising_Star) \
	f(eC3A_MP_Block_Party) \
	f(eC3A_MP_Odd_Job) \
	f(eC3A_MP_Lord_Of_The_Pings) \
	f(eC3A_MP_The_Specialist) \
	f(eC3A_MP_Bird_Of_Prey) \
	f(eC3A_MP_Going_Commando) \
	f(eC3A_MP_Hit_Me_Baby_One_More_Time) \
	f(eC3A_MP_Rudely_Interrupted) \
	f(eC3A_MP_No_More_Merry_Men) \
	f(eC3A_MP_Would_You_Kindly) \
	f(eC3A_MP_20_Metre_High_Club) \
	f(eC3A_MP_Kicking_Off_The_Training_Wheels) \


AUTOENUM_BUILDENUMWITHTYPE_WITHNUM(ECrysis3Achievement, C3Achievement, eC3A_NumAchievements);

class CGameAchievements : public IGameRulesKillListener, public IGameFrameworkListener,	public IHUDEventListener, public IPrivateGameListener
{
public:
	CGameAchievements();
	virtual ~CGameAchievements();

	void GiveAchievement(int achievement);
	void PlayerThrewObject(EntityId object);

	void AddHUDEventListeners();
	void RemoveHUDEventListeners();

	//IGameFrameworkListener
	virtual void OnPostUpdate(float fDeltaTime) {};
	virtual void OnSaveGame(ISaveGame* pSaveGame) {};
	virtual void OnLoadGame(ILoadGame* pLoadGame) {};
	virtual void OnLevelEnd(const char* nextLevel) {};
	virtual void OnActionEvent(const SActionEvent& event);
	//IGameFrameworkListener

	// IGameRulesKillListener
	virtual void OnEntityKilledEarly(const HitInfo &hitInfo) {};
	virtual void OnEntityKilled(const HitInfo &hitInfo);
	// ~IGameRulesKillListener

	//IHUDEventListener
	virtual void OnHUDEvent(const SHUDEvent& event);
	//~IHUDEventListener

	//IPrivateGameListener
	virtual void SetPrivateGame(const bool privateGame);
	//~IPrivateGameListener

protected:
	
	enum ECategorisedHitType
	{
		eCHT_Collision,
		eCHT_Bullet,
		eCHT_Grenade,
		eCHT_Other,
	};
	ECategorisedHitType CategoriseHit(int hitType);
	ILINE bool AllowAchievement();

	// track some things for detecting certain achievements.
	//	NB: DO NOT SERIALIZE... potential for cheating:
	//	- player throws object
	//	- saves
	//	- on load object kills enemy
	//	- repeated loading = achievement
	EntityId m_lastPlayerThrownObject;
	CTimeValue m_lastPlayerThrownTime;

	EntityId m_lastPlayerKillBulletId;
	CTimeValue m_lastPlayerKillBulletSpawnTime;

	EntityId m_lastPlayerKillGrenadeId;
	int m_killsWithOneGrenade;
	CTimeValue m_lastPlayerKillGrenadeSpawnTime;

	bool m_allowAchievements;
};

#endif // __GAME_ACHIEVEMENTS_H__
