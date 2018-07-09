// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 09:03:2012		Created by Colin Gulliver
*************************************************************************/

#include "StdAfx.h"

#include "StatsEntityIdRegistry.h"

#include <CryEntitySystem/IEntitySystem.h>

//-------------------------------------------------------------------------
CStatsEntityIdRegistry::CStatsEntityIdRegistry()
	: m_defaultGameMode(0)
	, m_defaultMap(0)
	, m_defaultWeapon(0)
	, m_defaultHitType(0)
{
	XmlNodeRef xmlRoot = gEnv->pSystem->LoadXmlFromFile("Scripts/Progression/StatsEntityIds.xml");
	if (xmlRoot)
	{
		const int numChildren = xmlRoot->getChildCount();
		for (int i = 0; i < numChildren; ++ i)
		{
			XmlNodeRef xmlChild = xmlRoot->getChild(i);
			const char *pTag = xmlChild->getTag();
			if (!stricmp(pTag, "GameModes"))
			{
				ReadClassIds(xmlChild, m_defaultGameMode, m_gameModes);
			}
			else if (!stricmp(pTag, "Weapons"))
			{
				ReadClassIds(xmlChild, m_defaultWeapon, m_weapons);
			}
			else if (!stricmp(pTag, "WeaponExtensions"))
			{
				ReadStringIds(xmlChild, m_defaultWeapon, m_weaponExtensions);
			}
			else if (!stricmp(pTag, "Maps"))
			{
				ReadStringIds(xmlChild, m_defaultMap, m_maps);
			}
			else if (!stricmp(pTag, "DamageTypes"))
			{
				ReadStringIds(xmlChild, m_defaultHitType, m_hitTypes);
			}
		}
	}
}

//-------------------------------------------------------------------------
/* static */ void CStatsEntityIdRegistry::ReadClassIds( XmlNodeRef xmlNode, uint16 &defaultId, TClassIdVec &vec )
{
	const char *pDefault;
	if (xmlNode->getAttr("default", &pDefault))
	{
		defaultId = atoi(pDefault);
	}

	const IEntityClassRegistry *pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
	const int numEntities = xmlNode->getChildCount();
	vec.reserve(numEntities);
	for (int i = 0; i < numEntities; ++ i)
	{
		XmlNodeRef xmlEntity = xmlNode->getChild(i);

		const char *pName;
		const char *pValue;
		if (xmlEntity->getAttr("name", &pName) && xmlEntity->getAttr("value", &pValue))
		{
			SClassId entity;
			entity.m_pClass = pClassRegistry->FindClass(pName);
			CRY_ASSERT_TRACE(entity.m_pClass, ("Failed to find class '%s' referenced in StatsEntityIds.xml", pName));
			entity.m_id = atoi(pValue);
			vec.push_back(entity);
		}
	}
}

//-------------------------------------------------------------------------
/* static */ void CStatsEntityIdRegistry::ReadStringIds( XmlNodeRef xmlNode, uint16 &defaultId, TStringIdVec &vec )
{
	const char *pDefault;
	if (xmlNode->getAttr("default", &pDefault))
	{
		defaultId = atoi(pDefault);
	}

	const int numEntities = xmlNode->getChildCount();
	vec.reserve(numEntities);
	for (int i = 0; i < numEntities; ++ i)
	{
		XmlNodeRef xmlEntity = xmlNode->getChild(i);

		const char *pName;
		const char *pValue;
		if (xmlEntity->getAttr("name", &pName) && xmlEntity->getAttr("value", &pValue))
		{
			SStringId entity;
			entity.m_name = pName;
			entity.m_id = atoi(pValue);
			vec.push_back(entity);
		}
	}
}

//-------------------------------------------------------------------------
uint16 CStatsEntityIdRegistry::GetGameModeId( const char *pModeName ) const
{
	const IEntityClass *pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(pModeName);
	CRY_ASSERT_TRACE(pClass, ("Failed to find class '%s'", pModeName));
	return GetGameModeId(pClass);
}

//-------------------------------------------------------------------------
uint16 CStatsEntityIdRegistry::GetGameModeId( const IEntityClass *pClass ) const
{
	int numModes = m_gameModes.size();
	for (int i = 0; i < numModes; ++ i)
	{
		if (m_gameModes[i].m_pClass == pClass)
		{
			return m_gameModes[i].m_id;
		}
	}
	return m_defaultGameMode;
}

//-------------------------------------------------------------------------
uint16 CStatsEntityIdRegistry::GetMapId( const char *pMapName ) const
{
	int numMaps = m_maps.size();
	for (int i = 0; i < numMaps; ++ i)
	{
		if (!stricmp(m_maps[i].m_name.c_str(), pMapName))
		{
			return m_maps[i].m_id;
		}
	}
	return m_defaultMap;
}

//-------------------------------------------------------------------------
uint16 CStatsEntityIdRegistry::GetWeaponId( const char *pWeaponName ) const
{
	if( const IEntityClass *pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(pWeaponName) )
	{
		return GetWeaponId( pClass );
	}
	else
	{
		int numWeaponExtensions = m_weaponExtensions.size();
		for( int i = 0; i < numWeaponExtensions; ++i )
		{
			if( !stricmp( m_weaponExtensions[i].m_name.c_str(), pWeaponName ) )
			{
				return m_weaponExtensions[i].m_id;
			}
		}
		return m_defaultWeapon;
	}
	
}

//-------------------------------------------------------------------------
uint16 CStatsEntityIdRegistry::GetWeaponId( const IEntityClass *pClass ) const
{
	int numWeapons = m_weapons.size();
	for (int i = 0; i < numWeapons; ++ i)
	{
		if (m_weapons[i].m_pClass == pClass)
		{
			return m_weapons[i].m_id;
		}
	}
	return m_defaultWeapon;
}

//-------------------------------------------------------------------------
uint16 CStatsEntityIdRegistry::GetHitTypeId( const char* pHitTypeName ) const
{
	int numHitTypes = m_hitTypes.size();
	for (int i = 0; i < numHitTypes; ++ i)
	{
		if (!stricmp(m_hitTypes[i].m_name.c_str(), pHitTypeName))
		{
			return m_hitTypes[i].m_id;
		}
	}
	return m_defaultHitType;
}

//-------------------------------------------------------------------------
const char * CStatsEntityIdRegistry::GetGameMode( uint16 id ) const
{
	int numModes = m_gameModes.size();
	for (int i = 0; i < numModes; ++ i)
	{
		if (m_gameModes[i].m_id == id)
		{
			return m_gameModes[i].m_pClass->GetName();
		}
	}
	return NULL;
}

//-------------------------------------------------------------------------
const char * CStatsEntityIdRegistry::GetMap( uint16 id ) const
{
	int numMaps = m_maps.size();
	for (int i = 0; i < numMaps; ++ i)
	{
		if (m_maps[i].m_id == id)
		{
			return m_maps[i].m_name.c_str();
		}
	}
	return NULL;
}

//-------------------------------------------------------------------------
const char * CStatsEntityIdRegistry::GetWeapon( uint16 id ) const
{
	int numWeapons = m_weapons.size();
	for( int i = 0; i < numWeapons; ++i )
	{
		if( m_weapons[i].m_id == id )
		{
			return m_weapons[i].m_pClass->GetName();
		}
	}

	//not in main weapons, try extensions
	numWeapons = m_weaponExtensions.size();
	for( int i = 0; i < numWeapons; ++i )
	{
		if( m_weaponExtensions[i].m_id == id )
		{
			return m_weaponExtensions[i].m_name.c_str();
		}
	}

	return NULL;
}

//-------------------------------------------------------------------------
const char* CStatsEntityIdRegistry::GetHitType( uint16 id ) const
{
	int numHitTypes = m_hitTypes.size();
	for (int i = 0; i < numHitTypes; ++ i)
	{
		if (m_hitTypes[i].m_id == id)
		{
			return m_hitTypes[i].m_name;
		}
	}
	return NULL;
}
