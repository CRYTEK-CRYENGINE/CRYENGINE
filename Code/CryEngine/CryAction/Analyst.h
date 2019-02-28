// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ANALYST_H__
#define __ANALYST_H__

#pragma once

#include "IGameplayRecorder.h"

class CGameplayAnalyst : public IGameplayListener
{
public:
	CGameplayAnalyst();
	virtual ~CGameplayAnalyst();

	// IGameplayListener
	virtual void OnGameplayEvent(IEntity* pEntity, const GameplayEvent& event);
	//~IGameplayListener

	void ProcessPlayerEvent(EntityId id, const GameplayEvent& event);

	// Feel free to add
	void GetMemoryStatistics(ICrySizer* s) {}

	typedef struct WeaponAnalysis
	{
		WeaponAnalysis() : usage(0), timeUsed(0.0f), zoomUsage(0), timeZoomed(0.0f), firemodes(0), melee(0), shots(0), hits(0), reloads(0), kills(0), deaths(0), damage(0) {}

		int        usage;    // number of times this weapon was selected
		CTimeValue timeUsed; // total time this weapon was used

		int        zoomUsage;  // number of zoom ins
		CTimeValue timeZoomed; // total times zoomed with this weapon

		int        firemodes; // total number of firemode changes

		int        melee; // total number of melee attacks with this weapon
		int        shots; // total number of shots fired with this weapon
		int        hits;  // total number of hits with this weapon
		int        reloads;// total number of reloads with this weapon
		int        kills;  // kills with this weapon
		int        deaths; // deaths while carrying this weapon
		int        damage; // total damage dealt with this weapon
	}WeaponAnalysis;

	typedef struct PurchaseDetail
	{
		PurchaseDetail() : totalAmount(0), totalSpent(0) {}

		int totalAmount;    // total amount of times purchased
		int totalSpent;     // total amount of currency spent
	}PurchaseDetail;

	typedef struct CurrencyAnalysis
	{
		CurrencyAnalysis() : totalEarned(0), totalSpent(0), nMin(0), nMax(0) {}

		int                              totalEarned; // total number of currency earned
		int                              totalSpent;  // total number of currency spent
		int                              nMin;        // lowest ever currency value
		int                              nMax;        // max ever currency value

		std::map<string, PurchaseDetail> spentDetail; // total currency spent on each item
	} CurrencyAnalysis;

	typedef struct SuitAnalysis
	{
		SuitAnalysis()
			: mode(3)
		{
			for (int i = 0; i < 4; i++)
			{
				timeUsed[i] = 0.0f;
				usage[i] = 0;
				kills[i] = 0;
				deaths[i] = 0;
			}
		}

		CTimeValue usageStart;
		CTimeValue timeUsed[4];   // total time using each suit mode
		int        usage[4];      // number of times this suit mode was selected

		int        kills[4];      // number of kills using each suit mode
		int        deaths[4];     // number of deaths using each suit mode
		int        mode;
	} SuitAnalysis;

	typedef std::map<string, WeaponAnalysis> Weapons;

	typedef struct PlayerAnalysis
	{
		PlayerAnalysis()
			: promotions(0),
			demotions(0),
			rank(0),
			rankStart(0.0f),
			maxRank(0),
			zoomUsage(0),
			itemUsage(0),
			firemodes(0),
			melee(0),
			shots(0),
			hits(0),
			reloads(0),
			damage(0),
			kills(0),
			deaths(0),
			deathStart(0.0f),
			timeDead(0.0f),
			timeAlive(0.0f),
			timeStart(0.0f),
			alive(false)
		{
			memset(rankTime, 0, sizeof(rankTime));
		};

		string           name; // player name

		int              promotions;
		int              demotions;
		int              rank;
		CTimeValue       rankStart;

		int              maxRank; // max rank achieved
		CTimeValue       rankTime[10];// total time spent in each rank

		int              zoomUsage; // total number of times zoomed
		int              itemUsage; // total number of item selections
		int              firemodes; // total number of firemode changes

		int              melee;   // total number of melee attacks
		int              shots;   // total number of shots
		int              hits;    // total number of hits
		int              reloads; // total number of reloads
		int              damage;  // total damage dealt
		int              kills;   // total number of kills
		int              deaths;  // total number of deaths
		bool             alive;

		CTimeValue       deathStart;
		CTimeValue       timeDead;  // time dead / waiting to respawn
		CTimeValue       timeAlive; // time alive
		CTimeValue       timeStart; // total time played

		Weapons          weapons;   // weapon analysis
		SuitAnalysis     suit;
		CurrencyAnalysis currency;  // currency analysis
	} PlayerAnalysis;

	typedef std::map<EntityId, PlayerAnalysis> Players;

	typedef struct
	{
		Players players;
	} GameAnalysis;

	void Release() { delete this; };

	void Reset();
	void DumpToTXT();

	void DumpWeapon(EntityId playerId, string& lines);
	void DumpRank(string& lines);
	void DumpSuit(string& lines);
	void DumpCurrency(string& lines);

private:
	void            NewPlayer(IEntity* pEntity);
	bool            IsPlayer(EntityId entityId) const;

	PlayerAnalysis& GetPlayer(EntityId playerId);

	WeaponAnalysis& GetWeapon(EntityId playerId, EntityId weaponId);
	WeaponAnalysis& GetWeapon(EntityId playerId, const char* weapon);
	WeaponAnalysis& GetCurrentWeapon(EntityId playerId);

	GameAnalysis    m_gameanalysis;
};
#endif
