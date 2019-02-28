// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name: PipeUserMovementActorAdapter.h
 *********************************************************************/

#ifndef _PIPEUSERMOVEMENTACTORADAPTER_H__
#define _PIPEUSERMOVEMENTACTORADAPTER_H__

#include <CryAISystem/IMovementSystem.h>
#include <CryAISystem/IAIActor.h>

class CPipeUser;

class PipeUserMovementActorAdapter : public IMovementActorAdapter
{
public:
	PipeUserMovementActorAdapter(CPipeUser& pipeUser)
		: m_lookTimeOffset(.0f)
		, m_attachedPipeUser(pipeUser)
	{
	}

	virtual ~PipeUserMovementActorAdapter() {}

	// IMovementActorCommunicationInterface
	virtual void                  OnMovementPlanProduced() override;

	virtual NavigationAgentTypeID GetNavigationAgentTypeID() const override;
	virtual Vec3                  GetPhysicsPosition() const override;
	virtual Vec3                  GetVelocity() const override;
	virtual Vec3                  GetMoveDirection() const override;
	virtual Vec3                  GetAnimationBodyDirection() const override;
	virtual EActorTargetPhase     GetActorPhase() const override;
	virtual void                  SetMovementOutputValue(const PathFollowResult& result) override;
	virtual void                  SetBodyTargetDirection(const Vec3& direction) override;
	virtual void                  ResetMovementContext() override;
	virtual void                  ClearMovementState() override;
	virtual void                  ResetBodyTarget() override;
	virtual void                  ResetActorTargetRequest() override;
	virtual bool                  IsMoving() const override;

	virtual void                  RequestExactPosition(const SAIActorTargetRequest* request, const bool lowerPrecision) override;

	virtual bool                  IsClosestToUseTheSmartObject(const OffMeshLink_SmartObject& smartObjectLink) const override;
	virtual bool                  PrepareNavigateSmartObject(CSmartObject* pSmartObject, OffMeshLink_SmartObject* pSmartObjectLink) override;
	virtual void                  InvalidateSmartObjectLink(CSmartObject* pSmartObject, OffMeshLink_SmartObject* pSmartObjectLink) override;

	virtual bool                  GetDesignedPath(SShape& pathShape) const override;
	virtual void                  CancelRequestedPath() override;
	virtual void                  ConfigurePathfollower(const MovementStyle& style) override;

	virtual void                  SetActorPath(const MovementStyle& style, const INavPath& navPath) override;
	virtual void                  SetActorStyle(const MovementStyle& style, const INavPath& navPath) override;
	virtual void                  SetStance(const MovementStyle::Stance stance) override;

	virtual std::shared_ptr<Vec3> CreateLookTarget() override;
	virtual void                  SetLookTimeOffset(float lookTimeOffset) override;
	virtual void                  UpdateLooking(float updateTime, std::shared_ptr<Vec3> lookTarget, const bool targetReachable, const float pathDistanceToEnd, const Vec3& followTargetPosition, const MovementStyle& style) override;
	// ~IMovementActorCommunicationInterface

private:
	float      m_lookTimeOffset;
	CPipeUser& m_attachedPipeUser;
};

#endif // _PIPEUSERMOVEMENTACTORADAPTER_H__
