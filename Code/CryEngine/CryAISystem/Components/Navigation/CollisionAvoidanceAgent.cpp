// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CollisionAvoidanceAgent.h"
#include "NavigationComponent.h"

CCollisionAvoidanceAgent::~CCollisionAvoidanceAgent()
{
	Release();
}

void CCollisionAvoidanceAgent::Initialize(IEntity* pEntity)
{
	if (m_pAttachedEntity != pEntity)
	{
		Release();
	}

	m_pAttachedEntity = pEntity;

	if (pEntity)
	{
		gAIEnv.pCollisionAvoidanceSystem->RegisterAgent(this);
	}
}

void CCollisionAvoidanceAgent::Release()
{
	if (m_pAttachedEntity)
	{
		gAIEnv.pCollisionAvoidanceSystem->UnregisterAgent(this);
		m_pAttachedEntity = nullptr;
	}
}

NavigationAgentTypeID CCollisionAvoidanceAgent::GetNavigationTypeId() const
{
	return m_pOwningNavigationComponent->GetNavigationTypeId();
}

const INavMeshQueryFilter* CCollisionAvoidanceAgent::GetNavigationQueryFilter() const
{
	return m_pOwningNavigationComponent->GetNavigationQueryFilter();
}

const char* CCollisionAvoidanceAgent::GetName() const
{
	return m_pAttachedEntity->GetName();
}

ICollisionAvoidanceAgent::TreatType CCollisionAvoidanceAgent::GetTreatmentType() const
{
	switch (m_pOwningNavigationComponent->GetCollisionAvoidanceProperties().type)
	{
	case CEntityAINavigationComponent::SCollisionAvoidanceProperties::EType::Active:
		if (m_pOwningNavigationComponent->GetRequestedVelocity().GetLengthSquared() > 0.00001f)
		{
			return ICollisionAvoidanceAgent::TreatType::Agent;
		}
	case CEntityAINavigationComponent::SCollisionAvoidanceProperties::EType::Passive:
		return ICollisionAvoidanceAgent::TreatType::Obstacle;
	default:
		return ICollisionAvoidanceAgent::TreatType::None;
	}
}

void CCollisionAvoidanceAgent::InitializeCollisionAgent(CCollisionAvoidanceSystem::SAgentParams& agent) const
{
	// Set current state
	agent.currentLocation = m_pOwningNavigationComponent->GetPosition();
	agent.currentVelocity = m_pOwningNavigationComponent->GetVelocity();
	agent.currentLookDirection = m_pOwningNavigationComponent->GetVelocity();
	agent.desiredVelocity = m_pOwningNavigationComponent->GetRequestedVelocity();

	// Set agent properties
	agent.maxSpeed = m_pOwningNavigationComponent->GetMovementProperties().maxSpeed;
	agent.maxAcceleration = m_pOwningNavigationComponent->GetMovementProperties().maxAcceleration;
	agent.radius = m_pOwningNavigationComponent->GetCollisionAvoidanceProperties().radius;
}
void CCollisionAvoidanceAgent::InitializeCollisionObstacle(CCollisionAvoidanceSystem::SObstacleParams& obstacle) const
{
	obstacle.currentLocation = m_pOwningNavigationComponent->GetPosition();
	obstacle.currentVelocity = m_pOwningNavigationComponent->GetVelocity();
	obstacle.radius = m_pOwningNavigationComponent->GetCollisionAvoidanceProperties().radius;
}

void CCollisionAvoidanceAgent::ApplyComputedVelocity(const Vec2& avoidanceVelocity, float updateTime)
{
	PathFollowResult result;
	result.velocityOut = Vec3(avoidanceVelocity.x, avoidanceVelocity.y, m_pOwningNavigationComponent->GetRequestedVelocity().z);
	m_pOwningNavigationComponent->NewStateComputed(result, this);
}
