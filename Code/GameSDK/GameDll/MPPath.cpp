// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MPPath.h"

#include "GameRules.h"
#include "MPPathFollowingManager.h"

bool CMPPath::Init( IGameObject *pGameObject )
{
	SetGameObject(pGameObject);

	return true;
}

//------------------------------------------------------------------------
void CMPPath::Release()
{
	CMPPathFollowingManager* pPathFollowingManager = g_pGame->GetGameRules() ? g_pGame->GetGameRules()->GetMPPathFollowingManager() : NULL;
	if(pPathFollowingManager)
	{
		pPathFollowingManager->UnregisterPath(GetEntityId());
	}
	delete this;
}

//------------------------------------------------------------------------
void CMPPath::GetMemoryUsage(ICrySizer *pSizer) const
{
	pSizer->Add(*this);
}

//------------------------------------------------------------------------
bool CMPPath::ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params )
{
	ResetGameObject();
	CRY_ASSERT(!"CMPPath::ReloadExtension not implemented");
	return false;
}

//------------------------------------------------------------------------
bool CMPPath::GetEntityPoolSignature( TSerialize signature )
{
	CRY_ASSERT(!"CMPPath::GetEntityPoolSignature not implemented");
	return true;
}

void CMPPath::ProcessEvent( const SEntityEvent& details )
{
	if(details.event == ENTITY_EVENT_LEVEL_LOADED)
	{
		CMPPathFollowingManager* pPathFollowingManager = g_pGame->GetGameRules()->GetMPPathFollowingManager();
		if(pPathFollowingManager)
		{
			pPathFollowingManager->RegisterPath(GetEntityId());
		}
	}
}

uint64 CMPPath::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_LEVEL_LOADED);
}