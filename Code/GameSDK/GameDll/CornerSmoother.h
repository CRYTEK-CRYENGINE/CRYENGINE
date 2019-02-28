// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: 
  Responsible for going smoothly into and around corners
*************************************************************************/
#ifndef __CORNER_SMOOTHER_H__
#define __CORNER_SMOOTHER_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <CryAISystem/IAgent.h> // for SSpeedRange & AgentMovementAbility

#ifdef _RELEASE
#define INCLUDE_CORNERSMOOTHER_DEBUGGING() 0
#else
#define INCLUDE_CORNERSMOOTHER_DEBUGGING() 1
#endif

class IPathFollower;

namespace CornerSmoothing
{

	void OnLevelUnload();
	void SetLimitsFromMovementAbility(const struct AgentMovementAbility& movementAbility, const EStance stance, const float pseudoSpeed, struct SMovementLimits& limits);

	// ===========================================================================

	struct SMotionState
	{
		Vec3 position3D; // world space

		Vec2 position2D; // world space
		Vec2 movementDirection2D; // world space

		float speed;

#if !defined(_RELEASE)
		bool IsValid() const
		{
			return
				position3D.IsValid() &&
				position2D.IsValid() &&
				movementDirection2D.IsValid() &&
				(movementDirection2D.GetLength2() > 0.999f*0.999f) && // TODO: Make IsUnit for Vec2
				NumberValid(speed) &&
				(speed >= 0.0f);
		}
#endif
	};

	// ===========================================================================

	struct SMovementLimits
	{
		float maximumAcceleration;
		float maximumDeceleration; // positive number!
		AgentMovementSpeeds::SSpeedRange speedRange;

#if !defined(_RELEASE)
		bool IsValid() const
		{
			return
				NumberValid(maximumAcceleration) &&
				(maximumAcceleration >= 0.0f) &&
				NumberValid(maximumDeceleration) &&
				(maximumDeceleration >= 0.0f) &&
				NumberValid(speedRange.def) &&
				NumberValid(speedRange.min) &&
				NumberValid(speedRange.max) &&
				(speedRange.min >= 0.0f) &&
				// note that speedRange.min could be bigger than the current
				// motion.speed or desiredSpeed in edge cases (collision, avoidance, subtle bugs, ...)
				// so clamp it appropriately when used
				(speedRange.max >= 0.0f) &&
				(speedRange.max >= speedRange.min) &&
				(speedRange.def >= speedRange.min) &&
				(speedRange.def <= speedRange.max);
		}
#endif
	};

	// ===========================================================================

	struct SState
	{
		IPathFollower* pPathFollower; // for checkWalkability queries. When null, ignore walkability
		
		// Current State of Motion

		SMotionState motion;

		// Request

		float frameTime;

		float desiredSpeed;
		Vec2 desiredMovementDirection2D;

		bool hasMoveTarget;
		Vec3 moveTarget;
		bool hasInflectionPoint;
		Vec3 inflectionPoint;
		float distToPathEnd;

		// Limits

		SMovementLimits limits;

#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
		IActor* pActor;
#endif

#if !defined(_RELEASE)
		bool IsValid() const
		{
			return
				   motion.IsValid()
				&& NumberValid(frameTime)
				&& (frameTime >= 0.0f)
				&& NumberValid(desiredSpeed)
				&& (desiredSpeed >= 0.0f)
				&& desiredMovementDirection2D.IsValid()
				&& (!hasMoveTarget || moveTarget.IsValid())
				&& (!hasInflectionPoint || inflectionPoint.IsValid())
				&& NumberValid(distToPathEnd)
				&& (distToPathEnd >= 0.0f)
				&& limits.IsValid()
#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
				&& (pActor != NULL)
#endif
				;
		}
#endif
	};

	// ===========================================================================

	struct SOutput
	{
		Vec2 desiredMovementDirection2D;
		float desiredSpeed;
	};

	// ===========================================================================

	class CCornerSmoother2
	{
	public:
		CCornerSmoother2();
		~CCornerSmoother2();
		inline void Reset() { m_pCurrentPlan = NULL; }

		// return true when we have a valid output
		bool Update(const SState& state, SOutput& output);

	private:
#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
		void DebugDraw(const SState& state, const bool hasOutput, const SOutput& calculatedOutput) const;
#endif
		struct IExecutingPlan* m_pCurrentPlan; // points to one of the pre-allocated plans below

		class CTakeCornerPlan* m_pTakeCornerPlan;
		class CStopPlan* m_pStopPlan;
	};

} // namespace CornerSmoothing
#endif // __CORNER_SMOOTHER_H__