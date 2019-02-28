// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 

-------------------------------------------------------------------------
History:
- 27:07:2010   Extracted from BodyDamage.h/.cpp by Benito Gangoso Rodriguez

*************************************************************************/

#include "StdAfx.h"
#include "BodyManagerCVars.h"
#include "Actor.h"
#include "BodyManager.h"

int CBodyManagerCVars::g_bodyDamage_log = 0;
int CBodyManagerCVars::g_bodyDestruction_debug = 0;
ICVar* CBodyManagerCVars::g_bodyDestruction_debugFilter = NULL;

void CBodyManagerCVars::RegisterCommands()
{
	REGISTER_COMMAND("g_bodyDamage_reload", ReloadBodyDamage, VF_CHEAT, "Reloads bodyDamage for the specified actor, or for everyone if not specified");
	REGISTER_COMMAND("g_bodyDestruction_reload", ReloadBodyDestruction, VF_CHEAT, "Reloads all body destruction data files");
}

void CBodyManagerCVars::UnregisterCommands(IConsole* pConsole)
{
	if (pConsole)
	{
		pConsole->RemoveCommand("g_bodyDamage_reload");
		pConsole->RemoveCommand("g_bodyDestruction_reload");
	}
}

void CBodyManagerCVars::RegisterVariables()
{
	REGISTER_CVAR(g_bodyDamage_log, 0, 0, "Enables/Disables BodyDamage logging");
	REGISTER_CVAR(g_bodyDestruction_debug, 0, 0, "Enables/Disables BodyDestruction Debug info");

	g_bodyDestruction_debugFilter = REGISTER_STRING("g_bodyDestruction_debugFilter","",VF_CHEAT,"");
	
	CRY_ASSERT(g_bodyDestruction_debugFilter);
}

void CBodyManagerCVars::UnregisterVariables(IConsole* pConsole)
{
	if (pConsole)
	{
		pConsole->UnregisterVariable("g_bodyDamage_log", true);
		pConsole->UnregisterVariable("g_bodyDestruction_debug", true);
		pConsole->UnregisterVariable("g_bodyDestruction_debugFilter", true);
	}
	g_bodyDestruction_debugFilter = NULL;
}

void CBodyManagerCVars::Reload(IActor* actor)
{
	CActor* pActor = static_cast<CActor*>(actor);
	if (pActor)
	{
		CBodyDamageManager *pBodyDamageManager = g_pGame->GetBodyDamageManager();
		assert(pBodyDamageManager);

		pBodyDamageManager->ReloadBodyDamage(*pActor);
	}
}

void CBodyManagerCVars::Reload(IEntity* pEntity)
{
	if (pEntity)
	{
		IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId());
		if (pActor)
		{
			Reload(pActor);
		}
		else
		{
			CBodyDamageManager *pBodyDamageManager = g_pGame->GetBodyDamageManager();
			assert(pBodyDamageManager);

			TBodyDamageProfileId profileId = pBodyDamageManager->FindBodyDamageProfileIdBinding(pEntity->GetId());
			if (profileId != INVALID_BODYDAMAGEPROFILEID)
			{
				pBodyDamageManager->ReloadBodyDamage(profileId, *pEntity);
			}
		}
	}
}

void CBodyManagerCVars::ReloadBodyDamage(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() > 1)
	{
		IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(pArgs->GetArg(1));
		Reload(pEntity);
		
	}
	else
	{
		CBodyDamageManager *pBodyDamageManager = g_pGame->GetBodyDamageManager();
		assert(pBodyDamageManager);

		pBodyDamageManager->ReloadBodyDamage();

		IActorIteratorPtr pIt = g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
		while (IActor* pActor = pIt->Next())
			Reload(pActor);
	}
}

void CBodyManagerCVars::ReloadBodyDestruction(IConsoleCmdArgs* pArgs)
{

	CBodyDamageManager *pBodyDamageManager = g_pGame->GetBodyDamageManager();
	assert(pBodyDamageManager);

	pBodyDamageManager->ReloadBodyDestruction();

	IActorIteratorPtr pIt = g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
	while (CActor* pActor = static_cast<CActor*>(pIt->Next()))
	{
		pActor->ReloadBodyDestruction();
	}
}