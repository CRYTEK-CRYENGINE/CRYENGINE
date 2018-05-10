// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CryAISystem/MovementRequestID.h>
#include <CryAISystem/MovementStyle.h>
#include "IPathfinder.h" // MNMDangersFlags, MNMCustomPathCostComputerSharedPtr

//! Passed along as a parameter to movement request callbacks.
struct MovementRequestResult
{
	enum Result
	{
		Success,
		Failure,

		//! Alias for 'Success' to increase readability in callback code.
		ReachedDestination = Success,
	};

	enum FailureReason
	{
		NoReason,
		CouldNotFindPathToRequestedDestination,
		CouldNotMoveAlongDesignerDesignedPath,
		FailedToProduceSuccessfulPlanAfterMaximumNumberOfAttempts,
	};

	MovementRequestResult(
	  const MovementRequestID& _id,
	  const Result _result,
	  const FailureReason _failureReason)
		: requestID(_id)
		, result(_result)
		, failureReason(_failureReason)
	{
	}

	MovementRequestResult(
	  const MovementRequestID& _id,
	  const Result _result)
		: requestID(_id)
		, result(_result)
		, failureReason(NoReason)
	{
	}

	bool operator==(const Result& rhs) const
	{
		return result == rhs;
	}

	operator bool() const
	{
		return result == Success;
	}

	const MovementRequestID requestID;
	const Result            result;
	const FailureReason     failureReason;
};

//! Contains everything needed for the movement system to make informed decisions about movement.
//! You specify where you want to move and how you want to do it. You can receive information about
//! your request by setting up a callback function.
struct MovementRequest
{
	typedef Functor1<const MovementRequestResult&> Callback;

	enum Type
	{
		MoveTo,
		Stop,
		CountTypes,  //!< This has to stay the last one for places where the number of elements of this enum is relevant.
	};

	MovementRequest()
		: destination(ZERO)
		, type(MoveTo)
		, callback(0)
		, entityID(0)
		, dangersFlags(eMNMDangers_None)
		, considerActorsAsPathObstacles(false)
		, lengthToTrimFromThePathEnd(0.0f)
		, pCustomPathCostComputer(nullptr)
	{
	}

	#ifdef COMPILE_WITH_MOVEMENT_SYSTEM_DEBUG
	static const char* GetTypeAsDebugName(Type type)
	{
		static_assert(CountTypes == 2, "Constant value is not as expected!"); // If this fails, then most likely a new value in the Type enum got introduced.

		if (type == MoveTo)
			return "MoveTo";
		else if (type == Stop)
			return "Stop";

		return "Undefined";
	}
	#endif

	MovementStyle   style;
	Vec3            destination;
	Type            type;
	Callback        callback;
	EntityId        entityID;
	MNMDangersFlags dangersFlags;
	bool            considerActorsAsPathObstacles;
	float           lengthToTrimFromThePathEnd;
	MNMCustomPathCostComputerSharedPtr pCustomPathCostComputer;
	SSnapToNavMeshRulesInfo snappingRules;
};

//! Contains information about the status of a request.
//! You'll see if it's in a queue, path finding, or what block of a plan it's currently executing.
struct MovementRequestStatus
{
	MovementRequestStatus() : id(NotQueued) {}

	enum ID
	{
		NotQueued,
		Queued,
		FindingPath,
		ExecutingPlan
	};
	operator ID() const { return id; }
	
	struct PlanStatus
	{
		enum Status
		{
			None,
			Running,
			Finished,
			CantFinish,
		};

		struct BlockInfo
		{
			BlockInfo() : name(nullptr) {}
			BlockInfo(const char* _name) : name(_name) {}

			const char* name;
		};
		typedef StaticDynArray<BlockInfo, 32> BlockInfos;
		
		PlanStatus() 
			: currentBlockIndex(0)
			, status(None)
			, abandoned(false)
		{}

		BlockInfos blockInfos;
		uint32     currentBlockIndex;
		Status     status;
		bool       abandoned;
	};

	PlanStatus planStatus;
	ID         id;
};

#if defined(COMPILE_WITH_MOVEMENT_SYSTEM_DEBUG)
inline void ConstructHumanReadableText(IN const MovementRequestStatus& status, OUT stack_string& statusText)
{
	switch (status)
	{
	case MovementRequestStatus::Queued:
		statusText = "Request In Queue";
		break;
	case MovementRequestStatus::FindingPath:
		statusText = "Finding Path";
		break;
	case MovementRequestStatus::ExecutingPlan:
		statusText = "Executing";
		break;
	case MovementRequestStatus::NotQueued:
		statusText = "Request Not Queued";
		break;
	default:
		statusText = "Unknown Status";
		break;
	}

	const size_t totalBlockInfos = status.planStatus.blockInfos.size();
	if (totalBlockInfos)
	{
		const bool cantFinish = status.planStatus.status == MovementRequestStatus::PlanStatus::CantFinish;
		const bool isAbandoned = status.planStatus.abandoned;

		statusText += " | Plan";
		if (isAbandoned)
		{
			statusText += "(Abandoned)";
		}
		statusText += ":";

		for (size_t index = 0; index < totalBlockInfos; ++index)
		{
			statusText += " ";

			const bool active = (index == status.planStatus.currentBlockIndex);

			if (active)
				statusText += cantFinish ? "x!" : "[";

			statusText += status.planStatus.blockInfos[index].name;

			if (active)
			{
				statusText += cantFinish ? "!x" : "]";
			}
		}
	}
}
#endif // COMPILE_WITH_MOVEMENT_SYSTEM_DEBUG

//! \endcond