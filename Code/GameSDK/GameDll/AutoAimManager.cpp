// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

20:11:2009 - Benito G.R.
*************************************************************************/

#include "StdAfx.h"
#include <CryAnimation/ICryAnimation.h>
#include "Game.h"
#include "AutoAimManager.h"
#include "GameRules.h"
#include <CryAISystem/IAIObject.h>
#include "Actor.h"
#include <CryAISystem/IFactionMap.h>

CAutoAimManager::CAutoAimManager()
: m_closeCombatSnapTargetId(0)
, m_closeCombatSnapTargetRange(0.f)
, m_closeCombatSnapTargetMoveSpeed(0.f)
, m_localPlayerFaction(IFactionMap::InvalidFactionID)
{
	CRY_ASSERT(kMaxAutoaimTargets > 0);
	
	m_autoaimTargets.reserve(kMaxAutoaimTargets);

#if DEBUG_AUTOAIM_MANAGER
	REGISTER_CVAR(g_autoAimManagerDebug, 0, 0, "Debug auto aim manager");
#endif
}

CAutoAimManager::~CAutoAimManager()
{
#if DEBUG_AUTOAIM_MANAGER
	if (gEnv->pConsole)
	{
		gEnv->pConsole->UnregisterVariable("g_autoAimManagerDebug", true);
	}
#endif
}

void CAutoAimManager::Update(float dt)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	const int numAutoaimTargets = m_autoaimTargets.size();

	//Update the autoaim positions for the entities
	for(int i = 0; i < numAutoaimTargets; i++)
	{
		UpdateTargetInfo(m_autoaimTargets[i], dt);
	}

#if DEBUG_AUTOAIM_MANAGER
	if (g_autoAimManagerDebug)
	{
		DebugDraw();
	}
#endif
}

void CAutoAimManager::UpdateTargetInfo(SAutoaimTarget& aaTarget, float fFrameTime)
{
	IEntity * pTargetEntity = gEnv->pEntitySystem->GetEntity(aaTarget.entityId);
	if(pTargetEntity)
	{
		CActorPtr pTargetActor = aaTarget.pActorWeak.lock();
		if (pTargetActor)
		{
			Vec3 characterPos;
			Quat characterRot;

			//Need this because of decouple catch-up movement 
			if (IAnimatedCharacter* pAnimatedCharacter = pTargetActor->GetAnimatedCharacter())
			{
				const QuatT& animationLocation = pAnimatedCharacter->GetAnimLocation();
				characterPos = animationLocation.t;
				characterRot = animationLocation.q;
			}
			else
			{
				const Matrix34& targetWorldTM = pTargetEntity->GetWorldTM();

				//Fallback to entity position
				characterPos = targetWorldTM.GetTranslation();
				characterRot = Quat(targetWorldTM);
			}

			Vec3 primaryOffset(0.0f, 0.0f, aaTarget.fallbackOffset);
			Vec3 secondaryOffset(0.0f, 0.0f, aaTarget.fallbackOffset);

			if (aaTarget.primaryBoneId >= 0)
			{
				if (pTargetActor->HasBoneID(aaTarget.primaryBoneId))
				{
					primaryOffset = pTargetActor->GetBoneTransform(aaTarget.primaryBoneId).t;
				}
				else
				{		
					GameWarning("CAutoAimManager: Character %s missing primary boneID: %s", pTargetEntity->GetName(), s_BONE_ID_NAME[aaTarget.primaryBoneId]);
					aaTarget.primaryBoneId = -1;
				}
			}
			if (aaTarget.secondaryBoneId >= 0)
			{
				if (pTargetActor->HasBoneID(aaTarget.secondaryBoneId))
				{
					secondaryOffset = pTargetActor->GetBoneTransform(aaTarget.secondaryBoneId).t;
				}
				else
				{		
					GameWarning("CAutoAimManager: Character %s missing secondary boneID: %s", pTargetEntity->GetName(), s_BONE_ID_NAME[aaTarget.secondaryBoneId]);
					aaTarget.secondaryBoneId = -1;
				}
			}

			aaTarget.primaryAimPosition = characterPos + (characterRot * primaryOffset);
			aaTarget.secondaryAimPosition = characterPos + (characterRot * secondaryOffset);

			//Update hostility (changes during gameplay)
			if (!gEnv->bMultiplayer)
			{
				if (gEnv->pAISystem->GetFactionMap().GetReaction(GetLocalPlayerFaction(), GetTargetFaction(*pTargetEntity)) == IFactionMap::Hostile)
				{
					aaTarget.SetFlag(eAATF_AIHostile);
				}
				else
				{
					aaTarget.RemoveFlag(eAATF_AIHostile);
				}
			}
		}
		else if(aaTarget.hasSkeleton)
		{
			//Not an actor but has a skeleton (and so can use bone offsets)
			ISkeletonPose* pSkeletonPose = pTargetEntity->GetCharacter(0)->GetISkeletonPose();

			const Matrix34& characterMat = pTargetEntity->GetWorldTM();
			const Vec3 characterPos = characterMat.GetTranslation();
			const Quat characterRot(characterMat);

			Vec3 primaryOffset(0.0f, 0.0f, aaTarget.fallbackOffset);
			Vec3 secondaryOffset(0.0f, 0.0f, aaTarget.fallbackOffset);

			if (aaTarget.primaryBoneId >= 0)
			{
				primaryOffset = pSkeletonPose->GetAbsJointByID(aaTarget.primaryBoneId).t;
			}
			if (aaTarget.secondaryBoneId >= 0)
			{
				secondaryOffset = pSkeletonPose->GetAbsJointByID(aaTarget.secondaryBoneId).t;
			}

			aaTarget.primaryAimPosition = characterPos + (characterRot * primaryOffset);
			aaTarget.secondaryAimPosition = characterPos + (characterRot * secondaryOffset);
		}
		else
		{
			//Must be an object
			const Matrix34& entityWorldTM = pTargetEntity->GetWorldTM(); 
			Vec3 primaryPosition = entityWorldTM.GetTranslation();
			Vec3 secondaryPosition = entityWorldTM.TransformPoint(Vec3(0.0f, 0.0f, 0.5f));

			AABB entityLocalBBox;
			pTargetEntity->GetLocalBounds(entityLocalBBox);
			if (!entityLocalBBox.IsEmpty())
			{
				const Vec3 offset (0.0f, 0.0f, entityLocalBBox.GetRadius() * 0.2f);
				const Vec3 objectCenter = entityLocalBBox.GetCenter();
				primaryPosition = entityWorldTM.TransformPoint((objectCenter - offset));
				secondaryPosition = entityWorldTM.TransformPoint((objectCenter + offset));
			}

			aaTarget.primaryAimPosition = primaryPosition;
			aaTarget.secondaryAimPosition = secondaryPosition;
		}

		//The physics drags the render proxy and entity behind it. If we auto aim at the render position,
		//	we will handicap the console players by failing to let them aim ahead of the target.
		if(IPhysicalEntity * pPhysicalEntity = pTargetEntity->GetPhysics())
		{
			pe_status_dynamics dyn;
			if(pPhysicalEntity->GetStatus(&dyn))
			{
				Vec3 lookAhead = (dyn.v * fFrameTime);
				aaTarget.primaryAimPosition = aaTarget.primaryAimPosition + lookAhead;
				aaTarget.secondaryAimPosition = aaTarget.secondaryAimPosition + lookAhead;
			}
		}
	}
}

bool CAutoAimManager::RegisterAutoaimTargetActor(const CActor& targetActor, const SAutoaimTargetRegisterParams& registerParams)
{
	if (IsEntityRegistered(targetActor.GetEntityId()))
	{
		GameWarning("Trying to register entity more than once in auto aim manager!");
		return false;
	}

	if (!IsSpaceAvailable())
	{
		if (gEnv->IsEditor() == false)
		{
			GameWarning("Maximum number of entities reached for auto aim manager (%d)!", kMaxAutoaimTargets);
		}
		return false;
	}

	RegisterCharacterTargetInfo(targetActor, registerParams);
	return true;

}

bool CAutoAimManager::RegisterAutoaimTargetObject(const EntityId targetObjectId, const SAutoaimTargetRegisterParams& registerParams)
{
	if (IsEntityRegistered(targetObjectId))
	{
		GameWarning("Trying to register entity more than once in auto aim manager!");
		return false;
	}

	if (!IsSpaceAvailable())
	{
		GameWarning("Maximum number of entities reached for auto aim manager (%d)!", kMaxAutoaimTargets);
		return false;
	}

	RegisterObjectTargetInfo(targetObjectId, registerParams);
	return true;
}

void CAutoAimManager::RegisterCharacterTargetInfo(const CActor& targetActor, const SAutoaimTargetRegisterParams& registerParams)
{
	SAutoaimTarget aimTarget;
	aimTarget.entityId = targetActor.GetEntityId();
	aimTarget.pActorWeak = targetActor.GetWeakPtr();
	aimTarget.fallbackOffset = registerParams.fallbackOffset;
	aimTarget.primaryBoneId = registerParams.primaryBoneId;
	aimTarget.physicsBoneId = registerParams.physicsBoneId;
	aimTarget.secondaryBoneId = registerParams.secondaryBoneId;
	aimTarget.innerRadius = registerParams.innerRadius;
	aimTarget.outerRadius = registerParams.outerRadius;
	aimTarget.snapRadius = registerParams.snapRadius;
	aimTarget.snapRadiusTagged = registerParams.snapRadiusTagged;

	if (!gEnv->bMultiplayer)
	{
		IEntity* pTargetEntity = targetActor.GetEntity();

		//Instance properties, other stuff could be added here easily (grab enemy, sliding hit, etc)
		SmartScriptTable props;
		SmartScriptTable propsPlayerInteractions;
		IScriptTable* pScriptTable = pTargetEntity->GetScriptTable();
		if (pScriptTable && pScriptTable->GetValue("Properties", props))
		{
			if (props->GetValue("PlayerInteractions", propsPlayerInteractions))
			{
				int stealhKill = 0;
				if (propsPlayerInteractions->GetValue("bStealthKill", stealhKill) && (stealhKill != 0))
				{
					aimTarget.SetFlag(eAATF_StealthKillable);
				}
				int canBeGrabbed = 0;
				if (propsPlayerInteractions->GetValue("bCanBeGrabbed", canBeGrabbed) && (canBeGrabbed != 0))
				{
					aimTarget.SetFlag(eAATF_CanBeGrabbed);
				}
			}
		}
	}

	m_autoaimTargets.push_back(aimTarget);
}

void CAutoAimManager::RegisterObjectTargetInfo(const EntityId targetObjectId, const SAutoaimTargetRegisterParams& registerParams)
{
	SAutoaimTarget aimTarget;
	aimTarget.entityId = targetObjectId;
	aimTarget.pActorWeak.reset();
	aimTarget.fallbackOffset = registerParams.fallbackOffset;
	aimTarget.primaryBoneId = registerParams.primaryBoneId;
	aimTarget.physicsBoneId = registerParams.physicsBoneId;
	aimTarget.secondaryBoneId = registerParams.secondaryBoneId;
	aimTarget.innerRadius = registerParams.innerRadius;
	aimTarget.outerRadius = registerParams.outerRadius;
	aimTarget.snapRadius = registerParams.snapRadius;
	aimTarget.snapRadiusTagged = registerParams.snapRadiusTagged;
	aimTarget.SetFlag(eAATF_AIHostile);		//Not very nice, but will allow the player to auto-target it
	aimTarget.hasSkeleton = registerParams.hasSkeleton;

	m_autoaimTargets.push_back(aimTarget);
}

void CAutoAimManager::UnregisterAutoaimTarget(const EntityId entityId)
{	
	TAutoaimTargets::iterator it = m_autoaimTargets.begin();
	TAutoaimTargets::iterator end = m_autoaimTargets.end();

	for (; it != end; ++it)
	{
		if (it->entityId == entityId)
		{
			it->pActorWeak.reset();
			m_autoaimTargets.erase(it);
			return;
		}
	}

	GameWarning("Trying to unregister non registered entity from autoaim manager");
}

bool CAutoAimManager::IsEntityRegistered(EntityId entityId) const
{
	const int numAutoaimTargets = (int)m_autoaimTargets.size();

	for(int i = 0; i < numAutoaimTargets; i++)
	{
		if(m_autoaimTargets[i].entityId == entityId)
		{
			return true;
		}
	}

	return false;
}

bool CAutoAimManager::IsSpaceAvailable() const
{
	return (m_autoaimTargets.size() < kMaxAutoaimTargets);
}

const SAutoaimTarget* CAutoAimManager::GetTargetInfo( EntityId targetId ) const
{
	const int numAutoaimTargets = (int)m_autoaimTargets.size();
		
	int currentTarget = 0;
	while((currentTarget < numAutoaimTargets) && (m_autoaimTargets[currentTarget].entityId != targetId))
	{
		currentTarget++;
	}

	return (currentTarget < numAutoaimTargets) ? &(m_autoaimTargets[currentTarget]) : NULL;
}

SAutoaimTarget* CAutoAimManager::GetTargetInfoInternal(EntityId targetId)
{
	const int numAutoaimTargets = (int)m_autoaimTargets.size();

	int currentTarget = 0;
	while((currentTarget < numAutoaimTargets) && (m_autoaimTargets[currentTarget].entityId != targetId))
	{
		currentTarget++;
	}

	return (currentTarget < numAutoaimTargets) ? &(m_autoaimTargets[currentTarget]) : NULL;
}

bool CAutoAimManager::SetTargetTagged( EntityId targetId )
{
	bool enemyTagged = false;

	SAutoaimTarget* pTargetInfo = GetTargetInfoInternal(targetId);
	if(pTargetInfo)
	{
		pTargetInfo->SetFlag(eAATF_AIRadarTagged);
		enemyTagged = pTargetInfo->HasFlagSet(eAATF_AIHostile);
	}

	return enemyTagged;
}

void CAutoAimManager::SetTargetAsCanBeGrabbed(const EntityId targetId, const bool canBeGrabbed)
{
	SAutoaimTarget* pTargetInfo = GetTargetInfoInternal(targetId);
	if(pTargetInfo)
	{
		if(canBeGrabbed)
		{
			pTargetInfo->SetFlag(eAATF_CanBeGrabbed);
		}
		else
		{
			pTargetInfo->RemoveFlag(eAATF_CanBeGrabbed);
		}	
	}
}

void CAutoAimManager::OnEditorReset()
{
	const int numAutoaimTargets = (int)m_autoaimTargets.size();
	
	for (int i = 0; i < numAutoaimTargets; ++i)
	{
		m_autoaimTargets[i].RemoveFlag(eAATF_AIRadarTagged);
	}
}

uint8 CAutoAimManager::GetLocalPlayerFaction() const
{
	if (m_localPlayerFaction != IFactionMap::InvalidFactionID)
	{
		return m_localPlayerFaction;
	}
	else
	{
		IEntity* pLocalPlayerEntity = gEnv->pEntitySystem->GetEntity(g_pGame->GetIGameFramework()->GetClientActorId());
		if (pLocalPlayerEntity)
		{
			IAIObject* pAIObject = pLocalPlayerEntity->GetAI();
			if (pAIObject)
			{
				m_localPlayerFaction = pAIObject->GetFactionID();
			}
		}
	}

	return m_localPlayerFaction;
}

uint8 CAutoAimManager::GetTargetFaction( IEntity& targetEntity ) const
{
	IAIObject* pAIObject = targetEntity.GetAI();
	return pAIObject ? pAIObject->GetFactionID() : IFactionMap::InvalidFactionID;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if DEBUG_AUTOAIM_MANAGER

void CAutoAimManager::DebugDraw()
{
	IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();
	const int numAutoaimTargets = m_autoaimTargets.size();

	const Vec3 viewPos = gEnv->pSystem->GetViewCamera().GetPosition();

	SAuxGeomRenderFlags oldFlags = pRenderAux->GetRenderFlags();
	SAuxGeomRenderFlags newFlags = e_Def3DPublicRenderflags;
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	newFlags.SetDepthTestFlag(e_DepthTestOff);
	newFlags.SetCullMode(e_CullModeNone); 
	pRenderAux->SetRenderFlags(newFlags);

	const ColorB enemyColor(255,0,0,128);
	const ColorB friendlyColor(0,255,0,128);
	const ColorB followColorInner(255,255,0,64);
	const ColorB followColorOuter(255,255,0,0);
	const ColorB snapColor(255,255,255,64);

	for(int i = 0; i < numAutoaimTargets; i++)
	{
		const SAutoaimTarget& aaTarget = m_autoaimTargets[i];

		Vec3 dirToTarget = aaTarget.primaryAimPosition - viewPos;
		dirToTarget.NormalizeSafe();
		
		const float snapRadius = aaTarget.HasFlagSet(eAATF_AIRadarTagged) ?	aaTarget.snapRadiusTagged * g_pGameCVars->aim_assistSnapRadiusTaggedScale : 
																			aaTarget.snapRadius * g_pGameCVars->aim_assistSnapRadiusScale;

		pRenderAux->DrawSphere(aaTarget.primaryAimPosition, aaTarget.innerRadius, aaTarget.HasFlagSet(eAATF_AIHostile) ? enemyColor : friendlyColor);
		pRenderAux->DrawSphere(aaTarget.secondaryAimPosition, 0.2f, aaTarget.HasFlagSet(eAATF_AIHostile) ? enemyColor : friendlyColor);
		DrawDisc(aaTarget.primaryAimPosition, dirToTarget, aaTarget.innerRadius, aaTarget.outerRadius, followColorInner, followColorOuter);
		DrawDisc(aaTarget.primaryAimPosition, dirToTarget, aaTarget.outerRadius, snapRadius, followColorOuter, snapColor);
	}

	pRenderAux->SetRenderFlags(oldFlags);

	const float white[4] = {1.0f, 1.0f, 1.0f, 0.75f};
	IRenderAuxText::Draw2dLabel(50.0f, 50.0f, 1.5f, white, false, "Number of targets: %d", numAutoaimTargets);
}

void CAutoAimManager::DrawDisc(const Vec3& center, Vec3 axis, float innerRadius, float outerRadius, const ColorB& innerColor, const ColorB& outerColor)
{
	axis.NormalizeSafe(Vec3Constants<float>::fVec3_OneZ);
	Vec3 u = ((axis * Vec3Constants<float>::fVec3_OneZ) > 0.5f) ? Vec3Constants<float>::fVec3_OneY : Vec3Constants<float>::fVec3_OneZ;
	Vec3 v = u.cross(axis);
	u = v.cross(axis);

	float sides = ceil_tpl(3.0f * (float)g_PI * outerRadius);
	//sides = 20.0f;
	float step = (sides > 0.0f) ? 1.0f / sides : 1.0f;
	for (float i = 0.0f; i < 0.99f; i += step)
	{
		float a0 = i * 2.0f * (float)g_PI;
		float a1 = (i+step) * 2.0f * (float)g_PI;
		Vec3 i0 = center + innerRadius * (u*cos_tpl(a0) + v*sin_tpl(a0));
		Vec3 i1 = center + innerRadius * (u*cos_tpl(a1) + v*sin_tpl(a1));
		Vec3 o0 = center + outerRadius * (u*cos_tpl(a0) + v*sin_tpl(a0));
		Vec3 o1 = center + outerRadius * (u*cos_tpl(a1) + v*sin_tpl(a1));
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawTriangle(i0, innerColor, i1, innerColor, o1, outerColor);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawTriangle(i0, innerColor, o1, outerColor, o0, outerColor);
	}
}
#endif //DEBUG_AUTOAIM_MANAGER


