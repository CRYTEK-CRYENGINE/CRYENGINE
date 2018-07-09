// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BehaviorTreeNodes_Helicopter.h"

#include <CryAISystem/BehaviorTree/Action.h>
#include <CryAISystem/BehaviorTree/IBehaviorTree.h>

#include "PipeUser.h"
#include "AIPlayer.h"
#include "Puppet.h"

#include <CryMath/Cry_Geo.h>
#include <CryMath/Cry_GeoDistance.h>
#include <CryMath/Cry_GeoOverlap.h>

#include "FlyHelpers_Tactical.h"
#include "FlyHelpers_PathFollower.h"

#include "AIPlayer.h"
#include "BehaviorTreeManager.h"

#define MIN_ORIENTATION_SPEED_FACTOR 0.25f
#define MIN_ORIENTATION_SPEED_RANGE  (1.0f + MIN_ORIENTATION_SPEED_FACTOR)

namespace FlyHelpers
{
ILINE PathEntityIn CreatePathEntityIn(const CPipeUser* pPipeUser)
{
	CRY_ASSERT(pPipeUser);

	PathEntityIn pathEntityIn;

	pathEntityIn.position = pPipeUser->GetPos();
	pathEntityIn.forward = pPipeUser->GetViewDir();
	pathEntityIn.velocity = pPipeUser->GetVelocity();

	return pathEntityIn;
}

ILINE void SendEvent(CPipeUser* pPipeUser, const char* eventName)
{
	CRY_ASSERT(pPipeUser);
	CRY_ASSERT(eventName);

	SEntityEvent event(ENTITY_EVENT_SCRIPT_EVENT);
	event.nParam[0] = (INT_PTR)(eventName);
	event.nParam[1] = IEntityClass::EVT_BOOL;
	static const bool s_value = true;
	event.nParam[2] = (INT_PTR)(&s_value);

	IEntity* pEntity = pPipeUser->GetEntity();
	pEntity->SendEvent(event);
}

float CalculateAdHocSpeedMultiplierForOrientation(const PathEntityIn& pathEntityIn, const PathEntityOut& pathEntityOut)
{
	// TODO: This is temp test code that needs revisiting.
	const Vec3 currentMoveDirection = pathEntityIn.velocity.GetNormalizedSafe(pathEntityIn.forward);

	const float lookDirectionDotMoveDirection = pathEntityOut.lookDirection.Dot(currentMoveDirection);
	const float lookDirectionFactor = max(0.0f, lookDirectionDotMoveDirection);
	const float lookDirectionFactorSquared = lookDirectionFactor * lookDirectionFactor;
	const float orientationSpeedFactor = (MIN_ORIENTATION_SPEED_FACTOR + lookDirectionFactorSquared) / MIN_ORIENTATION_SPEED_RANGE;

	const float lookDirectionDotForward = pathEntityOut.lookDirection.Dot(pathEntityIn.forward);
	const float lookDirectionFactor2 = max(0.0f, lookDirectionDotForward);
	const float lookDirectionFactorSquared2 = lookDirectionFactor2 * lookDirectionFactor2;
	const float orientationSpeedFactor2 = (MIN_ORIENTATION_SPEED_FACTOR + lookDirectionFactorSquared2) / MIN_ORIENTATION_SPEED_RANGE;

	const float speedMultiplier = orientationSpeedFactor * orientationSpeedFactor2;
	return speedMultiplier;
}
}

//////////////////////////////////////////////////////////////////////////
namespace BehaviorTree
{
bool GetClampedXmlAttribute(const XmlNodeRef& xml, const char* const attributeName, float& valueInOut, const float minValue, const float maxValue)
{
	CRY_ASSERT(minValue <= maxValue);

	const bool readAttributeSuccesss = xml->getAttr(attributeName, valueInOut);
	if (readAttributeSuccesss)
	{
		valueInOut = clamp_tpl(valueInOut, minValue, maxValue);
	}
	return readAttributeSuccesss;
}

class Hover
	: public Action
{
public:
	struct RuntimeData
	{
		Vec3 targetPosition;

		RuntimeData() : targetPosition(ZERO)
		{
		}
	};

	virtual void OnInitialize(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		runtimeData.targetPosition = context.entity.GetAI()->CastToCPipeUser()->GetPos();
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		CPipeUser* pPipeUser = context.entity.GetAI()->CastToCPipeUser();

		const Vec3 currentPosition = pPipeUser->GetPos();
		const Vec3 velocity = pPipeUser->GetVelocity();
		const float speed = velocity.GetLength();

		pPipeUser->m_State.predictedCharacterStates.nStates = 0;
		const Vec3 currentPositionToTargetPosition = runtimeData.targetPosition - currentPosition;
		pPipeUser->m_State.vMoveDir = currentPositionToTargetPosition.GetNormalizedSafe(Vec3(ZERO));
		pPipeUser->m_State.fDistanceToPathEnd = 0;

		// TODO: Make this frame rate independent.
		//const float desiredSpeed = max( 0.0f, ( speed * 0.6f ) - 0.2f );
		const float desiredSpeed = 0.5f;
		pPipeUser->m_State.fDesiredSpeed = desiredSpeed;
		pPipeUser->m_State.fTargetSpeed = desiredSpeed;

		return Running;
	}
};

class FlyShoot
	: public Action
{
public:
	struct RuntimeData
	{
	};

	FlyShoot()
		: m_useSecondaryWeapon(false)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		m_useSecondaryWeapon = false;
		xml->getAttr("useSecondaryWeapon", m_useSecondaryWeapon);

		return LoadSuccess;
	}

	virtual void OnInitialize(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = context.entity.GetAI()->CastToCPipeUser();
		pPipeUser->SetFireTarget(GetWeakRef(static_cast<CAIObject*>(pPipeUser->GetAttentionTarget())));

		if (!m_useSecondaryWeapon)
		{
			pPipeUser->SetFireMode(FIREMODE_FORCED);
			pPipeUser->m_State.fire = eAIFS_On;
		}
	}

	virtual Status Update(const UpdateContext& context) override
	{
		if (m_useSecondaryWeapon)
		{
			CPipeUser* pPipeUser = context.entity.GetAI()->CastToCPipeUser();
			Vec3 aimingPosition(ZERO);
			const bool validAimingPosition = CalculateAimingPosition(pPipeUser, aimingPosition);
			if (validAimingPosition)
			{
				SOBJECTSTATE& pipeUserState = pPipeUser->m_State;

				pPipeUser->SetFireMode(FIREMODE_VEHICLE);
				pipeUserState.fireSecondary = eAIFS_On;
				pipeUserState.secondaryIndex = SOBJECTSTATE::eFireControllerIndex_All;

				pipeUserState.vAimTargetPos = aimingPosition;
				pipeUserState.aimTargetIsValid = true;
			}
			else
			{
				// We do not care about failure cases when we cannot find a valid aim position.
				return Running;
			}
		}
		return Running;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = context.entity.GetAI()->CastToCPipeUser();
		pPipeUser->SetFireTarget(NILREF);

		pPipeUser->SetFireMode(FIREMODE_VEHICLE);
		if (m_useSecondaryWeapon)
		{
			pPipeUser->m_State.fireSecondary = eAIFS_Off;
			pPipeUser->m_State.secondaryIndex = SOBJECTSTATE::eFireControllerIndex_None;
		}
		else
		{
			pPipeUser->m_State.fire = eAIFS_Off;
		}
	}

private:

	bool CalculateAimingPosition(CPipeUser* pPipeUser, Vec3& aimingPositionOut) const
	{
		aimingPositionOut = Vec3(ZERO);
		IAIObject* pAttentionTarget = pPipeUser->GetAttentionTarget();
		if (pAttentionTarget)
		{
			aimingPositionOut = pAttentionTarget->GetPos();
			CPuppet* pPuppet = pPipeUser->CastToCPuppet();
			if (pPuppet)
			{
				const float maxRadiansForAdjustingFireTargetPosition = DEG2RAD(5.5f);
				const float missExtraOffsef = 1.0f;
				const bool canHitTarget = true;
				pPuppet->AdjustFireTarget(static_cast<CAIObject*>(pAttentionTarget), aimingPositionOut, canHitTarget, missExtraOffsef, maxRadiansForAdjustingFireTargetPosition, &aimingPositionOut);
			}
			return true;
		}

		return false;
	}

private:
	bool m_useSecondaryWeapon;
};

class WaitAlignedWithAttentionTarget
	: public Action
{
public:
	struct RuntimeData
	{
	};

	WaitAlignedWithAttentionTarget()
		: m_toleranceDegrees(20.0f)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		GetClampedXmlAttribute(xml, "toleranceDegrees", m_toleranceDegrees, 0.0f, 180.0f);
		return LoadSuccess;
	}

	virtual Status Update(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = context.entity.GetAI()->CastToCPipeUser();

		IAIObject* pAttentionTarget = pPipeUser->GetAttentionTarget();
		IF_UNLIKELY (!pAttentionTarget)
		{
			return Failure;
		}

		const float validFovCos = cos(DEG2RAD(m_toleranceDegrees));

		const Vec3& position = pPipeUser->GetPos();
		const Vec3 forward = pPipeUser->GetEntity()->GetForwardDir();

		const Vec3& targetPosition = pAttentionTarget->GetPos();
		const Vec3 directionToTarget = (targetPosition - position).GetNormalizedSafe(Vec3(0, 1, 0));

		const float dot = directionToTarget.Dot(forward);

		const bool inFov = (validFovCos <= dot);

		return inFov ? Success : Running;
	}

private:
	float m_toleranceDegrees;
};

class Fly
	: public Action
{
public:
	struct RuntimeData
	{
		FlyHelpers::PathFollower pathFollower;
		CTimeValue               lastUpdateTime;
		bool                     isValidPath;
		bool                     arrivedCloseToPathEndEventSent;
		bool                     arrivedAtPathEndEventSent;

		RuntimeData()
			: lastUpdateTime(0.0f)
			, isValidPath(false)
			, arrivedCloseToPathEndEventSent(false)
			, arrivedAtPathEndEventSent(false)
		{
		}
	};

	Fly()
		: m_pathEndDistance(1.0f)
		, m_goToRefPoint(false)
	{
	}

	virtual void OnInitialize(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		runtimeData.arrivedAtPathEndEventSent = false;
		runtimeData.arrivedCloseToPathEndEventSent = false;

		CPipeUser* pPipeUser = context.entity.GetAI()->CastToCPipeUser();

		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(context.entityId);
		CRY_ASSERT(pEntity);

		IScriptTable* pScriptTable = pEntity->GetScriptTable();
		IF_UNLIKELY (!pScriptTable)
		{
			return;
		}

		const char* pathName = pPipeUser->GetPathToFollow();
		CRY_ASSERT(pathName);

		pScriptTable->GetValue("Helicopter_Loop", m_params.loopAlongPath);
		pScriptTable->GetValue("Helicopter_Speed", m_params.desiredSpeed);
		pScriptTable->GetValue("Helicopter_StartFromClosestLocation", m_params.startPathFromClosestLocation);

		bool isValidPath = false;
		SShape path;
		const bool getPathSuccess = gAIEnv.pNavigation->GetDesignerPath(pathName, path);
		IF_LIKELY (getPathSuccess)
		{
			isValidPath = (!path.shape.empty());
		}

		runtimeData.isValidPath = isValidPath;
		if (!isValidPath)
		{
			ErrorReporter(*this, context).LogError("Invalid path '%s'", pathName);
			return;
		}

		runtimeData.lastUpdateTime = gEnv->pTimer->GetFrameStartTime();

		const FlyHelpers::PathEntityIn pathEntityIn = FlyHelpers::CreatePathEntityIn(pPipeUser);
		runtimeData.pathFollower.Init(path, m_params, pathEntityIn);

		if (m_goToRefPoint)
		{
			const Vec3 targetPosition = pPipeUser->GetRefPoint()->GetPos();

			runtimeData.pathFollower.SetFinalPathLocation(targetPosition);
		}

		runtimeData.pathFollower.Update(pathEntityIn, 1.0f);
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		IF_UNLIKELY (!runtimeData.isValidPath)
		{
			return Failure;
		}

		CPipeUser* pPipeUser = context.entity.GetAI()->CastToCPipeUser();
		const FlyHelpers::PathEntityIn pathEntityIn = FlyHelpers::CreatePathEntityIn(pPipeUser);

		const CTimeValue timeNow = gEnv->pTimer->GetFrameStartTime();
		const CTimeValue timeDelta = timeNow - runtimeData.lastUpdateTime;
		runtimeData.lastUpdateTime = timeNow;

		// TODO: find a better way to pass on data from flow-graph in run-time (make it event based).
		IEntity* pEntity = pPipeUser->GetEntity();
		assert(pEntity != NULL);
		IScriptTable* pScriptTable = pEntity->GetScriptTable();
		IF_LIKELY (pScriptTable != NULL)
		{
			float desiredSpeed = 0.0f;
			if (pScriptTable->GetValue("Helicopter_Speed", desiredSpeed))
			{
				runtimeData.pathFollower.SetDesiredSpeed(desiredSpeed);
			}
		}

		const float elapsedSeconds = timeDelta.GetSeconds();
		FlyHelpers::PathEntityOut pathEntityOut = runtimeData.pathFollower.Update(pathEntityIn, elapsedSeconds);

		pPipeUser->m_State.predictedCharacterStates.nStates = 0;
		pPipeUser->m_State.vMoveDir = pathEntityOut.moveDirection;
		pPipeUser->m_State.fDistanceToPathEnd = pathEntityOut.distanceToPathEnd;
		pPipeUser->m_State.vLookTargetPos = pathEntityOut.lookPosition;

		// TODO: Handle threat

		//pPipeUser->SetBodyTargetDir( pathEntityOut.lookDirection );
		pPipeUser->SetDesiredBodyDirectionAtTarget(pathEntityOut.lookDirection);

		float speedMultiplier = 1.0f;
		if (!m_goToRefPoint)
		{
			speedMultiplier = CalculateAdHocSpeedMultiplierForOrientation(pathEntityIn, pathEntityOut);
		}

		pPipeUser->m_State.fDesiredSpeed = pathEntityOut.speed * speedMultiplier;
		pPipeUser->m_State.fTargetSpeed = pathEntityOut.speed * speedMultiplier;

		if (gAIEnv.CVars.DebugPathFinding)
		{
			runtimeData.pathFollower.Draw();

			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pathEntityIn.position, Col_Yellow, pathEntityIn.position + pathEntityIn.forward, Col_Yellow);
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pathEntityIn.position, Col_ForestGreen, pathEntityIn.position + pathEntityOut.moveDirection * pathEntityOut.speed, Col_ForestGreen);
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pathEntityIn.position, Col_BlueViolet, pathEntityIn.position + pathEntityOut.lookDirection * 5.0f, Col_BlueViolet);

			const Vec3 velocity = pPipeUser->GetVelocity();
			const float speed = velocity.GetLength();
			IRenderAuxText::DrawLabelF(pathEntityIn.position + Vec3(0, 0, -2.0f), 1.0f, "velocity <%f, %f, %f> speed %f", velocity.x, velocity.y, velocity.z, speed);
			IRenderAuxText::DrawLabelF(pathEntityIn.position + Vec3(0, 0, -3.5f), 1.0f, "next requested speed %f", pathEntityOut.speed);
		}

		const float closeToPathEndDistance = max(m_params.decelerateDistance, m_pathEndDistance);
		const bool arrivedCloseToPathEnd = (pathEntityOut.distanceToPathEnd <= closeToPathEndDistance);
		if (arrivedCloseToPathEnd && !runtimeData.arrivedCloseToPathEndEventSent)
		{
			FlyHelpers::SendEvent(pPipeUser, "ArrivedCloseToPathEnd");
			runtimeData.arrivedCloseToPathEndEventSent = true;
		}

		const bool arrivedToPathEnd = (pathEntityOut.distanceToPathEnd <= m_pathEndDistance);
		if (arrivedToPathEnd)
		{
			if (!runtimeData.arrivedAtPathEndEventSent)
			{
				FlyHelpers::SendEvent(pPipeUser, "ArrivedAtPathEnd");
				runtimeData.arrivedAtPathEndEventSent = true;
			}

			return Success;
		}
		else
		{
			return Running;
		}
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		GetClampedXmlAttribute(xml, "desiredSpeed", m_params.desiredSpeed, 0.0f, FLT_MAX);
		GetClampedXmlAttribute(xml, "pathRadius", m_params.pathRadius, 0.0f, FLT_MAX);
		GetClampedXmlAttribute(xml, "lookAheadDistance", m_params.lookAheadDistance, 0.0f, FLT_MAX);
		GetClampedXmlAttribute(xml, "decelerateDistance", m_params.decelerateDistance, 0.0f, FLT_MAX);
		GetClampedXmlAttribute(xml, "maxStartDistanceAlongNonLoopingPath", m_params.maxStartDistanceAlongNonLoopingPath, 0.0f, FLT_MAX);
		xml->getAttr("loopAlongPath", m_params.loopAlongPath);
		xml->getAttr("startPathFromClosestLocation", m_params.startPathFromClosestLocation);
		GetClampedXmlAttribute(xml, "pathEndDistance", m_pathEndDistance, 0.0f, FLT_MAX);
		xml->getAttr("goToRefPoint", m_goToRefPoint);

		return LoadSuccess;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = context.entity.GetAI()->CastToCPipeUser();
		FlyHelpers::SendEvent(pPipeUser, "PathFollowingStopped");
	}

private:
	FlyHelpers::PathFollowerParams m_params;
	float                          m_pathEndDistance;
	bool                           m_goToRefPoint;
};

class FlyForceAttentionTarget
	: public Action
{
public:
	struct RuntimeData
	{
		EntityId targetEntityId;

		RuntimeData() : targetEntityId(0)
		{
		}
	};

	virtual void OnInitialize(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		runtimeData.targetEntityId = 0;

		CPipeUser* pPipeUser = context.entity.GetAI()->CastToCPipeUser();

		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(context.entityId);
		CRY_ASSERT(pEntity);

		IScriptTable* pScriptTable = pEntity->GetScriptTable();
		IF_UNLIKELY (!pScriptTable)
		{
			return;
		}

		pScriptTable->GetValue("Helicopter_ForcedTargetId", runtimeData.targetEntityId);
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity(runtimeData.targetEntityId);
		IF_UNLIKELY (pTargetEntity == NULL)
		{
			return Running;
		}

		IAIObject* pTargetAi = pTargetEntity->GetAI();
		IF_UNLIKELY (pTargetEntity == NULL)
		{
			return Running;
		}

		context.entity.GetAI()->CastToCPipeUser()->SetAttentionTarget(GetWeakRef(static_cast<CAIObject*>(pTargetAi)));
		return Running;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		FlyHelpers::SendEvent(context.entity.GetAI()->CastToCPipeUser(), "ForceAttentionTargetFinished");
	}

private:
};

class FlyAimAtCombatTarget
	: public Action
{
public:
	struct RuntimeData
	{
	};

	FlyAimAtCombatTarget()
	{
	}

	virtual Status Update(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = context.entity.GetAI()->CastToCPipeUser();

		CAIObject* pTargetAi = static_cast<CAIObject*>(pPipeUser->GetAttentionTarget());
		if (pTargetAi)
		{
			const Vec3 position = pPipeUser->GetPos();
			const Vec3 viewDirection = pPipeUser->GetViewDir();
			const Vec3 targetPosition = pTargetAi->GetPos();

			Vec3 adjustedTargetPosition = targetPosition;

			CWeakRef<CAIObject> refTarget = GetWeakRef<CAIObject>(pTargetAi);
			CPuppet* pPuppet = pPipeUser->CastToCPuppet();
			if (pPuppet)
			{
				pPuppet->UpdateTargetTracking(refTarget, targetPosition);
				pPuppet->m_Parameters.m_fAccuracy = 1.0f;
				const bool canDamage = pPuppet->CanDamageTarget(pTargetAi);

				pPuppet->AdjustFireTarget(pTargetAi, targetPosition, canDamage, 1.0f, DEG2RAD(5.5f), &adjustedTargetPosition);
			}

			const Vec3 targetDirection = (adjustedTargetPosition - position).GetNormalizedSafe(viewDirection);

			pPipeUser->m_State.vLookTargetPos = adjustedTargetPosition;
			pPipeUser->SetBodyTargetDir(targetDirection);
			pPipeUser->SetDesiredBodyDirectionAtTarget(targetDirection);

			pPipeUser->m_State.aimTargetIsValid = true;
			pPipeUser->m_State.vAimTargetPos = adjustedTargetPosition;
		}
		else
		{
			pPipeUser->m_State.aimTargetIsValid = false;
		}

		return Running;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		context.entity.GetAI()->CastToCPipeUser()->m_State.aimTargetIsValid = false;
	}
};

}

//////////////////////////////////////////////////////////////////////////
void RegisterBehaviorTreeNodesHelicopter()
{
	using namespace BehaviorTree;

	assert(gAIEnv.pBehaviorTreeManager);

	BehaviorTree::IBehaviorTreeManager& manager = *gAIEnv.pBehaviorTreeManager;

	REGISTER_BEHAVIOR_TREE_NODE(manager, Fly);
	REGISTER_BEHAVIOR_TREE_NODE(manager, FlyShoot);
	REGISTER_BEHAVIOR_TREE_NODE(manager, WaitAlignedWithAttentionTarget);
	REGISTER_BEHAVIOR_TREE_NODE(manager, Hover);
	REGISTER_BEHAVIOR_TREE_NODE(manager, FlyAimAtCombatTarget);
	REGISTER_BEHAVIOR_TREE_NODE(manager, FlyForceAttentionTarget);
}
