// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "ScriptBind_HitDeathReactions.h"
#include "HitDeathReactions.h"

#include "Player.h"

#include "GameRules.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CScriptBind_HitDeathReactions::CScriptBind_HitDeathReactions(ISystem* pSystem, IGameFramework* pGameFramework) : m_pSystem(pSystem), m_pGameFW(pGameFramework)
{
	Init(pSystem->GetIScriptSystem(), pSystem, 1);

	//////////////////////////////////////////////////////////////////////////
	// Init tables.
	//////////////////////////////////////////////////////////////////////////
	m_pParams.Create(m_pSS);

#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_HitDeathReactions::

	SCRIPT_REG_TEMPLFUNC(OnHit, "scriptHitInfo");
	SCRIPT_REG_TEMPLFUNC(ExecuteHitReaction, "reactionParams");
	SCRIPT_REG_TEMPLFUNC(ExecuteDeathReaction, "reactionParams");
	SCRIPT_REG_FUNC(EndCurrentReaction);
	SCRIPT_REG_FUNC(StartReactionAnim);
	SCRIPT_REG_FUNC(EndReactionAnim);
	SCRIPT_REG_TEMPLFUNC(IsValidReaction, "reactionParams, scriptHitInfo");
	SCRIPT_REG_TEMPLFUNC(StartInteractiveAction, "szActionName");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CScriptBind_HitDeathReactions::~CScriptBind_HitDeathReactions()
{
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
int	CScriptBind_HitDeathReactions::OnHit(IFunctionHandler *pH, SmartScriptTable scriptHitInfo)
{
	bool bRet = false;

	CHitDeathReactionsPtr pHitDeathReactions = GetHitDeathReactions(pH);
	if (pHitDeathReactions)
	{
		HitInfo hitInfo;
		CGameRules::CreateHitInfoFromScript(scriptHitInfo, hitInfo);

		bRet = pHitDeathReactions->OnHit(hitInfo);
	}
	
	return pH->EndFunction(bRet);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
int CScriptBind_HitDeathReactions::ExecuteDeathReaction (IFunctionHandler *pH, SmartScriptTable reactionParams)
{
	CHitDeathReactionsPtr pHitDeathReactions = GetHitDeathReactions(pH);
	if (pHitDeathReactions)
		pHitDeathReactions->ExecuteDeathReaction(reactionParams);

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
int CScriptBind_HitDeathReactions::ExecuteHitReaction (IFunctionHandler *pH, SmartScriptTable reactionParams)
{
	CHitDeathReactionsPtr pHitDeathReactions = GetHitDeathReactions(pH);
	if (pHitDeathReactions)
		pHitDeathReactions->ExecuteHitReaction(reactionParams);

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
int CScriptBind_HitDeathReactions::EndCurrentReaction(IFunctionHandler *pH)
{
	bool bSuccess = false;

	CHitDeathReactionsPtr pHitDeathReactions = GetHitDeathReactions(pH);
	if (pHitDeathReactions)
		bSuccess = pHitDeathReactions->EndCurrentReaction();

	return pH->EndFunction(bSuccess);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
int	CScriptBind_HitDeathReactions::IsValidReaction(IFunctionHandler *pH, SmartScriptTable validationParams, SmartScriptTable scriptHitInfo)
{
	bool bResult = false;

	CHitDeathReactionsPtr pHitDeathReactions = GetHitDeathReactions(pH);
	if (pHitDeathReactions)
	{
		HitInfo hitInfo;
		CGameRules::CreateHitInfoFromScript(scriptHitInfo, hitInfo);

		float fCausedDamage = 0.0f;
		if (pH->GetParamCount() > 2)
			pH->GetParam(3, fCausedDamage);

		bResult = pHitDeathReactions->IsValidReaction(hitInfo, validationParams, fCausedDamage);
	}

	return pH->EndFunction(bResult);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
int CScriptBind_HitDeathReactions::StartReactionAnim(IFunctionHandler *pH)
{
	bool bResult = false;

	CHitDeathReactionsPtr pHitDeathReactions = GetHitDeathReactions(pH);
	if (pHitDeathReactions)
	{
		const char* szAnimName = NULL;
		bool bLoop = false;
		float fBlendTime = 0.2f;
		int iSlot = 0;
		int iLayer = 0;
		float fAniSpeed = 1.0f;
		uint32 animFlags = 0;

		if (!pH->GetParam(1, szAnimName))
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CScriptBind_HitDeathReactions::StartReactionAnim, animation name not specified");
		}
		else if (pH->GetParamCount() > 1)
		{
			pH->GetParam(2, bLoop);
			if (pH->GetParamCount() > 2)
			{
				pH->GetParam(3, fBlendTime);
				if (pH->GetParamCount() > 3)
				{
					pH->GetParam(4, iSlot);
					if (pH->GetParamCount() > 4)
					{
						pH->GetParam(5, iLayer);
						if (pH->GetParamCount() > 5)
						{
							pH->GetParam(5, fAniSpeed);
							if (pH->GetParamCount() > 6)
							{
								pH->GetParam(6, animFlags);
							}
						}
					}
				}
			}
		}
		
		bResult = pHitDeathReactions->StartReactionAnim(szAnimName, bLoop, fBlendTime, iSlot, iLayer, animFlags, fAniSpeed);
	}

	return pH->EndFunction(bResult);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
int CScriptBind_HitDeathReactions::EndReactionAnim(IFunctionHandler *pH)
{
	CHitDeathReactionsPtr pHitDeathReactions = GetHitDeathReactions(pH);
	if (pHitDeathReactions)
	{
		pHitDeathReactions->EndReactionAnim();
	}

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
int CScriptBind_HitDeathReactions::StartInteractiveAction(IFunctionHandler *pH, const char* szActionName)
{
	CPlayer* pPlayer = GetAssociatedActor(pH);
	if (pPlayer)
		pPlayer->StartInteractiveActionByName(szActionName,true);

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CPlayer* CScriptBind_HitDeathReactions::GetAssociatedActor(IFunctionHandler *pH) const
{
	SmartScriptTable selfTable;
	pH->GetSelf(selfTable);

	CRY_ASSERT(selfTable->HaveValue("__actor"));

	ScriptHandle actorEntityId;
	selfTable->GetValue("__actor", actorEntityId);

	IActor* pActor = m_pGameFW->GetIActorSystem()->GetActor(static_cast<EntityId>(actorEntityId.n));

	// [*DavidR | 13/Nov/2009] WARNING: This downcast could be dangerous if CHitDeathReactions is moved to 
	// CActor classes
	CRY_ASSERT(pActor && (pActor->GetActorClass() == CPlayer::GetActorClassType()));
	return static_cast<CPlayer*>(pActor);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CHitDeathReactionsPtr CScriptBind_HitDeathReactions::GetHitDeathReactions(IFunctionHandler *pH) const
{
	CPlayer* pActor = GetAssociatedActor(pH);
	CRY_ASSERT(pActor);

	return pActor ? pActor->GetHitDeathReactions() : CHitDeathReactionsPtr();
}
