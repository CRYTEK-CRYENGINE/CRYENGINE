// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: 
	Manages the special transitions for one specific entity class
-------------------------------------------------------------------------
History:
- 4:29:2010	19:48 : Created by Sven Van Soom
*************************************************************************/
#pragma once
#ifndef __MOVEMENT_TRANSITIONS_H
#define __MOVEMENT_TRANSITIONS_H

#include <CryAISystem/IAgent.h>
#include "MovementTransitionsController.h"

struct STransitionSelectionParams;
struct STransitionMatch;
struct STransition;
struct SActorFrameMovementParams;
struct IItemParamsNode;
struct SMovementTransitionsSample;
class CMovementRequest;
class CMovementTransitionsController;
class CPlayer;


///////////////////////////////////////////////////////////////////////////////
class CMovementTransitions
{
public:
	// public methods
	CMovementTransitions(IEntityClass* pEntityClass);
	~CMovementTransitions();

	void Load();
	void GetMemoryUsage(ICrySizer* s) const; // only used in CMovementTransitionsSystem
	IEntityClass* GetEntityClass() const { return m_pEntityClass; } // only used in CMovementTransitionsSystem
	float GetMinStopDistance(float pseudoSpeed, float travelAngle) const; // only used for quick stops during selection params calculation (all this quick stop logic move into STransition somehow)
	ILINE float GetMinDistanceAfterDirectionChange() const { return m_minDistanceAfterDirectionChange; } // only used in selection params calculation

	EMovementTransitionState Update(
		const uint8 allowedTransitionFlags,
		const Lineseg& safeLine,
		const CTimeValue runningDuration,
		const bool bHasLockedBodyTarget,
		const Vec3& playerPos,
		const SMovementTransitionsSample& oldSample,
		const SMovementTransitionsSample& newSample,
		const float entitySpeed2D,
		const float entitySpeed2DAvg,
		const SExactPositioningTarget*const pExactPositioningTarget,

		CMovementTransitionsController*const pController,
		CPlayer*const pPlayer,
		CMovementRequest*const pRequest,
		SActorFrameMovementParams*const pMoveParams,
		float*const pJukeTurnRateFraction,
		Vec3*const pBodyTarget,
		const char**const pBodyTargetType) const;
private:
	// private types
	typedef std::vector<STransition> TransitionVector;

	// private methods
	void FindBestMatch( const STransitionSelectionParams& transParams, const STransition** ppBestTransition, int* pBestIndex, STransitionMatch* pBestMatch ) const;
	void ReadGeneralParams( const struct IItemParamsNode* pParams );
	bool ReadTransitionsParams( const struct IItemParamsNode* pParams );

	ILINE string GetXMLFilename() const
	{
		const char* MOVEMENTTRANSITIONS_FILENAME_PREFIX = "Libs/MovementTransitions/MovementTransitions_";
		const char* MOVEMENTTRANSITIONS_FILENAME_SUFFIX = ".xml";

		CryFixedStringT<128> sResult;
		sResult.Format("%s%s%s", MOVEMENTTRANSITIONS_FILENAME_PREFIX, m_pEntityClass->GetName(), MOVEMENTTRANSITIONS_FILENAME_SUFFIX);
		return sResult;
	}

	void Reset();

	// private fields
	float							m_minDistanceAfterDirectionChange; // minimum distance we need to run after the direction change before we trigger a direction transition
	IEntityClass*			m_pEntityClass;
	TransitionVector	m_transitions;
	bool							m_isDataValid; // XML data is valid

	friend struct STransitionSelectionParams; // as this also needs the FindBestMatch function
};


///////////////////////////////////////////////////////////////////////////////
// Describes how good a transition fits the current situation
// Can be used to compare specific transitions & select the best one
struct STransitionMatch
{
	float						angleDifference;

	STransitionMatch() : angleDifference(FLT_MAX) {}
	ILINE bool IsBetterThan(const STransitionMatch& otherMatch) const { return angleDifference < otherMatch.angleDifference; }
};


///////////////////////////////////////////////////////////////////////////////
enum ETransitionType
{
	eTT_None = -1,

	eTT_Start,
	eTT_Stop,
	eTT_DirectionChange,
};

///////////////////////////////////////////////////////////////////////////////
// Input parameters used for determining whether a certain transition should
// be executed
struct STransitionSelectionParams
{
	ETransitionType	m_transitionType;
	float						m_pseudoSpeed;	// pseudoSpeed (1.0=Run, 0.4=Walk)
	EStance					m_stance; // current player stance
	float						m_transitionDistance;
	float						m_travelAngle;	// signed angle, in radians, between current body [animated character] and move direction, as seen from above, in 2D.
	float						m_targetTravelAngle;	// signed angle, in radians, between target body and move direction, as seen from above, in 2D.
	float						m_arrivalAngle; // signed angle, in radians, between desired body angle and move direction, as seen from above, in 2D. (only used for eTT_Stop)
	unsigned int		m_context; // movement context 'flags' passed along by AI

	float						m_jukeAngle;		// for eTT_DirectionChange: The change in movement direction, as seen from above, in 2D.  e.g. 0 means no change. PI/2 means turn or strafe to the right.

	// This is not used for selection, but it's useful to calculate it only once, before the selection takes place
	STransitionFuture m_future;
	bool						m_bPredicted;	// true when this transition is predicted to happen in the future; false if it's a direct transition request

	STransitionSelectionParams(
		const CMovementTransitions& transitions,
		const CPlayer& player,
		const CMovementRequest& request,
		const Vec3& playerPos,
		const SMovementTransitionsSample& oldSample,
		const SMovementTransitionsSample& newSample,
		const bool hasLockedBodyTarget,
		const Vec3& targetBodyDirection,
		const Lineseg& safeLine,
		const CTimeValue runningDuration,
		const uint8 allowedTransitionFlags,
		const float entitySpeed2D,
		const float entitySpeed2DAvg,
		const SExactPositioningTarget*const pExactPositioningTarget,
		const EStance stance,

		SActorFrameMovementParams*const pMoveParams);
};


///////////////////////////////////////////////////////////////////////////////
// Describes when to send a specific animation graph signal for a specific transition
struct STransitionMatch;
struct STransition
{
public:
	// public fields

	// Parameters for all transition types
	string					animGraphSignal;
	ETransitionType	transitionType;
	EStance					stance;												// stance, or STANCE_NULL when you don't care
	unsigned int		context;											// bit flags describing the 'context' which goes beyond stance (could be describing things like "going into cover"). Default value = 0.
	float						pseudoSpeed;									// pseudoSpeed (1.0=Run, 0.4=Walk)
	float						minDistance;									// signal is sent when within minDistance <= distance <= maxDistance. (For eTT_Start maxDistance is FLT_MAX)

	float						desiredTravelAngle;						// signed angle, in radians, between body and move direction (as seen in 2D, from above).
	float						travelAngleTolerance;					// travel angle tolerance, in radians, for starting the transition. Transition can be executed when 0 <= angleDifference(travelAngle, desiredTravelAngle) <= travelAngleTolerance. Should be >= 0.

	// Parameters for eTT_Stop & eTT_DirectionChange only
	float						maxDistance;
	float						prepareDistance;							// distance where we try to turn the character towards the desiredTravelAngle. Turning takes place when maxDistance < distance <= prepareDistance. Constraint: prepareDistance >= maxDistance.
	float						prepareTravelAngleTolerance;	// angle tolerance, in radians, for trying to make the desired angle by turning the character. Turning will be done when 0 <= angleDifference(travelAngle, desiredTravelAngle) <= prepareTravelAngleTolerance. Should be >= 0.

	// Parameters for eTT_Start & eTT_DirectionChange
	float						desiredTargetTravelAngle;			// signed angle, in radians, between target body and move direction (as seen in 2D, from above).
	float						targetTravelAngleTolerance;		// target travel angle tolerance, in radians, for starting the transition. Transition can be executed when 0 <= angleDifference(targetTravelAngle, desiredTargetTravelAngle) <= targetTravelAngleTolerance. Should be >= 0.

	// Parameters for eTT_Stop only
	float						desiredArrivalAngle;					// signed angle, in radians, between desired body direction at arrival & movement direction at arrival
	float						arrivalAngleTolerance;				// arrival angle tolerance, in radians, for starting the transition. Transition can be executed when 0 <= angleDifference(arrivalAngle, desiredArrivalAngle) <= arrivalAngleTolerance. Should be >= 0.

	// Parameters for eTT_DirectionChange only
	float						desiredJukeAngle;							// for eTT_DirectionChange: desired angle, in radians, between original and new movement direction (as seen in 2D, from above)
	float						jukeAngleTolerance;						// for eTT_DirectionChange: Juke is executed when 0 <= angleDifference(jukeAngle, desiredJukeAngle) <= jukeAngleTolerance. Should be >= 0.

	EStance					targetStance;									// stance that this movement transition ends in, or STANCE_NULL when it should not change the stance.

	// public methods
	ILINE bool IsMatch(const STransitionSelectionParams& transParams, STransitionMatch*const pMatch) const
	{
		if (transParams.m_transitionType != transitionType)
			return false;

		if (transParams.m_pseudoSpeed != pseudoSpeed)
			return false;

		if ((stance != STANCE_NULL) && (transParams.m_stance != stance))
			return false;

		if ((context != 0) && ((transParams.m_context & context) != context))
			return false;

		if (transParams.m_transitionDistance >= 0)
		{
			if (transParams.m_transitionDistance < minDistance)
				return false;

			if ((transitionType != eTT_Start) && (transParams.m_transitionDistance > prepareDistance))
				return false;
		}

		if (transitionType == eTT_Start)
		{
			float targetTravelAngleDifference = ::AngleDifference(transParams.m_targetTravelAngle, desiredTargetTravelAngle);
			if (targetTravelAngleDifference > targetTravelAngleTolerance)
				return false;
		}
		else if (transitionType == eTT_Stop)
		{
			float arrivalAngleDifference = ::AngleDifference(transParams.m_arrivalAngle, desiredArrivalAngle);
			if (arrivalAngleDifference > arrivalAngleTolerance)
				return false;
		}
		else if (transitionType == eTT_DirectionChange)
		{
			float jukeAngleDifference = ::AngleDifference(transParams.m_jukeAngle, desiredJukeAngle);
			if (jukeAngleDifference > jukeAngleTolerance)
				return false;

			float targetTravelAngleDifference = ::AngleDifference(transParams.m_targetTravelAngle, desiredTargetTravelAngle);
			if (targetTravelAngleDifference > targetTravelAngleTolerance)
				return false;
		}

		pMatch->angleDifference = ::AngleDifference(transParams.m_travelAngle, desiredTravelAngle);
		return true;
	}

	// Update returns eTS_Requesting_Succeeded only when the transition signal was successfully requested
	EMovementTransitionState Update(
		const CMovementTransitions& transitions,
		const STransitionSelectionParams& transParams,
		const STransitionMatch& match,
		const Vec3& playerPos,
		const Vec3& oldMoveDirection,
		const Vec3& newMoveDirection,

		float*const pJukeTurnRateFraction,
		Vec3*const pBodyTarget,
		const char**const pBodyTargetType,
		CPlayer*const pPlayer,
		CMovementTransitionsController*const pController) const;

	// Perform spatial queries when necessary to make sure the transition is possible
	bool CheckSpaceToPerformTransition(const CMovementTransitions& transitions, const STransitionSelectionParams& transParams, const Vec3& oldMoveDirection, const Vec3& playerPos, CPlayer*const pPlayer) const;

	bool ReadParams( ETransitionType _transitionType, const struct IItemParamsNode*const pParams );

#ifndef _RELEASE
	string GetDescription() const;
#endif
private:
	// private methods
	void Execute(const STransitionMatch& match, IAnimationGraphState*const pAnimationGraphState) const;

	bool ReadStanceParam( const char* name, bool required, const struct IItemParamsNode*const pParams, EStance*const pStance ) const;
	bool ReadPseudoSpeedParam( const char* name, bool required, const struct IItemParamsNode*const pParams, float*const pPseudoSpeed ) const;
	bool ReadIntParam( const char* name, bool required, int min, int max, const struct IItemParamsNode*const pParams, int*const pI ) const;
	bool ReadFloatParam( const char* name, bool required, float min, float max, const struct IItemParamsNode*const pParams, float*const pF ) const;
	bool ReadAngleParam( const char* name, bool required, float min, float max, const struct IItemParamsNode*const pParams, float*const pAngle ) const;
};


#endif // __MOVEMENT_TRANSITIONS_H
