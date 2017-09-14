// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef MovementActor_h
	#define MovementActor_h

	#include <CryAISystem/IMovementActor.h>
	#include <CryAISystem/MovementRequest.h>
	#include <CryAISystem/IPathfinder.h>

// The movement system processes the movement requests for an actor
// in the order they come. Eaten off front-to-back. This is the queue.
typedef std::deque<MovementRequestID> MovementRequestQueue;

// -------------------------------------------------------------------------------------------

// Contains the information related to one specific actor in the
// context of movement.
// Retrieve information about an actor through this object.
struct MovementActor : public IMovementActor
{
	MovementActor(const EntityId _entityID, IMovementActorAdapter* _pAdapter)
		: entityID(_entityID)
		, requestIdCurrentlyInPlanner(0)
		, lastPointInPathIsSmartObject(false)
		, pAdapter(_pAdapter)
	{
		assert(pAdapter);
	}

	virtual ~MovementActor() {}

	// IMovementActor
	virtual IMovementActorAdapter&    GetAdapter() const override;
	virtual void                      RequestPathTo(const Vec3& destination, float lengthToTrimFromThePathEnd, const MNMDangersFlags dangersFlags,
	                                                const bool considerActorsAsPathObstacles, const MNMCustomPathCostComputerSharedPtr& pCustomPathCostComputer) override;
	virtual Movement::PathfinderState GetPathfinderState() const override;
	virtual const char*               GetName() const override;
	virtual void                      Log(const char* message) override;

	virtual bool                      IsLastPointInPathASmartObject() const override { return lastPointInPathIsSmartObject; }
	virtual EntityId                  GetEntityId() const override                   { return entityID; };
	virtual MovementActorCallbacks    GetCallbacks() const override                  { return callbacks; };
	// ~IMovementActor

	void      SetLowCoverStance();

	CAIActor* GetAIActor();

	std::shared_ptr<Movement::IPlanner> planner;
	EntityId                            entityID;
	MovementRequestQueue                requestQueue;
	MovementRequestID                   requestIdCurrentlyInPlanner;
	bool                                lastPointInPathIsSmartObject;

	MovementActorCallbacks              callbacks;
private:
	IMovementActorAdapter*              pAdapter;
};

#endif // MovementActor_h
