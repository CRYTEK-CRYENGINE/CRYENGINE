// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 30:8:2005   12:33 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Item.h"
#include "ItemSharedParams.h"
#include "Game.h"

//------------------------------------------------------------------------
void CItem::ReadProperties(IScriptTable *pProperties)
{
	if (pProperties)
	{
		GetEntityProperty("HitPoints", m_properties.hitpoints);
		GetEntityProperty("bPickable", m_properties.pickable);
		GetEntityProperty("bMounted", m_properties.mounted);
		GetEntityProperty("bUsable", m_properties.usable);

		GetEntityProperty("Respawn", "bRespawn", m_respawnprops.respawn);
		GetEntityProperty("Respawn", "nTimer", m_respawnprops.timer);
		GetEntityProperty("Respawn", "bUnique", m_respawnprops.unique);

		int physicsTemp;
		GetEntityProperty("eiPhysicsType", physicsTemp);
		m_properties.physics = (ePhysicalization)physicsTemp;

		if(!gEnv->bMultiplayer)
		{
			GetEntityProperty("bSpecialSelect", m_properties.specialSelect);

			ReadMountedProperties(pProperties);
		}
	}
}

//------------------------------------------------------------------------
void CItem::ReadMountedProperties(IScriptTable* pScriptTable)
{
	float minPitch = 0.f;
	float maxPitch = 0.f;

	GetEntityProperty("MountedLimits", "pitchMin", minPitch);
	GetEntityProperty("MountedLimits", "pitchMax", maxPitch);
	GetEntityProperty("MountedLimits", "yaw", m_properties.mounted_yaw_range);

	m_properties.mounted_min_pitch = min(minPitch,maxPitch);
	m_properties.mounted_max_pitch = max(minPitch,maxPitch);

	GetEntityProperty("Respawn", "bRespawn", m_respawnprops.respawn);
	GetEntityProperty("Respawn", "nTimer", m_respawnprops.timer);
	GetEntityProperty("Respawn", "bUnique", m_respawnprops.unique);
}

//------------------------------------------------------------------------
void CItem::InitItemFromParams()
{
	InitGeometry();
	InitAccessories();
	InitDamageLevels();
}

//------------------------------------------------------------------------
void CItem::InitGeometry()
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	//skip loading the first person geometry for now, it may never be used
	m_sharedparams->LoadGeometryForItem(this, eIGS_FirstPerson);
}

//-----------------------------------------------------------------------
void CItem::InitAccessories()
{
	m_initialSetup = m_sharedparams->initialSetup;
}

//-----------------------------------------------------------------------
void CItem::InitDamageLevels()
{
	int numLevels = m_sharedparams->damageLevels.size();

	m_damageLevelEffects.resize(numLevels);

	for(int i = 0; i < numLevels; i++)
	{
		m_damageLevelEffects[i] = -1;
	}
}