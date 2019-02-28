#include "StdAfx.h"
#include "PathfindingComponent.h"

namespace Cry
{
namespace DefaultComponents
{

struct SOnMovementRecommendation
{
	SOnMovementRecommendation() {}
	SOnMovementRecommendation(const Vec3& velocity)
		: m_velocity(velocity) {}

	Vec3 m_velocity;
};

static void ReflectType(Schematyc::CTypeDesc<SOnMovementRecommendation>& desc)
{
	desc.SetGUID("{A2C579DA-37FC-4622-9EA0-95971DE23BE1}"_cry_guid);
	desc.SetLabel("On Movement Recommendation");
	desc.AddMember(&SOnMovementRecommendation::m_velocity, 'vel', "Velocity", "Velocity", "The velocity proposed by the path finding solver", Vec3(0.0f));
}

void CPathfindingComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SOnMovementRecommendation));
}

void CPathfindingComponent::Initialize()
{
	Reset();

	m_navigationAgentTypeId = gEnv->pAISystem->GetNavigationSystem()->GetAgentTypeID("MediumSizedCharacters");

	m_callbacks.queuePathRequestFunction = functor(*this, &CPathfindingComponent::RequestPathTo);
	m_callbacks.checkOnPathfinderStateFunction = functor(*this, &CPathfindingComponent::GetPathfinderState);
	m_callbacks.getPathFollowerFunction = functor(*this, &CPathfindingComponent::GetPathFollower);
	m_callbacks.getPathFunction = functor(*this, &CPathfindingComponent::GetINavPath);

	gEnv->pAISystem->GetMovementSystem()->RegisterEntity(GetEntityId(), m_callbacks, *this);

	if (m_pPathFollower == nullptr)
	{
		PathFollowerParams params;
		params.maxAccel = m_maxAcceleration;
		params.maxSpeed = params.maxAccel;
		params.minSpeed = 0.f;
		params.normalSpeed = params.maxSpeed;

		params.use2D = false;

		m_pPathFollower = gEnv->pAISystem->CreateAndReturnNewDefaultPathFollower(params, m_pathObstacles);
	}

	m_movementAbility.b3DMove = true;
}

void CPathfindingComponent::SetMaxAcceleration(float maxAcceleration) 
{
	m_maxAcceleration = maxAcceleration; 

	PathFollowerParams params;
	params.maxAccel = m_maxAcceleration;
	params.maxSpeed = params.maxAccel;
	params.minSpeed = 0.f;
	params.normalSpeed = params.maxSpeed;

	params.use2D = false;

	m_pPathFollower->SetParams(params);
}

void CPathfindingComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
		case ENTITY_EVENT_START_GAME:
		{
			Initialize();
		}
		break;
		case ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED:
		{
			PathFollowerParams params;
			params.maxAccel = m_maxAcceleration;
			params.maxSpeed = params.maxAccel;
			params.minSpeed = 0.f;
			params.normalSpeed = params.maxSpeed;

			params.use2D = false;

			m_pPathFollower->SetParams(params);
		}
		break;
	}
}

uint64 CPathfindingComponent::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_START_GAME) | ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);
}

void CPathfindingComponent::SetMovementOutputValue(const PathFollowResult& result)
{
	if (Schematyc::IObject* pSchematycObject = m_pEntity->GetSchematycObject())
	{
		pSchematycObject->ProcessSignal(SOnMovementRecommendation(result.velocityOut), GetGUID());
	}

	if (m_movementRecommendationCallback)
	{
		m_movementRecommendationCallback(result.velocityOut);
	}
}

void CPathfindingComponent::ClearMovementState()
{
	if (Schematyc::IObject* pSchematycObject = m_pEntity->GetSchematycObject())
	{
		pSchematycObject->ProcessSignal(SOnMovementRecommendation(ZERO), GetGUID());
	}

	if (m_movementRecommendationCallback)
	{
		m_movementRecommendationCallback(ZERO);
	}
}
}
}
