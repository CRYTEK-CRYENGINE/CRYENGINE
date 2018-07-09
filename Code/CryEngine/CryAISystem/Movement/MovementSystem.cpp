// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MovementSystem.h"
#include <CryAISystem/MovementRequest.h>
#include <CryAISystem/MovementUpdateContext.h>
#include "MovementPlanner.h"
#include "AIConsoleVariables.h"

#include "MovementBlock_DefaultEmpty.h"

#if defined(COMPILE_WITH_MOVEMENT_SYSTEM_DEBUG)
	#include <CryAISystem/IAIDebugRenderer.h>
#endif

bool IsActorValidForMovementUpdateContextCreation(MovementActor& actor)
{
	if (actor.callbacks.getPathFollowerFunction && actor.callbacks.getPathFollowerFunction())
	{
		assert(actor.planner.get() != NULL);
		IF_UNLIKELY (actor.planner.get() == NULL)
			return false;

		return true;
	}

	return false;
}

MovementUpdateContext CreateMovementUpdateContextFrom(
  MovementActor& actor, MovementSystem& system, const float updateTime)
{
	IPathFollower* pathFollower = actor.callbacks.getPathFollowerFunction();

	assert(pathFollower);

	return MovementUpdateContext(
	  actor,
	  system,
	  *pathFollower,
	  *actor.planner.get(),
	  updateTime);
}

class ActorMatchesIdPredicate
{
public:
	ActorMatchesIdPredicate(const EntityId entityID)
		: m_entityID(entityID)
	{
	}

	bool operator()(const MovementActor& actor)
	{
		return actor.entityID == m_entityID;
	}

private:
	EntityId m_entityID;
};

MovementSystem::MovementSystem() : m_nextUniqueRequestID(0)
{
}

void MovementSystem::RegisterEntity(const EntityId entityId, MovementActorCallbacks callbacksConfiguration, IMovementActorAdapter& adapter)
{
	MovementActor& actor = MovementSystem::GetExistingActorOrCreateNewOne(entityId, adapter);

	actor.callbacks = callbacksConfiguration;
}

void MovementSystem::UnregisterEntity(const EntityId entityId)
{
	m_actors.erase(std::remove_if(m_actors.begin(), m_actors.end(), ActorMatchesIdPredicate(entityId)), m_actors.end());
}

bool MovementSystem::IsEntityRegistered(const EntityId entityId)
{
	Actors::const_iterator actorIt = std::find_if(m_actors.begin(), m_actors.end(), ActorMatchesIdPredicate(entityId));
	return actorIt != m_actors.end();
}

MovementRequestID MovementSystem::QueueRequest(const MovementRequest& request)
{
	assert(request.entityID);

	const MovementRequestID requestID = GenerateUniqueMovementRequestID();

	MovementActor* pActor = GetExistingActor(request.entityID);

	if (!pActor)
	{
		return MovementRequestID::Invalid();
	}

	pActor->requestQueue.push_back(requestID);

	m_requests.push_back(MovementRequestPair(requestID, request));

	return requestID;
}

class RequestMatchesIdPredicate
{
public:
	RequestMatchesIdPredicate(const MovementRequestID& id)
		: m_id(id)
	{
	}

	bool operator()(const MovementSystem::MovementRequestPair& requestPair) const
	{
		return requestPair.first == m_id;
	}

private:
	MovementRequestID m_id;
};

void MovementSystem::CancelRequest(const MovementRequestID& requestID)
{
	const Requests::iterator requestIt = std::find_if(m_requests.begin(), m_requests.end(), RequestMatchesIdPredicate(requestID));

	if (requestIt != m_requests.end())
	{
		const MovementRequest& request = requestIt->second;

		const Actors::iterator actorIt = std::find_if(m_actors.begin(), m_actors.end(), ActorMatchesIdPredicate(request.entityID));

		if (actorIt != m_actors.end())
		{
			MovementActor& actor = *actorIt;

			if (IsPlannerWorkingOnRequestID(actor, requestID))
			{
				actor.planner->CancelCurrentRequest(actor);
				actor.requestIdCurrentlyInPlanner = 0;
			}

			stl::find_and_erase(actor.requestQueue, requestID);
		}

		m_requests.erase(requestIt);
	}
}

void MovementSystem::GetRequestStatus(const MovementRequestID& requestID, MovementRequestStatus& status) const
{
	status.id = MovementRequestStatus::NotQueued;

	Actors::const_iterator actorIt = m_actors.begin();
	Actors::const_iterator actorEnd = m_actors.end();

	for (; actorIt != actorEnd; ++actorIt)
	{
		const MovementActor& actor = *actorIt;

		// Is the request being processed right now?
		if (IsPlannerWorkingOnRequestID(actor, requestID))
		{
			actor.planner->GetStatus(status);
			return;
		}

		// See if it's at least queued
		MovementRequestQueue::const_iterator requestIt = actor.requestQueue.begin();
		MovementRequestQueue::const_iterator requestEnd = actor.requestQueue.end();

		for (; requestIt != requestEnd; ++requestIt)
		{
			if (*requestIt == requestID)
			{
				status.id = MovementRequestStatus::Queued;
				return;
			}
		}
	}
}

void MovementSystem::Update(float updateTime)
{
	UpdateActors(updateTime);

#if defined(COMPILE_WITH_MOVEMENT_SYSTEM_DEBUG)
	if (gAIEnv.CVars.DebugMovementSystem)
	{
		DrawDebugInformation();
	}
	if (gAIEnv.CVars.DebugMovementSystemActorRequests)
	{
		if (gAIEnv.CVars.DebugMovementSystemActorRequests == 1)
		{
			Actors::const_iterator itActor = std::find_if(m_actors.begin(), m_actors.end(), ActorMatchesIdPredicate(GetAISystem()->GetAgentDebugTarget()));
			if (itActor != m_actors.end())
			{
				DrawMovementRequestsForActor(*itActor);
			}
		}
		else if (gAIEnv.CVars.DebugMovementSystemActorRequests == 2)
		{
			for (Actors::const_iterator itActor = m_actors.begin(); itActor != m_actors.end(); ++itActor)
			{
				DrawMovementRequestsForActor(*itActor);
			}
		}
	}
#endif
}

void MovementSystem::Reset()
{
	m_nextUniqueRequestID = 0;

	stl::free_container(m_actors);
	stl::free_container(m_requests);
}

void MovementSystem::RegisterFunctionToConstructMovementBlockForCustomNavigationType(Movement::CustomNavigationBlockCreatorFunction blockFactoryFunction)
{
	m_createMovementBlockToHandleCustomNavigationType = blockFactoryFunction;
}

//////////////////////////////////////////////////////////////////////////
bool MovementSystem::AddActionAbilityCallbacks(const EntityId entityId, const SMovementActionAbilityCallbacks& ability)
{
	MovementActor* pActor = GetExistingActor(entityId);
	if (!pActor)
		return false;

	return pActor->AddActionAbilityCallbacks(ability);
}

bool MovementSystem::RemoveActionAbilityCallbacks(const EntityId entityId, const SMovementActionAbilityCallbacks& ability)
{
	MovementActor* pActor = GetExistingActor(entityId);
	if (!pActor)
		return false;

	return pActor->RemoveActionAbilityCallbacks(ability);
}
//////////////////////////////////////////////////////////////////////////

Movement::BlockPtr MovementSystem::CreateCustomBlock(const CNavPath& path, const PathPointDescriptor::OffMeshLinkData& mnmData, const MovementStyle& style)
{
	if (m_createMovementBlockToHandleCustomNavigationType)
	{
		return m_createMovementBlockToHandleCustomNavigationType(path, mnmData, style);
	}
	else
	{
		AIError("The movement block to handle a path point of the NAV_CUSTOM_NAVIGATION type is not defined.");
		static Movement::BlockPtr emptyBlock(new Movement::MovementBlocks::DefaultEmpty());
		return emptyBlock;
	}
}

MovementRequestID MovementSystem::GenerateUniqueMovementRequestID()
{
	while (!++m_nextUniqueRequestID)
		;
	return m_nextUniqueRequestID;
}

void MovementSystem::FinishRequest(
  MovementActor& actor,
  const MovementRequestID requestId, // pass requestId by value to make a copy to be sure, that it is not changed due to the callback
  MovementRequestResult::Result resultID,
  MovementRequestResult::FailureReason failureReason)
{
	Requests::iterator itRequest = std::find_if(m_requests.begin(), m_requests.end(), RequestMatchesIdPredicate(requestId));
	if (itRequest != m_requests.end())
	{
		MovementRequest::Callback& callback = itRequest->second.callback;
		if (callback)
		{
			MovementRequestResult result(requestId, resultID, failureReason);
			callback(result); // Caution! The user callback code might invalidate iterators.
		}
	}

	CleanUpAfterFinishedRequest(actor, requestId);
}

void MovementSystem::CleanUpAfterFinishedRequest(MovementActor& actor, MovementRequestID activeRequestID)
{
	Requests::iterator itRequest = std::find_if(m_requests.begin(), m_requests.end(), RequestMatchesIdPredicate(activeRequestID));
	if (itRequest != m_requests.end())
	{
		m_requests.erase(itRequest);
	}

	MovementRequestQueue::iterator itActorRequest = std::find(actor.requestQueue.begin(), actor.requestQueue.end(), activeRequestID);
	if (itActorRequest != actor.requestQueue.end())
	{
		actor.requestQueue.erase(itActorRequest);
	}

	if (actor.requestIdCurrentlyInPlanner == activeRequestID)
	{
		actor.requestIdCurrentlyInPlanner = 0;
	}
}

MovementRequest& MovementSystem::GetActiveRequest(MovementActor& actor, MovementRequestID* outRequestID)
{
	MovementRequestQueue& queue = actor.requestQueue;
	assert(!queue.empty());

	MovementRequestID requestID = 0;
	if (!queue.empty())
		requestID = queue.front();
	else
		requestID = 0;

	if (outRequestID)
		*outRequestID = requestID;

	Requests::iterator it = std::find_if(m_requests.begin(), m_requests.end(), RequestMatchesIdPredicate(requestID));

	if (it != m_requests.end())
		return it->second;
	else
	{
		assert(0);
		static MovementRequest dummy;
		return dummy;
	}
}

MovementActor& MovementSystem::GetExistingActorOrCreateNewOne(const EntityId entityId, IMovementActorAdapter& adapter)
{
	Actors::iterator actorIt = std::find_if(m_actors.begin(), m_actors.end(), ActorMatchesIdPredicate(entityId));

	if (actorIt != m_actors.end())
	{
		// Return existing actor
		return *actorIt;
	}
	else
	{
		// Create new actor
		MovementActor actor(entityId, &adapter);
		actor.planner.reset(new Movement::GenericPlanner(adapter.GetNavigationAgentTypeID()));
		m_actors.push_back(actor);
		return m_actors.back();
	}
}

MovementActor* MovementSystem::GetExistingActor(const EntityId entityId)
{
	Actors::iterator actorIt = std::find_if(m_actors.begin(), m_actors.end(), ActorMatchesIdPredicate(entityId));

	if (actorIt != m_actors.end())
	{
		// Return existing actor
		return &(*actorIt);
	}

	return NULL;
}

bool MovementSystem::IsPlannerWorkingOnRequestID(const MovementActor& actor, const MovementRequestID& id) const
{
	return (actor.requestIdCurrentlyInPlanner == id) && (id != MovementRequestID::Invalid());
}

void MovementSystem::UpdateActors(float updateTime)
{
	using namespace Movement;

	Actors::iterator actorIt = m_actors.begin();

	while (actorIt != m_actors.end())
	{
		MovementActor& actor = *actorIt;

		const ActorUpdateStatus status = UpdateActor(actor, updateTime);

		++actorIt;
	}
}

MovementSystem::ActorUpdateStatus MovementSystem::UpdateActor(MovementActor& actor, float updateTime)
{
	// Construct a movement context which contains everything needed for
	// updating the passed in actor. Everything is validated once, here,
	// before anything else is called, so there's no need to validate again!

	IF_UNLIKELY (!IsActorValidForMovementUpdateContextCreation(actor))
	{
		return ActorCanBeRemoved;
	}

	MovementUpdateContext context = CreateMovementUpdateContextFrom(actor, *this, updateTime);

	StartWorkingOnNewRequestIfPossible(context);

	if (context.planner.IsUpdateNeeded())
	{
		UpdatePlannerAndDealWithResult(context);
	}

	if (!actor.requestQueue.empty() || context.planner.IsUpdateNeeded())
		return KeepUpdatingActor;
	else
		return ActorCanBeRemoved;
}

void MovementSystem::StartWorkingOnNewRequestIfPossible(const MovementUpdateContext& context)
{
	using namespace Movement;

	MovementActor& actor = static_cast<MovementActor&>(context.actor);
	MovementRequestQueue& requestQueue = actor.requestQueue;

	if (!requestQueue.empty())
	{
		MovementRequestID frontRequestID;
		MovementRequest& request = GetActiveRequest(actor, OUT & frontRequestID);

		IPlanner& planner = context.planner;
		if (!IsPlannerWorkingOnRequestID(actor, frontRequestID) && planner.IsReadyForNewRequest())
		{
			planner.StartWorkingOnRequest(frontRequestID, request, context);
			actor.requestIdCurrentlyInPlanner = frontRequestID;
		}
	}
}

void MovementSystem::UpdatePlannerAndDealWithResult(const MovementUpdateContext& context)
{
	using namespace Movement;

	IPlanner& planner = context.planner;
	const IPlanner::Status status = planner.Update(context);

	// The front request could have been canceled and removed but
	// the planner is still executing a plan that was produced to
	// satisfy it.
	// It's therefore important to be informed whether the planner is
	// working on the front request or not before reacting to the
	// result of the plan execution.

	MovementActor& actor = static_cast<MovementActor&>(context.actor);
	MovementRequestQueue& requestQueue = actor.requestQueue;

	if (!requestQueue.empty() && IsPlannerWorkingOnRequestID(actor, requestQueue.front()))
	{
		if (status.HasPathfinderFailed())
			FinishRequest(actor, status.GetRequestId(), MovementRequestResult::Failure, MovementRequestResult::CouldNotFindPathToRequestedDestination);
		else if (status.HasMovingAlongPathFailed())
			FinishRequest(actor, status.GetRequestId(), MovementRequestResult::Failure, MovementRequestResult::CouldNotMoveAlongDesignerDesignedPath);
		else if (status.HasReachedTheMaximumNumberOfReplansAllowed())
			FinishRequest(actor, status.GetRequestId(), MovementRequestResult::Failure, MovementRequestResult::FailedToProduceSuccessfulPlanAfterMaximumNumberOfAttempts);
		else if (status.HasRequestBeenSatisfied())
			FinishRequest(actor, status.GetRequestId(), MovementRequestResult::Success);
	}
}

#if defined(COMPILE_WITH_MOVEMENT_SYSTEM_DEBUG)
void MovementSystem::DrawDebugInformation() const
{
	const float fontSize = 1.25f;
	const float lineHeight = 11.5f * fontSize;
	const float x = 10.0f;
	float y = 30.0f;

	IAIDebugRenderer* renderer = gEnv->pAISystem->GetAIDebugRenderer();

	// Draw title
	renderer->Draw2dLabel(x, y, fontSize, Col_White, false, "Movement System Information");
	y += lineHeight;

	Actors::const_iterator actorIt = m_actors.begin();

	for (; actorIt != m_actors.end(); ++actorIt)
	{
		const MovementActor& actor = *actorIt;

		y += lineHeight;

		// Draw actor's name
		renderer->Draw2dLabel(x, y, fontSize, Col_LightBlue, false, "%s", actor.GetName());
		y += lineHeight;

		// Draw actor's movement status
		MovementRequestStatus status;
		actor.planner->GetStatus(OUT status);
		stack_string statusText;
		ConstructHumanReadableText(status, statusText);
		renderer->Draw2dLabel(x, y, fontSize, Col_White, false, "%s", statusText.c_str());
		y += lineHeight;
	}
}

void MovementSystem::DrawMovementRequestsForActor(const MovementActor& actor) const
{
	if (const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(actor.GetEntityId()))
	{
		const float spacing = 0.5f;

		Vec3 renderPos = pEntity->GetPos();
		renderPos.z += 2.0f + (float)actor.requestQueue.size() * spacing;

		for (MovementRequestQueue::const_iterator itQ = actor.requestQueue.begin(); itQ != actor.requestQueue.end(); ++itQ)
		{
			const MovementRequestID& requestID = *itQ;
			Requests::const_iterator itRequest = std::find_if(m_requests.begin(), m_requests.end(), RequestMatchesIdPredicate(requestID));

			if (itRequest != m_requests.end())
			{
				const MovementRequest& request = itRequest->second;
				const char* requestTypeStr = MovementRequest::GetTypeAsDebugName(request.type);
				IAIDebugRenderer* renderer = gEnv->pAISystem->GetAIDebugRenderer();
				renderer->Draw3dLabelEx(renderPos, 2.0f, IsPlannerWorkingOnRequestID(actor, requestID) ? Col_LightBlue : Col_White, true, false, false, false, "type = %s, id = %i", requestTypeStr, requestID.id);
				renderPos.z -= spacing;
			}
		}
	}
}

#endif // COMPILE_WITH_MOVEMENT_SYSTEM_DEBUG
