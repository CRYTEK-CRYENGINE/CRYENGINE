// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements the Player fly state

-------------------------------------------------------------------------
History:
- 27.10.10: Created by Stephen M. North

*************************************************************************/
#include "StdAfx.h"

#include "PlayerStateFly.h"
#include "Player.h"
#include "PlayerStateUtil.h"
#include "PlayerInput.h"

#include "UI/UIManager.h"
#include "UI/Utils/ScreenLayoutManager.h"
#include "Graphics/2DRenderUtils.h"

CPlayerStateFly::CPlayerStateFly()
	: m_flyMode(0)
	, m_flyModeDisplayTime(0.0f)
{
}

void CPlayerStateFly::OnEnter( CPlayer& player )
{
	pe_player_dynamics simPar;

	IPhysicalEntity* piPhysics = player.GetEntity()->GetPhysics();
	if (!piPhysics || piPhysics->GetParams(&simPar) == 0)
	{
		return;
	}

	player.m_actorPhysics.velocity = player.m_actorPhysics.velocityUnconstrained.Set(0,0,0);
	player.m_actorPhysics.speed = player.m_stats.speedFlat = 0.0f;
	player.m_actorPhysics.groundMaterialIdx = -1;
	player.m_actorPhysics.gravity = simPar.gravity;

	player.m_stats.fallSpeed = 0.0f;
	player.m_stats.inFiring = 0;

	CPlayerStateUtil::PhySetFly( player );
}

bool CPlayerStateFly::OnPrePhysicsUpdate( CPlayer& player, const SActorFrameMovementParams &movement, float frameTime )
{
	if( player.m_stats.spectatorInfo.mode == 0 )
	{
#ifndef _RELEASE
		const uint8 flyMode = player.GetFlyMode();

		if( m_flyMode != flyMode )
		{
			if( flyMode==0 )
				return false;

			m_flyModeDisplayTime = 2.f;
			m_flyMode = flyMode;
		}
		else
		{
			if( m_flyModeDisplayTime > 0.f )
			{
				char buffer[20];

				switch( flyMode )
				{
				case 1:
					cry_sprintf( buffer,"FlyMode ON" );
					break;
				case 2:
					cry_sprintf( buffer,"FlyMode/NoClip ON" );
					break;
				}
				IRenderAuxText::Draw2dLabel( 20.f, 20.f, 1.5f, Col_White, 0, "%s", buffer );
				m_flyModeDisplayTime-=gEnv->pTimer->GetFrameTime();
			}
		}
#else
		return false;
#endif
	}

	ProcessFlyMode( player, movement );

	return true;
}

void CPlayerStateFly::OnExit( CPlayer& player )
{
	player.CreateScriptEvent("printhud",0,"FlyMode/NoClip OFF");

	pe_player_dynamics simPar;

	IPhysicalEntity* piPhysics = player.GetEntity()->GetPhysics();
	if (!piPhysics || piPhysics->GetParams(&simPar) == 0)
	{
		return;
	}

	CPlayerStateUtil::PhySetNoFly( player, simPar.gravity );
}

void CPlayerStateFly::ProcessFlyMode( CPlayer& player, const SActorFrameMovementParams &movement )
{
	const Quat viewQuat = player.GetViewQuat();
	Vec3 move = viewQuat * movement.desiredVelocity;

	float zMove(0.0f);
	{
		const IPlayerInput* piPlayerInput = player.GetPlayerInput();
		if( piPlayerInput->GetType() == IPlayerInput::PLAYER_INPUT )
		{
			if (player.m_jumpButtonIsPressed)
				zMove += 1.0f;
			if (static_cast<const CPlayerInput*> (piPlayerInput)->IsCrouchButtonDown())
				zMove -= 1.0f;
		}
	}


	move += viewQuat.GetColumn2() * zMove;

	//cap the movement vector to max 1
	float moveModule(move.len());

	if (moveModule > 1.0f)
		move /= moveModule;

	move *= player.GetTotalSpeedMultiplier() * player.GetWeaponMovementFactor();
	move *= 30.0f;

	if (player.m_actions & ACTION_SPRINT)
		move *= 10.0f;

	CPlayerStateUtil::PhySetFly( player );

	player.GetMoveRequest().type = eCMT_Fly;
	player.GetMoveRequest().velocity = move;
}


////////////////////////////////////////////////////////
// Player State Spectate

#define INVERSE_SPECTATE_FADE_OVER_TIME 1.f / 1.5f

CPlayerStateSpectate::CPlayerStateSpectate()
	: m_fFadeOutAmount(0.0f)
	,	m_fFadeForTime(0.0f)
{
}

void CPlayerStateSpectate::OnEnter( CPlayer& player )
{
	if(player.IsClient())
	{
		m_fFadeOutAmount = 1.0f;
		m_fFadeForTime = STAY_FADED_TIME;
	}

	CHUDEventDispatcher::AddHUDEventListener(this, "OnPostHUDDraw");

	inherited::OnEnter( player );
}

void CPlayerStateSpectate::UpdateFade( float frameTime )
{
	if(m_fFadeForTime > 0.f)
	{
		m_fFadeForTime -= gEnv->pTimer->GetFrameTime(ITimer::ETIMER_UI);
	}
	else
	{
		m_fFadeOutAmount = max(m_fFadeOutAmount - frameTime * INVERSE_SPECTATE_FADE_OVER_TIME, 0.0f);
	}
}


void CPlayerStateSpectate::OnHUDEvent(const SHUDEvent& event)
{
	switch(event.eventType)
	{
	case eHUDEvent_OnPostHUDDraw:
		{
			if(m_fFadeOutAmount > 0.0f)
				DrawSpectatorFade();
			break;
		}
	}
}

void CPlayerStateSpectate::DrawSpectatorFade()
{
	// If we're in a host migration we don't get ticked since the game is paused, still need to update the 
	// fade though otherwise players end up on a black screen for several seconds
	if (g_pGame->GetHostMigrationState() != CGame::eHMS_NotMigrating)
	{
		m_fFadeOutAmount = max(m_fFadeOutAmount - gEnv->pTimer->GetFrameTime(ITimer::ETIMER_UI), 0.f);
	}

	const float fCurrentFadeOut = m_fFadeOutAmount;
	C2DRenderUtils* pRenderUtils = g_pGame->GetUI()->Get2DRenderUtils();
	ScreenLayoutManager* pLayoutManager = g_pGame->GetUI()->GetLayoutManager();

	ScreenLayoutStates prevLayoutState = pLayoutManager->GetState(); 
	pLayoutManager->SetState(eSLO_DoNotAdaptToSafeArea|eSLO_ScaleMethod_None);

	ColorF color(0.f, 0.f, 0.f, fCurrentFadeOut);
	float width  = pLayoutManager->GetVirtualWidth();
	float height = pLayoutManager->GetVirtualHeight();

	//gEnv->pRenderer->SetState(GS_NODEPTHTEST);
	pRenderUtils->DrawQuad(0, 0, width, height, color);

	pLayoutManager->SetState(prevLayoutState);
}

void CPlayerStateSpectate::OnExit( CPlayer& player )
{
	if(player.IsClient())
	{
		m_fFadeOutAmount = 0.0f;
	}

	CHUDEventDispatcher::RemoveHUDEventListener(this);

	inherited::OnExit( player );
}
