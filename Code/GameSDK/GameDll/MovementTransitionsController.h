// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: 
Dynamic state of the transitions system
-------------------------------------------------------------------------
History:
- 5:16:2010	15:29 : Created by Sven Van Soom
*************************************************************************/
#pragma once
#ifndef __MOVEMENT_TRANSITIONS_CONTROLLER_H
#define __MOVEMENT_TRANSITIONS_CONTROLLER_H

#include <CrySystem/TimeValue.h>
#include "MovementTransitionsUtil.h"

// TODO: Implement proper state machine, while only allocating the state data when absolutely necessary:
enum EMovementTransitionState
{
	eMTS_None = 0,

	// This transition matches, although the distance is still far off
	eMTS_WaitingForRange,

	// This seems to be the transition we will do next, we start preparing our travel direction etc. IF current state doesn't match anymore, stop transition
	eMTS_Preparing,

	// The transition now matches perfectly, so we will request it from the animation system. Other running states might delay it though.
	// [TODO] IF Request took too long: Cancel request, stop transition
	eMTS_Requesting_Succeeded,

	// 'Delayed' states mean we are perfectly within range to make a request, but it failed or some other conditions weren't matched perfectly.
	eMTS_Requesting_DelayedBecauseControllerRequestFailed,
	eMTS_Requesting_DelayedBecauseAngleOutOfRange,
	eMTS_Requesting_DelayedBecauseWalkabilityFail,

	// [TODO] Playing State
	// Transition is running. The animation is playing. (at the moment during this state update isn't called anymore, as it isn't really necessary - yet)
	// IF animation is done or interrupted, stop transition.

	eMTS_COUNT,
};


///////////////////////////////////////////////////////////////////////////////
// CMovementTransitionsController, the dynamic part of the movement transition system
class CMovementTransitions;
struct STransition;
class CMovementTransitionsController
{
public:
	// public methods
	CMovementTransitionsController(CPlayer* pPlayer);
	~CMovementTransitionsController();

	ILINE bool IsEnabled() const { return false; }

	ILINE bool IsTransitionRequestedOrPlaying() const
	{ 
		return 
			   (m_pAnimAction && ((m_pAnimAction->GetStatus()==IAction::Pending) || (m_pAnimAction->GetStatus()==IAction::Installed)))
			|| (m_pPendingAnimAction && ((m_pPendingAnimAction->GetStatus()==IAction::Pending) || (m_pPendingAnimAction->GetStatus()==IAction::Installed)));
	}

	void Reset();
	void Update(
		const Vec3& playerPos,
		const float newPseudoSpeed,
		const Vec3& currentBodyDirection,
		const Vec3& newMoveDirection,
		const bool bHasLockedBodyTarget,
		const SExactPositioningTarget*const pExactPositioningTarget,

		CMovementRequest*const pRequest,
		SActorFrameMovementParams*const pMoveParams,
		float*const pJukeTurnRateFraction,
		Vec3*const pBodyTarget,
		const char**const pBodyTargetType);

	void UpdatePathFollowerState(); // disables/enables cutting corners in the pathfollower
		
	bool HandleEvent( const SGameObjectEvent& event ); // returns true when the event was handled

	bool RequestTransition(const char* szFragmentID, const STransition*const pTransition, const STransitionFuture& future); // returns false when the fragmentID was invalid (or any other reason why we couldn't start the IAction)
	void CancelTransition();

	EStance GetUpcomingStance() const; // when a movementtransition is playing, return the stance we expect to be in at the end of it (useful for selection of next actions). STANCE_NULL when there is none or unknown.

private:
	// private methods

	void UpdateSafeLine(const CMovementRequest& request, const Vec3& playerPos, const Vec3& moveDirection);
	CTimeValue UpdateRunningDuration( const float oldPseudoSpeed);

	IActionController* GetActionController() const
	{
		IAnimatedCharacter* pAnimChar = m_pPlayer->GetAnimatedCharacter();
		CRY_ASSERT(pAnimChar);

		if (pAnimChar->GetActionController())
			return pAnimChar->GetActionController();
		else
			return NULL;
	}

	// private fields
	CPlayer*								m_pPlayer;

	CMovementTransitions*		m_pMovementTransitions;		// points to the static part of the transitions system; is NULL when not enabled
	Lineseg									m_safeLine;
	uint8										m_allowedTransitionFlags;

	float										m_prevEntitySpeed2D; // used to calculate average (-1 when not initialized)

	CTimeValue							m_runStartTime;	// when we started to run *physically*; or 0 when we are not (not to be confused with m_transitionStartTime)

	SMovementTransitionsSample m_oldMovementSample, m_newMovementSample;

	EMovementTransitionState m_state;

	_smart_ptr<IAction>			m_pAnimAction;
	_smart_ptr<IAction>			m_pPendingAnimAction;
};


///////////////////////////////////////////////////////////////////////////////
// CCornerSmoother, helper class that smooths out sudden changes in movetarget.
//
// * Before smoothing it predicts the curve that will be taken and does a
// 'CheckWalkability' to make sure it can be done safely.
// * As prediction, and especially CheckWalkability(), is relatively heavy,
//   the total # of predictions per frame is limited by MAX_PREDICTIONS_PER_FRAME
//
class CCornerSmoother
{
public:
	ILINE CCornerSmoother(CPlayer* pPlayer) :
		m_pPlayer(pPlayer)
	{
		Reset();
	}

	ILINE bool IsRunning() const
	{
		return (m_simplePrediction.numSamples > 0);
	}

	ILINE void Cancel()
	{
		m_simplePrediction.Reset();
		m_prediction.Reset();
		m_oldMoveTarget.zero();
	}

	void Reset()
	{
		m_dirSmoothTime = 0.0001f;
		m_prediction.Reset();
		m_simplePrediction.Reset();
		m_travelSpeedRate = 0;
		m_oldMoveTarget.zero();

		const SActorParams &actorParams = m_pPlayer->GetActorParams();
		m_maxTurnSpeed = actorParams.maxDeltaAngleRateNormal;
		CRY_ASSERT(m_maxTurnSpeed > 0);
	}

	void Update(
		const Vec3& playerPos, const Vec2& oldMovementDir, const float oldMovementSpeed, const Vec3& animBodyDirection,
		const float desiredSpeed, const Vec3& desiredMovement, const float distToPathEnd,
		const Vec3& moveTarget,
		const bool hasLockedBodyTarget,
		const float maxTurnSpeed, // rad/s
		const float frameTime,
		float*const pNewDesiredSpeed, Vec3*const pNewDesiredMovement);

	void DebugRender(float z);

	void SmoothLocalVDir(Vec3*const pLocalVDir, const float frameTime) const
	{
		float lambda = logf(2.0f) / m_dirSmoothTime;
		*pLocalVDir = Vec3::CreateSlerp(*pLocalVDir, Vec3(0,1,0), expf(-lambda * frameTime));
	}

	float GetMaxTurnSpeed() const { return m_maxTurnSpeed; }

private:
	static const int MAX_SAMPLES = 100;
	static const int MAX_PREDICTIONS_PER_FRAME = 6;

	struct SCurveSettings
	{
		float wrongWaySpeed;   // speed to slow down to when going in the opposite direction

		float speedSmoothTime; // parameter to SmoothCD for speed (aka the 'lag time', the lower the number, the 'stiffer' the damped spring)

		float walkSmoothTime;  // parameter to SmoothExp for smoothing the orientation (aka the half-time for reaching the target direction), used when walking
		float walkSpeed;       // speed at which we use the walkSmoothTime

		float runSmoothTime;   // parameter to SmoothExp for smoothing the orientation (aka half-time for reaching the target direction), used when running
		float runSpeed;        // speed at which we use the runSmoothTime
	};

	struct SPredictionSettings
	{
		const SCurveSettings* pCurveSettings;
		Vec3 playerPos;
		float oldMovementSpeed;
		Vec2 dir2D;
		float desiredSpeed;
		Vec3 moveTarget;
		float maxTurnSpeed;
		float travelSpeedRate;
	};

	struct SPrediction
	{
		SPrediction() : numSamples(0) {}
		void Reset() { numSamples = 0; }

		int numSamples;
		Vec2 samples[MAX_SAMPLES];
	};

private:
	void PredictAndVerifySmoothTurn(const SPredictionSettings& settings);
	void UpdatePathFollowerState() const;

private:
	static void PredictSmoothTurn(const SPredictionSettings& settings, SPrediction*const pPrediction);
	static void SimplifyPrediction(const Vec3& playerPos, const SPrediction& input, SPrediction*const pOutput);
	static bool CheckWalkability(const SPrediction& prediction, CPlayer* pPlayer);

	static float CalculateDirSmoothTime(const SCurveSettings& settings, const float desiredSpeed, const float distToMoveTarget2D, const float endOfPathDistance);

	static bool EnoughTimePassedSinceLastPrediction(const int currentFrameID)
	{
		if (( m_lastPredictionFrameID == 0) || (m_lastPredictionFrameID != currentFrameID))
			return true;
		else
		{
			return (m_numPredictionsThisFrame < MAX_PREDICTIONS_PER_FRAME);
		}
	}

	static void RememberLastPredictionFrameID(const int currentFrameID)
	{
		if (currentFrameID == m_lastPredictionFrameID)
		{
			++m_numPredictionsThisFrame;
		}
		else
		{
			CRY_ASSERT(currentFrameID > m_lastPredictionFrameID);
			m_lastPredictionFrameID = currentFrameID;
			m_numPredictionsThisFrame = 1;
		}
	}

private:
	static int m_lastPredictionFrameID;
	static int m_numPredictionsThisFrame;

	static SCurveSettings normalCurveSettings;
	//static SCurveSettings sharpCurveSettings;

private:
	CPlayer* m_pPlayer;
	Vec3 m_oldMoveTarget;
	float m_dirSmoothTime;

	SPrediction m_prediction;
	SPrediction m_simplePrediction;

	float m_travelSpeedRate;
	float m_maxTurnSpeed;
};


#endif // __MOVEMENT_TRANSITIONS_CONTROLLER_H
