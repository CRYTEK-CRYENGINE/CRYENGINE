// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Definitions for Lua-caching helpers used by Actors to avoid
				Lua accessing at game time
  
 -------------------------------------------------------------------------
  History:
  - 01:07:2010: Created by Kevin Kirst

*************************************************************************/

#include "StdAfx.h"
#include "ActorLuaCache.h"
#include "Actor.h"

//////////////////////////////////////////////////////////////////////////
void SLuaCache_ActorPhysicsParams::GetMemoryUsage(ICrySizer *s) const
{
	s->Add(*this);
}

//////////////////////////////////////////////////////////////////////////
bool SLuaCache_ActorPhysicsParams::CacheFromTable(SmartScriptTable pEntityTable, const char* szEntityClassName)
{
	assert((bool)pEntityTable);

	if (!bIsCached)
	{
		bIsCached = CActor::LoadPhysicsParams(pEntityTable, szEntityClassName, params, playerDim, playerDyn);
	}

	return bIsCached;
}

//////////////////////////////////////////////////////////////////////////
void SLuaCache_ActorGameParams::GetMemoryUsage(ICrySizer *s) const
{
	s->Add(*this);
}

//////////////////////////////////////////////////////////////////////////
bool SLuaCache_ActorGameParams::CacheFromTable(SmartScriptTable pEntityTable)
{
	assert((bool)pEntityTable);

	if (!bIsCached)
	{
		bIsCached = CActor::LoadGameParams(pEntityTable, gameParams);
		bIsCached &= CActor::LoadAutoAimParams(pEntityTable, autoAimParams);
	}

	return bIsCached;
}

//////////////////////////////////////////////////////////////////////////
void SLuaCache_ActorProperties::GetMemoryUsage(ICrySizer *s) const
{
	s->Add(*this);
	s->AddContainer(fileModelInfo.IKLimbInfo);
}

//////////////////////////////////////////////////////////////////////////
bool SLuaCache_ActorProperties::CacheFromTable(SmartScriptTable pEntityTable, SmartScriptTable pProperties)
{
	assert((bool)pEntityTable);

	if (!bIsCached)
	{
		bIsCached = CActor::LoadFileModelInfo(pEntityTable, pProperties, fileModelInfo);

		if (pProperties)
		{
			pProperties->GetValue("physicMassMult", fPhysicMassMult);
		}
	}

	return bIsCached;
}