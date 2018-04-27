// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CollisionAvoidance/CollisionAvoidanceSystem.h"

class CEntityAINavigationComponent;
struct IEntity;

class CCollisionAvoidanceAgent : public ICollisionAvoidanceAgent
{
public:
	CCollisionAvoidanceAgent(CEntityAINavigationComponent* pOwner)
		: m_pOwningNavigationComponent(pOwner)
		, m_pAttachedEntity(nullptr)
	{}

	~CCollisionAvoidanceAgent();

	//! Registers/Unregisters entity to/from collision avoidance system. Passing nullptr to pEntity parameter unregisters the entity.
	void                                  Initialize(IEntity* pEntity);
	void                                  Release();

	virtual NavigationAgentTypeID         GetNavigationTypeId() const override;
	virtual const INavMeshQueryFilter* GetNavigationQueryFilter() const override;
	virtual const char*                   GetName() const override;
	virtual TreatType                     GetTreatmentType() const override;
	virtual void                          InitializeCollisionAgent(CCollisionAvoidanceSystem::SAgentParams& agent) const override;
	virtual void                          InitializeCollisionObstacle(CCollisionAvoidanceSystem::SObstacleParams& obstacle) const override;
	virtual void                          ApplyComputedVelocity(const Vec2& avoidanceVelocity, float updateTime) override;

private:
	CEntityAINavigationComponent* m_pOwningNavigationComponent;
	IEntity*                      m_pAttachedEntity;
};
