// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Controls player when playing an interactive animation

-------------------------------------------------------------------------
History:
- 19:12:2009   Created by Benito Gangoso Rodriguez

*************************************************************************/

#pragma once

#ifndef _INTERACTIVE_ACTION_CONTROLLER_H_
#define _INTERACTIVE_ACTION_CONTROLLER_H_

class CPlayer;
struct SActorFrameMovementParams;
struct SCharacterMoveRequest;

class CInteractiveActionController
{
	friend class CActionInteractive;

	enum EState
	{
		eState_None = 0,
		eState_PositioningUser,
		eState_PlayingAnimation,
		eState_InstallPending,
		eState_Done
	};

	struct SCameraControl
	{
		SCameraControl()
		{
			Reset();
		}

		void Reset()
		{
			limitVerticalRads = 0.0f;
			limitHorizontalRads = 0.0f;
			addCameraRotation.Set(0.0f, 0.0f, 0.0f);
		}

		float limitVerticalRads;
		float limitHorizontalRads;
		Ang3  addCameraRotation;
	};

public:
	CInteractiveActionController()
		: m_interactiveObjectId(0)
		, m_state(eState_None)
		, m_targetLocation(IDENTITY)
		, m_runningTime(0.0f)
		, m_interactionName("")
		, m_pAction(NULL)
	{
	}
	~CInteractiveActionController();

	ILINE bool HasFinished() const { return m_state == eState_Done; }
	ILINE bool IsInInteractiveAction() const { return (m_state != eState_None) && !HasFinished(); }

	void OnEnter(CPlayer& player, EntityId interactiveObjectId, int interactionIndex = 0, float actionSpeed = 1.0f);
	void OnEnterByName(CPlayer& player, const char* interaction, float actionSpeed = 1.0f);
	void OnLeave(CPlayer& player);
	void Update(CPlayer& player, float frameTime, const SActorFrameMovementParams& movement);
	EntityId GetInteractiveObjectId() const { return m_interactiveObjectId; }
	void Abort(CPlayer& player);

	void FullSerialize(CPlayer& player, TSerialize &ser);
	ILINE void MannequinActionInstalled()
	{
		m_state = eState_PositioningUser;
	}
	
private:
	void NotifyFinished(CPlayer& player);
	void CalculateStartTargetLocation(CPlayer& player, int interactionIndex);
	void SafelyReleaseAction();

	CryFixedStringT<32> m_interactionName;
	QuatT				m_targetLocation;
	EntityId		m_interactiveObjectId;
	float				m_runningTime;
	EState			m_state;
	class CActionInteractive* m_pAction;
};

#endif //_INTERACTIVE_ACTION_CONTROLLER_H_