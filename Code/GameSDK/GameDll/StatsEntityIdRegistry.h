// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 09:03:2012		Created by Colin Gulliver
*************************************************************************/

#pragma once

#include <CryString/CryFixedString.h>

struct IEntityClass;

class CStatsEntityIdRegistry
{
public:
	CStatsEntityIdRegistry();

	uint16 GetGameModeId(const char *pModeName) const;
	uint16 GetGameModeId(const IEntityClass *pClass) const;

	uint16 GetMapId(const char *pMapName) const;
	
	uint16 GetWeaponId(const char *pWeaponName) const;
	uint16 GetWeaponId(const IEntityClass *pClass) const;

	uint16 GetHitTypeId(const char* pHitTypeName) const; 
	uint16 GetDefaultWeapon() const { return m_defaultWeapon; }

	const char* GetGameMode(uint16 id) const;
	const char* GetMap(uint16 id) const;
	const char* GetWeapon(uint16 id) const;
	const char* GetHitType(uint16 id) const; 
	
private:
	typedef CryFixedStringT<32> TFixedString;

	struct SClassId
	{
		const IEntityClass *m_pClass;
		uint16 m_id;
	};

	struct SStringId
	{
		TFixedString m_name;
		uint16 m_id;
	};

	typedef std::vector<SClassId> TClassIdVec;
	typedef std::vector<SStringId> TStringIdVec;

	static void ReadClassIds(XmlNodeRef xmlNode, uint16 &defaultId, TClassIdVec &vec );
	static void ReadStringIds(XmlNodeRef xmlNode, uint16 &defaultId, TStringIdVec &vec );
	
	TClassIdVec m_gameModes;
	TClassIdVec m_weapons;
	TStringIdVec m_weaponExtensions;
	TStringIdVec m_maps;
	TStringIdVec	m_hitTypes;
	
	uint16 m_defaultGameMode;
	uint16 m_defaultWeapon;
	uint16 m_defaultMap;
	uint16 m_defaultHitType;
};
