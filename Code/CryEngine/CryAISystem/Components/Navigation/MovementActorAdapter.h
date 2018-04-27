// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/IMovementSystem.h>

class CEntityAINavigationComponent;

class CMovementActorAdapter : public IMovementActorAdapter
{
public:	
	CMovementActorAdapter(CEntityAINavigationComponent* owner)
		: m_pOwningNavigationComponent(owner)
		, m_navigationAgentTypeID(NavigationAgentTypeID())
		, m_pPathfollower(nullptr)
	{
	}

	// IMovementActorCommunicationInterface
	virtual void OnMovementPlanProduced() override {}
	virtual NavigationAgentTypeID GetNavigationAgentTypeID() const override { return m_navigationAgentTypeID; }
	virtual bool GetDesignedPath(SShape& pathShape) const override { return false; }
	virtual Vec3 GetPhysicsPosition() const override;
	virtual void SetActorPath(const MovementStyle& style, const INavPath& navPath) override {}
	virtual void SetActorStyle(const MovementStyle& style, const INavPath& navPath) override {}
	virtual std::shared_ptr<Vec3> CreateLookTarget() override { std::shared_ptr<Vec3> zero(new Vec3(ZERO)); return zero; }
	virtual void ResetMovementContext() override {}
	virtual void SetLookTimeOffset(float lookTimeOffset) override {}
	virtual void UpdateLooking(float updateTime, std::shared_ptr<Vec3> lookTarget, const bool targetReachable, const float pathDistanceToEnd, const Vec3& followTargetPosition, const MovementStyle& style) override {}
	virtual Vec3 GetVelocity() const override;
	virtual Vec3 GetMoveDirection() const override { return Vec3(ZERO); }
	virtual void ConfigurePathfollower(const MovementStyle& style) override;
	virtual void SetMovementOutputValue(const PathFollowResult& result) override;
	virtual void ClearMovementState() override;
	virtual void SetStance(const MovementStyle::Stance stance) override {}
	virtual bool IsMoving() const override;
	virtual void ResetBodyTarget() override {}
	virtual void SetBodyTargetDirection(const Vec3& direction) override {}
	virtual Vec3 GetAnimationBodyDirection() const override { return Vec3(ZERO); }
	virtual void RequestExactPosition(const SAIActorTargetRequest* request, const bool lowerPrecision) override {}
	virtual EActorTargetPhase GetActorPhase() const override { return eATP_None; }
	virtual bool IsClosestToUseTheSmartObject(const OffMeshLink_SmartObject& smartObjectLink) const override { return false; }
	virtual bool PrepareNavigateSmartObject(CSmartObject* pSmartObject, OffMeshLink_SmartObject* pSmartObjectLink) override { return false; }
	virtual void InvalidateSmartObjectLink(CSmartObject* pSmartObject, OffMeshLink_SmartObject* pSmartObjectLink) override {}
	virtual void ResetActorTargetRequest() override {}
	virtual void CancelRequestedPath() override {};
	// ~IMovementActorCommunicationInterface

	void SetNavigationAgentTypeID(NavigationAgentTypeID navigationAgentTypeID) { m_navigationAgentTypeID = navigationAgentTypeID; }
	void SetPathFollower(IPathFollower* pPathFollower) { m_pPathfollower = pPathFollower; }

private:
	CEntityAINavigationComponent* m_pOwningNavigationComponent;
	NavigationAgentTypeID m_navigationAgentTypeID;
	IPathFollower* m_pPathfollower;
};