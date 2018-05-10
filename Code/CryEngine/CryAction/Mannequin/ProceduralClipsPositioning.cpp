// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "ICryMannequin.h"
#include <CryExtension/ClassWeaver.h>

#include "IActorSystem.h"
#include "IAnimatedCharacter.h"
#include "ProceduralClipsPositioning.h"

#include <Mannequin/Serialization.h>

void DebugDrawLocation(const QuatT& location, ColorB colorPos, ColorB colorX, ColorB colorY, ColorB colorZ)
{
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	const float thickness = 7.0f;
	const Vec3 pushUp(0.0f, 0.03f, 0.0f);

	pAuxGeom->DrawLine(location.t + pushUp, colorX, location.t + pushUp + location.q.GetColumn0(), colorX, thickness);
	pAuxGeom->DrawLine(location.t + pushUp, colorY, location.t + pushUp + location.q.GetColumn1(), colorY, thickness);
	pAuxGeom->DrawLine(location.t + pushUp, colorZ, location.t + pushUp + location.q.GetColumn2(), colorZ, thickness);

	const float radius = 0.06f;
	pAuxGeom->DrawSphere(location.t + pushUp, radius, colorPos);
}

void SProceduralClipPosAdjustTargetLocatorParams::Serialize(Serialization::IArchive& ar)
{
	ar(targetScopeName, "TargetScopeName", "Target Scope Name");
	ar(targetJointName, "TargetJointName", "Target Joint Name");
	ar(targetStateName, "TargetStateName", "Target State Name");
}

struct SPositionAdjustParams : public IProceduralParams
{
	SPositionAdjustParams()
		: idealOffset(ZERO)
		, idealYaw(0.f)
		, ignoreRotation(false)
		, ignorePosition(false)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(idealOffset, "Offset", "Offset");
		ar(idealYaw, "Yaw", "Yaw");
		ar(ignoreRotation, "IgnoreRotation", "Ignore Rotation");
		ar(ignorePosition, "IgnorePosition", "Ignore Position");
	}

	Vec3  idealOffset;
	float idealYaw;
	bool  ignoreRotation;
	bool  ignorePosition;
};

struct SPositionAdjuster
{
	SPositionAdjuster()
		: m_targetLoc(IDENTITY)
		, m_delta(ZERO)
		, m_deltaRot(IDENTITY)
		, m_lastTime(0.f)
		, m_targetTime(0.f)
		, m_invalid(true)
		, m_pAnimatedCharacter(NULL)
	{
	}

	void Init(IEntity& entity, const float blendTime)
	{
		m_targetTime = blendTime;
		m_lastTime = 0.0f;
		m_invalid = false;

		m_delta.zero();
		m_deltaRot.SetIdentity();

		IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(entity.GetId());
		m_pAnimatedCharacter = pActor ? pActor->GetAnimatedCharacter() : NULL;
	}

	void Update(IEntity& entity, float timePassed)
	{
		if (m_invalid)
			return;

		QuatT applyingDelta(IDENTITY);

		const float newTime = min(m_lastTime + timePassed, m_targetTime);
		const float deltaTime = newTime - m_lastTime;
		if (deltaTime > 0.0f)
		{
			Quat totalDeltaRot, lastTotalDeltaRot;

			const float targetTimeInverse = (float)__fres(m_targetTime);

			const float dt = deltaTime * targetTimeInverse;

			const float t = newTime * targetTimeInverse;
			const float lastT = m_lastTime * targetTimeInverse;

			totalDeltaRot.SetSlerp(Quat(IDENTITY), m_deltaRot, t);
			lastTotalDeltaRot.SetSlerp(Quat(IDENTITY), m_deltaRot, lastT);

			applyingDelta.q = (!lastTotalDeltaRot * totalDeltaRot);
			applyingDelta.t = (m_delta * dt);

			m_lastTime = newTime;
		}
		else if (m_targetTime == 0.f)
		{
			applyingDelta.t = m_delta;
			applyingDelta.q = m_deltaRot;
			m_invalid = true;
		}

		//CryLog("Moving %s from (%f, %f, %f) to (%f, %f, %f) delta (%f, %f, %f) timeDelta: %f time: %f timeTgt: %f",
		//			entity.GetName(),
		//			entity.GetPos().x, entity.GetPos().y, entity.GetPos().z,
		//			(entity.GetPos()+applyingDelta.t).x, (entity.GetPos()+applyingDelta.t).y, (entity.GetPos()+applyingDelta.t).z,
		//			applyingDelta.t.x, applyingDelta.t.y, applyingDelta.t.z,
		//			deltaTime, newTime, m_targetTime);

		if (m_pAnimatedCharacter)
		{
			m_pAnimatedCharacter->ForceMovement(applyingDelta);
		}
		else
		{
			if (IEntity* pParent = entity.GetParent())
				applyingDelta.t = !pParent->GetRotation() * applyingDelta.t;

			entity.SetPosRotScale(entity.GetPos() + applyingDelta.t, entity.GetRotation() * applyingDelta.q, entity.GetScale());
		}
	}

	QuatT               m_targetLoc;
	Vec3                m_delta;
	Quat                m_deltaRot;
	float               m_lastTime;
	float               m_targetTime;
	bool                m_invalid;
	IAnimatedCharacter* m_pAnimatedCharacter;
};

struct SCollisionAdjustParams
{
	SCollisionAdjustParams()
		: maxAdjustmentSpeed(0.f)
		, heightOffset(0.f)
		, heightMult(0.f)
		, widthMult(0.f) {}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(maxAdjustmentSpeed, "MaxAdjustmentSpeed", "Max Adjustment Speed");
		ar(heightOffset, "HeightOffset", "Height Offset");
		ar(heightMult, "HeightMult", "Height Mult");
		ar(widthMult, "WidthMult", "Width Mult");
	}

	float maxAdjustmentSpeed;
	float heightOffset;
	float heightMult;
	float widthMult;
};

class CProceduralContext_AdjustPos : public IProceduralContext
{
private:
	typedef IProceduralContext BaseClass;

	CProceduralContext_AdjustPos();
	virtual ~CProceduralContext_AdjustPos() {}

public:
	PROCEDURAL_CONTEXT(CProceduralContext_AdjustPos, "AdjustPosContext", "c6c08712-1421-4854-adc5-6ab6422834bd"_cry_guid);

	virtual void Update(float timePassed) override
	{
		if (m_enabled)
		{
			if (m_collisionCheck)
			{
				UpdateCollisionCheck(timePassed);
			}

			const uint32 numAdjusts = m_posAdjusts.size();
			for (uint32 i = 0; i < numAdjusts; i++)
			{
				SPosAdjust& posAdjust = m_posAdjusts[i];
				IScope* pScope = m_actionController->GetScope(posAdjust.scopeID);
				UpdateDeltas(posAdjust, pScope);

				posAdjust.posAdjuster.Update(pScope->GetEntity(), timePassed);
			}
		}
	}

	void StartPosAdjust(QuatT targetLocation, float blendTime, bool ignorePos, bool ignoreRot, IScope* pScope)
	{
		if (pScope)
		{
			if (ICharacterInstance* pCharInst = pScope->GetCharInst())
			{
				if (CAnimation* pAnim = pScope->GetTopAnim(0))
				{
					QuatT animStartLoc(IDENTITY);
					const bool success = pCharInst->GetIAnimationSet()->GetAnimationDCCWorldSpaceLocation(pAnim, animStartLoc, pCharInst->GetIDefaultSkeleton().GetControllerIDByID(0));

					if (success)
					{
						SPosAdjust& posAdjust = GetOrAddPosAdjustForScope(pScope->GetID());

						IEntity& entity = pScope->GetEntity();

						posAdjust.posAdjuster.Init(entity, blendTime);

						pAnim->SetStaticFlag(CA_FULL_ROOT_PRIORITY);

						QuatT entityLoc(entity.GetWorldPos(), entity.GetWorldRotation());

						posAdjust.posAdjuster.m_targetLoc = targetLocation;

						if (!ignorePos)
						{
							const Vec3 actualOffset = posAdjust.posAdjuster.m_targetLoc.t - entityLoc.t;
							const Vec3 idealOffset = posAdjust.posAdjuster.m_targetLoc.q * animStartLoc.t;
							posAdjust.posAdjuster.m_delta = actualOffset + idealOffset;
						}

						if (!ignoreRot)
						{
							const Quat actualRotOffset = !entityLoc.q * posAdjust.posAdjuster.m_targetLoc.q;
							posAdjust.posAdjuster.m_deltaRot = actualRotOffset * animStartLoc.q;
						}

						posAdjust.ignorePosition = ignorePos;
						posAdjust.ignoreRotation = ignoreRot;

						m_enabled = true;
					}
				}
			}
		}
	}

	void UpdatePosAdjust(QuatT targetLocation, IScope* pScope)
	{
		if (pScope)
		{
			if (SPosAdjust* pPosAdjust = GetPosAdjustForScope(pScope->GetID()))
			{
				pPosAdjust->posAdjuster.m_targetLoc = targetLocation;
				UpdateDeltas(*pPosAdjust, pScope);
			}
		}
	}

	void EndPosAdjust(IScope* pScope)
	{
		const uint32 numAdjusts = m_posAdjusts.size();
		for (uint32 i = 0; i < numAdjusts; i++)
		{
			if (m_posAdjusts[i].scopeID == pScope->GetID())
			{
				m_posAdjusts.erase(m_posAdjusts.begin() + i);
				break;
			}
		}

		m_enabled = m_posAdjusts.size() > 0;
		m_collisionCheck = false;
		m_slaveScopeIds.clear();
	};

	void StartCollisionCheck(const SCollisionAdjustParams& params, ActionScopes installedScopeMask)
	{
		m_collisionCheck = false;
		m_slaveScopeIds.clear();

		if (installedScopeMask != 0)
		{
			uint32 numScopes = m_actionController->GetTotalScopes();

			for (int i = 0; i < numScopes; i++)
			{
				if (installedScopeMask & BIT64(i))
				{
					if (IActionController* pSlaveActionController = m_actionController->GetScope(i)->GetEnslavedActionController())
					{
						m_slaveScopeIds.push_back(i);
						m_collisionCheck = true;
						m_collisionAdjust = params;
					}
				}
			}
		}
	}

	void AdjustTargetLocation(Vec3 adjustment)
	{
		const uint32 numAdjusts = m_posAdjusts.size();
		for (uint32 i = 0; i < numAdjusts; i++)
		{
			m_posAdjusts[i].posAdjuster.m_targetLoc.t += adjustment;
		}
	}

private:

	struct SPosAdjust
	{
		SPositionAdjuster posAdjuster;
		int               scopeID;
		bool              ignorePosition;
		bool              ignoreRotation;
	};

	void UpdateDeltas(SPosAdjust& posAdjust, IScope* pScope)
	{
		ICharacterInstance* pCharInst = pScope->GetCharInst();
		IEntity& entity = pScope->GetEntity();
		const CAnimation* pAnim = pScope->GetTopAnim(0);

		if (pCharInst && pAnim && (pAnim->GetTransitionWeight() > 0.0f))
		{
			QuatT startLoc;

			pCharInst->GetIAnimationSet()->GetAnimationDCCWorldSpaceLocation(pAnim, startLoc, pCharInst->GetIDefaultSkeleton().GetControllerIDByID(0));

			QuatT requiredAnimLoc = posAdjust.posAdjuster.m_targetLoc * startLoc;

			QuatT entityLoc(entity.GetWorldPos(), entity.GetWorldRotation());

			if (!posAdjust.ignorePosition)
			{
				posAdjust.posAdjuster.m_delta = requiredAnimLoc.t - entityLoc.t;
			}

			if (!posAdjust.ignoreRotation)
			{
				posAdjust.posAdjuster.m_deltaRot = !entityLoc.q * requiredAnimLoc.q;
				posAdjust.posAdjuster.m_deltaRot.Normalize();
			}

			/*
			   DebugDrawLocation(
			   requiredAnimLoc,
			   ColorB(255, 0, 0, 255),
			   ColorB(255, 128, 128, 255),
			   ColorB(255, 128, 128, 255),
			   ColorB(255, 128, 128, 255));

			   DebugDrawLocation(
			   m_posAdjuster.m_targetLoc,
			   ColorB(0, 255, 0, 255),
			   ColorB(128, 255, 128, 255),
			   ColorB(128, 255, 128, 255),
			   ColorB(128, 255, 128, 255));

			   DebugDrawLocation(
			   entityLoc,
			   ColorB(0, 0, 255, 255),
			   ColorB(128, 128, 255, 255),
			   ColorB(128, 128, 255, 255),
			   ColorB(128, 128, 255, 255));
			 */
		}

		posAdjust.posAdjuster.m_targetTime -= posAdjust.posAdjuster.m_lastTime;
		posAdjust.posAdjuster.m_lastTime = 0;
		posAdjust.posAdjuster.m_invalid = false;
	}

	void UpdateCollisionCheck(float timePassed)
	{
		AABB totalAABB(AABB::RESET);

		const uint32 numAdjusts = m_posAdjusts.size();
		for (uint32 i = 0; i < numAdjusts; i++)
		{
			IScope* pScope = m_actionController->GetScope(m_posAdjusts[i].scopeID);
			AABB targetAABB;
			pScope->GetEntity().GetWorldBounds(targetAABB);

			totalAABB.Add(targetAABB);
		}
		if (!m_slaveScopeIds.empty())
		{
			std::vector<CProceduralContext_AdjustPos*> slaveProceduralContexts;

			int numSlaves = m_slaveScopeIds.size();

			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawAABB(totalAABB, false, ColorB(0, 255, 0, 255), eBBD_Faceted);

			for (int i = 0; i < numSlaves; i++)
			{
				if (IScope* pSlaveScope = m_actionController->GetScope(m_slaveScopeIds[i]))
				{
					if (IActionController* pActionController = pSlaveScope->GetEnslavedActionController())
					{
						if (IEntity* pSlaveEntity = &(pActionController->GetEntity()))
						{
							if (CProceduralContext_AdjustPos* pSlaveContext = static_cast<CProceduralContext_AdjustPos*>(pActionController->FindProceduralContext(CProceduralContext_AdjustPos::GetCID())))
							{
								slaveProceduralContexts.push_back(pSlaveContext);

								AABB targetAABB;
								pSlaveEntity->GetWorldBounds(targetAABB);
								totalAABB.Add(targetAABB);

								//gEnv->pRenderer->GetIRenderAuxGeom()->DrawAABB(targetAABB, false, ColorB(255, 0, 0, 255), eBBD_Faceted);
							}
						}
					}
				}
			}

			if (!slaveProceduralContexts.empty())
			{
				Vec3 totalSize = totalAABB.GetSize();
				Vec3 totalCentre = totalAABB.GetCenter();

				//gEnv->pRenderer->GetIRenderAuxGeom()->DrawAABB(totalAABB, false, ColorB(0, 0, 255, 255), eBBD_Faceted);

				totalSize *= 0.5f;

				primitives::box physPrimitive;
				physPrimitive.Basis.SetIdentity();

				physPrimitive.center = Vec3(totalCentre.x, totalCentre.y, totalCentre.z + m_collisionAdjust.heightOffset);
				physPrimitive.size = Vec3(0.0f, totalSize.y * m_collisionAdjust.widthMult, totalSize.z * m_collisionAdjust.heightMult);
				physPrimitive.bOriented = false;

				IPhysicalWorld::SPWIParams intersectionParams;

				intersectionParams.itype = physPrimitive.type;
				intersectionParams.pprim = &physPrimitive;
				intersectionParams.entTypes = (ent_all & ~ent_living) | ent_ignore_noncolliding;
				intersectionParams.collclass = SCollisionClass(0, collision_class_living | collision_class_articulated);
				intersectionParams.geomFlagsAll = 0;
				intersectionParams.geomFlagsAny = geom_colltype0 | geom_colltype_player;

				intersectionParams.sweepDir = Vec3(totalSize.x, 0.f, 0.f);
				float distanceXForward = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(intersectionParams);

				intersectionParams.sweepDir.x = -totalSize.x;
				float distanceXBack = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(intersectionParams);

				physPrimitive.size = Vec3(totalSize.y * m_collisionAdjust.widthMult, 0.0f, totalSize.z * m_collisionAdjust.heightMult);
				intersectionParams.sweepDir = Vec3(0.f, totalSize.y, 0.f);
				float distanceYForward = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(intersectionParams);

				intersectionParams.sweepDir.y = -totalSize.y;
				float distanceYBack = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(intersectionParams);

				Vec3 move(ZERO);

				bool distanceXForwardValid = (distanceXForward > 0.f) && (distanceXForward < totalSize.x);
				bool distanceXBackValid = (distanceXBack > 0.f) && (distanceXBack < totalSize.x);
				bool distanceYForwardValid = (distanceYForward > 0.f) && (distanceYForward < totalSize.y);
				bool distanceYBackValid = (distanceYBack > 0.f) && (distanceYBack < totalSize.y);

				float maxFrameMove = m_collisionAdjust.maxAdjustmentSpeed * timePassed;

				if (distanceXForwardValid != distanceXBackValid)
				{
					float xMove = distanceXForwardValid ? (distanceXForward - totalSize.x) : (totalSize.x - distanceXBack);
					move.x = CLAMP(xMove, -maxFrameMove, maxFrameMove);
				}

				if (distanceYForwardValid != distanceYBackValid)
				{
					float yMove = distanceYForwardValid ? (distanceYForward - totalSize.y) : (totalSize.y - distanceYBack);
					move.y = CLAMP(yMove, -maxFrameMove, maxFrameMove);
				}

				if (!move.IsZero())
				{
					AdjustTargetLocation(move);

					int numSlaveContexts = slaveProceduralContexts.size();

					for (int i = 0; i < numSlaveContexts; i++)
					{
						slaveProceduralContexts[i]->AdjustTargetLocation(move);
					}
				}
			}
		}
	}

	SPosAdjust& GetOrAddPosAdjustForScope(int scopeID)
	{
		if (SPosAdjust* pPosAdjust = GetPosAdjustForScope(scopeID))
		{
			return *pPosAdjust;
		}

		const uint32 numAdjusts = m_posAdjusts.size();
		m_posAdjusts.resize(numAdjusts + 1);
		m_posAdjusts[numAdjusts].scopeID = scopeID;

		return m_posAdjusts[numAdjusts];
	}

	SPosAdjust* GetPosAdjustForScope(int scopeID)
	{
		const uint32 numAdjusts = m_posAdjusts.size();
		for (uint32 i = 0; i < numAdjusts; i++)
		{
			if (m_posAdjusts[i].scopeID == scopeID)
			{
				return &m_posAdjusts[i];
			}
		}

		return NULL;
	}

	std::vector<SPosAdjust> m_posAdjusts;
	SCollisionAdjustParams  m_collisionAdjust;
	std::vector<int>        m_slaveScopeIds;
	bool                    m_enabled;
	bool                    m_collisionCheck;
};

CProceduralContext_AdjustPos::CProceduralContext_AdjustPos()
	: m_enabled(false)
	, m_collisionCheck(false)
{
}

CRYREGISTER_CLASS(CProceduralContext_AdjustPos);

class CProceduralClipPosAdjust : public TProceduralClip<SPositionAdjustParams>
{
public:
	CProceduralClipPosAdjust()
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SPositionAdjustParams& params)
	{
		m_posAdjuster.Init(m_scope->GetEntity(), blendTime);

		const QuatT entityLoc(m_entity->GetWorldPos(), m_entity->GetWorldRotation());

		CAnimation* anim = m_scope->GetTopAnim(0);
		if (anim)
		{
			anim->SetStaticFlag(CA_FULL_ROOT_PRIORITY);
		}

		const bool isRootEntity = IsRootEntity();
		const bool hasParam = GetParam("TargetPos", m_posAdjuster.m_targetLoc);

		if (isRootEntity && hasParam)
		{
			if (params.ignorePosition)
			{
				m_posAdjuster.m_delta.zero();
			}
			else
			{
				Vec3 actualOffset = m_posAdjuster.m_targetLoc.t - entityLoc.t;
				Vec3 idealOffset = m_posAdjuster.m_targetLoc.q * params.idealOffset;
				m_posAdjuster.m_delta = actualOffset - idealOffset;
			}

			Quat idealRotOffset;
			idealRotOffset.SetRotationAA(params.ignoreRotation ? 0.0f : DEG2RAD(params.idealYaw), Vec3Constants<f32>::fVec3_OneZ);
			Quat actualRotOffset = m_posAdjuster.m_targetLoc.q * !entityLoc.q;
			m_posAdjuster.m_deltaRot = actualRotOffset * !idealRotOffset;

			//CryLog("Init (%f, %f, %f) Target  (%f, %f, %f) Delta: (%f, %f, %f)", entityLoc.t.x, entityLoc.t.y, entityLoc.t.z, m_posAdjuster.m_targetLoc.t.x, m_posAdjuster.m_targetLoc.t.y, m_posAdjuster.m_targetLoc.t.z, m_posAdjuster.m_delta.x, m_posAdjuster.m_delta.y, m_posAdjuster.m_delta.z);
		}
		else
		{
			m_posAdjuster.m_targetLoc.t = entityLoc.t;
			m_posAdjuster.m_targetLoc.q = entityLoc.q;
			m_posAdjuster.m_invalid = true;

			if (!hasParam)
			{
				CryLog("Failed to init PositionAdjust due to missing parameter: TargetPos");
			}
		}
	}

	virtual void OnExit(float blendTime)
	{
	}

	virtual void Update(float timePassed)
	{
		m_posAdjuster.Update(*m_entity, timePassed);
	}

protected:
	SPositionAdjuster m_posAdjuster;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipPosAdjust, "PositionAdjust");

struct SPositionAdjustAnimParams : public IProceduralParams
{
	SPositionAdjustAnimParams()
		: ignoreRotation(false)
		, ignorePosition(false)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(ignoreRotation, "IgnoreRotation", "Ignore Rotation");
		ar(ignorePosition, "IgnorePosition", "Ignore Position");
	}

	bool ignoreRotation;
	bool ignorePosition;
};

struct SPositionAdjustWithCollisionCheck : public SPositionAdjustAnimParams
{
	SPositionAdjustWithCollisionCheck()
		: collisioncheck(false)
		, updateLocation(false)
	{
		collisionParams.maxAdjustmentSpeed = 3.f;
		collisionParams.heightOffset = 0.2f;
		collisionParams.heightMult = 0.4f;
		collisionParams.widthMult = 0.5f;
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(paramName, "ParamName", "Param Name");
		SPositionAdjustAnimParams::Serialize(ar);
		ar(collisioncheck, "CollisionCheck", "Collision Check");
		ar(collisionParams, "CollisionParams", "Collision Params");
		ar(updateLocation, "UpdateLocation", "Update Location");
	}

	TProcClipString        paramName;
	bool                   collisioncheck;
	SCollisionAdjustParams collisionParams;

	bool                   updateLocation;
};

class CProceduralClipPosAdjustAnimPos : public TProceduralContextualClip<CProceduralContext_AdjustPos, SPositionAdjustWithCollisionCheck>
{
public:
	CProceduralClipPosAdjustAnimPos()
		: m_paramNameCRC(0)
		, m_originalLocation(IDENTITY)
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SPositionAdjustWithCollisionCheck& params)
	{
		const bool isRootEntity = IsRootEntity();

		if (isRootEntity)
		{
			QuatT targetLocation(IDENTITY);

			const char* pParamName = params.paramName.empty() ? "TargetPos" : params.paramName.c_str();

			uint32 paramNameCRC = CCrc32::ComputeLowercase(pParamName);

			if (GetParam(paramNameCRC, targetLocation))
			{
				if (params.updateLocation)
				{
					m_paramNameCRC = paramNameCRC;
					m_originalLocation = targetLocation;
				}

				m_context->StartPosAdjust(targetLocation, blendTime, params.ignorePosition, params.ignoreRotation, m_scope);

				if (params.collisioncheck)
				{
					m_context->StartCollisionCheck(params.collisionParams, GetActionInstalledScopeMask());
				}
			}
			else
			{
				CryLog("Failed to init PositionAdjust due to missing parameter: %s", pParamName);
			}
		}
	}

	virtual void OnExit(float blendTime)
	{
		m_context->EndPosAdjust(m_scope);
	}

	virtual void Update(float timePassed)
	{
		if (m_paramNameCRC != 0)
		{
			QuatT targetLocation(IDENTITY);
			if (GetParam(m_paramNameCRC, targetLocation))
			{
				if ((m_originalLocation.q != targetLocation.q) || (m_originalLocation.t != targetLocation.t))
				{
					m_originalLocation = targetLocation;
					m_context->UpdatePosAdjust(targetLocation, m_scope);
				}
			}
		}
	}

private:
	uint32 m_paramNameCRC;
	QuatT  m_originalLocation;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipPosAdjustAnimPos, "PositionAdjustAnimPos");

class CProceduralClipPosAdjustAnimPosContinuously : public TProceduralClip<>
{
public:
	CProceduralClipPosAdjustAnimPosContinuously()
	{
	}

	virtual void OnEnter(float blendTime, float duration, const TParamsType& params)
	{
		m_posAdjuster.Init(m_scope->GetEntity(), blendTime);

		CAnimation* anim = m_scope->GetTopAnim(0);
		if (anim)
		{
			anim->SetStaticFlag(CA_FULL_ROOT_PRIORITY);
		}

		QuatT animStartLoc(IDENTITY);
		const bool hasStartLocation = (anim && m_charInstance->GetIAnimationSet()->GetAnimationDCCWorldSpaceLocation(anim, animStartLoc, m_charInstance->GetIDefaultSkeleton().GetControllerIDByID(0)));
		const bool isRootEntity = IsRootEntity();
		const bool hasParam = GetParam("TargetPos", m_posAdjuster.m_targetLoc);

		if (isRootEntity && hasParam && hasStartLocation)
		{
			UpdateDeltas(animStartLoc);
		}
		else
		{
			m_posAdjuster.m_targetLoc.t = m_entity->GetPos();
			m_posAdjuster.m_targetLoc.q = m_entity->GetRotation();
			m_posAdjuster.m_invalid = true;

			if (!hasParam)
			{
				CryLog("Failed to init PositionAdjust due to missing parameter: TargetPos");
			}
		}
	}

	virtual void OnExit(float blendTime)
	{
	}

	virtual void Update(float timePassed)
	{
		if (m_posAdjuster.m_lastTime > 0)
		{
			CAnimation* anim = m_scope->GetTopAnim(0);
			QuatT animStartLoc(IDENTITY);
			const bool hasStartLocation = (anim && m_charInstance->GetIAnimationSet()->GetAnimationDCCWorldSpaceLocation(anim, animStartLoc, m_charInstance->GetIDefaultSkeleton().GetControllerIDByID(0)));

			if (hasStartLocation)
			{
				UpdateDeltas(animStartLoc);

				m_posAdjuster.m_targetTime -= m_posAdjuster.m_lastTime;
				m_posAdjuster.m_lastTime = 0;
			}
		}

		m_posAdjuster.Update(*m_entity, timePassed);
	}

	void UpdateDeltas(QuatT& animStartLoc)
	{
		IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(m_entity->GetId());
		IAnimatedCharacter* pAnimatedCharacter = pActor ? pActor->GetAnimatedCharacter() : NULL;
		const Vec3 vExpectedMovement = pAnimatedCharacter ? pAnimatedCharacter->GetExpectedEntMovement() : Vec3Constants<float>::fVec3_Zero;

		const QuatT entityLoc(m_entity->GetPos() + vExpectedMovement, m_entity->GetRotation());

		Vec3 actualOffset = m_posAdjuster.m_targetLoc.t - entityLoc.t;
		Vec3 idealOffset = m_posAdjuster.m_targetLoc.q * -animStartLoc.t;
		m_posAdjuster.m_delta = actualOffset - idealOffset;
		Quat actualRotOffset = !entityLoc.q * m_posAdjuster.m_targetLoc.q;
		m_posAdjuster.m_deltaRot = actualRotOffset * animStartLoc.q;

		//CryLog("Init %s (%f, %f, %f) Target  (%f, %f, %f) Delta: (%f, %f, %f)", m_scope->GetEntity().GetName(), entityLoc.t.x, entityLoc.t.y, entityLoc.t.z, m_posAdjuster.m_targetLoc.t.x, m_posAdjuster.m_targetLoc.t.y, m_posAdjuster.m_targetLoc.t.z, m_posAdjuster.m_delta.x, m_posAdjuster.m_delta.y, m_posAdjuster.m_delta.z);

		/*DebugDrawLocation(
		   entityLoc,
		   RGBA8(0x30,0x30,0x30,0xff),
		   RGBA8(0x50,0x30,0x30,0xff),
		   RGBA8(0x30,0x50,0x30,0xff),
		   RGBA8(0x30,0x30,0x50,0xff));

		   const QuatT worldLoc = entityLoc * animStartLoc.GetInverted();

		   DebugDrawLocation(
		   worldLoc,
		   RGBA8(0x80,0x80,0x80,0xff),
		   RGBA8(0xb0,0x80,0x80,0xff),
		   RGBA8(0x80,0xb0,0x80,0xff),
		   RGBA8(0x80,0x80,0xb0,0xff));
		 */
	}
protected:
	SPositionAdjuster m_posAdjuster;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipPosAdjustAnimPosContinuously, "PositionAdjustAnimPosContinuously");

class CProceduralClipPosAdjustAlignEnd : public TProceduralClip<SPositionAdjustAnimParams>
{
public:
	CProceduralClipPosAdjustAlignEnd();

	virtual void OnEnter(float blendTime, float duration, const SPositionAdjustAnimParams& params)
	{
		m_posAdjuster.Init(m_scope->GetEntity(), blendTime);

		CAnimation* anim = m_scope->GetTopAnim(0);

		QuatT animStartLoc(IDENTITY);
		const bool isRootEntity = IsRootEntity();
		const bool hasParam = GetParam("TargetPos", m_posAdjuster.m_targetLoc);
		m_targetAccuracy = 0.0f;
		GetParam("TargetAccuracy", m_targetAccuracy);

		if (!isRootEntity || !hasParam)
		{
			m_posAdjuster.m_targetLoc.t = m_entity->GetPos();
			m_posAdjuster.m_targetLoc.q = m_entity->GetRotation();
			m_posAdjuster.m_invalid = true;

			if (!hasParam)
			{
				CryLog("Failed to init PositionAdjust due to missing parameter: TargetPos");
			}
		}
	}

	virtual void OnExit(float blendTime)
	{
	}

	virtual void Update(float timePassed)
	{
		{
			UpdateDeltas();

			m_posAdjuster.m_targetTime -= m_posAdjuster.m_lastTime;
			m_posAdjuster.m_lastTime = 0;
		}

		m_posAdjuster.Update(*m_entity, timePassed);
	}

	void UpdateDeltas()
	{
		const CAnimation* pAnim = m_scope->GetTopAnim(0);

		if (pAnim && (pAnim->GetTransitionWeight() > 0.0f))
		{
			QuatT curLoc, endLoc;
			float curANTime = m_charInstance->GetISkeletonAnim()->GetAnimationNormalizedTime(pAnim);

			// TODO Jean: hook this up when CryAnimation integration is done
			//m_charInstance->GetISkeletonAnim().CalculateAnimationLocation(curLoc, pAnim, curANTime);
			//m_charInstance->GetISkeletonAnim().CalculateAnimationLocation(endLoc, pAnim, 1.0f);

			QuatT pendingMovement = endLoc * curLoc.GetInverted();
			QuatT entLoc(m_entity->GetPos(), m_entity->GetRotation());
			QuatT expectedLoc = (entLoc * pendingMovement);
			QuatT requiredDelta;
			requiredDelta.q = expectedLoc.q.GetInverted() * m_posAdjuster.m_targetLoc.q;
			requiredDelta.t = m_posAdjuster.m_targetLoc.t - expectedLoc.t;

			if (GetParams().ignorePosition == 0.0f)
			{
				float distance = requiredDelta.t.GetLength();
				if (distance > FLT_EPSILON)
				{
					float adjustedDist = max(distance - m_targetAccuracy, 0.0f);
					m_posAdjuster.m_delta = requiredDelta.t * (adjustedDist / distance);
				}
				else
				{
					m_posAdjuster.m_delta.Set(0.0f, 0.0f, 0.0f);
				}
			}
			if (GetParams().ignoreRotation == 0.0f)
			{
				m_posAdjuster.m_deltaRot = requiredDelta.q;
			}

			static bool showDebug = false;

			if (showDebug)
			{
				float fWhite[] = { 1.0f, 1.0f, 1.0f, 1.0f };
				static float xPos = 400.0f;
				float yPos = 50.0f;
				IRenderAuxText::Draw2dLabel(xPos, yPos, 1.5f, fWhite, false, "Target: (%f %f %f)", m_posAdjuster.m_targetLoc.t.x, m_posAdjuster.m_targetLoc.t.y, m_posAdjuster.m_targetLoc.t.z);
				yPos += 20.0f;
				IRenderAuxText::Draw2dLabel(xPos, yPos, 1.5f, fWhite, false, "Current: (%f %f %f)", entLoc.t.x, entLoc.t.y, entLoc.t.z);
				yPos += 20.0f;
				IRenderAuxText::Draw2dLabel(xPos, yPos, 1.5f, fWhite, false, "Anim Remainder: (%f %f %f)", pendingMovement.t.x, pendingMovement.t.y, pendingMovement.t.z);
				yPos += 20.0f;
				IRenderAuxText::Draw2dLabel(xPos, yPos, 1.5f, fWhite, false, "Delta: (%f %f %f)", requiredDelta.t.x, requiredDelta.t.y, requiredDelta.t.z);
				yPos += 20.0f;
				IRenderAuxText::Draw2dLabel(xPos, yPos, 1.5f, fWhite, false, "CNT: %f tgtTime: %f", curANTime, m_posAdjuster.m_targetTime);

				DebugDrawLocation(
				  expectedLoc,
				  RGBA8(0x30, 0x30, 0x30, 0xff),
				  RGBA8(0x50, 0x30, 0x30, 0xff),
				  RGBA8(0x30, 0x50, 0x30, 0xff),
				  RGBA8(0x30, 0x30, 0x50, 0xff));

				DebugDrawLocation(
				  m_posAdjuster.m_targetLoc,
				  RGBA8(0x80, 0x80, 0x80, 0xff),
				  RGBA8(0xb0, 0x80, 0x80, 0xff),
				  RGBA8(0x80, 0xb0, 0x80, 0xff),
				  RGBA8(0x80, 0x80, 0xb0, 0xff));
			}

		}

		//CryLog("Init %s (%f, %f, %f) Target  (%f, %f, %f) Delta: (%f, %f, %f)", m_scope->GetEntity().GetName(), entityLoc.t.x, entityLoc.t.y, entityLoc.t.z, m_posAdjuster.m_targetLoc.t.x, m_posAdjuster.m_targetLoc.t.y, m_posAdjuster.m_targetLoc.t.z, m_posAdjuster.m_delta.x, m_posAdjuster.m_delta.y, m_posAdjuster.m_delta.z);
	}
protected:
	SPositionAdjuster m_posAdjuster;
	float             m_targetAccuracy;
};

CProceduralClipPosAdjustAlignEnd::CProceduralClipPosAdjustAlignEnd() : m_targetAccuracy(0.0f)
{
}

// TODO Jean: Re-activate
// REGISTER_PROCEDURAL_CLIP(CProceduralClipPosAdjustAlignEnd, "PositionAdjustAlignEnd");

class CProceduralClipPosAdjustTargetLocator : public TProceduralClip<SProceduralClipPosAdjustTargetLocatorParams>
{
public:
	CProceduralClipPosAdjustTargetLocator()
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SProceduralClipPosAdjustTargetLocatorParams& params)
	{
		m_posAdjuster.Init(m_scope->GetEntity(), blendTime);

		const SControllerDef& contDef = m_scope->GetActionController().GetContext().controllerDef;
		int scopeID = contDef.m_scopeIDs.Find(params.targetScopeName.crc);
		IScope* scope = (scopeID >= 0) ? m_scope->GetActionController().GetScope(scopeID) : NULL;

		QuatT targetPos(IDENTITY);
		bool hasParam = false;
		if (scope && scope->GetCharInst())
		{
			targetPos.t = scope->GetEntity().GetPos();
			targetPos.q = scope->GetEntity().GetRotation();

			ISkeletonPose& skelPose = *scope->GetCharInst()->GetISkeletonPose();
			IDefaultSkeleton& rIDefaultSkeleton = scope->GetCharInst()->GetIDefaultSkeleton();
			int jointID = rIDefaultSkeleton.GetJointIDByName(params.targetJointName.c_str());
			if (jointID >= 0)
			{
				targetPos = targetPos * skelPose.GetAbsJointByID(jointID);
				hasParam = true;
			}
		}

		m_posAdjuster.m_targetLoc = targetPos;

		CAnimation* anim = m_scope->GetTopAnim(0);
		if (anim)
		{
			anim->SetStaticFlag(CA_FULL_ROOT_PRIORITY);
		}

		const QuatT entityLoc(m_entity->GetPos(), m_entity->GetRotation());

		const bool isRootEntity = IsRootEntity();

		if (isRootEntity && hasParam)
		{
			Vec3 actualOffset = m_posAdjuster.m_targetLoc.t - entityLoc.t;
			m_posAdjuster.m_delta = actualOffset;
			Quat actualRotOffset = m_posAdjuster.m_targetLoc.q * !entityLoc.q;
			m_posAdjuster.m_deltaRot = actualRotOffset;

			//CryLog("Init %s (%f, %f, %f) Target  (%f, %f, %f) Delta: (%f, %f, %f)", m_scope->GetEntity().GetName(), entityLoc.t.x, entityLoc.t.y, entityLoc.t.z, m_posAdjuster.m_targetLoc.t.x, m_posAdjuster.m_targetLoc.t.y, m_posAdjuster.m_targetLoc.t.z, m_posAdjuster.m_delta.x, m_posAdjuster.m_delta.y, m_posAdjuster.m_delta.z);
		}
		else
		{
			m_posAdjuster.m_targetLoc.t = m_entity->GetPos();
			m_posAdjuster.m_targetLoc.q = m_entity->GetRotation();
			m_posAdjuster.m_invalid = true;

			if (!hasParam)
			{
				CryLog("Failed to init PositionAdjust due to missing target object or joint: %s", params.targetJointName.c_str());
			}
		}
	}

	virtual void OnExit(float blendTime)
	{
	}

	virtual void Update(float timePassed)
	{
		m_posAdjuster.Update(*m_entity, timePassed);
	}

protected:
	SPositionAdjuster m_posAdjuster;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipPosAdjustTargetLocator, "PositionAdjustTargetLocator");
