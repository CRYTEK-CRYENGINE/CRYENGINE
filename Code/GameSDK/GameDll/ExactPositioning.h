// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//-------------------------------------------------------------------------
//
// Description: 
//  Component of MovementController that handles ExactPositioning, using an AnimatedCharacter
//
//  Only a subset of the original ExactPositioning is supported.
//
////////////////////////////////////////////////////////////////////////////
#ifndef __MANNEQUINEXACTPOSITIONING_H__
#define __MANNEQUINEXACTPOSITIONING_H__
#pragma once

#include "ExactPositioningTrigger.h"
#include "IMovementController.h" // for the exactpositioning structures

// ============================================================================
// ============================================================================

class CExactPositioning
{
public:
	CExactPositioning( IAnimatedCharacter* pAnimatedCharacter );
	~CExactPositioning();

	bool SetActorTarget( const SActorTargetParams& actorTargetParams );
	void ClearActorTarget();

	void SetExactPositioningListener( IExactPositioningListener* pExactPositioningListener );

	const SExactPositioningTarget* GetExactPositioningTarget() const;
	void Update(); 

private:
	enum ETriggerState
	{
		eTS_Disabled,         // No pending request.
		eTS_Considering,      // Received and committed request. AnimTarget is set up. We are far from the target, not really doing anything yet.
		eTS_Waiting,          // We are within the END_CONSIDERING_DISTANCE (15m).
		eTS_Preparing,        // We are within the prepareRadius, tell the game code to force the character to the right point/orientation.
		eTS_FinalPreparation, // ExactPositioningTrigger triggered, so we are very close. The animation action is now pending.
		eTS_Running,          // Animation Action has entered, so the animation is running. [in old code, it waited for the state to be properly entered, so after the transition animations]
		eTS_Completing        // For signal/non-looping actions: We finished the action.  For looping actions: immediately after entering the action. So the looping action continues but ExactPos is disabled already.
	};

	enum EStateMachineEventType
	{
		ESME_Update,
		ESME_Exit,
		ESME_Enter,
	};

	struct SStateMachineEvent
	{
		EStateMachineEventType type;
	};

	enum EStateMachineSendEventMethod
	{
		ESMSEM_RepeatSendingUntilStateDoesntChange,
		ESMSEM_DontAllowStateChanges,
	};

	typedef ETriggerState ( CExactPositioning::*StateEventHandler )( SStateMachineEvent& event );

private:

	// Simple state machine
	void StateMachine_ChangeStateTo( ETriggerState newState ); // this should not be used, use events!  Returns true when the state changed.
	void StateMachine_SendEvent( SStateMachineEvent& event );
	void StateMachine_SendEventUntilStateDoesntChange( SStateMachineEvent& event );
	void StateMachine_Initialize( ETriggerState initialState );

	ETriggerState StateDisabled_HandleEvent( SStateMachineEvent& event );
	ETriggerState StateConsidering_HandleEvent( SStateMachineEvent& event );
	ETriggerState StateWaiting_HandleEvent( SStateMachineEvent& event );
	ETriggerState StatePreparing_HandleEvent( SStateMachineEvent& event );
	ETriggerState StateFinalPreparation_HandleEvent( SStateMachineEvent& event );
	ETriggerState StateRunning_HandleEvent( SStateMachineEvent& event );
	ETriggerState StateCompleting_HandleEvent( SStateMachineEvent& event );

	ETriggerState HandlePendingRequest(); // process incoming trigger request, after this call the request is cleared
	void CommitPendingRequest();
	void SendFailureEvents();
	void ClearExactPositioningTarget();
	void ClearAnimAction();
	void UpdateAnimationTrigger();
	void UpdateTargetPointToFinishPoint(); // figure out where the 'expected endpoint' will be
	void SendQueryComplete( TExactPositioningQueryID queryID, bool success );

private:
	static TExactPositioningQueryID GenerateQueryID() { ++s_lastQueryID; CRY_ASSERT(s_lastQueryID); return s_lastQueryID; }

private:
	IAnimatedCharacter* m_pAnimatedCharacter;

	IExactPositioningListener* m_pExactPositioningListener;

	std::unique_ptr<SActorTargetParams> m_pPendingRequest; // pending requests will be queued by SetActorTarget, and will be consumed in CommitPendingRequest (in Enter of Considering state)

	// Values that only make sense after the request is committed
	std::unique_ptr<SExactPositioningTarget> m_pExactPositioningTarget;
	SActorTargetParams m_actorTargetParams;
	TExactPositioningQueryID m_triggerQueryStart;
	TExactPositioningQueryID m_triggerQueryEnd;
	CExactPositioningTrigger m_exactPositioningTrigger;

	// Value that is only filled when action is started
	_smart_ptr<IAction> m_pAnimAction;

	// Simple state machine
	static StateEventHandler s_stateEventHandlers[];
	static SStateMachineEvent s_enterEvent;
	static SStateMachineEvent s_exitEvent;
	static SStateMachineEvent s_updateEvent;
	bool m_changingState;
	bool m_initializingState;

	ETriggerState m_state;
	StateEventHandler m_stateEventHandler;

private:
	static TExactPositioningQueryID s_lastQueryID;
};

#endif // __MANNEQUINEXACTPOSITIONING_H__
