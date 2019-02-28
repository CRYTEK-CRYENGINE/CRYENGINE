// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "IAnimatedCharacter.h"
#include "PersistantDebug.h" // to show target point
#include "ExactPositioning.h"


// ============================================================================
// ============================================================================

// * Check whether everyone that calls SetTarget also calls ClearTarget
//   when going away prematurely, to prevent the pQueryXXX pointers to be used
//   in the next update!

// ============================================================================
// ============================================================================

static const float END_CONSIDERING_DISTANCE = 15.0f; // distance within which we end the Considering state

// ============================================================================
// ============================================================================

CExactPositioning::StateEventHandler CExactPositioning::s_stateEventHandlers[] = 
{
	&CExactPositioning::StateDisabled_HandleEvent,
	&CExactPositioning::StateConsidering_HandleEvent,
	&CExactPositioning::StateWaiting_HandleEvent,
	&CExactPositioning::StatePreparing_HandleEvent,
	&CExactPositioning::StateFinalPreparation_HandleEvent,
	&CExactPositioning::StateRunning_HandleEvent,
	&CExactPositioning::StateCompleting_HandleEvent
};

CExactPositioning::SStateMachineEvent CExactPositioning::s_enterEvent = { CExactPositioning::ESME_Enter };
CExactPositioning::SStateMachineEvent CExactPositioning::s_exitEvent = { CExactPositioning::ESME_Exit };
CExactPositioning::SStateMachineEvent CExactPositioning::s_updateEvent = { CExactPositioning::ESME_Update };

// ============================================================================
// ============================================================================

TExactPositioningQueryID CExactPositioning::s_lastQueryID = TExactPositioningQueryID(0);

// ============================================================================
// ============================================================================


// ----------------------------------------------------------------------------
CExactPositioning::CExactPositioning( IAnimatedCharacter* pAnimatedCharacter )
	:
	m_pAnimatedCharacter( pAnimatedCharacter ),
	m_pExactPositioningListener( nullptr ),
	m_pPendingRequest( nullptr ),
	m_triggerQueryStart( 0 ),
	m_triggerQueryEnd( 0 ),
	m_changingState( false ),
	m_initializingState( false)
{
	CRY_ASSERT( m_pAnimatedCharacter );

	StateMachine_Initialize( eTS_Disabled );
}

// ----------------------------------------------------------------------------
CExactPositioning::~CExactPositioning()
{
	ClearAnimAction();
}


// ============================================================================
// ============================================================================

// ----------------------------------------------------------------------------
bool CExactPositioning::SetActorTarget( const SActorTargetParams& actorTargetParams )
{
	// TODO: what if we get multiple SetTarget calls in the same frame?

	CRY_ASSERT( actorTargetParams.location.GetLength() > 0.1f );
	CRY_ASSERT( actorTargetParams.direction.GetLength() > 0.5f );
	CRY_ASSERT( fabs( actorTargetParams.startArcAngle ) <= gf_PI );
	CRY_ASSERT( actorTargetParams.directionTolerance >= 0.0f );
	CRY_ASSERT( actorTargetParams.prepareRadius > 1.0f );
	CRY_ASSERT( actorTargetParams.startWidth >= 0.0f );
	CRY_ASSERT( actorTargetParams.pQueryStart != NULL );
	CRY_ASSERT( actorTargetParams.pQueryEnd != NULL );

	CRY_ASSERT( actorTargetParams.pAction != NULL );
	if ( !actorTargetParams.pAction )
		return false;
	
	if ( actorTargetParams.pAction->GetStatus() != IAction::None )
		return false;

	m_pPendingRequest.reset( new SActorTargetParams( actorTargetParams ) );

	return true;
}

// ----------------------------------------------------------------------------
void CExactPositioning::ClearActorTarget()
{
	if ( m_state < eTS_Running ) 
		StateMachine_ChangeStateTo( eTS_Disabled ); // NOTE: bad practice as update won't run until next frame?

	m_pPendingRequest.reset();
}

// ----------------------------------------------------------------------------
const SExactPositioningTarget* CExactPositioning::GetExactPositioningTarget() const
{
	return m_pExactPositioningTarget.get();
}

// ----------------------------------------------------------------------------
void CExactPositioning::Update()
{
	// Sanity checks so we don't need to check this again in the Update event handlers

	if ( !m_pAnimatedCharacter )
		return;

	const IEntity* pEntity = m_pAnimatedCharacter->GetEntity();
	if ( !pEntity )
		return;

	static SStateMachineEvent updateEvent = { ESME_Update };
	StateMachine_SendEventUntilStateDoesntChange( updateEvent );
}


// ============================================================================
// ============================================================================

// ----------------------------------------------------------------------------
void CExactPositioning::StateMachine_Initialize( ETriggerState newState )
{
	if (m_changingState || m_initializingState)
	{
		CRY_ASSERT(false); // recursion not allowed!
		return;
	}

	m_initializingState = true;

	do
	{
		m_state = newState;
		
		CRY_ASSERT(m_state <= eTS_Completing);
		m_stateEventHandler = s_stateEventHandlers[m_state];

		newState = ( this->*m_stateEventHandler )( s_enterEvent );
	}
	while ( newState != m_state );

	m_initializingState = false;
}

// ----------------------------------------------------------------------------
void CExactPositioning::StateMachine_ChangeStateTo( ETriggerState newState )
{
	if (m_changingState || m_initializingState)
	{
		CRY_ASSERT(false); // recursion not allowed!
		return;
	}

	if ( newState == m_state )
		return;

	m_changingState = true;

	StateMachine_SendEvent( s_exitEvent );

#ifdef _DEBUG
	std::vector<ETriggerState> debugOldStates;
#endif

	do
	{
#ifdef _DEBUG
		debugOldStates.push_back(m_state);
#endif

		m_state = newState;

		CRY_ASSERT(m_state <= eTS_Completing);
		m_stateEventHandler = s_stateEventHandlers[m_state];

		newState = ( this->*m_stateEventHandler )( s_enterEvent );
	}
	while ( newState != m_state );

	m_changingState = false;
}

// ----------------------------------------------------------------------------
void CExactPositioning::CommitPendingRequest()
{
	m_triggerQueryStart = *m_pPendingRequest->pQueryStart = CExactPositioning::GenerateQueryID();
	m_triggerQueryEnd = *m_pPendingRequest->pQueryEnd = CExactPositioning::GenerateQueryID();

	// Construct SExactPositioningTarget (which is interpreted by outside world to track state and guide entity to target)

	SExactPositioningTarget* pTgt = new SExactPositioningTarget();

	// Roughly mimicking CExactPositioning::CalculateTarget in the Considering state
	// (though 'triggermovement' is not taken into account)

	pTgt->location = QuatT(m_pPendingRequest->location, Quat::CreateRotationV0V1(FORWARD_DIRECTION, m_pPendingRequest->direction));
	pTgt->startWidth = m_pPendingRequest->startWidth;
	pTgt->positionWidth = m_pPendingRequest->prepareRadius + m_pPendingRequest->startWidth;
	pTgt->positionDepth = m_pPendingRequest->prepareRadius;
	pTgt->isNavigationalSO = m_pPendingRequest->navSO;
	pTgt->orientationTolerance = m_pPendingRequest->directionTolerance;
	pTgt->pAction = m_pPendingRequest->pAction;

	m_pExactPositioningTarget.reset( pTgt );

	// The original exact positioning code would callback into the PointVerifier (typically the AI)
	// asking for adjustment to fit the animation
	//
	// CTargetPointRequest targetPointRequest = CTargetPointRequest( pTgt->position );
	// if ( m_pPointVerifier && m_pPointVerifier->CanTargetPointBeReached( targetPointRequest ) == eTS_false )
	// {
	// 	// Error???
	// }

	// Set a trigger, taking into account the transition movement, if any (pathfind is running in the background every 0.3s)

	m_exactPositioningTrigger = CExactPositioningTrigger(
		pTgt->location.t,
		pTgt->startWidth,
		Vec3( pTgt->positionWidth, pTgt->positionDepth, 20.0f ),
		pTgt->location.q,
		pTgt->orientationTolerance, 
		0.0f  // this used to be the triggermovement translation length (inside AnimationTrigger this 0.0f is currently clamped to 2.0f)
		);

	if ( pTgt->isNavigationalSO )
	{
		m_exactPositioningTrigger.SetDistanceErrorFactor( 0.5f );
	}

	// The original exact positioning code would callback into the PointVerifier (typically the AI)
	// committing the same targetPointRequest which was verified before
	//
	// if (m_pAGState->m_pPointVerifier)
	//	 m_pAGState->m_pPointVerifier->UseTargetPointRequest( targetPointRequest );

	m_actorTargetParams = *m_pPendingRequest;

	m_pPendingRequest.reset();
}

// ----------------------------------------------------------------------------
void CExactPositioning::SendQueryComplete( TExactPositioningQueryID queryID, bool success )
{
	if (!m_pExactPositioningListener)
		return;

	m_pExactPositioningListener->ExactPositioningQueryComplete( queryID, success );
}

// ----------------------------------------------------------------------------
CExactPositioning::ETriggerState CExactPositioning::StateDisabled_HandleEvent( SStateMachineEvent& event )
{
	switch( event.type )
	{
	case ESME_Enter:
		SendFailureEvents();
		ClearExactPositioningTarget();
		ClearAnimAction();
		break;

	case ESME_Update:
		if ( m_pPendingRequest.get() )
		{
			return eTS_Considering;
		}

		break;
	};

	return m_state;
}

// ----------------------------------------------------------------------------
void CExactPositioning::UpdateAnimationTrigger() 
{
	QuatT curLocation = m_pAnimatedCharacter->GetAnimLocation();

	// Code that tries to adjust for the fact that somtimes physics will only process 
	// the request during this frame and we will get our results next frame.
	//
	// TODO: Investigate
	//
	//IEntityRender* pIEntityRender =  pEntity->GetProxy( ENTITY_PROXY_RENDER );
	//bool hasSplitUpdate = (pIEntityRender != NULL) && pIEntityRender->IsCharactersUpdatedBeforePhysics();
	//if ( hasSplitUpdate && gEnv->pPhysicalWorld->GetPhysVars()->bMultithreaded )
	//{
	//	curLocation.t += m_pState->GetAnimatedCharacter()->GetRequestedEntityMovement().t;
	//}

	m_exactPositioningTrigger.Update( gEnv->pTimer->GetFrameTime(), curLocation.t, curLocation.q, true );
}

// ----------------------------------------------------------------------------
CExactPositioning::ETriggerState CExactPositioning::StateConsidering_HandleEvent( SStateMachineEvent& event )
{
	switch( event.type )
	{
	case ESME_Enter:
		if ( !m_pPendingRequest.get() ) // paranoia
		{
			CRY_ASSERT(false);
			return eTS_Disabled;
		}

		CommitPendingRequest();

		break;

	case ESME_Update:
		if ( m_pPendingRequest.get() )
			return eTS_Disabled;

		if ( m_pExactPositioningTarget->pAction->GetStatus() != IAction::None )
			return eTS_Disabled;

		//	--> if (startRadiusSq <= square(END_CONSIDERING_DISTANCE) && m_ds.pathfindRetryTimer <= 0.0f)
		//	[within 15m AND (we're not waiting for a re-pathfind??)]
		Vec3 worldPos = m_pAnimatedCharacter->GetEntity()->GetWorldPos();
		float startRadiusSq = worldPos.GetSquaredDistance( m_pExactPositioningTarget->location.t );

		const bool withinConsideringDistance = ( startRadiusSq <= square( END_CONSIDERING_DISTANCE ) );
		if ( withinConsideringDistance )
			return eTS_Waiting;

		UpdateAnimationTrigger(); // Mimicking the old exact positioning here: is this really necessary? we're not even checking it...

		break;
	};

	return m_state;
}

// ----------------------------------------------------------------------------
CExactPositioning::ETriggerState CExactPositioning::StateWaiting_HandleEvent( SStateMachineEvent& event )
{
	switch( event.type )
	{
	case ESME_Enter:
		CRY_ASSERT( !m_pExactPositioningTarget->preparing );
		CRY_ASSERT( !m_pExactPositioningTarget->activated );
		break;

	case ESME_Update:
		if ( m_pPendingRequest.get() )
			return eTS_Disabled;

		if ( m_pExactPositioningTarget->pAction->GetStatus() != IAction::None )
			return eTS_Disabled;

		// The old exact positioning adjusted the target position & orientation, taking into account
		// the animation movement (see CExactPositioning::CalculateTarget)

		//	--> if (pathfindperformed && (startRadiusSq <= square(m_animationTargetRequest.prepareRadius)))
		//	[by default, 3m. Should always be >1m according to asserts]

		Vec3 worldPos = m_pAnimatedCharacter->GetEntity()->GetWorldPos();
		float startRadiusSq = worldPos.GetSquaredDistance( m_pExactPositioningTarget->location.t );

		const bool withinPrepareRadius = ( startRadiusSq <= square( m_actorTargetParams.prepareRadius ) );
		if ( withinPrepareRadius )
			return eTS_Preparing;

		break;
	};

	return m_state;
}

// ----------------------------------------------------------------------------
CExactPositioning::ETriggerState CExactPositioning::StatePreparing_HandleEvent( SStateMachineEvent& event )
{
	switch( event.type )
	{
	case ESME_Enter:
		m_pExactPositioningTarget->preparing = true;
		CRY_ASSERT( !m_pExactPositioningTarget->activated );

		// we're very close, start forcing the character to the right point

		// Adjust the positionWidth & positionDepth, replacing the prepareRadius by 0.1m in the calculation of positionWidth/Depth
		// so instead of going towards the big box of 3.1m width/depth, we now go to a small box of 0.1m width/depth?

		m_pExactPositioningTarget->positionWidth = 0.1f + m_actorTargetParams.startWidth; // NOTE: the positionWidth gets forced to 0.1f in SetTriggerState, should we do this too??
		m_pExactPositioningTarget->positionDepth = 0.1f;

		// NOTE: the box size doesn't seem consistent with the positionWidth/positionDepth above. Just mimicking ExactPositioning here. Thankfully the values above are not used anywhere. *sigh*
		m_exactPositioningTrigger.ResetRadius( Vec3( m_pExactPositioningTarget->startWidth, 0.0f, 20.0f ), m_pExactPositioningTarget->orientationTolerance ); // note: for the orientationTolerance it would go back to the original request, which I don't store anymore, possibly because the animationTarget contained 0 tolerance until now. What does it mean????!!!

		break;

	case ESME_Update:
		if ( m_pPendingRequest.get() )
			return eTS_Disabled;

		if ( m_pExactPositioningTarget->pAction->GetStatus() != IAction::None )
			return eTS_Disabled;

		UpdateAnimationTrigger(); // NOTE: because Update() was already called in the ResetRadius during Enter, it will be called twice during one frame

		//	[our first AnimationTrigger got reached, which is a box of 0.1m (sometimes +startWidth) around the adjusted trigger position]
		if ( m_exactPositioningTrigger.IsTriggered() )
			return eTS_FinalPreparation;

		break;
	};

	return m_state;
}

// ----------------------------------------------------------------------------
CExactPositioning::ETriggerState CExactPositioning::StateFinalPreparation_HandleEvent( SStateMachineEvent& event )
{
	switch( event.type )
	{
	case ESME_Enter:
		{
			CRY_ASSERT( m_pExactPositioningTarget->preparing );
			CRY_ASSERT( !m_pExactPositioningTarget->activated );

			if ( !m_pAnimatedCharacter )
				return eTS_Disabled;

			IActionController* pActionController = m_pAnimatedCharacter->GetActionController();
			if ( !pActionController )
				return eTS_Disabled;

			if ( m_pExactPositioningTarget->pAction->GetStatus() != IAction::None )
				return eTS_Disabled;

			m_pAnimAction = m_pExactPositioningTarget->pAction;
			pActionController->Queue( *m_pAnimAction );

			// We've queued the animation, let listeners know
			if ( m_triggerQueryStart )
			{
				SendQueryComplete( m_triggerQueryStart, true );
				m_triggerQueryStart = 0;
			}

			break;
		}

	case ESME_Update:
		if ( m_pPendingRequest.get() )
			return eTS_Disabled;

		CRY_ASSERT( m_pAnimAction.get() );
		switch( m_pAnimAction->GetStatus() )
		{
		case IAction::Pending:
			break;
		case IAction::Installed:
			return eTS_Running;
			break;
		case IAction::Finished: // this means 'about to finish'
		case IAction::None:
			return eTS_Completing;
			break;
		}

		break;
	};

	return m_state;
}


// ----------------------------------------------------------------------------
CExactPositioning::ETriggerState CExactPositioning::StateRunning_HandleEvent( SStateMachineEvent& event )
{
	switch( event.type )
	{
	case ESME_Enter:
		CRY_ASSERT( m_pExactPositioningTarget->preparing );
		CRY_ASSERT( !m_pExactPositioningTarget->activated );
		m_pExactPositioningTarget->preparing = false;
		m_pExactPositioningTarget->activated = true;

		UpdateTargetPointToFinishPoint();

		if ( m_actorTargetParams.completeWhenActivated )
		{
			// Disconnect m_pAnimAction
			m_pAnimAction = NULL;

			return eTS_Completing; // and go straight to the completing stage
		}

		break;

	case ESME_Update:
		//if ( m_pPendingRequest.get() )
		//	Ignore any pending request until we are completed

		CRY_ASSERT( m_pAnimAction.get() );
		switch( m_pAnimAction->GetStatus() )
		{
		case IAction::Pending:
			CRY_ASSERT( false ); // should not happen, we already entered before, but paranoia!
			return eTS_Disabled;
			break;
		case IAction::Installed:
			break;
		case IAction::Finished: // this means 'about to finish'
		case IAction::None:
			return eTS_Completing;
			break;
		}

		break;
	};

	return m_state;
}

// ----------------------------------------------------------------------------
CExactPositioning::ETriggerState CExactPositioning::StateCompleting_HandleEvent( SStateMachineEvent& event )
{
	switch( event.type )
	{
	case ESME_Enter:
		// we've ended the animation, or are not interested in it anymore, let listeners know
		UpdateTargetPointToFinishPoint();

		if ( m_triggerQueryEnd )
		{
			SendQueryComplete( m_triggerQueryEnd, true );
			m_triggerQueryEnd = 0;
		}
		return eTS_Disabled;

	case ESME_Update:
		CRY_ASSERT(false); // this whole state seems unnecessary
		return eTS_Disabled;

		break;
	};

	return m_state;
}

// ----------------------------------------------------------------------------
void CExactPositioning::StateMachine_SendEvent( SStateMachineEvent& event )
{
	ETriggerState newState = ( this->*m_stateEventHandler )( event );
	CRY_ASSERT( newState == m_state );
}

// ----------------------------------------------------------------------------
void CExactPositioning::StateMachine_SendEventUntilStateDoesntChange( SStateMachineEvent& event )
{
	for( ;; )
	{
		ETriggerState newState = ( this->*m_stateEventHandler )( event );
		if ( newState == m_state )
			break;

		StateMachine_ChangeStateTo( newState );
	}
}

// ----------------------------------------------------------------------------
void CExactPositioning::SendFailureEvents()
{
	if ( m_triggerQueryStart )
	{
		SendQueryComplete( m_triggerQueryStart, false );
		m_triggerQueryStart = 0;
	}

	if ( m_triggerQueryEnd )
	{
		SendQueryComplete( m_triggerQueryEnd, false );
		m_triggerQueryEnd = 0;
	}
}

// ----------------------------------------------------------------------------
void CExactPositioning::ClearExactPositioningTarget() 
{
	// TODO: Clear triggers too, if any (maybe combine all in one 'dynamicstate' as in exactpositioning)

	m_pExactPositioningTarget.reset();
}

// ----------------------------------------------------------------------------
void CExactPositioning::ClearAnimAction() 
{
	// NOTE: Can also be called from the destructor!

	if ( m_pAnimAction )
	{
		if (m_pAnimAction->GetStatus() != IAction::None)
			m_pAnimAction->ForceFinish();
		m_pAnimAction = NULL;
	}
}

// ----------------------------------------------------------------------------
void CExactPositioning::UpdateTargetPointToFinishPoint() 
{
	if ( !m_pAnimatedCharacter )
		return;

	IEntity* pEntity = m_pAnimatedCharacter->GetEntity();
	if ( !pEntity )
		return;

	IActionController* pActionController = m_pAnimatedCharacter->GetActionController();
	if ( !pActionController )
		return;

	/*
	CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();
	bool debug = CAnimationGraphCVars::Get().m_debugExactPos != 0;
	if (debug)
		pPD->Begin( string( pEntity->GetName() ) + "_recalculatetriggerpositions", true );			

	Quat actorRot = pEntity->GetWorldRotation();

	Vec3 targetPosition = m_animationTargetRequest.position;
	Vec3 targetDirection = m_animationTargetRequest.direction.GetNormalizedSafe(ZERO);

	Vec3 bumpUp(0,0, pEntity->GetWorldPos().z+4);

	SAnimationMovement movement;
	float seconds = 0.0f;
	ColorF dbgClr1, dbgClr2;

	// TODO: figure out animationmovement and duration

	//seconds = m_triggerMovement.duration;
	//movement = m_triggerMovement;

	if ( m_pExactPositioningTarget->isNavigationalSO && movement.translation.IsZero( FLT_EPSILON ) )
	{
#if STORE_TAG_STRINGS
		const char* fragmentName = pActionController->GetContext().controllerDef.m_fragmentIDs.GetTagName( m_fragmentID );
#else
		const char* fragmentName = "<n/a>";
#endif

		CryWarning(
			VALIDATOR_MODULE_GAME,
			VALIDATOR_WARNING,
			"MannequinAGExactPositioning: Beaming back to the beginning of the smart object because the fragment had no translation: '%s' (id = %d) (entity = '%s'; controllerDef = '%s')",
			fragmentName ? fragmentName : "<invalid>",
			m_fragmentID,
			pEntity ? pEntity->GetName() : "<invalid>",
			pActionController->GetContext().controllerDef.m_filename );
	}

	targetPosition += Quat::CreateRotationV0V1( FORWARD_DIRECTION, targetDirection ) * movement.translation;
	targetDirection = movement.rotation * targetDirection;
	seconds = movement.duration;
	dbgClr1 = ColorF(1,1,0,1);
	dbgClr2 = ColorF(1,0,0,1);

	Vec3 realStartPosition = pEntity->GetWorldPos();
	Quat realStartOrientation = actorRot;

	if (debug)
	{
		pPD->Begin( string(pEntity->GetName()) + "_finalpathline", true );
		Vec3 targetPosition2D(targetPosition.x, targetPosition.y, realStartPosition.z);
		Vec3 realStartDirection = realStartOrientation.GetColumn1();
		pPD->AddLine(realStartPosition, targetPosition2D, ColorF(1,1,0,1), 5.0f);
		pPD->AddDirection(realStartPosition, 0.5f, realStartDirection, ColorF(1,0,0,1), 5.0f);
		pPD->AddDirection(targetPosition2D, 0.5f, targetDirection, ColorF(1,1,1,1), 5.0f);

		pPD->AddDirection(realStartPosition + bumpUp, 1, realStartOrientation.GetColumn1(), dbgClr1, 5);
		pPD->AddSphere(realStartPosition + bumpUp, 0.1f, dbgClr1, 5);
	}

	Quat startOrientation = realStartOrientation;
	// start orientation may not be within bounds; we'll fake things to get it there
	// figure out our expected end orientation, and hence direction
	Quat realEndOrientation = startOrientation * movement.rotation;
	Quat endOrientation = realEndOrientation;
	Vec3 realEndDirection = realEndOrientation.GetColumn1();
	Vec3 endDirection = realEndDirection;
	// how much are we out from our limitations?
	float dot = realEndDirection.Dot(targetDirection);
	float angleToDesiredDirection = acos_tpl( clamp_tpl(dot,-1.f,1.f) );
	if (angleToDesiredDirection > m_animationTargetRequest.directionTolerance)
	{
		float fractionOfTurnNeeded = m_animationTargetRequest.directionTolerance / angleToDesiredDirection;
		Quat rotationFromDesiredToCurrent = Quat::CreateRotationV0V1( targetDirection, realEndDirection );
		Quat amountToRotate = Quat::CreateSlerp( Quat::CreateIdentity(), rotationFromDesiredToCurrent, fractionOfTurnNeeded );
		endDirection = amountToRotate * targetDirection;
		endOrientation = Quat::CreateRotationV0V1( FORWARD_DIRECTION, endDirection );
		startOrientation = endOrientation * movement.rotation.GetInverted();
	}
	// find out approx. where the animation ends
	Vec3 realEndPoint = realStartPosition + realStartOrientation * movement.translation;
	Vec3 endPoint = realStartPosition + startOrientation * movement.translation;
	if (debug)
	{
		pPD->AddDirection(realEndPoint + bumpUp, 1, realEndDirection, dbgClr1, 5);
		pPD->AddSphere(realEndPoint + bumpUp, 0.1f, dbgClr1, 5);
		pPD->AddSphere(endPoint + bumpUp, 0.1f, dbgClr2, 5);
	}
	// if the animation is outside of the target sphere, project it to the boundary
	Vec3 dirTargetToEndPoint = endPoint - targetPosition;
	float distanceTargetToEndPoint = dirTargetToEndPoint.GetLength();
	if (distanceTargetToEndPoint > 0.1f)
	{
		dirTargetToEndPoint /= distanceTargetToEndPoint;
		dirTargetToEndPoint *= 0.08f;
		endPoint = targetPosition + dirTargetToEndPoint;
	}
	// calculate error velocities (between where we expect we'll end up, and where we'll really end up)
	if (seconds > FLT_EPSILON)
	{
		m_pExactPositioningTarget->errorRotationalVelocity = Quat::CreateSlerp( Quat::CreateIdentity(), realEndOrientation.GetInverted() * endOrientation, 1.0f/seconds );
		m_pExactPositioningTarget->errorVelocity = (endPoint - realEndPoint)/ seconds;
		if (debug)
			pPD->AddDirection(endPoint + bumpUp, 1, m_pExactPositioningTarget->errorVelocity, dbgClr2, 5);
	}
	else
	{
		m_pExactPositioningTarget->errorRotationalVelocity = Quat::CreateIdentity();
		m_pExactPositioningTarget->errorVelocity = ZERO;
		endPoint = targetPosition;
		endOrientation = m_pExactPositioningTarget->orientation;
	}
	*/

	// Until we can figure out where the animation is going to end, only
	// update the animtarget on Completing
	if (m_state == eTS_Completing)
	{
		Vec3 endPoint = pEntity->GetWorldPos();
		const Quat& endOrientation = pEntity->GetWorldRotation();

		// Project end point on ground.
		if (m_actorTargetParams.projectEnd == true)
		{
			ray_hit hit;
			int rayFlags = rwi_stop_at_pierceable|(geom_colltype_player<<rwi_colltype_bit);
			IPhysicalEntity* skipEnts[1];
			skipEnts[0] = pEntity->GetPhysics();
			if (gEnv->pPhysicalWorld->RayWorldIntersection(endPoint + Vec3(0,0,1.0f), Vec3(0,0,-2.0f), 
				ent_terrain | ent_static | ent_rigid | ent_sleeping_rigid, rayFlags, &hit, 1, skipEnts, 1))
			{
				endPoint = hit.pt;
			}
		}

		// set the position/orientation
		m_pExactPositioningTarget->location.t = endPoint;

		m_pExactPositioningTarget->location.q = endOrientation;
		m_pExactPositioningTarget->orientationTolerance = m_actorTargetParams.directionTolerance;
		m_pExactPositioningTarget->startWidth = m_actorTargetParams.startWidth;
		m_pExactPositioningTarget->positionWidth = 0.05f;
		m_pExactPositioningTarget->positionDepth = 0.05f;

		CRY_ASSERT(m_pExactPositioningTarget->location.IsValid());
		CRY_ASSERT(NumberValid(m_pExactPositioningTarget->startWidth));
		CRY_ASSERT(NumberValid(m_pExactPositioningTarget->positionWidth));
		CRY_ASSERT(NumberValid(m_pExactPositioningTarget->positionDepth));
		CRY_ASSERT(NumberValid(m_pExactPositioningTarget->orientationTolerance));
	}

	// tell AI the ending position
	if ( m_pExactPositioningListener )
	{
		m_pExactPositioningListener->ExactPositioningNotifyFinishPoint( m_pExactPositioningTarget->location.t );
	}
}

// ----------------------------------------------------------------------------
void CExactPositioning::SetExactPositioningListener( IExactPositioningListener* pExactPositioningListener )
{
	m_pExactPositioningListener = pExactPositioningListener;
}

/*
// ----------------------------------------------------------------------------
void CExactPositioning::DebugDraw( IRenderer * pRend, int& x, int& y, int YSPACE )
{
	float white[4] = {1,1,1,1};
	float red[4] = {1,0,0,1};
	float yellow[4] = {1,1,0,1};
	float green[4] = {0,1,0,1};

	static const size_t BUFSZ = 512;
	char buf[BUFSZ];

	if (m_ds.state != eTS_Disabled)
	{
		y += 2*YSPACE;

		const char * stateName = "<<erm, better fix me>>";
		float radius = 0.0f;
		Vec3 point = m_ds.target.position;

		switch (m_ds.state)
		{
		case eTS_Considering:
			stateName = "Considering";
			radius = 10.0f;
			break;
		case eTS_Waiting:
			stateName = "Waiting for";
			radius = m_trigger.prepareRadius + m_trigger.startWidth;
			break;
		case eTS_FinalPreparation:
			stateName = "About to start";
			radius = m_trigger.startWidth;
			break;
		case eTS_Preparing:
			stateName = "Preparing for";
			radius = m_trigger.startWidth;
			break;
		case eTS_Running:
			stateName = "Running";
			radius = 0.1f;
			break;
		case eTS_Completing:
			stateName = "Completing";
			radius = 0.1f;
			break;
		}
		pRend->Draw2dLabel((float)x, (float)y, 1.f, white, false, "%s transition to %s", stateName, m_ds.targetStateID!=INVALID_STATE? m_pState->GetGraph()->m_states[m_ds.targetStateID].id.c_str() : "<<invalid state>>");
		y += YSPACE;
		if (m_ds.state <= eTS_Preparing)
		{
			pRend->Draw2dLabel((float)x, (float)y, 1.f, white, false, "Distance to target: %f", m_ds.target.position.GetDistance(m_pState->GetEntity()->GetWorldPos()));
			y += YSPACE;
			pRend->Draw2dLabel((float)x, (float)y, 1.f, white, false, "Distance to end: %f", m_trigger.position.GetDistance(m_pState->GetEntity()->GetWorldPos()));
			y += YSPACE;

			Vec3 worldOrientation = m_pState->GetEntity()->GetWorldRotation().GetColumn1();
			float fDot = worldOrientation.Dot(m_ds.target.orientation.GetColumn1());
			fDot = clamp_tpl(fDot,-1.0f,1.0f);
			float directionErrorRadians = acos_tpl( fDot );
			bool inDirectionRange = directionErrorRadians < (m_trigger.directionTolerance + EXTRA_DIRECTION_SLOP_IN_PREPARATION);
			float directionErrorDegrees = RAD2DEG(directionErrorRadians);

			float * clr = white;
			if (!inDirectionRange)
				clr = red;
			pRend->Draw2dLabel((float)x, (float)y, 1.f, clr, false, "Direction error: %f\0260", directionErrorDegrees);
			y += YSPACE;
		}
		pRend->Draw2dLabel((float)x, (float)y, 1.f, white, false, "Trigger:");
		y += YSPACE;
		pRend->Draw2dLabel((float)(x+10), (float)y, 1.f, white, false, "pos: %.2f %.2f %.2f + %.2f", m_trigger.position.x, m_trigger.position.y, m_trigger.position.z, m_trigger.startWidth);
		y += YSPACE;
		pRend->Draw2dLabel((float)(x+10), (float)y, 1.f, white, false, "dir: %.2f %.2f %.2f + %.2f", m_trigger.direction.x, m_trigger.direction.y, m_trigger.direction.z, m_trigger.directionTolerance);
		y += YSPACE;
		pRend->Draw2dLabel((float)x, (float)y, 1.f, white, false, "Movement:");
		y += YSPACE;
		pRend->Draw2dLabel((float)(x+10), (float)y, 1.f, white, false, "dist: %.2f %.2f %.2f", m_triggerMovement.translation.x, m_triggerMovement.translation.y, m_triggerMovement.translation.z);
		y += YSPACE;
		pRend->Draw2dLabel((float)(x+10), (float)y, 1.f, white, false, "orient: %.2f %.2f %.2f %.2f", m_triggerMovement.rotation.v.x, m_triggerMovement.rotation.v.y, m_triggerMovement.rotation.v.z, m_triggerMovement.rotation.w);
		y += YSPACE;
		pRend->Draw2dLabel((float)(x+10), (float)y, 1.f, white, false, "time: %.2f", m_triggerMovement.duration);
		y += YSPACE;
		pRend->Draw2dLabel((float)x, (float)y, 1.f, m_ds.target.allowActivation? green : yellow, false, "Target: [%d%d]", m_ds.target.activated, m_ds.target.preparing);
		y += YSPACE;
		pRend->Draw2dLabel((float)(x+10), (float)y, 1.f, white, false, "pos: %.2f %.2f %.2f", m_ds.target.position.x, m_ds.target.position.y, m_ds.target.position.z);
		y += YSPACE;
		pRend->Draw2dLabel((float)(x+10), (float)y, 1.f, white, false, "orient: %.2f %.2f %.2f %.2f", m_ds.target.orientation.v.x, m_ds.target.orientation.v.y, m_ds.target.orientation.v.z, m_ds.target.orientation.w);
		y += YSPACE;
		pRend->Draw2dLabel((float)(x+10), (float)y, 1.f, white, false, "time: %.2f", m_ds.target.activationTimeRemaining);
		y += YSPACE;
		pRend->Draw2dLabel((float)x, (float)y, 1.f, white, false, "Inputs:");
		y += YSPACE;

		for (int i=0; i<CAnimationGraph::MAX_INPUTS; i++)
		{
			if (m_triggerInputs[i] != CStateIndex::INPUT_VALUE_DONT_CARE)
			{
				m_pState->GetGraph()->m_inputValues[i]->DebugText( buf, m_triggerInputs[i], NULL );
				int correctness = 0;
				if (m_pState->GetCurrentState() != INVALID_STATE)
					correctness |= (int) m_pState->GetGraph()->m_stateIndex.StateMatchesInput( m_pState->GetCurrentState(), i, m_triggerInputs[i], eSMIF_ConsiderMatchesAny );
				if (m_pState->GetQueriedState() != INVALID_STATE)
					correctness |= 2 * m_pState->GetGraph()->m_stateIndex.StateMatchesInput( m_pState->GetQueriedState(), i, m_triggerInputs[i], eSMIF_ConsiderMatchesAny );

				float * clr;
				switch (correctness)
				{
				default:
					clr = red;
					break;
				case 1:
					clr = yellow;
					break;
				case 2:
					clr = green;
					break;
				case 3:
					clr = white;
					break;
				}

				pRend->Draw2dLabel( (float)(x+10), (float)y, 1.f, clr, false, "  %.20s: [%d] %s", m_pState->GetGraph()->m_inputValues[i]->name.c_str(), m_triggerInputs[i], buf );
				y += YSPACE;
			}
		}

		IRenderAuxGeom * pAux = pRend->GetIRenderAuxGeom();
		pAux->SetRenderFlags( e_Mode3D | e_AlphaBlended | e_DrawInFrontOff | e_FillModeSolid | e_CullModeBack | e_DepthWriteOn | e_DepthTestOn );
		if (radius)
			pAux->DrawSphere( point, radius, ColorF(0,1,1,0.1f) );
		pAux->DrawSphere( m_trigger.position, 0.2f, ColorF(1,0,0,1) );

		if (m_ds.target.errorVelocity.GetLength() > 0.001f)
		{
			Vec3 ptLineMid = point + Vec3(0,0,2);
			Vec3 ptLineStart = ptLineMid - m_ds.target.errorVelocity * 5.0f;
			Vec3 ptLineEnd = ptLineMid + m_ds.target.errorVelocity * 5.0f;

			pAux->DrawLine( ptLineStart, ColorB(241,193,92,255), ptLineEnd, ColorB(241,193,92,255) );
			pAux->DrawCone( ptLineEnd, m_ds.target.errorVelocity.GetNormalized(), 0.4f, 0.4f, ColorB(241,193,92,255) );
		}
		if (true)
		{
			Vec3 dir = m_ds.target.orientation.GetColumn1();

			Vec3 ptLineMid = point + Vec3(0,0,2);
			Vec3 ptLineStart = ptLineMid - dir * 5.0f;
			Vec3 ptLineEnd = ptLineMid + dir * 5.0f;

			pAux->DrawLine( ptLineStart, ColorB(156,14,241,255), ptLineEnd, ColorB(156,14,241,255) );
			pAux->DrawCone( ptLineEnd, dir, 0.4f, 0.4f, ColorB(156,14,241,255) );
		}
	}
	else
	{
		pRend->Draw2dLabel( (float)x, (float)y, 1.f, white, false, "Exact positioning disabled for %f seconds", m_ds.disabledTime );
		y += YSPACE;
	}

	m_ds.trigger.DebugDraw();
}
*/
