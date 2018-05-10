// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef MovementSystem_h
	#define MovementSystem_h

	#include <CryAISystem/IMovementSystem.h>
	#include <CryAISystem/MovementRequest.h>
	#include "MovementActor.h"

struct MovementUpdateContext;

class MovementSystem : public IMovementSystem
{
public:
	MovementSystem();

	// IMovementSystem
	virtual void              RegisterEntity(const EntityId entityId, MovementActorCallbacks callbacksConfiguration, IMovementActorAdapter& adapter) override;
	virtual void              UnregisterEntity(const EntityId entityId) override;
	virtual bool              IsEntityRegistered(const EntityId entityId) override;
	virtual MovementRequestID QueueRequest(const MovementRequest& request) override;
	virtual void              CancelRequest(const MovementRequestID& id) override;
	virtual void              GetRequestStatus(const MovementRequestID& id, MovementRequestStatus& status) const override;
	virtual void              Update(float updateTime) override;
	virtual void              Reset() override;
	virtual void              RegisterFunctionToConstructMovementBlockForCustomNavigationType(Movement::CustomNavigationBlockCreatorFunction blockFactoryFunction) override;
	virtual bool              AddActionAbilityCallbacks(const EntityId entityId, const SMovementActionAbilityCallbacks& ability) override;
	virtual bool              RemoveActionAbilityCallbacks(const EntityId entityId, const SMovementActionAbilityCallbacks& ability) override;
	// ~IMovementSystem

	Movement::BlockPtr CreateCustomBlock(const CNavPath& path, const PathPointDescriptor::OffMeshLinkData& mnmData, const MovementStyle& style);

	typedef std::pair<MovementRequestID, MovementRequest> MovementRequestPair;

private:
	enum ActorUpdateStatus
	{
		KeepUpdatingActor,
		ActorCanBeRemoved
	};

	MovementRequestID GenerateUniqueMovementRequestID();
	void              FinishRequest(MovementActor& actor, const MovementRequestID requestId, MovementRequestResult::Result result,
	                                MovementRequestResult::FailureReason failureReason = MovementRequestResult::NoReason);
	void              CleanUpAfterFinishedRequest(MovementActor& actor, MovementRequestID activeRequestID);
	MovementRequest&  GetActiveRequest(MovementActor& actor, MovementRequestID* outRequestID = NULL);
	MovementActor&    GetExistingActorOrCreateNewOne(const EntityId entityId, IMovementActorAdapter& adapter);
	MovementActor*    GetExistingActor(const EntityId entityId);
	bool              IsPlannerWorkingOnRequestID(const MovementActor& actor, const MovementRequestID& id) const;
	void              UpdateActors(float updateTime);
	ActorUpdateStatus UpdateActor(MovementActor& actor, float updateTime);
	void              UpdatePlannerAndDealWithResult(const MovementUpdateContext& context);
	void              StartWorkingOnNewRequestIfPossible(const MovementUpdateContext& context);

	#if defined(COMPILE_WITH_MOVEMENT_SYSTEM_DEBUG)
	void DrawDebugInformation() const;
	void DrawMovementRequestsForActor(const MovementActor& actor) const;
	#endif

	// Contains pairs of MovementRequestID and MovementRequest
	typedef std::vector<MovementRequestPair> Requests;
	Requests m_requests;

	// The actors are added and removed when they get new requests and
	// when they run out of them.
	typedef std::vector<MovementActor> Actors;
	Actors                                         m_actors;

	MovementRequestID                              m_nextUniqueRequestID;
	Movement::CustomNavigationBlockCreatorFunction m_createMovementBlockToHandleCustomNavigationType;
};

#endif // MovementSystem_h
