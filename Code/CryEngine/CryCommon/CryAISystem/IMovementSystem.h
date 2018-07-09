// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   IMovementSystem.h
//  Version:     v1.00
//  Created:     2011-12-20 by Jonas Johansson.
//  Description: Interface for the AI Movement System.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <CryAISystem/IAgent.h>
#include <CryAISystem/MovementStyle.h>
#include <CryAISystem/MovementBlock.h>

struct MovementRequest;
struct MovementRequestID;
struct MovementRequestStatus;
struct MNMPathRequest;
class INavPath;
class IPathFollower;
struct SShape;
struct PathFollowResult;
struct OffMeshLink_SmartObject;
class CSmartObject;

namespace Movement
{
struct IPlanner;

enum PathfinderState
{
	StillFinding,
	FoundPath,
	CouldNotFindPath,
	Canceled,
};
}

typedef Functor1<const Vec3&>                   SetOutputVelocityFunction;
typedef Functor1<MNMPathRequest&>               PathRequestFunction;
typedef Functor0wRet<Movement::PathfinderState> CheckOnPathfinderStateFunction;
typedef Functor0wRet<INavPath*>                 GetPathFunction;
typedef Functor0wRet<IPathFollower*>            GetPathFollowerFunction;

struct MovementActorCallbacks
{
	MovementActorCallbacks()
		: queuePathRequestFunction(NULL)
		, checkOnPathfinderStateFunction(NULL)
		, getPathFunction(NULL)
		, getPathFollowerFunction(NULL)
	{}

	PathRequestFunction            queuePathRequestFunction;
	CheckOnPathfinderStateFunction checkOnPathfinderStateFunction;
	GetPathFunction                getPathFunction;
	GetPathFollowerFunction        getPathFollowerFunction;
};

struct SMovementActionAbilityCallbacks
{
	typedef DynArray<Movement::BlockPtr> Blocks;
	typedef Functor2<Blocks&, const MovementRequest&> CreateBlocksCallback;
	typedef Functor2<const MovementUpdateContext&, bool> MovementUpdateCallback;

	bool operator==(const SMovementActionAbilityCallbacks& other) const
	{
		return addStartMovementBlocksCallback == other.addStartMovementBlocksCallback && addEndMovementBlocksCallback == other.addEndMovementBlocksCallback;
	}

	MovementUpdateCallback prePathFollowingUpdateCallback;
	MovementUpdateCallback postPathFollowingUpdateCallback;
	CreateBlocksCallback addStartMovementBlocksCallback;
	CreateBlocksCallback addEndMovementBlocksCallback;
};

//////////////////////////////////////////////////////////////////////////

struct IMovementActorAdapter
{
	virtual ~IMovementActorAdapter() {}

	virtual void                  OnMovementPlanProduced() = 0;

	virtual NavigationAgentTypeID GetNavigationAgentTypeID() const = 0;
	virtual Vec3                  GetPhysicsPosition() const = 0;
	virtual Vec3                  GetVelocity() const = 0;
	virtual Vec3                  GetMoveDirection() const = 0;
	virtual Vec3                  GetAnimationBodyDirection() const = 0;
	virtual EActorTargetPhase     GetActorPhase() const = 0;
	virtual void                  SetMovementOutputValue(const PathFollowResult& result) = 0;
	virtual void                  SetBodyTargetDirection(const Vec3& direction) = 0;
	virtual void                  ResetMovementContext() = 0;
	virtual void                  ClearMovementState() = 0;
	virtual void                  ResetBodyTarget() = 0;
	virtual void                  ResetActorTargetRequest() = 0;
	virtual bool                  IsMoving() const = 0;

	virtual void                  RequestExactPosition(const SAIActorTargetRequest* request, const bool lowerPrecision) = 0;

	virtual bool                  IsClosestToUseTheSmartObject(const OffMeshLink_SmartObject& smartObjectLink) const = 0;
	virtual bool                  PrepareNavigateSmartObject(CSmartObject* pSmartObject, OffMeshLink_SmartObject* pSmartObjectLink) = 0;
	virtual void                  InvalidateSmartObjectLink(CSmartObject* pSmartObject, OffMeshLink_SmartObject* pSmartObjectLink) = 0;

	virtual bool                  GetDesignedPath(SShape& pathShape) const = 0;
	virtual void                  CancelRequestedPath() = 0;
	virtual void                  ConfigurePathfollower(const MovementStyle& style) = 0;

	virtual void                  SetActorPath(const MovementStyle& style, const INavPath& navPath) = 0;
	virtual void                  SetActorStyle(const MovementStyle& style, const INavPath& navPath) = 0;
	virtual void                  SetStance(const MovementStyle::Stance stance) = 0;

	virtual std::shared_ptr<Vec3> CreateLookTarget() = 0;
	virtual void                  SetLookTimeOffset(float lookTimeOffset) = 0;
	virtual void                  UpdateLooking(float updateTime, std::shared_ptr<Vec3> lookTarget, const bool targetReachable, const float pathDistanceToEnd, const Vec3& followTargetPosition, const MovementStyle& style) = 0;
};

struct IMovementSystem
{
	virtual ~IMovementSystem() {}

	//! Register your entity to the movement system and provides the necessary information for the movement system to fulfill his work.
	virtual void RegisterEntity(const EntityId entityId, MovementActorCallbacks callbacksConfiguration, IMovementActorAdapter& adapter) = 0;

	//! Unregister your entity from the movement system to stop using its services.
	virtual void UnregisterEntity(const EntityId entityId) = 0;

	//! Check whether the entity is registered in the movement system.
	//! \return True if the entity is registered in the movement system or false otherwise.
	virtual bool IsEntityRegistered(const EntityId entityId) = 0;

	//! Ask the movement system to satisfy your request.
	//! The movement system will contact you via the callback to inform you whether the request has been satisfied or not.
	//! If you submit a callback you are responsible for calling CancelRequest before the callback becomes invalid.
	virtual MovementRequestID QueueRequest(const MovementRequest& request) = 0;

	//! Tell the movement system you are no longer interested in getting your request satisfied.
	//! Note that this doesn't necessarily mean that the actor will stop, it simply means that you no longer care about the outcome.
	virtual void CancelRequest(const MovementRequestID& id) = 0;

	//! Get information about the current status of a request.
	//! You'll see if it's in queue, path finding, or what block of a plan it's currently executing.
	virtual void GetRequestStatus(const MovementRequestID& id, MovementRequestStatus& status) const = 0;

	//! This resets the movement system to it's initial state.
	//! The internal data will be cleaned out and reset.
	//! No callbacks will be called during reset process.
	virtual void Reset() = 0;

	//! This is where the movement system will do it's work.
	virtual void Update(float updateTime) = 0;

	//! When a path constructed by the Navigation System contains a point belonging to the NAV_CUSTOM_NAVIGATION type this function will be invoked if it is registered.
	//! This should instantiate a movement block able to handle the movement through that type of navigation type that is usually created in the game code.
	virtual void RegisterFunctionToConstructMovementBlockForCustomNavigationType(Movement::CustomNavigationBlockCreatorFunction blockFactoryFunction) = 0;

	//! Register callbacks for special movement ability (extending planner)
	virtual bool AddActionAbilityCallbacks(const EntityId entityId, const SMovementActionAbilityCallbacks& ability) = 0;

	//! Unregister callbacks for special movement ability (extending planner)
	virtual bool RemoveActionAbilityCallbacks(const EntityId entityId, const SMovementActionAbilityCallbacks& ability) = 0;

};
