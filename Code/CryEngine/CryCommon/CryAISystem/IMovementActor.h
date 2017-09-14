// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IMOVEMENTACTOR_H__
#define __IMOVEMENTACTOR_H__

#include <CryAISystem/IMovementSystem.h>

struct IMovementActorAdapter;

struct IMovementActor
{
	virtual ~IMovementActor() {}

	virtual IMovementActorAdapter&    GetAdapter() const = 0;
	virtual void                      RequestPathTo(const Vec3& destination, float lengthToTrimFromThePathEnd, const MNMDangersFlags dangersFlags = eMNMDangers_None,
	                                                const bool considerActorsAsPathObstacles = false, const MNMCustomPathCostComputerSharedPtr& pCustomPathCostComputer = nullptr) = 0;
	virtual Movement::PathfinderState GetPathfinderState() const = 0;
	virtual const char*               GetName() const = 0;
	virtual void                      Log(const char* message) = 0;

	virtual bool                      IsLastPointInPathASmartObject() const = 0;
	virtual EntityId                  GetEntityId() const = 0;
	virtual MovementActorCallbacks    GetCallbacks() const = 0;
};

#endif // __IMOVEMENTACTOR_H__
