// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id: CReplayActor.cpp$
$DateTime$
Description: A replay actor spawned during KillCam replay

-------------------------------------------------------------------------
History:
- 03/19/2010 09:15:00: Created by Martin Sherburn

*************************************************************************/

#include "StdAfx.h"
#include "ReplayActor.h"
#include "IAnimatedCharacter.h"
#include "Item.h"

#include "GameCVars.h"

#include "PlayerAnimation.h"
#include "ItemSharedParams.h"
#include "TeamVisualizationManager.h"
#include "GamePhysicsSettings.h"
#include "GameRules.h"

CReplayActor::CReplayActor()
: m_gunId(0)
, m_flags(0)
, m_trackerPreviousPos(ZERO)
, m_trackerUpdateTime(0.0f)
, m_drawPos(ZERO)
, m_size(0)
, m_lastFrameUpdated(0)
, m_healthPercentage(100)
, m_rank(0)
, m_reincarnations(0)
, m_headBoneID(-2)
, m_cameraBoneID(-2)
, m_team(0)
, m_headPos(IDENTITY)
, m_cameraPos(IDENTITY)
, m_velocity(ZERO)
, m_onGround(false)
, m_isFriendly(false)
, m_pAnimContext(NULL)
, m_pActionController(NULL)
, m_pAnimDatabase1P(NULL)
, m_pAnimDatabase3P(NULL)
{
	m_GunRemovalListener.pReplayActor = this;
}

CReplayActor::~CReplayActor()
{
	SAFE_RELEASE(m_pActionController);
	SAFE_DELETE(m_pAnimContext);
	m_itemList.OnActionControllerDeleted();
}

//------------------------------------------------------------------------
void CReplayActor::Release()
{
	// Remove the cloned gun
	RemoveGun(true);
	delete this;
}

//------------------------------------------------------------------------
void CReplayActor::PostInit(IGameObject *pGameObject)
{
	GetGameObject()->EnableUpdateSlot(this, 0);
}


//------------------------------------------------------------------------
void CReplayActor::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_PREPHYSICSUPDATE:
		{
			const int currentFrameId = gEnv->nMainFrameID;
			if(currentFrameId==m_lastFrameUpdated)
				return;
			m_lastFrameUpdated = currentFrameId;

			if (m_pActionController)
			{
				float frameTime = gEnv->pTimer->GetFrameTime();
				m_pActionController->Update(frameTime);
			}

			ICharacterInstance* pShadowCharacter = GetShadowCharacter();
			if (pShadowCharacter)
			{
				QuatTS pose = (QuatTS)GetEntity()->GetWorldTM();
				SAnimationProcessParams params;
				params.locationAnimation = pose;
				params.bOnRender = 1;
				params.zoomAdjustedDistanceFromCamera = (GetISystem()->GetViewCamera().GetPosition()-pose.t).GetLength();
				pShadowCharacter->StartAnimationProcessing(params);
				//--- Ensure that we disable the automatic update if we're doing it explicitly
				pShadowCharacter->SetFlags( pShadowCharacter->GetFlags()&(~CS_FLAG_UPDATE) );
			}

			ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
			ISkeletonPose *pSkelPose = pCharacter ? pCharacter->GetISkeletonPose() : NULL;

			if(pSkelPose)
			{
				IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();

				// GetJointIDByName() returns -1 if it fails; m_headBoneID is initialised to -2
				// since -1 is failure and 0+ are valid bone indices.
				if (m_headBoneID == -2)
				{
					// TODO: Should move this to an initialisation function
					m_headBoneID = rIDefaultSkeleton.GetJointIDByName("Bip01 Head");
					m_cameraBoneID = rIDefaultSkeleton.GetJointIDByName("Bip01 Camera");
				}

				if (m_headBoneID > -1)
				{
					m_headPos = pSkelPose->GetAbsJointByID(m_headBoneID);
				}
				if (m_cameraBoneID > -1)
				{
					m_cameraPos = pSkelPose->GetAbsJointByID(m_cameraBoneID);
				}
			}
		}
		break;
	case ENTITY_EVENT_DONE:
		{
			SAFE_RELEASE(m_pActionController);
			SAFE_DELETE(m_pAnimContext);
			m_itemList.OnActionControllerDeleted();
		}
		break;
	}
}

uint64 CReplayActor::GetEventMask() const
{
	return 
		ENTITY_EVENT_BIT(ENTITY_EVENT_PREPHYSICSUPDATE) |
		ENTITY_EVENT_BIT(ENTITY_EVENT_DONE);
}

//------------------------------------------------------------------------
void CReplayActor::OnRemove()
{
#ifndef _RELEASE
	if (m_pActionController && g_pGameCVars->kc_debugMannequin)
	{
		m_pActionController->SetFlag(AC_DumpState, true);
		m_pActionController->Update(0.0f);
	}
#endif //_RELEASE
	RemoveGun(true);
}

//------------------------------------------------------------------------
void CReplayActor::SetupActionController(const char *controllerDef, const char *animDB1P, const char *animDB3P)
{
	IMannequin &mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	const SControllerDef *pContDef = NULL;
	if (controllerDef)
	{
		pContDef = mannequinSys.GetAnimationDatabaseManager().LoadControllerDef(controllerDef);
	}
	if (animDB1P)
	{
		m_pAnimDatabase1P = mannequinSys.GetAnimationDatabaseManager().Load(animDB1P);
	}
	if (animDB3P)
	{
		m_pAnimDatabase3P = mannequinSys.GetAnimationDatabaseManager().Load(animDB3P);
	}

	if (pContDef)
	{
		SAFE_RELEASE(m_pActionController);
		SAFE_DELETE(m_pAnimContext);

		m_pAnimContext = new SAnimationContext(*pContDef);
		m_pActionController = mannequinSys.CreateActionController(GetEntity(), *m_pAnimContext);

		UpdateCharacter();
	}
}

void CReplayActor::SetGunId(EntityId gunId)
{ 
	if(gunId)
	{
		if(IEntity * pGunEntity = gEnv->pEntitySystem->GetEntity(gunId))
		{
			//This will be unregistered in IEntity::Shutdown() when the gun is deleted
			gEnv->pEntitySystem->AddEntityEventListener(gunId, ENTITY_EVENT_DONE, &m_GunRemovalListener);
		}
	}

	// Remove Existing gun, but don't delete it.
	RemoveGun(false);
	 
	m_gunId = gunId;

	UpdateScopeContexts();
}

void CReplayActor::UpdateScopeContexts()
{
	if (m_pActionController)
	{
		//--- Update variable scope contexts
		CItem *pItem = static_cast<CItem*>(g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_gunId));
		ICharacterInstance *pICharInst = (m_flags & eRAF_FirstPerson) && pItem ? pItem->GetEntity()->GetCharacter(0) : NULL;
		IMannequin &mannequinSys = gEnv->pGameFramework->GetMannequinInterface();

		const int contextID = PlayerMannequin.contextIDs.Weapon;
		if (contextID >= 0)
		{
			const uint32 numScopes = m_pActionController->GetContext().controllerDef.m_scopeDef.size();
			ICharacterInstance *scopeCharInst = NULL;
			for (uint32 s=0; s<numScopes; s++)
			{
				if (m_pActionController->GetContext().controllerDef.m_scopeDef[s].context == contextID)
				{
					scopeCharInst = m_pActionController->GetScope(s)->GetCharInst();
					break;
				}
			}
			if (scopeCharInst != pICharInst)
			{
				if (pICharInst)
				{
					const SParams& weaponParams = pItem->GetParams();
					const IAnimationDatabase *animDB = !weaponParams.adbFile.empty() ? mannequinSys.GetAnimationDatabaseManager().Load(weaponParams.adbFile.c_str()) : NULL;
					if (animDB)
					{
						m_pActionController->SetScopeContext(contextID, *pItem->GetEntity(), pICharInst, animDB);
					}
					else
					{
						m_pActionController->ClearScopeContext(contextID, false);
					}
				}
				else
				{
					m_pActionController->ClearScopeContext(contextID, false);
				}
			}
		}

		const uint32 NUM_ACCESSORY_SLOTS = 2;
		uint32 ACCESSORY_CONTEXT_IDS[NUM_ACCESSORY_SLOTS] = {PlayerMannequin.contextIDs.attachment_top, PlayerMannequin.contextIDs.attachment_bottom};
		for (uint32 attachmentSlot=0; attachmentSlot<NUM_ACCESSORY_SLOTS; attachmentSlot++)
		{
			const int attachmentContextID = ACCESSORY_CONTEXT_IDS[attachmentSlot];
			ICharacterInstance *scopeCharInst = NULL;
			ICharacterInstance *accessoryCharInst = NULL;
			CItem *accessory = NULL;
			const char *contextName = m_pActionController->GetContext().controllerDef.m_scopeContexts.GetTagName(attachmentContextID);

			const uint32 numScopes = m_pActionController->GetContext().controllerDef.m_scopeDef.size();
			for (uint32 s=0; s<numScopes; s++)
			{
				if (m_pActionController->GetContext().controllerDef.m_scopeDef[s].context == attachmentContextID)
				{
					scopeCharInst = m_pActionController->GetScope(s)->GetCharInst();
					break;
				}
			}

			if (pItem)
			{
				//--- Find attachments
				const CItem::TAccessoryArray &accessories = pItem->GetAccessories();
				for (CItem::TAccessoryArray::const_iterator iter=accessories.begin(); iter != accessories.end(); ++iter)
				{
					const CItem::SAccessoryInfo &accessoryInfo = *iter;
					const SAccessoryParams *aparams = pItem->GetAccessoryParams(accessoryInfo.pClass);

					if (strcmp(aparams->attach_helper.c_str(), contextName) == 0)
					{
						//--- Found a match
						accessory = pItem->GetAccessory(accessoryInfo.pClass);
						accessoryCharInst = accessory->GetEntity()->GetCharacter(0);
						break;
					}
				}
			}

			if (scopeCharInst != accessoryCharInst)
			{
				if (accessoryCharInst && !accessory->GetParams().adbFile.empty())
				{
					const IAnimationDatabase *animDB = mannequinSys.GetAnimationDatabaseManager().Load(accessory->GetParams().adbFile.c_str());
					if (animDB)
					{
						m_pActionController->SetScopeContext(attachmentContextID, *pItem->GetEntity(), accessoryCharInst, animDB);
					}
				}
				else
				{
					m_pActionController->ClearScopeContext(attachmentContextID, false);
				}
			}
		}
	}
}

void CReplayActor::UpdateCharacter()
{
	if (m_pActionController)
	{
		const bool isFirstPerson = (m_flags & eRAF_FirstPerson) != 0;
		ICharacterInstance *pCharacter			 = GetEntity()->GetCharacter(0);
		ICharacterInstance *pShadowCharacter = GetShadowCharacter();

		const int scopeContext1P = m_pActionController->GetContext().controllerDef.m_scopeContexts.Find("Char1P");
		const int scopeContext3P = m_pActionController->GetContext().controllerDef.m_scopeContexts.Find("Char3P");
		if (isFirstPerson)
		{
#ifndef _RELEASE
			if (m_pActionController && (g_pGameCVars->kc_debugMannequin == 2))
			{
				m_pActionController->SetFlag(AC_DebugDraw, true);
			}
#endif //!_RELEASE

			if ((scopeContext1P >= 0) && m_pAnimDatabase1P)
			{
				m_pActionController->SetScopeContext(scopeContext1P, *GetEntity(), pCharacter, m_pAnimDatabase1P);
			}
			if ((scopeContext3P >= 0) && m_pAnimDatabase3P)
			{
				m_pActionController->SetScopeContext(scopeContext3P, *GetEntity(), pShadowCharacter, m_pAnimDatabase3P);
			}
		}
		else
		{
			if (scopeContext1P >= 0)
			{
				m_pActionController->ClearScopeContext(scopeContext1P, false);
			}
			if ((scopeContext3P >= 0) && m_pAnimDatabase3P)
			{
				m_pActionController->SetScopeContext(scopeContext3P, *GetEntity(), pCharacter, m_pAnimDatabase3P);
			}
		}
	}

}

//------------------------------------------------------------------------
void CReplayActor::SetFirstPerson(bool firstperson)
{
	m_animationProxy.SetFirstPerson(firstperson);
	m_animationProxyUpper.SetFirstPerson(firstperson);
	if (firstperson)
	{
		m_flags |= eRAF_FirstPerson;
	}
	else
	{
		m_flags &= ~eRAF_FirstPerson;
	}

	CItem *pItem = static_cast<CItem*>(g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_gunId));
	if (pItem)
	{
		pItem->CheckViewChange();
	}

	UpdateCharacter();
	UpdateScopeContexts();
}

void CReplayActor::LoadCharacter(const char *filename)
{
	GetEntity()->LoadCharacter(0, filename);

	UpdateCharacter();
}

//------------------------------------------------------------------------
void CReplayActor::PlayAnimation(int animId, const CryCharAnimationParams& params, float speedMultiplier, float animTime)
{
	float adjustedLayerTime = 0;
	ICharacterInstance* pCharacterInstance = GetEntity()->GetCharacter(0);
	if (animTime == 0 || GetAdjustedLayerTime(pCharacterInstance, animId, params, speedMultiplier, animTime, adjustedLayerTime))
	{
		m_animationProxy.StartAnimationById(GetEntity(), animId, params, speedMultiplier);
		ISkeletonAnim* pSkeletonAnim = pCharacterInstance->GetISkeletonAnim();
		pSkeletonAnim->SetLayerNormalizedTime(params.m_nLayerID, adjustedLayerTime);
		ICharacterInstance* pShadowCharacter = GetShadowCharacter();
		if (pShadowCharacter)
		{
			pShadowCharacter->GetISkeletonAnim()->SetLayerNormalizedTime(params.m_nLayerID, adjustedLayerTime);
		}
	}
}

//------------------------------------------------------------------------
void CReplayActor::PlayUpperAnimation(int animId, const CryCharAnimationParams& params, float speedMultiplier, float animTime)
{
	float adjustedLayerTime = 0;
	ICharacterInstance* pCharacterInstance = GetEntity()->GetCharacter(0);
	if (animTime == 0 || GetAdjustedLayerTime(pCharacterInstance, animId, params, speedMultiplier, animTime, adjustedLayerTime))
	{
		m_animationProxyUpper.StartAnimationById(GetEntity(), animId, params, speedMultiplier);
		ISkeletonAnim* pSkeletonAnim = pCharacterInstance->GetISkeletonAnim();
		pSkeletonAnim->SetLayerNormalizedTime(params.m_nLayerID, adjustedLayerTime);
		ICharacterInstance* pShadowCharacter = GetShadowCharacter();
		if (pShadowCharacter)
		{
			pShadowCharacter->GetISkeletonAnim()->SetLayerNormalizedTime(params.m_nLayerID, adjustedLayerTime);
		}
	}
}

//------------------------------------------------------------------------
void CReplayActor::ApplyMannequinEvent(const SMannHistoryItem &mannEvent, float animTime)
{
	if(m_flags&(eRAF_Dead|eRAF_Invisible|eRAF_HaveSpawnedMyCorpse))
		return;

	switch(mannEvent.type)
	{
	case SMannHistoryItem::Fragment:
		{
			if(m_pActionController)
			{
				m_pActionController->Queue(*new CReplayObjectAction(mannEvent.fragment, mannEvent.tagState, mannEvent.optionIdx, mannEvent.trumpsPrevious, 4));
			}
		}
		break;
	case SMannHistoryItem::Tag:
		if(m_pAnimContext)
		{
			m_pAnimContext->state = mannEvent.tagState;
			m_pAnimContext->state.Set(PlayerMannequin.tagIDs.FP, (m_flags&eRAF_FirstPerson)!=0);
		}
		break;
	}
}

//------------------------------------------------------------------------
bool CReplayActor::GetAdjustedLayerTime(ICharacterInstance* pCharacterInstance, int animId, const CryCharAnimationParams& params, float speedMultiplier, float animTime, float &adjustedLayerTime)
{
	ISkeletonAnim* pSkeletonAnim = pCharacterInstance->GetISkeletonAnim();
	IAnimationSet* pAnimationSet = pCharacterInstance->GetIAnimationSet();
	float duration = pAnimationSet->GetDuration_sec(animId);
	animTime *= params.m_fPlaybackSpeed * speedMultiplier;
	CRY_ASSERT_MESSAGE(animTime >= 0, "Animation time shouldn't be less than 0");

	if (duration > 0.0f)
	{
		if (params.m_nFlags & CA_LOOP_ANIMATION)
		{
			animTime = fmod(animTime, duration);
		}
		else if (params.m_nFlags & CA_REPEAT_LAST_KEY)
		{
			animTime = min(animTime, duration);
		}
		else if (animTime >= duration)
		{
			// Animation has finished don't bother playing it...
			return false;
		}

		adjustedLayerTime = animTime / duration;
	}
	else
	{
		if (!(params.m_nFlags & (CA_LOOP_ANIMATION|CA_REPEAT_LAST_KEY)))
		{
			return false;
		}
		else
		{
			adjustedLayerTime = 0.0f;
		}
	}

	return true;
}


//This function was move from CSkeletonAnim to CRecordingSystem
const bool DO_INIT_ONLY_TEST = false;
#define MAX_EXEC_QUEUE (0x8u);
void SBasicReplayMovementParams::SetDesiredLocalLocation2( ISkeletonAnim* pSkeletonAnim, const QuatT& desiredLocalLocation, float lookaheadTime, float fDeltaTime, float turnSpeedMultiplier)
{
	QuatT m_desiredLocalLocationQTX			= desiredLocalLocation;
	assert(m_desiredLocalLocationQTX.q.IsValid());
	f32 m_desiredArrivalDeltaTimeQTX		= lookaheadTime;
	f32 m_desiredTurnSpeedMultiplierQTX = turnSpeedMultiplier;

	uint32 NumAnimsInQueue = pSkeletonAnim->GetNumAnimsInFIFO(0);
	uint32 nMaxActiveInQueue=MAX_EXEC_QUEUE;	
	if (nMaxActiveInQueue>NumAnimsInQueue)
		nMaxActiveInQueue=NumAnimsInQueue;
	if (NumAnimsInQueue>nMaxActiveInQueue)
		NumAnimsInQueue=nMaxActiveInQueue;

	uint32 active=0;
	for (uint32 a=0; a<NumAnimsInQueue; a++)
	{
		CAnimation& anim = pSkeletonAnim->GetAnimFromFIFO(0,a);
		active += anim.IsActivated() ? 1 : 0;
	}

	if (active>nMaxActiveInQueue)
		active=nMaxActiveInQueue;
	nMaxActiveInQueue=active;


	const QuatT &rDesiredLocation = m_desiredLocalLocationQTX;

	const SParametricSampler *pLMG = NULL;
	for (int32 a=nMaxActiveInQueue-1; (a >= 0) && !pLMG; --a)
	{
		const CAnimation& anim = pSkeletonAnim->GetAnimFromFIFO(0,a);
		pLMG = anim.GetParametricSampler();
	}

	Vec3 dir = rDesiredLocation.q * FORWARD_DIRECTION;
	Vec2 predictedDir( dir.x, dir.y );

	float predictedAngle = RAD2DEG(atan2f(-predictedDir.x, predictedDir.y));
	float turnSpeed = m_desiredTurnSpeedMultiplierQTX * DEG2RAD(predictedAngle) / max(0.1f, m_desiredArrivalDeltaTimeQTX);
	float turnAngle = DEG2RAD(predictedAngle);

	Vec2 deltaVector(rDesiredLocation.t.x, rDesiredLocation.t.y);
	float deltaDist = deltaVector.GetLength();

	const float thresholdDistMin = 0.05f;
	const float thresholdDistMax = 0.15f;
	f32 uniform_scale = 1.0f;//m_pInstance->CCharInstance::GetUniformScale();

	Vec2 deltaDir = (deltaDist > 0.0f) ? (deltaVector / deltaDist) : Vec2(0,0);
	float deltaAngle = (deltaDir.x == 0.0f && deltaDir.y == 0.0f  ? 0.0f : RAD2DEG(atan2f(-deltaDir.x, deltaDir.y)) );
	float travelAngle = DEG2RAD(deltaAngle);

	// Update travel direction only if distance bigger (more secure) than thresholdDistMax.
	// Though, also update travel direction if distance is small enough to not have any visible effect (since distance scale is zero).
	bool initOnly = DO_INIT_ONLY_TEST && ((deltaDist > thresholdDistMin) && (deltaDist < thresholdDistMax));

	Vec2 newStrafe = Vec2(-sin_tpl(travelAngle), cos_tpl(travelAngle));
	if (pLMG) 
	{
		SmoothCD(m_desiredStrafeSmoothQTX, m_desiredStrafeSmoothRateQTX, fDeltaTime, newStrafe, 0.10f);
	}
	else 
	{
		//	CryFatalError("CryAnimation: no smooth");
		m_desiredStrafeSmoothQTX=newStrafe;
		m_desiredStrafeSmoothRateQTX.zero();
	}
	travelAngle=Ang3::CreateRadZ(Vec2(0,1),m_desiredStrafeSmoothQTX);
	pSkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelAngle, travelAngle, fDeltaTime);

	float curvingFraction = 0.0f;

	if (pLMG)
	{
		SmoothCD(m_fDesiredTurnSpeedSmoothQTX, m_fDesiredTurnSpeedSmoothRateQTX, fDeltaTime, turnSpeed, 0.40f);
	}
	else
	{
		m_fDesiredTurnSpeedSmoothQTX=turnSpeed;
		m_fDesiredTurnSpeedSmoothRateQTX = 0.0f;
	}
	pSkeletonAnim->SetDesiredMotionParam(eMotionParamID_TurnSpeed, m_fDesiredTurnSpeedSmoothQTX, fDeltaTime);
	pSkeletonAnim->SetDesiredMotionParam(eMotionParamID_TurnAngle, turnAngle, fDeltaTime);

	// Curving Distance	
	float div = cos_tpl(DEG2RAD(90.0f - predictedAngle));
	float curveRadius = (deltaDist / 2.0f);

	if( abs(div) > 0.0f )
		curveRadius = curveRadius / div;
	float circumference = curveRadius * 2.0f * gf_PI;
	float curveDist = circumference * (predictedAngle * 2.0f / 360.0f);
	curveDist = max(deltaDist, curveDist);

	// Travel Speed/Distance
	float travelDist = LERP(deltaDist, curveDist, curvingFraction);
	float travelSpeed = travelDist / max(0.1f, m_desiredArrivalDeltaTimeQTX);
	//travelDist *= travelDistScale;
	//travelSpeed *= travelDistScale;

	//	float fColDebug[4] = {1,0,0,1};
	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 2.6f, fColDebug, false,"qqqtravelSpeed: atime: %f", travelSpeed );	
	//	g_YLine+=26.0f;
	SmoothCD(m_fDesiredMoveSpeedSmoothQTX, m_fDesiredMoveSpeedSmoothRateQTX, fDeltaTime, travelSpeed, 0.04f);
	pSkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelSpeed, m_fDesiredMoveSpeedSmoothQTX, fDeltaTime);
	pSkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelDist, travelDist, fDeltaTime);
}

void CReplayActor::GunRemovalListener::OnEntityEvent( IEntity *pEntity, const SEntityEvent& event )
{
	if(event.event == ENTITY_EVENT_DONE)
	{
		CRY_ASSERT_MESSAGE(pEntity->GetId() == pReplayActor->GetGunId(), "Received event for an entity that wasn't the gun we subscribed to");
		
		pReplayActor->SetGunId(0);
	}
}

void CReplayActor::ApplyRagdollImpulse( pe_action_impulse& impulse )
{
	IPhysicalEntity* pPhys = GetEntity()->GetPhysics();
	if(!pPhys || pPhys->GetType()!=PE_ARTICULATED)
	{
		m_ragdollImpulse = impulse;
	}
	else
	{
		pPhys->Action(&impulse);
	}
}

void CReplayActor::OnRagdollized()
{
	if(!m_ragdollImpulse.impulse.IsZero())
	{
		IPhysicalEntity* pPhys = GetEntity()->GetPhysics();
		CRY_ASSERT(pPhys && pPhys->GetType()==PE_ARTICULATED);
		pPhys->Action(&m_ragdollImpulse);
	}
	m_ragdollImpulse.impulse.zero();
}

void CReplayActor::RemoveGun( const bool bRemoveEntity )
{
	if (m_gunId != 0)
	{
		gEnv->pEntitySystem->RemoveEntityEventListener(m_gunId, ENTITY_EVENT_DONE, &m_GunRemovalListener);
		if(bRemoveEntity)
		{
			gEnv->pEntitySystem->RemoveEntity(m_gunId);
		}
		m_gunId = 0;
	}
}

void CReplayActor::Ragdollize()
{
	// Remove Gun when Ragdollizing.
	RemoveGun(true);	

	// Ragdollize.
	Physicalize();

	// Notify Ragdollized, to apply impulses, etc.
	OnRagdollized();
}

void CReplayActor::TransitionToCorpse(IEntity& corpse)
{
	IEntity* pEntity = GetEntity();
	if(ICharacterInstance *pCharInst = pEntity->GetCharacter(0))
	{
		corpse.SetFlags(corpse.GetFlags() | (ENTITY_FLAG_CASTSHADOW));

		Physicalize();
		pEntity->MoveSlot(&corpse, 0);
		pCharInst->SetFlags(pCharInst->GetFlags() | CS_FLAG_UPDATE);

		LoadCharacter(pCharInst->GetFilePath());

		// Make sure corpse is spawned with no glow parts
		if(CTeamVisualizationManager* pTeamVisManager = g_pGame->GetGameRules()->GetTeamVisualizationManager())
		{
			pTeamVisManager->RefreshTeamMaterial(&corpse, false, m_isFriendly); 
		}
	}
}

void CReplayActor::Physicalize()
{
	IEntity& entity = *GetEntity();
	SEntityPhysicalizeParams entityPhysParams;

	entityPhysParams.nSlot = 0;
	entityPhysParams.mass = 120.0f;
	entityPhysParams.bCopyJointVelocities = true;
	entityPhysParams.nLod = 1;

	pe_player_dimensions playerDim;
	pe_player_dynamics playerDyn;

	playerDyn.gravity.z = 15.0f;
	playerDyn.kInertia = 5.5f;

	entityPhysParams.pPlayerDimensions = &playerDim;
	entityPhysParams.pPlayerDynamics = &playerDyn;

	// Need to physicalize as a living entity first so that the animated bone velocities get transfered to the ragdoll.
	entityPhysParams.type = PE_LIVING;
	entity.Physicalize(entityPhysParams);

	if(!m_velocity.IsZero())
	{
		if(IPhysicalEntity *pPhysicalEntity = entity.GetPhysics())
		{
			pe_action_set_velocity setVel;
			setVel.v = m_velocity;
			pPhysicalEntity->Action(&setVel);
		}
	}

	// Now physicalize as Articulated Physics.
	entityPhysParams.type = PE_ARTICULATED;
	entity.Physicalize(entityPhysParams);

	if(ICharacterInstance* pCharacter = entity.GetCharacter(0))
	{
		pCharacter->GetISkeletonAnim()->StopAnimationsAllLayers();
		if(IAnimationPoseBlenderDir* pIPoseBlenderLook = pCharacter->GetISkeletonPose()->GetIPoseBlenderLook())
		{
			pIPoseBlenderLook->SetState(false);
		}
	}

	if(IPhysicalEntity* pPhysicalEntity = entity.GetPhysics())
	{
		pe_simulation_params sp;
		sp.gravity = sp.gravityFreefall = Vec3(0, 0, -13.f);
		sp.dampingFreefall = 0.0f;
		sp.mass = entityPhysParams.mass * 2.0f;
		pPhysicalEntity->SetParams(&sp);

		// Make sure the ragdolls don't collide with other objects not in the replay
		pe_params_part pp;
		pp.flagsAND = pp.flagsOR = geom_colltype8;
		pp.flagsColliderAND = pp.flagsColliderOR = geom_colltype8;
		pPhysicalEntity->SetParams(&pp);

		// Set the Collision Class.
		g_pGame->GetGamePhysicsSettings()->AddCollisionClassFlags( *pPhysicalEntity, gcc_ragdoll );
	}

	// Set ViewDistRatio.
	if(IEntityRender* pIEntityRender = entity.GetRenderInterface())
	{
		if(IRenderNode* pRenderNode = pIEntityRender->GetRenderNode())
		{
			pRenderNode->SetViewDistRatio(g_pGameCVars->g_actorViewDistRatio);
		}
	}
}

void CReplayActor::SetTeam( int8 team, const bool isFriendly )
{
	m_team = team;
	m_isFriendly = isFriendly;
	// Local, because it only exists locally.
	g_pGame->GetGameRules()->SetTeam(team, GetEntityId(), true);
}

void CReplayActor::AddItem( const EntityId itemId )
{
	m_itemList.AddItem(itemId);
}
