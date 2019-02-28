// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ActorImpulseHandler.h"

#include "BodyManager.h"
#include "Actor.h"
#include "Player.h"

#include <CryExtension/CryCreateClassInstance.h>
#ifndef _RELEASE
#include "Utility/CryWatch.h"
#endif //_RELEASE
#include "RecordingSystem.h"

DEFINE_SHARED_PARAMS_TYPE_INFO(CActorImpulseHandler::SSharedImpulseHandlerParams)

namespace
{
	const int UNSPECIFIED_PART = -2;

	const char ACTOR_IMPULSES_ROOT[] = "ActorImpulses";
	const char DEATH_IMPULSES[] = "DeathImpulses";

	const char *JOINT_NAMES[CActorImpulseHandler::NUM_JOINTS] = 
	{
		"Bip01 Spine1",
		"Bip01 Spine3",
		"Bip01 Head"
	};
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CActorImpulseHandler::SImpulseSet::SImpulse::SImpulse() : 
iPartID(UNSPECIFIED_PART),
vDirection(ZERO),
fStrength(0.0f),
bUseDirection(false),
bUseStrength(false)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CActorImpulseHandler::SImpulseSet::SImpulseSet() : fAngularImpulseScale(0.0f)
{

}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CActorImpulseHandler::SImpulseSet::MatchesHitInfo(const HitInfo& hitInfo) const
{
	IF_UNLIKELY(!allowedHitTypes.empty() && (allowedHitTypes.find(hitInfo.type) == allowedHitTypes.end()))
		return false;

	IF_UNLIKELY(!allowedBulletTypes.empty() && (allowedBulletTypes.find(hitInfo.bulletType) == allowedBulletTypes.end()))
		return false;

	IF_UNLIKELY(!allowedPartIDs.empty() && (allowedPartIDs.find(hitInfo.partId) == allowedPartIDs.end()))
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct CActorImpulseHandler::SFindMatchingSet : public std::unary_function<bool, const SImpulseSet&>
{
	SFindMatchingSet(const HitInfo& hitInfo) : hitInfo(hitInfo) {}

	bool operator () (const SImpulseSet& impulseSet) const
	{
		return impulseSet.MatchesHitInfo(hitInfo);
	}

	const HitInfo& hitInfo;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CActorImpulseHandler::SSharedImpulseHandlerParams::SSharedImpulseHandlerParams() : fMaxRagdollImpulse(-1.0f), fMass(80.0f)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// cppcheck-suppress uninitMemberVar
CActorImpulseHandler::CActorImpulseHandler(CActor& actor) : m_actor(actor)
#ifndef _RELEASE
	, m_currentDebugImpulse(0)
#endif //_RELEASE
{
	CryCreateClassInstanceForInterface(cryiidof<IAnimationOperatorQueue>(), m_poseModifier);
	m_impulseState = Imp_None;
	m_delayedDeathImpulse = false;
	m_onRagdollizeEventImpulseQueued = false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CActorImpulseHandler::ReadXmlData(const IItemParamsNode* pRootNode)
{
	ISharedParamsManager* pSharedParamsManager = gEnv->pGameFramework->GetISharedParamsManager();
	CRY_ASSERT(pSharedParamsManager);

	// If we change the SharedParamsManager to accept CRCs on its interface we could compute this once and store
	// the name's CRC32 instead of constructing it here each time this method is invoked (it shouldn't be invoked 
	// too often, though)
	const char* szEntityClassName = m_actor.GetEntityClassName();
	CryFixedStringT<64>	sharedParamsName;
	sharedParamsName.Format("%s_%s", SSharedImpulseHandlerParams::s_typeInfo.GetName(), szEntityClassName);

	ICharacterInstance* pCharacterInstance = m_actor.GetEntity()->GetCharacter(0);
	ISkeletonPose* pSkelPose = pCharacterInstance ? pCharacterInstance->GetISkeletonPose() : NULL;

	if (pSkelPose)
	{
		IDefaultSkeleton& rIDefaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();
		for (int i=0; i<NUM_JOINTS; i++)
		{
			m_jointIdx[i] = rIDefaultSkeleton.GetJointIDByName(::JOINT_NAMES[i]);
		}
	}

	// Get params from the shared repository
	ISharedParamsConstPtr pSharedParams = pSharedParamsManager->Get(sharedParamsName);
	if (pSharedParams)
	{
		m_pParams = CastSharedParamsPtr<SSharedImpulseHandlerParams>(pSharedParams);
		return;
	}

	m_pParams.reset();

	// If parameters aren't present for this entity class, use the Default params (parse them if still unparsed)
	const IItemParamsNode* pParams = pRootNode ? pRootNode->GetChild(ACTOR_IMPULSES_ROOT) : NULL;
	if (!pParams)
	{
		sharedParamsName.Format("%s_%s", SSharedImpulseHandlerParams::s_typeInfo.GetName(), CActor::DEFAULT_ENTITY_CLASS_NAME);

		pSharedParams = pSharedParamsManager->Get(sharedParamsName);
		if (!pSharedParams)
		{
			const IItemParamsNode* pDefaultClassRootNode = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActorParams(CActor::DEFAULT_ENTITY_CLASS_NAME);
			pParams = pDefaultClassRootNode ? pDefaultClassRootNode->GetChild(ACTOR_IMPULSES_ROOT) : NULL;
		}
		else
		{
			m_pParams = CastSharedParamsPtr<SSharedImpulseHandlerParams>(pSharedParams);
			return;
		}
	}

	if (pParams)
	{
		SSharedImpulseHandlerParams newParams;

		const IItemParamsNode* pDeathImpulses = pParams->GetChild(DEATH_IMPULSES);
		if (pDeathImpulses)
		{
			LoadDeathImpulses(pDeathImpulses, newParams.impulseSets);

			// Maximum impulse value to be applied
			pDeathImpulses->GetAttribute("maxRagdollImpulse", newParams.fMaxRagdollImpulse);

			// Cache physicsparam mass value from the physicsParams table of the entity script
			// [*DavidR | 27/Oct/2010] Keep in synch with CActor::LoadPhysicsParams mass extraction
			SmartScriptTable pEntityTable = m_actor.GetEntity()->GetScriptTable();
			SmartScriptTable physicsParams;
			if (pEntityTable && pEntityTable->GetValue("physicsParams", physicsParams))
			{
				CryFixedStringT<64> massParamName = CryFixedStringT<64>("mass") + "_" + m_actor.GetEntityClassName();
				if (!physicsParams->GetValue(massParamName.c_str(), newParams.fMass))
					physicsParams->GetValue("mass", newParams.fMass);
			}
		}

		m_pParams = CastSharedParamsPtr<SSharedImpulseHandlerParams>(pSharedParamsManager->Register(sharedParamsName, newParams));
	}
}

float GenFactor(float time, float duration, bool &recovery)
{
	const float currentFactor = time / duration;
	recovery = (currentFactor > g_pGameCVars->pl_impulseFullRecoilFactor);
	if (recovery)
	{
		return 1.0f - ((currentFactor - g_pGameCVars->pl_impulseFullRecoilFactor) / (1.0f - g_pGameCVars->pl_impulseFullRecoilFactor));
	}
	else
	{
		return currentFactor / g_pGameCVars->pl_impulseFullRecoilFactor;
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CActorImpulseHandler::AddLocalHitImpulse(const SHitImpulse& hitImpulse)
{
	IEntity *pPhysicsProxy = m_actor.GetEntity();

	const bool applyImpulse = (pPhysicsProxy != NULL) && !m_actor.GetLinkedVehicle();

	if (applyImpulse)
	{
		TBodyDamageProfileId bodyDamageProfileId = m_actor.GetCurrentBodyDamageProfileId();
		int partFlags = 0;
		if (bodyDamageProfileId != INVALID_BODYDAMAGEPROFILEID)
		{
			HitInfo tmpHitInfo;
			tmpHitInfo.partId = hitImpulse.m_partId;
			tmpHitInfo.material = hitImpulse.m_matId;
			tmpHitInfo.projectileClassId = hitImpulse.m_projectileClass;

			SBodyDamageImpulseFilter impulseFilter;
			CBodyDamageManager *pBodyDamageManager = g_pGame->GetBodyDamageManager();
			assert(pBodyDamageManager);
			pBodyDamageManager->GetHitImpulseFilter(bodyDamageProfileId, *m_actor.GetEntity(), tmpHitInfo, impulseFilter);

			partFlags = pBodyDamageManager->GetPartFlags( bodyDamageProfileId, *m_actor.GetEntity(), tmpHitInfo );

			if (impulseFilter.multiplier != 0.0f)
			{
				pPhysicsProxy->AddImpulse(hitImpulse.m_partId, hitImpulse.m_pos, hitImpulse.m_impulse * impulseFilter.multiplier, true, 1.0f, hitImpulse.m_fPushImpulseScale);
			}

			if ((impulseFilter.passOnMultiplier != 0.0f) && (impulseFilter.passOnPartId >= 0))
			{
				pPhysicsProxy->AddImpulse(impulseFilter.passOnPartId, hitImpulse.m_pos, hitImpulse.m_impulse * impulseFilter.passOnMultiplier, true, 1.0f, hitImpulse.m_fPushImpulseScale);
			}
		}
		else
		{
			pPhysicsProxy->AddImpulse(hitImpulse.m_partId, hitImpulse.m_pos, hitImpulse.m_impulse, true, 1.0f, hitImpulse.m_fPushImpulseScale);
		}

		m_actor.GetActorStats()->wasHit = true;

		if (!hitImpulse.m_impulse.IsZero() && g_pGameCVars->pl_impulseEnabled)
		{
			//--- Back up current
			if (m_impulseState != Imp_None)
			{
				bool recovery = false;
				const float currentFactor = GenFactor(m_impulseTime, m_impulseDuration, recovery);
				for (int i=0; i<NUM_JOINTS; i++)
				{
					m_initialOffsets[i].SetSlerp(m_initialOffsets[i], m_targetOffsets[i], currentFactor);
				}
			}
			else
			{
				for (int i=0; i<NUM_JOINTS; i++)
				{
					m_initialOffsets[i].SetIdentity();
				}
			}

			Vec3 msDir = m_actor.GetAnimatedCharacter()->GetAnimLocation().q.GetInverted() * hitImpulse.m_impulse;
			msDir.z = 0.0f;
			msDir.NormalizeSafe(Vec3Constants<float>::fVec3_OneY);
			Vec3 msTangent = msDir.Cross(Vec3Constants<float>::fVec3_OneZ);
			msTangent.Normalize();

			float tiltFactor = cry_random(-1.0f, 1.0f) * g_pGameCVars->pl_impulseMaxTwist;
			Quat qImpulse = Quat::CreateRotationAA(- g_pGameCVars->pl_impulseMaxPitch, msTangent) * Quat::CreateRotationAA(tiltFactor, Vec3Constants<float>::fVec3_OneZ);
			Quat qImpulseCounter = Quat::CreateRotationAA( g_pGameCVars->pl_impulseMaxPitch *  g_pGameCVars->pl_impulseCounterFactor, msTangent) * Quat::CreateRotationAA(-tiltFactor *  g_pGameCVars->pl_impulseCounterFactor, Vec3Constants<float>::fVec3_OneZ);


			m_impulseTime = 0.0f;
			m_impulseDuration =  g_pGameCVars->pl_impulseDuration;
			m_targetOffsets[0] = qImpulse;
			m_targetOffsets[1] = qImpulseCounter;
			m_targetOffsets[2] = (partFlags & eBodyDamage_PID_Headshot) ? qImpulse : qImpulseCounter;
			m_impulseState = Imp_Impulse;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// [*DavidR | 21/Oct/2010] Note:
// This is a direct port of the old BasicActor:ApplyDeathImpulse LUA method
// (so don't blame me for the intense use of magic numbers)
//////////////////////////////////////////////////////////////////////////
void CActorImpulseHandler::ApplyDeathImpulse(const HitInfo& lastHit)
{
	if (!m_pParams || (lastHit.damage < 0.01f))
		return;

	//CRY_ASSERT_MESSAGE(m_actor.GetEntity()->GetPhysics() && (m_actor.GetEntity()->GetPhysics()->GetType() == PE_ARTICULATED), "This method is intended to work only for ragdolls!");

	//************** Dir calculation	
	Vec3 dir = lastHit.dir;

	// Up the direction vector
	dir.z += cry_random(0.1f, 0.7f);

	//************** Hit pos calculation			
	// elevate player position 1 meter high (so it's approximately on the character's waist)
	Vec3 playerPos = m_actor.GetEntity()->GetWorldPos();
	playerPos.z += 1.0f;

	// Find impulse
	ImpulseSetsContainer::const_iterator itCandidate = std::find_if(m_pParams->impulseSets.begin(), m_pParams->impulseSets.end(), SFindMatchingSet(lastHit));
	if (itCandidate != m_pParams->impulseSets.end())
	{
		const SImpulseSet& impulseSet = *itCandidate;

		// Apply angular impulse
		if (impulseSet.fAngularImpulseScale != 0.0f)
			ApplyDeathAngularImpulse(impulseSet.fAngularImpulseScale, lastHit.pos, playerPos, lastHit);

		// Apply all the impulse data contained
		SImpulseSet::ImpulseContainer::const_iterator itEnd = impulseSet.impulses.end();
		for (SImpulseSet::ImpulseContainer::const_iterator it = impulseSet.impulses.begin(); it != itEnd; ++it)
		{
			const SImpulseSet::SImpulse& impulse = *it;

			// Impulse part of application
			int myPartId = -1;
			if(impulse.iPartID != UNSPECIFIED_PART)
				myPartId = impulse.iPartID;
			else
				myPartId = lastHit.partId;	

			// Impulse Direction					
			Vec3 myDirection(ZERO);
			if(impulse.bUseDirection)
				myDirection = lastHit.dir.CompMul(impulse.vDirection);
			else
				myDirection = impulse.vDirection;

			// randomize a bit scaling
			// ToDo: Is this really needed?
			myDirection = myDirection.CompMul(cry_random_componentwise(Vec3(0.8f), Vec3(1.2f)));

			// Impulse Strength
			float myImpulse = 0;
			if(impulse.bUseStrength)
				myImpulse = lastHit.damage * impulse.fStrength;
			else
				myImpulse = impulse.fStrength;

			// randomize scaling it from 0.8 to 1.2
			// ToDo: Is this really needed?
			myImpulse *= cry_random(0.8f, 1.2f);
			myImpulse = static_cast<float>(__fsel(m_pParams->fMaxRagdollImpulse, min(m_pParams->fMaxRagdollImpulse, myImpulse), myImpulse));

			// Impulse Application
			AddLocalHitImpulse( SHitImpulse(myPartId, lastHit.material, lastHit.pos, myDirection.GetNormalized() * myImpulse, 1.0f) );
		}
	}
}

void CActorImpulseHandler::QueueDeathImpulse(const HitInfo& hitInfo, const float delay)
{
	m_queuedDeathImpulse.hitInfo = hitInfo;
	m_queuedDeathImpulse.timeOut = delay;
	m_delayedDeathImpulse = true;
}

void CActorImpulseHandler::Update(float frameTime)
{
	bool applyModifier = (m_impulseState != Imp_None);

	if (applyModifier)
	{
		m_impulseTime += frameTime;
		bool recovery = false;
		float currentFactor = GenFactor(m_impulseTime, m_impulseDuration, recovery);
		Quat target;

		if ((m_impulseState == Imp_Impulse) && recovery)
		{
			m_impulseState = Imp_Recovery;
			for (uint32 i=0; i<NUM_JOINTS; i++)
			{
				m_initialOffsets[i].SetIdentity();
			}
		}

		if (m_impulseTime >= m_impulseDuration)
		{
			m_impulseDuration = 0.0f;
			m_impulseTime = 0.0f;
			m_impulseState = Imp_None;
		}

		for (uint32 i=0; i<NUM_JOINTS; i++)
		{
			target.SetSlerp(m_initialOffsets[i], m_targetOffsets[i], currentFactor);

			m_poseModifier->PushOrientation(m_jointIdx[i], IAnimationOperatorQueue::eOp_Additive, target);
		}
	}

	if (applyModifier)
	{
		ICharacterInstance* pCharacterInstance = m_actor.GetEntity()->GetCharacter(0);

		if (m_actor.IsPlayer())
		{
			CPlayer &player = (CPlayer&)(m_actor);
			if (!player.IsThirdPerson() && player.HasShadowCharacter())
			{
				pCharacterInstance = player.GetShadowCharacter();
			}
		}

		if (pCharacterInstance)
		{
			pCharacterInstance->GetISkeletonAnim()->PushPoseModifier(g_pGameCVars->pl_impulseLayer, m_poseModifier, "ActorImpulseHandler");
		}
	}

#ifndef _RELEASE
	if(g_pGameCVars->pl_impulseDebug)
	{
		const float time = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		for(int i=kDebugImpulseMax; i>0; i--)
		{
			SDebugImpulse& imp = m_debugImpulses[(m_currentDebugImpulse+kDebugImpulseMax-i)%kDebugImpulseMax];
			if(imp.valid)
			{
				CryWatch( "Impulse: Age[%.2fs] Applied[%s] Impulse[%.2f,%.2f,%.2f]", time-imp.time, imp.applied?"True":"False", imp.impulse.impulse.x, imp.impulse.impulse.y, imp.impulse.impulse.z );
			}
		}
	}
#endif //_RELEASE
}

void CActorImpulseHandler::UpdateDeath(float frameTime)
{
	if (m_delayedDeathImpulse == false)
		return;

	const float newTimeOut = m_queuedDeathImpulse.timeOut - frameTime;
	m_queuedDeathImpulse.timeOut = newTimeOut;
	
	if (newTimeOut > 0.0f)
		return;

	ApplyDeathImpulse(m_queuedDeathImpulse.hitInfo);

	m_delayedDeathImpulse = false;
}

//////////////////////////////////////////////////////////////////////////
// [*DavidR | 21/Oct/2010] Note:
// This is a direct port of the old BasicActor:ApplyDeathAngularImpulse LUA method
// (so don't blame me for the intense use of magic numbers)
//////////////////////////////////////////////////////////////////////////
void CActorImpulseHandler::ApplyDeathAngularImpulse(float fAngularImpulseScale, const Vec3& hitpos, const Vec3& playerPos, const HitInfo& lastHit)
{
	const QuatT& animLocation = m_actor.GetAnimatedCharacter()->GetAnimLocation();
	Vec3 rightVec(animLocation.GetColumn0());
	Vec3 fwdVec(animLocation.GetColumn1());
	Vec3 upVec(animLocation.GetColumn2());

	// Rotation Impulse Direction calculation
	// Is the up vector after subtracting the forward vector*0.35 from it (slightly inclined to the back up vector)
	Vec3 rotImpulseDir(upVec.x - fwdVec.x * 0.35f, upVec.y - fwdVec.y * 0.35f, upVec.z - fwdVec.z * 0.35f);
	rotImpulseDir.Normalize();

	Vec3 delta(hitpos - playerPos);
	delta.z = 0.0f;

	// Angular Impulse Calculation
	float impulse = std::min(650.0f, std::max(10.0f, lastHit.damage) * cry_random(6.0f, 9.0f));

	float dotRight = rightVec.dot(delta);
	float angImpulse = (dotRight * gf_PI) * (impulse / 650.0f) * m_pParams->fMass;

	// cap the angular impulse if necessary
	angImpulse = min(gf_PI * 0.5f * m_pParams->fMass, fabs_tpl(angImpulse)) * static_cast<float>(__fsel(angImpulse, 1.0f, -1.0f));
	angImpulse = static_cast<float>(__fsel(fwdVec.dot(delta), -angImpulse, angImpulse));

	// Apply angular impulse (copied from ScriptBind_Entity::AddImpulse)
	// fAngularImpulseScale is a scale factor for the calculated angular impulse
	IPhysicalEntity* ppEnt = m_actor.GetEntity()->GetPhysics();
	if (ppEnt)
	{
		pe_action_impulse aimpulse;
		aimpulse.partid = -1;
		aimpulse.impulse = rotImpulseDir * 100.0f;
		aimpulse.angImpulse = upVec * angImpulse * fAngularImpulseScale;
		aimpulse.point = hitpos;

		if(ppEnt->GetType()==PE_ARTICULATED)
		{
			ppEnt->Action(&aimpulse);
		}
		else
		{
			SetOnRagdollPhysicalizedImpulse(aimpulse);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CActorImpulseHandler::LoadDeathImpulses(const IItemParamsNode* pDeathImpulses, ImpulseSetsContainer& impulseSets)
{
	CRY_ASSERT(pDeathImpulses);

	ICharacterInstance* pCharacterInstance = m_actor.GetEntity()->GetCharacter(0);
	ISkeletonPose* pSkelPose = pCharacterInstance ? pCharacterInstance->GetISkeletonPose() : NULL;
	IF_UNLIKELY (!pSkelPose)
	{
		GameWarning("[CActorImpulseHandler]: Can't load death impulses on actor %s - No Character Instance/Skeleton Pose", m_actor.GetEntity()->GetName());
		return;
	}
	IDefaultSkeleton& rIDefaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();

#ifndef _RELEASE
	static std::set<const IItemParamsNode*> parsedDeathImpulseNodes;
	bool bRecursion = !parsedDeathImpulseNodes.insert(pDeathImpulses).second;
	if (bRecursion)
	{
		GameWarning("[CActorImpulseHandler]: Infinite recursion while parsing death impulses for actor %s. Watch for cyclic \"CopyImpulsesFrom\" inclusions", m_actor.GetEntity()->GetName());
		return;
	}
#endif

	const int iChildCount = pDeathImpulses->GetChildCount();
	for (int i = 0; i < iChildCount; ++i)
	{
		const IItemParamsNode* pChild = pDeathImpulses->GetChild(i);
		if (!pChild)
			continue;

		if (strcmp(pChild->GetName(), "ImpulseSet") == 0)
		{
			const IItemParamsNode* const pImpulseSet = pChild;
			SImpulseSet newImpulseSet;

			// Impulses
			const int iImpulseCount = pImpulseSet->GetChildCount();
			for (int iImpulse = 0; iImpulse < iImpulseCount; ++iImpulse)
			{
				const IItemParamsNode* pImpulse = pImpulseSet->GetChild(iImpulse);
				if (pImpulse && (strcmp(pImpulse->GetName(), "Impulse") == 0))
				{
					SImpulseSet::SImpulse newImpulse;

					// Check mandatory parameters
					const bool bHasDirection = pImpulse->GetAttribute("direction", newImpulse.vDirection);
					const bool bHasStrenght = pImpulse->GetAttribute("strength", newImpulse.fStrength);
					if (bHasDirection && bHasStrenght)
					{
						const char* szPartID = pImpulse->GetAttribute("partID");
						if (szPartID && szPartID[0])
						{
							const int iPartId = rIDefaultSkeleton.GetJointIDByName(szPartID);
							if (iPartId != -1)
							{
								newImpulse.iPartID = iPartId;
							}
							else
							{
								GameWarning("[CActorImpulseHandler]: Specified PartId(\"%s\") not found on actor %s while parsing its impulses. Skipping this impulse", szPartID, m_actor.GetEntity()->GetName());
								continue;
							}
						}

						int iUseStrength = 0;
						newImpulse.bUseStrength = pImpulse->GetAttribute("useStrength", iUseStrength) && (iUseStrength != 0);

						int iUseDirection = 0;
						newImpulse.bUseDirection = pImpulse->GetAttribute("useDirection", iUseDirection) && (iUseDirection != 0);

						newImpulseSet.impulses.push_back(newImpulse);
					}
				}
			}

			pImpulseSet->GetAttribute("angularImpulseScale", newImpulseSet.fAngularImpulseScale);

			// hitTypes
			{
				int iCurPos = 0;
				CryFixedStringT<64> sHitTypesTokenized = pImpulseSet->GetAttribute("hitTypes");
				CryFixedStringT<64> sHitType = sHitTypesTokenized.Tokenize(",", iCurPos);
				while (!sHitType.empty())
				{
					const int iHitType = g_pGame->GetGameRules()->GetHitTypeId(sHitType.c_str());
					if (iHitType > 0)
					{
						newImpulseSet.allowedHitTypes.insert(iHitType);
					}
					else
					{
						GameWarning("[CActorImpulseHandler]: Invalid hit type(\"%s\") on actor %s while parsing its impulse sets. Ignoring hit type condition", sHitType.c_str(), m_actor.GetEntity()->GetName());
					}

					sHitType = sHitTypesTokenized.Tokenize(",", iCurPos);
				}
			}

			// partIds
			{
				int iCurPos = 0;
				CryFixedStringT<64> sPartIDsTokenized = pImpulseSet->GetAttribute("partIDs");
				CryFixedStringT<64> sPartID = sPartIDsTokenized.Tokenize(",", iCurPos);
				while (!sPartID.empty())
				{
					const int iPartID = rIDefaultSkeleton.GetJointIDByName(sPartID.c_str());
					if (iPartID != -1)
					{
						newImpulseSet.allowedPartIDs.insert(iPartID);
					}
					else
					{
						GameWarning("[CActorImpulseHandler]: Specified PartId(\"%s\") not found on actor %s while parsing its impulse sets. Ignoring partId condition", sPartID.c_str(), m_actor.GetEntity()->GetName());
					}

					sPartID = sPartIDsTokenized.Tokenize(",", iCurPos);
				}
			}

			// bulletTypes
			{
				int iBulletType = 0;
				if (pImpulseSet->GetAttribute("bulletTypes", iBulletType))
				{
					if (iBulletType > 0)
						newImpulseSet.allowedBulletTypes.insert(iBulletType);
				}
				else
				{
					int iCurPos = 0;
					CryFixedStringT<64> sBulletTypesTokenized = pImpulseSet->GetAttribute("bulletTypes");
					CryFixedStringT<64> sBulletType = sBulletTypesTokenized.Tokenize(",", iCurPos);
					while (!sBulletType.empty())
					{
						iBulletType = atoi(sBulletType.c_str());
						if (iBulletType > 0)
							newImpulseSet.allowedBulletTypes.insert(iBulletType);

						sBulletType = sBulletTypesTokenized.Tokenize(",", iCurPos);
					}
				}
			}

			impulseSets.push_back(newImpulseSet);
		}
		else if (strcmp(pChild->GetName(), "CopyImpulsesFrom") == 0)
		{
			// We can copy impulses from another entity class's params
			const char* szCopyFrom = pChild->GetAttribute("class");
			if (szCopyFrom && *szCopyFrom)
			{
				const IItemParamsNode* pCopyFromRootNode = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActorParams(szCopyFrom);
				const IItemParamsNode* pCopyFromParams = pCopyFromRootNode ? pCopyFromRootNode->GetChild(ACTOR_IMPULSES_ROOT) : NULL;
				const IItemParamsNode* pCopyFromDeathImpulses = pCopyFromParams ? pCopyFromParams->GetChild(DEATH_IMPULSES) : NULL;
				if (pCopyFromDeathImpulses)
				{
					LoadDeathImpulses(pCopyFromDeathImpulses, impulseSets);
				}
				else
				{
					GameWarning("[CActorImpulseHandler]: Specified copyFrom class params(\"%s\") not found on while parsing actor %s impulse sets. Skipping", szCopyFrom, m_actor.GetEntity()->GetName());
				}
			}
		}
		else
		{
			CRY_ASSERT_TRACE(false, ("Unexpected element: %s", pChild->GetName()));
		}
	}

#ifndef _RELEASE
	parsedDeathImpulseNodes.erase(pDeathImpulses);
#endif
}

void CActorImpulseHandler::OnRagdollPhysicalized()
{
	// If we have a deferred impulse to be applied as soon as the actor is ragdollized, apply it and clear. 
	if(m_onRagdollizeEventImpulseQueued)
	{
		// Apply the queued event and disarm 
		IPhysicalEntity* pPhysics = m_actor.GetEntity()->GetPhysics();
		if (pPhysics)
		{
			pPhysics->Action(&m_onRagdollizeEventImpulse);
			m_onRagdollizeEventImpulseQueued = false;
			if(CRecordingSystem* pRecSys = g_pGame->GetRecordingSystem())
			{
				pRecSys->OnApplyImpulseToRagdoll(m_actor.GetEntityId(), m_onRagdollizeEventImpulse);
			}
	#ifndef _RELEASE
			if(g_pGameCVars->pl_impulseDebug)
			{
				m_debugImpulses[m_currentDebugImpulse].applied = true;
			}
	#endif //_RELEASE
		}
	}
}

void CActorImpulseHandler::SetOnRagdollPhysicalizedImpulse( pe_action_impulse& impulse )
{
	m_onRagdollizeEventImpulse			 = impulse;
	m_onRagdollizeEventImpulseQueued = true; 

#ifndef _RELEASE
	if(g_pGameCVars->pl_impulseDebug)
	{
		m_currentDebugImpulse = (m_currentDebugImpulse+1)%kDebugImpulseMax;
		m_debugImpulses[m_currentDebugImpulse].time = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		m_debugImpulses[m_currentDebugImpulse].impulse = impulse;
		m_debugImpulses[m_currentDebugImpulse].applied = false;
		m_debugImpulses[m_currentDebugImpulse].valid = true;
	}
#endif //_RELEASE
}
