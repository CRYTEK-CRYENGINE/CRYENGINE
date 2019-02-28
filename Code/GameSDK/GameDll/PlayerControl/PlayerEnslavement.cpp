// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Various support classes for having the player's animation enslaved to other entities.

#include "StdAfx.h"

#include "PlayerEnslavement.h"

#include "ICryMannequin.h"

#include "Actor.h"
#include "Player.h"
#include "PlayerStateEvents.h"
#include "GameConstantCVars.h"


// ===========================================================================
// ===========================================================================
// ===========================================================================
//
// -- CPlayerEnslavementForButtonMashing -- CPlayerEnslavementForButtonMashing --
//
// ===========================================================================
// ===========================================================================
// ===========================================================================


const char CPlayerEnslavementForButtonMashing::s_slaveContextName[] = "PlayerSlave";


CPlayerEnslavementForButtonMashing::CPlayerEnslavementForButtonMashing() :
	m_ADBFileName()
	, m_enslaved(false)
{
}


CPlayerEnslavementForButtonMashing::~CPlayerEnslavementForButtonMashing()
{
	CRY_ASSERT_MESSAGE(!m_enslaved, "Player animation is still synched with an other entity?!");	
}


void CPlayerEnslavementForButtonMashing::PreLoadADB(const char* adbFileName)
{
	assert(adbFileName != NULL);
	assert(strlen(adbFileName) < m_ADBFileName.MAX_SIZE);
	m_ADBFileName = adbFileName;

	IMannequin& mannequinInterface = g_pGame->GetIGameFramework()->GetMannequinInterface();

	mannequinInterface.GetAnimationDatabaseManager().Load( adbFileName );
}


// ===========================================================================
// Enslave/release the player animation synchronization.
//
// In:    Pointer to the action controller of the master animated character
//        (in this case the boss) (NULL will abort!)
// In:    True if the player animations should be enslaved and synchronized; 
//        false if released.
//
void CPlayerEnslavementForButtonMashing::EnslavePlayer(IActionController* pMasterActionController, const bool enslave)
{
	IF_UNLIKELY(m_enslaved == enslave)
		return;

	CActor *pClientActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
	IF_UNLIKELY(pClientActor == NULL)
		return;

	IActionController *pClientActionController = pClientActor->GetAnimatedCharacter() ? pClientActor->GetAnimatedCharacter()->GetActionController() : NULL;

	IF_UNLIKELY((pClientActionController == NULL) || (pMasterActionController == NULL))
		return;

	IMannequin &mannequinInterface = gEnv->pGameFramework->GetMannequinInterface();
	CMannequinUserParamsManager& mannequinUserParams = mannequinInterface.GetMannequinUserParamsManager();

	const IAnimationDatabase *pSlaveAnimationDB = mannequinInterface.GetAnimationDatabaseManager().Load( m_ADBFileName.c_str() );

	IF_UNLIKELY(pSlaveAnimationDB == NULL)
		return;

	const TagID tagID = pMasterActionController->GetContext().controllerDef.m_scopeContexts.Find( s_slaveContextName );
	IF_UNLIKELY (tagID == TAG_ID_INVALID)
	{
		CRY_ASSERT_MESSAGE(false, "Unable to enslave player because scope context is missing?!");
		return;
	}
	const uint32 contextID = (uint32)tagID; // (The tag ID is a universal 'interface' for mannequin).
	pMasterActionController->SetSlaveController( *pClientActionController, contextID, enslave, pSlaveAnimationDB );

	m_enslaved = enslave;

	if (enslave == false)
	{
		static_cast<CPlayer*>(pClientActor)->StateMachineHandleEventMovement( SStateEvent(PLAYER_EVENT_BUTTONMASHING_SEQUENCE_END) );
	}
}


// ===========================================================================
// ===========================================================================
// ===========================================================================
//
// -- CPlayerFightProgressionForButtonMashing -- CPlayerFightProgressionForButtonMashing --
//
// ===========================================================================
// ===========================================================================
// ===========================================================================


// How fast we are allowed to change the fight progression value to match it
// with the target (>= 0.0f) (in units / second).
const float CPlayerFightProgressionForButtonMashing::s_FightProgressionChangeSpeed = 3.0f;


CPlayerFightProgressionForButtonMashing::CPlayerFightProgressionForButtonMashing() :
	m_fightProgress(0.0f)
	, m_fightProgressInv(1.0f)
	, m_targetProgress(-1.0f)
{
	ResetFightProgress();
}


void CPlayerFightProgressionForButtonMashing::ResetFightProgress()
{
	// These settings were copied over from the original System-X boss files.
	const float initialProgress = GetGameConstCVar(g_SystemX_buttonMashing_initial);
	m_fightProgress = initialProgress;
	m_fightProgressInv = initialProgress;
	m_targetProgress = -1.0f;
}


void CPlayerFightProgressionForButtonMashing::SetFightProgress( const float targetProgress ) 
{ 	
	m_targetProgress = clamp_tpl(1.0f - targetProgress, 0.0f, 1.0f); 
}


void CPlayerFightProgressionForButtonMashing::UpdateAnimationParams( const float frameTime, float* currentFightProgress, float* currentFightProgressInv )
{
	assert(currentFightProgress != NULL);
	assert(currentFightProgressInv != NULL);

	*currentFightProgress = m_fightProgress;
	*currentFightProgressInv = m_fightProgressInv;

	IF_LIKELY (m_targetProgress >= 0.0f)
	{
		float newFightProgress = m_fightProgress;
		float newFightProgressInv = m_fightProgressInv;

		Interpolate( newFightProgress, m_targetProgress, s_FightProgressionChangeSpeed, frameTime, 0.001f );
		Interpolate( newFightProgressInv, (1.0f - m_targetProgress), s_FightProgressionChangeSpeed, frameTime, 0.001f );

		m_fightProgress = clamp_tpl( newFightProgress, 0.0f, 1.0f );
		m_fightProgressInv = clamp_tpl( newFightProgressInv, 0.0f, 1.0f );
	}
}
