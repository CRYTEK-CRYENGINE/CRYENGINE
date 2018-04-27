// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/MovementUpdateContext.h>
#include <CryAISystem/MovementBlock.h>
#include <CryAISystem/IMovementActor.h>


namespace Movement
{

	// The planner takes a movement request and tries to come up with
	// a plan to satisfy it. It will also execute the plan and report
	// when it has been satisfied.
	struct IPlanner
	{
		class Status
		{
		public:
			Status()
				: m_requestId()
				, m_requestSatisfied(false)
				, m_pathfinderFailed(false)
				, m_movingAlongPathFailed(false)
				, m_reachedMaxNumberOfReplansAllowed(false)
			{}

			void SetRequestSatisfied(const MovementRequestID& requestId)
			{
				m_requestSatisfied = true;
				m_requestId = requestId;
			}
			void SetPathfinderFailed(const MovementRequestID& requestId)
			{
				m_pathfinderFailed = true;
				m_requestId = requestId;
			}
			void SetMovingAlongPathFailed(const MovementRequestID& requestId)
			{
				m_movingAlongPathFailed = true;
				m_requestId = requestId;
			}
			void SetReachedMaxAllowedReplans(const MovementRequestID& requestId)
			{
				m_reachedMaxNumberOfReplansAllowed = true;
				m_requestId = requestId;
			}

			bool                     HasRequestBeenSatisfied() const { return m_requestSatisfied; }
			bool                     HasPathfinderFailed() const { return m_pathfinderFailed; }
			bool                     HasMovingAlongPathFailed() const { return m_movingAlongPathFailed; }
			bool                     HasReachedTheMaximumNumberOfReplansAllowed() const { return m_reachedMaxNumberOfReplansAllowed; }

			const MovementRequestID& GetRequestId() const { return m_requestId; }

		private:
			MovementRequestID m_requestId;
			bool              m_requestSatisfied;
			bool              m_pathfinderFailed;
			bool              m_movingAlongPathFailed;
			bool              m_reachedMaxNumberOfReplansAllowed;
		};

		virtual ~IPlanner() {}

		// The controller will be updated and kept alive while this is true.
		virtual bool IsUpdateNeeded() const = 0;

		// Called when the movement system has some work for the planner.
		// Only called when 'IsReadyForNewRequest' returns true.
		virtual void StartWorkingOnRequest(const MovementRequestID& requestId, const MovementRequest& request, const MovementUpdateContext& context) = 0;

		// The movement system calls this when the actor is no longer
		// interested in satisfying the request the planner is working on.
		// Note: This doesn't mean that the actor wants to stop! The planner
		// may continue executing the plan until a new request comes in.
		virtual void CancelCurrentRequest(IMovementActor& actor) = 0;

		// Do some work and report back the 'Status' of the current request.
		virtual Status Update(const MovementUpdateContext& context) = 0;

		// Checked before 'StartWorkingOnRequest' is called. The planner might
		// be in the middle of something and mustn't be disturbed right now.
		virtual bool IsReadyForNewRequest() const = 0;

		// Fill in the 'status' object and explain what's currently
		// being worked on. It's very handy for debugging.
		virtual void GetStatus(MovementRequestStatus& status) const = 0;
	};

}
