// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ICameraMode.h"

#include "Player.h"


ICameraMode::ICameraMode() 
	: m_isBlendingOff(false)
	, m_disableDrawNearest(false)
{

}

ICameraMode::~ICameraMode()
{

}

void ICameraMode::ActivateMode( const CPlayer & clientPlayer )
{
	m_isBlendingOff = false;

	if(m_disableDrawNearest)
	{
		SetDrawNearestFlag(clientPlayer,false);
	}

	Activate(clientPlayer);
}

void ICameraMode::DeactivateMode( const CPlayer & clientPlayer )
{
	m_isBlendingOff = true;

	if(m_disableDrawNearest)
	{
		SetDrawNearestFlag(clientPlayer,true);
	}

	Deactivate(clientPlayer);
}

bool ICameraMode::CanTransition()
{
	return false;
}

void ICameraMode::SetCameraAnimationFactor( const AnimationSettings& animationSettings )
{

}

void ICameraMode::GetCameraAnimationFactor( float &pos, float &rot )
{
	pos = 0.0f; rot = 0.0f;
}

void ICameraMode::Activate( const CPlayer & clientPlayer )
{

}

void ICameraMode::Deactivate( const CPlayer & clientPlayer )
{

}

void ICameraMode::SetDrawNearestFlag( const CPlayer & clientPlayer, bool bSetDrawNearestFlag )
{
	if(!clientPlayer.IsThirdPerson())
	{
		IEntity* pPlayerEntity = clientPlayer.GetEntity();
		if(pPlayerEntity)
		{
			uint32 entitySlotFlags = pPlayerEntity->GetSlotFlags(eIGS_FirstPerson);
			if(bSetDrawNearestFlag)
			{
				entitySlotFlags |= ENTITY_SLOT_RENDER_NEAREST;
			}
			else
			{
				entitySlotFlags &= ~ENTITY_SLOT_RENDER_NEAREST;
			}
			pPlayerEntity->SetSlotFlags(eIGS_FirstPerson,entitySlotFlags);
		}
	}
}