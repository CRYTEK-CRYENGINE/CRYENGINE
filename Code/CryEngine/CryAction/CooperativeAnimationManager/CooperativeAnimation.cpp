// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: synchronized animation playing on multiple actors
   including exact positioning and sliding the actors into position
   -------------------------------------------------------------------------
   History:
   - August 5th, 2008: Created by Michelle Martin
   - February 5th, 2009: Moved to CryAction by David Ramos
*************************************************************************/

#include "StdAfx.h"
#include "CooperativeAnimation.h"
#include "CooperativeAnimationManager.h"
#include "CryActionCVars.h"
#include "IAnimatedCharacter.h"
#include <CryGame/IGameFramework.h>

const static char* COOP_ANIM_NAME = "CoopAnimation";
const static int MY_ANIM = 54545;

CCooperativeAnimation::CCooperativeAnimation()
	: m_isInitialized(false)
	, m_bPauseAG(false)
	, m_activeState(eCS_PlayAndSlide)
	, m_refPoint(ZERO)
	, m_generalParams()
	, m_firstActorStartPosition(ZERO)
	, m_firstActorStartDirection(ZERO)
	, m_firstActorTargetPosition(ZERO)
	, m_firstActorTargetDirection(ZERO)
	, m_firstActorPushedDistance(0.0f)
{}

CCooperativeAnimation::~CCooperativeAnimation()
{
	//Remove any remaining entity listeners
	TCharacterParams::const_iterator itParamsEnd = m_paramsList.end();
	for (TCharacterParams::const_iterator itParams = m_paramsList.begin(); itParams != itParamsEnd; ++itParams)
	{
		IAnimatedCharacter* pAnimatedChar = itParams->pActor;
		if (pAnimatedChar)
		{
			gEnv->pEntitySystem->RemoveEntityEventListener(pAnimatedChar->GetEntityId(), ENTITY_EVENT_DONE, this);
		}
	}

	if (m_activeState != eCS_ForceStopping)
	{
		// Delete the params from the vector
		TCharacterParams::iterator it = m_paramsList.begin();
		TCharacterParams::iterator iend = m_paramsList.end();

		for (; it != iend; ++it)
		{
			if (!(*it).bAnimationPlaying)
				CleanupForFinishedCharacter((*it));
		}
	}

	m_paramsList.clear();
}

bool CCooperativeAnimation::IsUsingActor(const EntityId entID) const
{
	if (!m_isInitialized)
		return false;

	if (m_activeState == eCS_ForceStopping)
		return false;

	// Return true if the actor is in one of the params (and still needed)
	TCharacterParams::const_iterator it = m_paramsList.begin();
	TCharacterParams::const_iterator iend = m_paramsList.end();

	for (; it != iend; ++it)
	{
		// Check if this character is used AND if he is actually still
		// playing the animation (if he has finished, the character isn't needed anymore) AND
		// not being rough aligned or sliding (bAnimationPlaying is FALSE then, but you're
		// using the actor)
		if ((*it).pActor->GetEntityId() == entID &&
		    ((*it).bAnimationPlaying || (m_activeState == eCS_PlayAndSlide)))
			return true;
	}

	return false;
}

bool CCooperativeAnimation::AreActorsAlive()
{
	if (!m_isInitialized)
		return false;

	// Return true if all actors have health above zero
	TCharacterParams::iterator it = m_paramsList.begin();
	TCharacterParams::iterator iend = m_paramsList.end();

	for (; it != iend; ++it)
	{
		IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(it->pActor->GetEntityId());
		CRY_ASSERT(pActor);
		if (pActor)
		{
			if (pActor->IsDead())
				return false;
		}
	}

	return true;
}

bool CCooperativeAnimation::AreActorsValid() const
{
	if (!m_isInitialized)
		return false;

	TCharacterParams::const_iterator itEnd = m_paramsList.end();
	TCharacterParams::const_iterator it = std::find_if(m_paramsList.begin(), itEnd,
	                                                   std::not1(std::mem_fun_ref(&SCharacterParams::IsActorValid)));

	return (it == itEnd);
}

bool CCooperativeAnimation::Update(float dt)
{
	if (!m_isInitialized)
		return false;

	if (!AreActorsValid())
	{
		Stop();
		return false;
	}

	if (!m_generalParams.bIgnoreCharacterDeath && !AreActorsAlive())
	{
		Stop();
		return false;
	}

	switch (m_activeState)
	{
	case eCS_PlayAndSlide:
		{
			bool bAnimFailure, bAllStarted, bAllDone;
			PlayAndSlideCharacters(dt, bAnimFailure, bAllStarted, bAllDone);

			if (bAnimFailure)
			{
				Stop();
				return false;
			}

			if (!bAllStarted)
				break;

			if (bAllDone)
				m_activeState = eCS_WaitForAnimationFinish;
		}

	// no break, so the next case gets executed as well
	case eCS_WaitForAnimationFinish:
		if (AnimationsFinished())
		{
			return false;
		}
		break;

	case eCS_ForceStopping:
		return false;
		break;
	}
	;

	// DEBUG OUTPUT
	if (CCryActionCVars::Get().co_coopAnimDebug)
	{
		IRenderAuxGeom* pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		pRenderAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);

		// the debug output is different depending on the alignment type
		if (m_generalParams.eAlignment == eAF_FirstActorPosition)
		{
			// draw start position
			pRenderAuxGeom->DrawSphere(m_firstActorStartPosition, 0.1f, ColorB(255, 0, 255));
			// draw start direction
			pRenderAuxGeom->DrawLine(m_firstActorStartPosition + Vec3(0, 0, 0.2f), ColorB(255, 0, 255), m_firstActorStartPosition + m_firstActorStartDirection + Vec3(0, 0, 0.2f), ColorB(255, 0, 255));
			// draw target position
			pRenderAuxGeom->DrawSphere(m_firstActorTargetPosition, 0.1f, ColorB(255, 0, 255));
			// draw target direction
			pRenderAuxGeom->DrawLine(m_firstActorTargetPosition + Vec3(0, 0, 0.2f), ColorB(0, 255, 0), m_firstActorTargetPosition + m_firstActorTargetDirection + Vec3(0, 0, 0.2f), ColorB(0, 255, 0));

			// draw the pushed distance
			Vec3 pushDirection = (m_firstActorTargetPosition - m_firstActorStartPosition).GetNormalized();
			pRenderAuxGeom->DrawLine(m_firstActorStartPosition + Vec3(0, 0, 0.05f), ColorB(255, 255, 0), m_firstActorStartPosition + Vec3(0, 0, 0.05f) + pushDirection * m_firstActorPushedDistance, ColorB(255, 255, 0));
		}
		else
		{
			// Draw reference point
			pRenderAuxGeom->DrawSphere(m_refPoint.t, 0.2f, ColorB(255, 0, 0));

			// Draw targets and current entity pos
			TCharacterParams::const_iterator itEnd = m_paramsList.end();
			for (TCharacterParams::const_iterator it = m_paramsList.begin(); it != itEnd; ++it)
			{
				// Target
				pRenderAuxGeom->DrawSphere(it->qTarget.t, 0.2f, ColorB(255, 0, 255));

				// Pos
				pRenderAuxGeom->DrawSphere(it->pActor->GetEntity()->GetWorldPos(), 0.2f, ColorB(0, 0, 255));
			}
		}
	} // end of DEBUG OUTPUT

	return true;
}

void CCooperativeAnimation::Stop()
{
	m_activeState = eCS_ForceStopping;

	TCharacterParams::iterator it = m_paramsList.begin();
	TCharacterParams::iterator iend = m_paramsList.end();

	for (; it != iend; ++it)
	{
		// Don't check if the animation is currently playing, because we want
		// to clean up the current animations to make way for the next ones
		CleanupForFinishedCharacter((*it));
	}
}

int MovementCallback(ICharacterInstance* pCharacter, void* pEntity)
{
	IEntity* pEnt = static_cast<IEntity*>(pEntity);
	ISkeletonAnim* pISkeletonAnim = pEnt->GetCharacter(0)->GetISkeletonAnim();

	// this is the kinematic update
	const QuatT& relmove = pISkeletonAnim->GetRelMovement();

	QuatT chrLoc;
	chrLoc.q = pEnt->GetWorldRotation();
	chrLoc.t = pEnt->GetWorldPos();
	chrLoc = chrLoc * relmove;
	chrLoc.q.NormalizeSafe();

	pEnt->SetWorldTM(Matrix34(chrLoc));

	// Vec3 dir = chrLoc.q * FORWARD_DIRECTION; <- ??

	return 0;
}

bool CCooperativeAnimation::Init(TCharacterParams& characterParams, const SCooperativeAnimParams& generalParams)
{
	if (m_isInitialized)
		return true;

	m_paramsList = characterParams;
	m_generalParams = generalParams;

	m_bPauseAG = generalParams.bLooping || (CCryActionCVars::Get().co_usenewcoopanimsystem != 0); // false will use an old AG based method which needs special setup in each character's AG

	// sanity check parameters
	TCharacterParams::iterator it = m_paramsList.begin();
	TCharacterParams::iterator iend = m_paramsList.end();
	for (; it != iend; ++it)
	{
		SCharacterParams& chrParams = *it;

		if (!chrParams.IsActorValid())
		{
			CryLogAlways("Cooperative Animation Manager was passed an invalid actor");
			return false;
		}

		if (chrParams.sSignalName == "")
		{
			CryLogAlways("Cooperative Animation Manager was not passed a correct signal name");
			return false;
		}

		if (m_bPauseAG)
		{
			IAnimatedCharacter* pAnimChar = chrParams.pActor;
			CRY_ASSERT(pAnimChar && pAnimChar->GetEntity());

			ICharacterInstance* pICharacter = pAnimChar->GetEntity()->GetCharacter(0);
			CRY_ASSERT(pICharacter);

			IAnimationSet* pAnimSet = pICharacter->GetIAnimationSet();
			const int animId = pAnimSet->GetAnimIDByName(chrParams.sSignalName.c_str());
			if (animId < 0)
			{
				GameWarning("Cooperative Animation Manager was passed a non-existing anim (%s) for actor %s", chrParams.sSignalName.c_str(), chrParams.pActor->GetEntity()->GetName());
				return false;
			}

			// Cache FilePathCRC of this anim for efficient streaming queries
			chrParams.animFilepathCRC = pAnimSet->GetFilePathCRCByAnimID(animId);
			CRY_ASSERT(chrParams.animFilepathCRC != 0);
		}
	}

	// Calculate the reference point and calculate the target
	// position and orientation of every character relative to this.
	DetermineReferencePositionAndCalculateTargets();

	if (CCryActionCVars::Get().co_slideWhileStreaming)
	{
		// Gather the location offset information before the animation moves the character
		it = m_paramsList.begin();
		for (; it != iend; ++it)
		{
			SCharacterParams& params = *it;
			GatherSlidingInformation(params);
		}
	}

	//update the passed in params so the instigating code can get information about target positions etc.
	characterParams = m_paramsList;

	// After all parameters are checked,
	// determine the type of needed character alignment
	m_activeState = eCS_PlayAndSlide;

	//Register to listen to the entities involved - if one gets deleted we want to NULL the pActor pointer
	TCharacterParams::const_iterator itParamsEnd = characterParams.end();
	for (TCharacterParams::const_iterator itParams = characterParams.begin(); itParams != itParamsEnd; ++itParams)
	{
		gEnv->pEntitySystem->AddEntityEventListener(itParams->pActor->GetEntityId(), ENTITY_EVENT_DONE, this);
	}

	m_isInitialized = true;
	return true;
}

bool CCooperativeAnimation::Init(SCharacterParams& params1, SCharacterParams& params2, const SCooperativeAnimParams& generalParams)
{
	TCharacterParams characterParams;
	characterParams.push_back(params1);
	characterParams.push_back(params2);

	return Init(characterParams, generalParams);
}

bool CCooperativeAnimation::InitForOne(const SCharacterParams& params, const SCooperativeAnimParams& generalParams)
{
	if (m_isInitialized)
		return true;

	TCharacterParams::iterator it = m_paramsList.push_back(params);
	m_generalParams = generalParams;
	m_bPauseAG = generalParams.bLooping || (CCryActionCVars::Get().co_usenewcoopanimsystem != 0); // false will use an old AG based method which needs special setup in each character's AG

	// sanity check parameters
	if (!params.IsActorValid())
	{
		CryLogAlways("Cooperative Animation Manager was passed an invalid actor");
		return false;
	}

	if (params.sSignalName == "")
	{
		CryLogAlways("Cooperative Animation Manager was not passed a correct animation or signal name");
		return false;
	}

	if (m_bPauseAG)
	{
		SCharacterParams& characterParams = *it;

		IAnimatedCharacter* pAnimChar = characterParams.pActor;
		CRY_ASSERT(pAnimChar && pAnimChar->GetEntity());

		ICharacterInstance* pICharacter = pAnimChar->GetEntity()->GetCharacter(0);
		CRY_ASSERT(pICharacter);

		IAnimationSet* pAnimSet = pICharacter->GetIAnimationSet();
		const int animId = pAnimSet->GetAnimIDByName(characterParams.sSignalName.c_str());
		if (animId < 0)
		{
			GameWarning("Cooperative Animation Manager was passed a non-existing anim (%s) for actor %s", characterParams.sSignalName.c_str(), characterParams.pActor->GetEntity()->GetName());
			return false;
		}

		// Cache FilePathCRC of this anim for efficient streaming queries
		characterParams.animFilepathCRC = pAnimSet->GetFilePathCRCByAnimID(animId);
		CRY_ASSERT(characterParams.animFilepathCRC != 0);
	}

	// Sanity check the alignment type - not all make sense for a single character
	if (generalParams.eAlignment == eAF_WildMatch || generalParams.eAlignment == eAF_FirstActor)
		m_generalParams.eAlignment = eAF_FirstActorNoRot;

	// Calculate the reference point and calculate the target
	// position and orientation of every character relative to this.
	DetermineReferencePositionAndCalculateTargets();

	// After all parameters are checked,
	// determine the type of needed character alignment
	m_activeState = eCS_PlayAndSlide;

	m_isInitialized = true;
	return true;
}

bool CCooperativeAnimation::StartAnimations()
{
	// start animations for all characters
	TCharacterParams::iterator it = m_paramsList.begin();
	TCharacterParams::iterator iend = m_paramsList.end();
	SCharacterParams* params;
	for (int i = 0; it != iend; ++it, ++i)
	{
		// cast into the correct class
		params = &(*it);  // looks overly complicated, but is needed to compile for console

		if (!StartAnimation(*params, i))
			return false;
	}

	return true;
}

bool CCooperativeAnimation::StartAnimation(SCharacterParams& params, int characterIndex)
{
	CRY_ASSERT(params.pActor);
	IAnimatedCharacter* pAnimChar = params.pActor;
	CRY_ASSERT(pAnimChar);

	IAnimationGraphState* pGraphState = pAnimChar->GetAnimationGraphState();
	CRY_ASSERT_MESSAGE(pGraphState, "Cannot retrieve animation graph state");
	if (!pGraphState)
		return false;

	if (!CCryActionCVars::Get().co_slideWhileStreaming)
	{
		// Gather the location offset information before the animation moves the character
		GatherSlidingInformation(params);
	}

	if (m_bPauseAG)
	{
		CAnimationPlayerProxy* pAnimProxy = pAnimChar->GetAnimationPlayerProxy(eAnimationGraphLayer_FullBody);
		CRY_ASSERT(pAnimProxy);
		CryCharAnimationParams AParams;

		// prepare animation parameters
		string sAnimName = params.sSignalName;

		AParams.m_nLayerID = 0;
		AParams.m_fPlaybackSpeed = 1.0f;
		AParams.m_fTransTime = 0.25f;
		AParams.m_fAllowMultilayerAnim = 0;

		AParams.m_nFlags |= CA_REPEAT_LAST_KEY | CA_FORCE_SKELETON_UPDATE | CA_FULL_ROOT_PRIORITY | CA_DISABLE_MULTILAYER;
		if (m_generalParams.bForceStart)
			AParams.m_nFlags |= CA_FORCE_TRANSITION_TO_ANIM;
		if (m_generalParams.bLooping)
			AParams.m_nFlags |= CA_LOOP_ANIMATION;

		AParams.m_nUserToken = MY_ANIM;

		bool bAnimStarted = pAnimProxy->StartAnimation(params.pActor->GetEntity(), sAnimName.c_str(), /*0,*/ AParams);
		if (bAnimStarted)
		{
			CRY_ASSERT(bAnimStarted);

			//		if (characterIndex != 0)  // do not stop the graph for the master (if started from AG, not FG)
			pGraphState->Pause(true, eAGP_PlayAnimationNode);
		}
		else
		{
			return false;
		}
	}
	else
	{
		// If we're currently in a coop anim, we need to check which of the two states we're in and
		// switch to the other one so we can blend nicely into the next anim
		const char* cSignal = "coopanimation";
		const char* cOutPut = pGraphState->QueryOutput("CoopAnim");
		if (cOutPut && cOutPut[0] == '1')
		{
			cSignal = "coopanimation2";
		}

		// Set the signal (the "optional" prevents inputs to be set to defaults when the value doesn't
		// exist. Therefore, running animations won't be interrupted if this fails.
		if (!pGraphState->SetInputOptional("Signal", cSignal))
		{
			CRY_ASSERT_MESSAGE(0, "Could not set animation graph input 'coopanimation'.");
			CryLogAlways("Cooperative Animation start failed - animation graph not prepared?");
			return false;
		}

		// Set the variation
		if (!pGraphState->SetVariationInput("CoopAnimation", params.sSignalName))
		{
			CRY_ASSERT_MESSAGE(0, "Could not set animation graph variation 'CoopAnimation'.");
			CryLogAlways("Cooperative Animation start failed - animation graph not prepared?");
			return false;
		}

		// Note: This would cause the animation currently in layer1 to be removed, whether
		// or not RepeatLastKey is set to "1" in the template of the prev state.
		// So instead an update will have the same effect, since the template has
		// the property interruptCurrAnim set to "1".
		// In rare cases this might not cause the anim graph to jump to the desired state -
		// in this case it can be enforced. (see below)
		//params.pActor->GetAnimationGraphState()->ForceTeleportToQueriedState();

		//double check that the teleport worked
		pGraphState->Update();

		cOutPut = pGraphState->QueryOutput("CoopAnim");
		if (!cOutPut || cOutPut[0] == '\0')
		{
			CRY_ASSERT_MESSAGE(0, "Could not retrieve correct output from AG");
			CryLogAlways("Cooperative Animation - state teleport failed? (Could not retrieve correct output from AG graph state %s)", pGraphState->GetCurrentStateName());

			if (m_generalParams.bForceStart)
			{
				// if the animation should force start anyway, the new state can be teleported to,
				// but this will result in a snap.
				pGraphState->ForceTeleportToQueriedState();
			}
			else
			{
				return false;
			}
		}
	}  // old system

	params.bAnimationPlaying = true;
	params.pActor->SetNoMovementOverride(true);
	//IEntity * pEnt = params.pActor->GetEntity();
	//pEnt->GetCharacter(0)->GetISkeletonAnim()->SetLocomotionMacroCallback(MovementCallback, pEnt);
	SetColliderModeAndMCM(&params);

	return true;
}

bool CCooperativeAnimation::IsAnimationPlaying(const ISkeletonAnim* pISkeletonAnim, const CAnimation* pAnim)
{
	if (m_generalParams.bLooping)
	{
		return true;
	}

	const float ANIMATION_END_THRESHOLD = 0.0f;
	const float fTimeRemaining = (1.f - pISkeletonAnim->GetAnimationNormalizedTime(pAnim)) * pAnim->GetExpectedTotalDurationSeconds();
	if (fTimeRemaining > ANIMATION_END_THRESHOLD)
		return true;

	return false;
}

bool CCooperativeAnimation::AnimationsFinished()
{
	// Checks if all characters have finished playing their animation and return true, if they have

	TCharacterParams::iterator it = m_paramsList.begin();
	TCharacterParams::iterator iend = m_paramsList.end();
	SCharacterParams* params;
	int finishedCount = m_paramsList.size();
	for (; it != iend; ++it)
	{
		// cast into the correct class
		params = &(*it);  // looks overly complicated, but is needed to compile for console

		bool animationPlaying = false;

		if (m_bPauseAG)
		{
			ICharacterInstance* pICharacterInstance = params->pActor->GetEntity()->GetCharacter(0);
			CRY_ASSERT(pICharacterInstance);
			ISkeletonAnim* rISkeletonAnim = pICharacterInstance->GetISkeletonAnim();
			CRY_ASSERT(rISkeletonAnim);

			CAnimation* pAnim = rISkeletonAnim->FindAnimInFIFO(MY_ANIM, 0);
			if (pAnim)
				animationPlaying = IsAnimationPlaying(rISkeletonAnim, pAnim);
		}
		else
		{
			if (params->pActor->GetAnimationGraphState())
			{
				const char* stateName = params->pActor->GetAnimationGraphState()->GetCurrentStateName();
				if (!strnicmp(stateName, "x_coopAnimation", 10))   //this will match to x_coopAnimation2 as well
					animationPlaying = true;
			}
		}

		if (!params->bAnimationPlaying || !animationPlaying)
		{
			--finishedCount;
			if (params->bAnimationPlaying)
			{
				params->bAnimationPlaying = false;
				CleanupForFinishedCharacter(*params);
			}
		}
		else
		{
			SetColliderModeAndMCM(params);
		}
	}

	if (finishedCount <= 0)
		return true;

	return false;
}

void CCooperativeAnimation::GatherSlidingInformation(SCharacterParams& params) const
{
	// figures out how much the characters still need to slide

	// get difference in position and rotation from current position to target position
	Quat pRot;
	Vec3 pPos;

	// get current position and rotation
	IEntity* pEnt = params.pActor->GetEntity();
	CRY_ASSERT(pEnt);
	pRot = pEnt->GetWorldRotation();
	pPos = pEnt->GetWorldPos();

	params.currPos.t = pPos;
	params.currPos.q = pRot;

	// save the remaining differences to the target pos and orientation
	params.qSlideOffset.t = params.qTarget.t - pPos;
	params.qSlideOffset.q = params.qTarget.q * (!pRot);
	params.qSlideOffset.q.NormalizeSafe();

	// Note: After this, the target position (qTarget) in the params can no longer be
	// be used as reference. The animations will most likely move and rotate the locator.
}

void CCooperativeAnimation::PlayAndSlideCharacters(float dt, bool& bAnimFailure, bool& bAllStarted, bool& bAllDone)
{
	// Slide characters into position quickly and return true when done
	bAllStarted = true;
	bAllDone = true;
	bAnimFailure = false;
	bool bAllAnimsReady = true;

	// Streaming handling
	if (m_bPauseAG)
	{
		bAllAnimsReady = UpdateAnimationsStreaming();
		if (!bAllAnimsReady)
		{
			bAllDone = bAllStarted = false;

			if (!CCryActionCVars::Get().co_slideWhileStreaming)
				return;
		}
	}

	TCharacterParams::iterator it = m_paramsList.begin();
	TCharacterParams::iterator iend = m_paramsList.end();
	SCharacterParams* params;
	IEntity* pEnt = NULL;
	IAnimatedCharacter* pAC = NULL;
	Quat pRot;
	Vec3 pPos;
	for (int i = 0; it != iend; ++it, ++i)
	{
		// cast into the correct class
		params = &(*it);  // looks overly complicated, but is needed to compile for console
		pEnt = params->pActor->GetEntity();
		pAC = params->pActor;
		CRY_ASSERT(pEnt && pAC);
		bool animJustStarted = false;

		if (!params->bAnimationPlaying)
		{
			// Delayed start handling
			if (params->fStartDelay > 0.0f)
			{
				params->fStartDelayTimer += dt;
				if (params->fStartDelayTimer < params->fStartDelay)
				{
					bAllStarted = bAllDone = false;
					continue; // Skip any more processing of this character until start delay is up
				}
			}

			if (bAllAnimsReady)
			{
				if (!StartAnimation(*params, i))
				{
					bAnimFailure = true; // tell the calling function to abort the cooperative animation
					bAllStarted = bAllDone = false;
					return;
				}
				else
				{
					animJustStarted = true;
				}
			}
		}

		/*
		    // This commented code will teleport the characters to their target positions directly
		    // and is useful for debugging this system.

		    pEnt->SetPos(params->qTarget.t);
		    pEnt->SetRotation(params->qTarget.q);
		    params->pActor->GetAnimatedCharacter()->SetNoMovementOverride(false);
		    continue;
		 */

		// check if this character still needs to slide or is already done
		if (params->fSlidingTimer >= params->fSlidingDuration)
		{
			pAC->SetNoMovementOverride(false);
			//pEnt->GetCharacter(0)->GetISkeletonAnim()->SetLocomotionMacroCallback(MovementCallback, pEnt);
			continue;
		}

		// calculate the amount of movement that needs to be done this frame
		float slideAmount = dt;

		// check if this is the last step for this character
		if (params->fSlidingTimer + dt >= params->fSlidingDuration)
		{
			slideAmount = params->fSlidingDuration - params->fSlidingTimer;
			pAC->SetNoMovementOverride(false);
			//pEnt->GetCharacter(0)->GetISkeletonAnim()->SetLocomotionMacroCallback(MovementCallback, pEnt);
		}
		else
		{
			// there are more steps necessary
			bAllDone = false;
		}

		// Calculate new character position and rotation
		slideAmount = slideAmount / params->fSlidingDuration;
		pRot = pEnt->GetWorldRotation();
		pPos = pEnt->GetWorldPos();

		/*
		   pPos += slideAmount * params->qSlideOffset.t;
		   pRot = pRot * params->qSlideOffset.q.CreateSlerp(Quat(IDENTITY), params->qSlideOffset.q, slideAmount);
		   pEnt->SetPos(pPos);
		   pEnt->SetRotation(pRot);
		 */

		QuatT testQuat(IDENTITY);
		testQuat = params->currPos;
		Vec3 dist;
		dist = params->qSlideOffset.t;
		dist = dist * slideAmount;
		testQuat.t += dist;

		testQuat.q = testQuat.q * params->qSlideOffset.q.CreateSlerp(Quat(IDENTITY), params->qSlideOffset.q, slideAmount);
		testQuat.q.NormalizeSafe();
		params->currPos = testQuat;

		if (!animJustStarted || CCryActionCVars::Get().co_slideWhileStreaming)
		{
			// consider locator movement in the animation (don't do this if the anim just started because the rel movement won't be from this animation)
			ICharacterInstance* pCharacterInstance = pEnt->GetCharacter(0);
			const QuatT& animRelMovement = pCharacterInstance ? pCharacterInstance->GetISkeletonAnim()->GetRelMovement() : QuatT(IDENTITY);
			params->deltaLocatorMovement = params->deltaLocatorMovement * animRelMovement;

			testQuat = testQuat * params->deltaLocatorMovement;

			// Safety check to guarantee character will not fall through ground
			if (m_generalParams.bPreventFallingThroughTerrain)
				testQuat.t.z = max(testQuat.t.z, gEnv->p3DEngine->GetTerrainElevation(testQuat.t.x, testQuat.t.y));

			pEnt->SetPos(testQuat.t);
			pEnt->SetRotation(testQuat.q);
		}

		// increase the sliding timer for this character
		params->fSlidingTimer += dt;

		// DEBUG OUTPUT
		if (CCryActionCVars::Get().co_coopAnimDebug)
		{
			IRenderAuxGeom* pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
			pRenderAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);

			// draw an indication that this character is being sled
			pRenderAuxGeom->DrawSphere(pPos + Vec3(0, 0, 2.0f), 0.1f, ColorB(255, 255, 0));

			// add to the amount that this character has already been moved
			if (it == m_paramsList.begin())
				m_firstActorPushedDistance += (slideAmount * params->qSlideOffset.t).GetLength();
		} // end of DEBUG OUTPUT
	}

	return;
}

void CCooperativeAnimation::DetermineReferencePositionAndCalculateTargets()
{
	// Right now, only the reference type FirstCharacter is supported
	SCharacterParams* params;

	// Depending on the chosen alignment type the position and orientation of the reference
	// position is determined quite differently.
	switch (m_generalParams.eAlignment)
	{
	case eAF_Location:
		{
			// the actual reference position and orientation has been specified.

			m_refPoint = m_generalParams.qLocation;
		}
		break;
	case eAF_FirstActorPosition:
		{
			// the position and orientation for the first actor has been specified and
			// all must adjust accordingly

			// Reference Point is target position of the first character minus
			// the relative location of his start location
			params = &(*(m_paramsList.begin()));  // looks overly complicated, but is needed to compile for console
			QuatT relativeOffset = GetStartOffset(*params);
			IEntity* pEnt = params->pActor->GetEntity();
			m_refPoint.q = m_generalParams.qLocation.q;

			// rotating the actor rotation backwards with the rotation from the animation
			m_refPoint.q = m_refPoint.q * (!relativeOffset.q);

			// now get the correct world space vector that point from the actor to the
			// ref point (also in world space)
			Vec3 chartoRef = m_refPoint.q * (-relativeOffset.t);
			m_refPoint.t = m_generalParams.qLocation.t + chartoRef;

			// DEBUG OUTPUT
			// Set all the values
			if (CCryActionCVars::Get().co_coopAnimDebug)
			{
				Matrix34 firstActorMat = pEnt->GetWorldTM();
				m_firstActorStartPosition = firstActorMat.GetTranslation();
				m_firstActorStartDirection = firstActorMat.GetColumn1();
				m_firstActorTargetPosition = m_generalParams.qLocation.t;
				m_firstActorTargetDirection = m_generalParams.qLocation.q * FORWARD_DIRECTION;
				m_firstActorPushedDistance = 0.0f;
			} // end of DEBUG OUTPUT

		}
		break;
	case eAF_FirstActor:
	case eAF_FirstActorNoRot:
		{
			// first actor (not maintaining rotation)

			// Reference Point is the position of the first character minus
			// the relative location of his start location
			params = &(*(m_paramsList.begin()));  // looks overly complicated, but is needed to compile for console
			QuatT relativeOffset = GetStartOffset(*params);
			IEntity* pEnt = params->pActor->GetEntity();
			m_refPoint.q = pEnt->GetWorldRotation();

			if (m_generalParams.eAlignment == eAF_FirstActorNoRot)
			{
				m_refPoint.q = relativeOffset.q;
			}
			else
			{
				// rotating the actor rotation backwards with the rotation from the animation
				m_refPoint.q = m_refPoint.q * (!relativeOffset.q);
			}

			// now get the correct world space vector that points from the actor to the
			// ref point (also in world space)
			Vec3 chartoRef = m_refPoint.q * (-relativeOffset.t);
			m_refPoint.t = pEnt->GetWorldPos() + chartoRef;
		}
		break;
	case eAF_WildMatch:
	default:
		{
			// Wild Matching
			// (meaning least amount of movement, rather rotate than change position)

			// FIRST ITERATION: Only support for two characters

			// rotate towards the second character before making this the reference point
			// (if this step is left out, the entity rotation of the actor will be preserved,
			// but that causes big distances for all other characters to their target positions)

			// Reference Point is the position of the first character minus
			// the relative location of his start location
			params = &(*(m_paramsList.begin()));  // looks overly complicated, but is needed to compile for console
			QuatT relativeOffset = GetStartOffset(*params);
			IEntity* pEnt = params->pActor->GetEntity();
			m_refPoint.q = pEnt->GetWorldRotation();

			// rotating the actor rotation backwards with the rotation from the animation
			m_refPoint.q = m_refPoint.q * relativeOffset.q;

			// Get the vector from first to second character as it is in the animation
			SCharacterParams paramsSecond = (m_paramsList.at(1));
			Vec3 secondPos = GetStartOffset(paramsSecond).t;
			secondPos = (secondPos - relativeOffset.t);

			// rotate it into world space
			secondPos = m_refPoint.q * secondPos;
			secondPos.z = 0.0f; // only interested in the rotation around the up vector

			// Get the vector between first and second char in world space
			Vec3 firstToSecW = paramsSecond.pActor->GetEntity()->GetWorldPos() - pEnt->GetWorldPos();
			firstToSecW.z = 0.0f; // only interested in the rotation around the up vector

			bool invalidOffset = false;
			if (firstToSecW.IsZero() || secondPos.IsZero())
				invalidOffset = true; // this is ok, but it means that wild match cannot be used

			secondPos.normalize();
			firstToSecW.normalize();

			// get the rotation between those vectors (z axis rotation, from worldDir to animDir)
			float fDot = secondPos * firstToSecW;
			float angle = acos_tpl(fDot);

			// cross product to determine if the angle needs to be flipped - but only z component is relevant
			// so save processing time and not calculate the full cross
			if (((firstToSecW.x * secondPos.y) - (firstToSecW.y * secondPos.x)) < 0)
				angle = -angle;

			// Create a rotation out of this
			Quat rot(IDENTITY);
			rot.SetRotationAA(angle, Vec3(0.0f, 0.0f, 1.0f));

			// rotate the reference-QuatT by it
			if (rot.IsValid() || invalidOffset)  // safety check
				m_refPoint.q = m_refPoint.q * (!rot);
			else
			{
				CRY_ASSERT_MESSAGE(0, "Possibly an invalid quaternion, or only z deviation in animations.");
			}

			// now get the correct world space vector that point from the actor to the
			// ref point (also in world space)
			Vec3 chartoRef = m_refPoint.q * relativeOffset.t;
			m_refPoint.t = pEnt->GetWorldPos() + chartoRef;
		} // end of wild matching
		break;
	}

	// Now go through all characters and calculate their final target positions
	TCharacterParams::iterator it = m_paramsList.begin();
	TCharacterParams::iterator itend = m_paramsList.end();
	for (; it != itend; ++it)
	{
		// cast into the correct class
		params = &(*it);  // looks overly complicated, but is needed to compile for console

		// Calculate target position
		QuatT relativeOffset = GetStartOffset(*params);
		params->qTarget = m_refPoint * relativeOffset;
		params->qTarget.q.NormalizeSafe();
		//params->qSlideOffset.q.NormalizeSafe();
	}
}

QuatT CCooperativeAnimation::GetStartOffset(SCharacterParams& params)
{
	QuatT retVal(IDENTITY);

	if (!params.pActor)
	{
		CRY_ASSERT_MESSAGE(params.pActor, "No valid Actor received!");
		return retVal;
	}

	ICharacterInstance* pCharacter = params.pActor->GetEntity()->GetCharacter(0);
	CRY_ASSERT(pCharacter != NULL);

	IAnimationSet* pAnimSet = pCharacter->GetIAnimationSet();
	if (pAnimSet == NULL)
		return retVal;

	//string animation0 = pState == NULL ? m_animations[0].c_str() : pState->ExpandVariationInputs( m_animations[0].c_str() ).c_str();
	string animation0 = params.sSignalName;

	int id = pAnimSet->GetAnimIDByName(animation0.c_str());
	if (id < 0)
		return retVal;

	uint32 flags = pAnimSet->GetAnimationFlags(id);
	if ((flags & CA_ASSET_CREATED) == 0)
		return retVal;

	float animDuration = pAnimSet->GetDuration_sec(id);
	if (animDuration < params.fSlidingDuration)
	{
		CRY_ASSERT_MESSAGE(animDuration > params.fSlidingDuration, "Incorrect parameter: Sliding Duration longer than animation.");
		CryLogAlways("Warning: sliding duration longer than actual animation. Will adjust given parameter.");
		params.fSlidingDuration = min(animDuration, animParamDefaultSlidingDuration);
	}

	// the first key will always be in the center so this helper function will
	// reconstruct the actual starting location of the character's root from the
	// original scene
	const bool success = pAnimSet->GetAnimationDCCWorldSpaceLocation(id, retVal);
	retVal.q.NormalizeSafe();

	return retVal;
}

void CCooperativeAnimation::CleanupForFinishedCharacter(SCharacterParams& params)
{
	if ((!params.pActor) || (!params.IsActorValid()))
		return;

	IAnimatedCharacter* pAC = params.pActor;

	const ICooperativeAnimationManager* const pCAManager = gEnv->pGameFramework->GetICooperativeAnimationManager();
	CRY_ASSERT(pCAManager);

	CRY_ASSERT_MESSAGE(!pCAManager->IsActorBusy(params.pActor, this), "Cleaning up for a character that's already playing a second animation");

	// reset the movementOverride for the characters that haven't
	// finished their animations yet (in case of premature termination)
	if (!params.bAllowHorizontalPhysics)
	{
		pAC->SetNoMovementOverride(false);
	}

	pAC->RequestPhysicalColliderMode(eColliderMode_Pushable, eColliderModeLayer_Game, COOP_ANIM_NAME);

	// enable look&aim ik for this character again
	pAC->AllowLookIk(true);
	pAC->AllowAimIk(true);

	if (m_generalParams.bNoCollisionsBetweenFirstActorAndRest)
		IgnoreCollisionsWithFirstActor(params, false);

	if (m_bPauseAG && pAC->GetAnimationGraphState())
	{
		// Release reference
		if (params.animationState != SCharacterParams::AS_Unrequested)
		{
			const bool bReleased = gEnv->pCharacterManager->CAF_Release(params.animFilepathCRC);
			CRY_ASSERT(bReleased);

			params.animationState = SCharacterParams::AS_Unrequested;
		}

		ISkeletonAnim* pISkeletonAnim = params.pActor->GetEntity()->GetCharacter(0)->GetISkeletonAnim();
		CRY_ASSERT(pISkeletonAnim);

		// reactivate animation graph
		pAC->GetAnimationGraphState()->Pause(false, eAGP_PlayAnimationNode);

		pAC->GetAnimationGraphState()->Update();
	}
}

void CCooperativeAnimation::IgnoreCollisionsWithFirstActor(SCharacterParams& characterParams, bool ignore)
{
	IPhysicalEntity* pActorPE = characterParams.pActor->GetEntity()->GetPhysics();
	if (pActorPE)
	{
		if (ignore && !characterParams.collisionsDisabledWithFirstActor)
		{
			IAnimatedCharacter* pFirstActor = (m_paramsList.size() > 1) ? m_paramsList.begin()->pActor : NULL;
			if (pFirstActor && (pFirstActor != characterParams.pActor))
			{
				pe_player_dynamics ppd;
				ppd.pLivingEntToIgnore = pFirstActor->GetEntity()->GetPhysics();

				characterParams.collisionsDisabledWithFirstActor = pActorPE->SetParams(&ppd) != 0;
			}
		}
		else if (!ignore && characterParams.collisionsDisabledWithFirstActor)
		{
			pe_player_dynamics ppd;
			ppd.pLivingEntToIgnore = NULL;
			pActorPE->SetParams(&ppd);

			characterParams.collisionsDisabledWithFirstActor = false;
		}
	}
}

void CCooperativeAnimation::SetColliderModeAndMCM(SCharacterParams* params)
{
	EMovementControlMethod eMoveVertical = eMCM_Animation;

	if (m_bPauseAG)
		params->pActor->ForceRefreshPhysicalColliderMode();

	if (params->bAllowHorizontalPhysics)
	{
		params->pActor->SetMovementControlMethods(eMCM_AnimationHCollision, eMoveVertical);
		params->pActor->RequestPhysicalColliderMode(eColliderMode_Pushable, eColliderModeLayer_Game, COOP_ANIM_NAME);
	}
	else
	{
		if (params->fSlidingTimer >= params->fSlidingDuration)
		{
			//params->pActor->SetNoMovementOverride(true);
		}
		params->pActor->SetMovementControlMethods(eMCM_Animation, eMoveVertical);
		params->pActor->RequestPhysicalColliderMode(eColliderMode_PushesPlayersOnly, eColliderModeLayer_Game, COOP_ANIM_NAME);
	}

	// disable look & aim ik for this character
	ISkeletonPose* pISkeletonPose = params->pActor->GetEntity()->GetCharacter(0)->GetISkeletonPose();
	CRY_ASSERT(pISkeletonPose);

	IAnimationPoseBlenderDir* pIPoseBlenderLook = pISkeletonPose->GetIPoseBlenderLook();
	if (pIPoseBlenderLook)
	{
		pIPoseBlenderLook->SetState(false);
	}

	params->pActor->AllowLookIk(false);
	params->pActor->AllowAimIk(false);

	if (m_generalParams.bNoCollisionsBetweenFirstActorAndRest)
		IgnoreCollisionsWithFirstActor(*params, true);
}

float CCooperativeAnimation::GetAnimationNormalizedTime(const EntityId entID) const
{
	ISkeletonAnim* pISkeletonAnim = gEnv->pEntitySystem->GetEntity(entID)->GetCharacter(0)->GetISkeletonAnim();
	int iAnimCount = pISkeletonAnim->GetNumAnimsInFIFO(0);

	for (int i = 0; i < iAnimCount; ++i)
	{
		const CAnimation& anim = pISkeletonAnim->GetAnimFromFIFO(0, i);
		if (anim.HasUserToken(MY_ANIM))
		{
			return pISkeletonAnim->GetAnimationNormalizedTime(&anim);
		}
	}

	return -1.0f;
}

bool CCooperativeAnimation::UpdateAnimationsStreaming()
{
	CRY_ASSERT(m_bPauseAG);

	uint animsReady = 0;

	TCharacterParams::iterator it = m_paramsList.begin();
	TCharacterParams::iterator iend = m_paramsList.end();
	for (; it != iend; ++it)
	{
		SCharacterParams* params = &(*it);  // looks overly complicated, but is needed to compile for console

		if (params->animationState != SCharacterParams::AS_Ready)
		{
			switch (params->animationState)
			{
			case SCharacterParams::AS_Unrequested:
				{
					// Request
					const bool bRefAdded = gEnv->pCharacterManager->CAF_AddRef(params->animFilepathCRC);
					CRY_ASSERT(bRefAdded);

					params->animationState = SCharacterParams::AS_NotReady;
				}
				break;
			case SCharacterParams::AS_NotReady:
				{
					// Poll for streaming finish
					if (gEnv->pCharacterManager->CAF_IsLoaded(params->animFilepathCRC))
					{
						params->animationState = SCharacterParams::AS_Ready;
						++animsReady;
					}
				}
				break;
			default:
				break;
			}
		}
		else
		{
			++animsReady;
		}
	}

	return (animsReady == m_paramsList.size());
}

void CCooperativeAnimation::OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_DONE)
	{
		EntityId removedId = pEntity->GetId();

		TCharacterParams::const_iterator itParamsEnd = m_paramsList.end();
		for (TCharacterParams::const_iterator itParams = m_paramsList.begin(); itParams != itParamsEnd; ++itParams)
		{
			IAnimatedCharacter* pAnimatedCharacter = itParams->pActor;
			if (pAnimatedCharacter && pAnimatedCharacter->GetEntityId() == removedId)
			{
				gEnv->pEntitySystem->RemoveEntityEventListener(removedId, ENTITY_EVENT_DONE, this);
				itParams->pActor = NULL;
				break;
			}
		}
	}
}
