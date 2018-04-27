// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 23:09:2009   : Created by Jan Neugebauer

*************************************************************************/

#pragma once

#ifndef WEAPONSTATS_H
#define WEAPONSTATS_H

#include "ItemParamsRegistration.h"

class CItemSharedParams;
struct SAccessoryParams;

enum EWeaponStat
{
	eWeaponStat_Accuracy = 0,
	eWeaponStat_RateOfFire,
	eWeaponStat_Mobility,
	eWeaponStat_Damage,
	eWeaponStat_Range,
	eWeaponStat_Recoil,
	eWeaponStat_NumStats
};

static const char* s_weaponStatNames[eWeaponStat_NumStats] = { "stat_accuracy", "stat_rate_of_fire", "stat_mobility", "stat_damage", "stat_range", "stat_recoil" };

struct SWeaponStatsData
{
	SWeaponStatsData();

	void ReadStats(const XmlNodeRef& paramsNode, bool defaultInit=false);
	void ReadStatsByAttribute(const XmlNodeRef& paramsNode, bool defaultInit = false);

	void GetMemoryUsage(ICrySizer* s) const {}

	typedef CryFixedArray<int, eWeaponStat_NumStats> StatsArray;

#define WEAPON_STATSDATA_PARAMS(f)
#define WEAPON_STATSDATA_PARAM_VECTORS(f) \
	f(StatsArray, stats)

	REGISTER_STRUCT(WEAPON_STATSDATA_PARAM_VECTORS, SWeaponStatsData)
};

struct SWeaponStatStoredData
{
	SWeaponStatStoredData()
	: totalValue(0)
	, baseValue(0)
	, accessoryModifier(0)
	, firemodeModifier(0)
	{
	}

	void UpdateTotalValue();

	int totalValue;
	int baseValue;
	int accessoryModifier;
	int firemodeModifier;

};

class CWeaponStats
{

public:

	CWeaponStats();
	~CWeaponStats();

	void SetBaseStats(const CItemSharedParams* params);
	void SetStatsFromAccessory(const SAccessoryParams *params, const bool attach);
	void SetStatsFromFiremode(const SWeaponStatsData *pStats);
	const int GetStat(const EWeaponStat stat) const;
	const SWeaponStatStoredData& GetStatData(const EWeaponStat stat) const;
	void SetStat(const EWeaponStat stat, const SWeaponStatStoredData& statData);
	void UpdateTotalValue();
private:

	CryFixedArray<SWeaponStatStoredData, eWeaponStat_NumStats> m_weaponStats;
	
	typedef std::set<const IEntityClass*> TWeaponStatsAttachments;
	TWeaponStatsAttachments m_attachments;
};

#endif //WEAPONSTATS_H
