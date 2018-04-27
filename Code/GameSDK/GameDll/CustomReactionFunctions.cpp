// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CustomReactionFunctions.h"

#include "HitDeathReactionsSystem.h"
#include "HitDeathReactions.h"
#include "GameRules.h"
#include "Player.h"
#include "ActorImpulseHandler.h"

namespace
{
	float SHOTGUN_HIGH_CALIBER_DISTANCE = 9.0f; // maximum distance where the shotgun shot will be considered "high caliber"
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CCustomReactionFunctions::CCustomReactionFunctions() : m_shotgunShellProjectile(0)
{
	RegisterCustomFunctions();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CCustomReactionFunctions::InitCustomReactionsData()
{
	g_pGame->GetIGameFramework()->GetNetworkSafeClassId(m_shotgunShellProjectile, "shotgunshell");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CCustomReactionFunctions::CallCustomValidationFunction(bool& bResult, ScriptTablePtr hitDeathReactionsTable, CActor& actor, const SReactionParams::SValidationParams& validationParams, const HitInfo& hitInfo, float fCausedDamage) const
{
	CRY_ASSERT(!validationParams.sCustomValidationFunc.empty());

	bool bSuccess = false;

	// Try to find a LUA function first. If not present, try to find the C++ version
	HSCRIPTFUNCTION validationFunc = NULL;
	if (!hitDeathReactionsTable->GetValue(validationParams.sCustomValidationFunc.c_str(), validationFunc))
	{
		// C++ custom validation
		ValidationFncContainer::const_iterator itFind = m_validationFunctors.find(validationParams.sCustomValidationFunc);
		if (itFind != m_validationFunctors.end())
		{
			bResult = itFind->second(actor, validationParams, hitInfo, fCausedDamage);
			bSuccess = true;
		}
	}
	else
	{
		CRY_ASSERT(validationFunc);

		ScriptTablePtr scriptHitInfo(gEnv->pScriptSystem);
		g_pGame->GetGameRules()->CreateScriptHitInfo(scriptHitInfo, hitInfo);

		bSuccess = Script::CallReturn(gEnv->pScriptSystem, validationFunc, hitDeathReactionsTable, validationParams.validationParamsScriptTable, scriptHitInfo, fCausedDamage, bResult);
		gEnv->pScriptSystem->ReleaseFunc(validationFunc);
	}

#ifndef _RELEASE
	if (!bSuccess)
		CHitDeathReactionsSystem::Warning("Couldn't find custom validation function (%s)", validationParams.sCustomValidationFunc.c_str());
#endif

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CCustomReactionFunctions::CallCustomExecutionFunction(ScriptTablePtr hitDeathReactionsTable, const string& function, CActor& actor, const SReactionParams& reactionParams, const HitInfo& hitInfo) const
{
	CRY_ASSERT(!function.empty());

	bool bSuccess = false;

	// try LUA first. This is so overriding C++ functions can be easily done on LUA, without the need to recompile or to change 
	// the name of the LUA methods in both reactionParams and script code
	HSCRIPTFUNCTION executionFunc = NULL;
	if (hitDeathReactionsTable->GetValue(function.c_str(), executionFunc))
	{
		IScriptSystem* pScriptSystem = hitDeathReactionsTable->GetScriptSystem();
		bSuccess = Script::Call(pScriptSystem, executionFunc, hitDeathReactionsTable, reactionParams.reactionScriptTable);
		pScriptSystem->ReleaseFunc(executionFunc);
	}

	// Try C++ now
	if (!bSuccess)
	{
		// C++ custom execution
		ExecutionFncContainer::const_iterator itFind = m_executionFunctors.find(function);
		if (itFind != m_executionFunctors.end())
		{
			// [*DavidR | 21/Oct/2010] C++ custom functions have the advantage of receiving a reference to the hitinfo, LUA methods can get the "lastHit" so we
			// avoid the expensive construction of the hitInfo table
			itFind->second(actor, reactionParams, hitInfo);
			bSuccess = true;
		}
		else
			CHitDeathReactionsSystem::Warning("Couldn't find custom execution function (%s)", function.c_str());
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CCustomReactionFunctions::RegisterCustomValidationFunction(const string& sName, const ValidationFunctor& validationFunctor)
{
	return m_validationFunctors.insert(ValidationFncContainer::value_type(sName, validationFunctor)).second;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CCustomReactionFunctions::RegisterCustomExecutionFunction(const string& sName, const ExecutionFunctor& executionFunctor)
{
	return m_executionFunctors.insert(ExecutionFncContainer::value_type(sName, executionFunctor)).second;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CCustomReactionFunctions::RegisterCustomFunctions()
{
	// Validation functions
	
	// Execution functions
	RegisterCustomExecutionFunction("FallAndPlay_Reaction", functor(*this, &CCustomReactionFunctions::FallAndPlay_Reaction));
	RegisterCustomExecutionFunction("DeathImpulse_Reaction", functor(*this, &CCustomReactionFunctions::DeathImpulse_Reaction));
	RegisterCustomExecutionFunction("DeathImpulse_PowerMelee", functor(*this, &CCustomReactionFunctions::DeathImpulse_PowerMelee));
	RegisterCustomExecutionFunction("ReactionDoNothing", functor(*this, &CCustomReactionFunctions::ReactionDoNothing));
	RegisterCustomExecutionFunction("MeleeDeath_Reaction", functor(*this, &CCustomReactionFunctions::MeleeDeath_Reaction));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CHitDeathReactionsPtr	CCustomReactionFunctions::GetActorHitDeathReactions(CActor& actor) const
{
	CRY_ASSERT(actor.GetActorClass() == CPlayer::GetActorClassType());
	CPlayer& player = static_cast<CPlayer&>(actor);
	return player.GetHitDeathReactions();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CCustomReactionFunctions::FallAndPlay_Reaction(CActor& actor, const SReactionParams& reactionParams, const HitInfo& hitInfo)
{
	actor.Fall();
	DeathImpulse_Reaction(actor, reactionParams, hitInfo);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CCustomReactionFunctions::DeathImpulse_Reaction(CActor& actor, const SReactionParams& reactionParams, const HitInfo& hitInfo)
{
	CHitDeathReactionsPtr pHitDeathReactions = GetActorHitDeathReactions(actor);
	if (pHitDeathReactions)
	{
		pHitDeathReactions->EndCurrentReaction();
	}

	CRY_ASSERT(actor.GetImpulseHander());
	actor.GetImpulseHander()->ApplyDeathImpulse(hitInfo);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CCustomReactionFunctions::DeathImpulse_PowerMelee(CActor& actor, const SReactionParams& reactionParams, const HitInfo& hitInfo)
{
	CRY_ASSERT(actor.GetImpulseHander());
	actor.GetImpulseHander()->QueueDeathImpulse(hitInfo, 0.05f);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CCustomReactionFunctions::MeleeDeath_Reaction(CActor& actor, const SReactionParams& reactionParams, const HitInfo& hitInfo)
{
	CHitDeathReactionsPtr pHitDeathReactions = GetActorHitDeathReactions(actor);
	if (pHitDeathReactions)
	{
		if ((g_pGameCVars->pl_melee.impulses_enable == SPlayerMelee::ei_FullyEnabled) || (g_pGameCVars->pl_melee.impulses_enable == SPlayerMelee::ei_OnlyToDead))
		{
			// Ragdollize and let CMelee do the work
			pHitDeathReactions->EndCurrentReaction();
		}
		else
		{
			if (!reactionParams.reactionAnim->animCRCs.empty())
			{
				// If an animation is present, execute the default reaction code
				pHitDeathReactions->ExecuteDeathReaction(reactionParams);
			}
			else
			{
				// Apply Death impulses
				DeathImpulse_Reaction(actor, reactionParams, hitInfo);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CCustomReactionFunctions::ReactionDoNothing(CActor& actor, const SReactionParams& reactionParams, const HitInfo& hitInfo)
{
	CHitDeathReactionsPtr pHitDeathReactions = GetActorHitDeathReactions(actor);
	if (pHitDeathReactions)
	{
		pHitDeathReactions->EndCurrentReaction();
	}
}
