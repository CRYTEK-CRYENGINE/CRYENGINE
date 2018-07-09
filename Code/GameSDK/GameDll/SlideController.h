// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Controls sliding mechanic

-------------------------------------------------------------------------
History:
- 26:08:2010: Created by Benito Gangoso Rodriguez

*************************************************************************/

#pragma once

#ifndef __SLIDE_CONTROLLER_H__
#define __SLIDE_CONTROLLER_H__

#include "Utility/CryHash.h"

class CPlayer;
struct SCharacterMoveRequest;
struct ICharacterInstance;
struct CryCharAnimationParams;
struct SActorFrameMovementParams;

enum ESlideState
{
	eSlideState_None = 0,
	eSlideState_Sliding,
	eSlideState_ExitingSlide
};

class CSlideController
{
public:
	CSlideController();

	void Update( CPlayer& player, float frameTime, const SActorFrameMovementParams& frameMovementParams, bool isNetSliding, bool isNetExitingSlide, SCharacterMoveRequest& movementRequest);

	bool CanDoSlideKick(const CPlayer& player) const;
	float DoSlideKick(CPlayer& player);
	void GoToState(CPlayer& player, ESlideState newState);
	void LazyExitSlide(CPlayer& player);

	ILINE bool IsActive() const { return (m_state != eSlideState_None); } 
	ILINE bool IsSliding() const { return (m_state == eSlideState_Sliding); }
	ILINE bool IsExitingSlide() const { return (m_state == eSlideState_ExitingSlide); }
	ILINE ESlideState GetCurrentState() const { return m_state; }

	void ExitedSlide(CPlayer& player);

private:
	
	void UpdateSlidingState(CPlayer& player, float frameTime, bool continueSliding);
	bool UpdateMovementRequest(CPlayer& player, float frameTime, const SActorFrameMovementParams& frameMovementParams, bool isNetSliding, bool isNetExitingSlide, SCharacterMoveRequest& movementRequest);
	void OnSlideStateChanged( CPlayer& player );

	static Vec3  GetTargetDirection(CPlayer& player);

	Vec3 m_lastPosition;
	ESlideState	m_state;

	float		m_kickTimer;
	float		m_lazyExitTimer;

	class CPlayerSlideAction *m_slideAction;
};

extern const float SLIDE_EXIT_TRANSITION_TIME;


#endif
