// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/ICollisionAvoidance.h>

class CEntityAINavigationComponent;
struct IEntity;

class CCollisionAvoidanceAgent : public Cry::AI::CollisionAvoidance::IAgent
{
public:
	CCollisionAvoidanceAgent(CEntityAINavigationComponent* pOwner)
		: m_pOwningNavigationComponent(pOwner)
		, m_pAttachedEntity(nullptr)
	{}

    virtual ~CCollisionAvoidanceAgent() override;

    bool                                  Register(IEntity* pEntity);
	bool                                  Unregister();

	virtual NavigationAgentTypeID         GetNavigationTypeId() const override;
	virtual const INavMeshQueryFilter*    GetNavigationQueryFilter() const override;
	virtual const char*                   GetDebugName() const override;
	virtual Cry::AI::CollisionAvoidance::ETreatType GetTreatmentDuringUpdateTick(Cry::AI::CollisionAvoidance::SAgentParams& outAgent, Cry::AI::CollisionAvoidance::SObstacleParams& outObstacle) const override;
	virtual void                          ApplyComputedVelocity(const Vec2& avoidanceVelocity, float updateTime) override;

private:
	CEntityAINavigationComponent* m_pOwningNavigationComponent;
	IEntity*                      m_pAttachedEntity;
};
