// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Central manager for synchronized animations on multiple actors

   -------------------------------------------------------------------------
   History:
   - August 5th, 2008: Created by Michelle Martin
   - February 5th, 2009: Moved to CryAction by David Ramos
*************************************************************************/

#include "StdAfx.h"
#include "CooperativeAnimationManager.h"
#include "IAnimatedCharacter.h"

// -------------------------------------------
//     Now to the actual implementation
// -------------------------------------------

CCooperativeAnimationManager::~CCooperativeAnimationManager()
{
	// delete all active animations
	TActiveAnimations::iterator it = m_activeAnimations.begin();
	TActiveAnimations::iterator iend = m_activeAnimations.end();
	for (; it != iend; ++it)
		SAFE_DELETE(*it);

	m_activeAnimations.clear();
}

bool CCooperativeAnimationManager::StartNewCooperativeAnimation(TCharacterParams& characterParams, const SCooperativeAnimParams& generalParams)
{
	// tries to start a new cooperative animation

	// By default, the animation should only start if all actors are available
	// But if force start, we will stop any other coop animations these actors are in
	TActiveAnimations::iterator it = m_activeAnimations.begin();
	TActiveAnimations::iterator iend = m_activeAnimations.end();
	for (; it != iend; ++it)
	{
		CCooperativeAnimation* pAnim = *it;
		TCharacterParams::const_iterator itParamsEnd = characterParams.end();
		bool bUsingSomeActor = false;

		for (TCharacterParams::const_iterator itParams = characterParams.begin(); (itParams != itParamsEnd) && !bUsingSomeActor; ++itParams)
		{
			bUsingSomeActor = pAnim->IsUsingActor(itParams->GetActor());
		}

		if (bUsingSomeActor)
		{
			if (generalParams.bForceStart)
			{
				pAnim->Stop();
			}
			else
			{
				return false;
			}
		}
	}

	// create the new animation an push it into the vector of active coop-animations
	CCooperativeAnimation* newAnimation = new CCooperativeAnimation();
	if (!newAnimation->Init(characterParams, generalParams))
	{
		// if initialization fails for some reason, don't continue
		CRY_ASSERT_MESSAGE(0, "Cannot initialize cooperative animation. Init returned false.");
		CryLogAlways("Cannot initialize cooperative animation. Init returned false.");
		SAFE_DELETE(newAnimation);
		return false;
	}

	m_activeAnimations.push_back(newAnimation);

	return true;
}

bool CCooperativeAnimationManager::StartNewCooperativeAnimation(SCharacterParams& params1, SCharacterParams& params2, const SCooperativeAnimParams& generalParams)
{
	TCharacterParams characterParams;
	characterParams.push_back(params1);
	characterParams.push_back(params2);

	bool bReturn = StartNewCooperativeAnimation(characterParams, generalParams);

	params1 = characterParams[0];
	params2 = characterParams[1];

	return bReturn;
}

bool CCooperativeAnimationManager::StartExactPositioningAnimation(const SCharacterParams& params, const SCooperativeAnimParams& generalParams)
{
	// tries to start a new cooperative animation for a single character

	// By default, the animation should only start if all actors are available
	// But if force start, we will stop any other coop animations these actors are in
	TActiveAnimations::iterator it = m_activeAnimations.begin();
	TActiveAnimations::iterator iend = m_activeAnimations.end();
	for (; it != iend; ++it)
	{
		CCooperativeAnimation* pAnim = *it;
		if (pAnim->IsUsingActor(params.GetActor()))
		{
			if (generalParams.bForceStart)
			{
				pAnim->Stop();
			}
			else
			{
				return false;
			}
		}
	}

	// create the new animation an push it into the vector of active coop-animations
	CCooperativeAnimation* newAnimation = new CCooperativeAnimation();
	if (!newAnimation->InitForOne(params, generalParams))
	{
		// if initialization fails for some reason, don't continue
		CRY_ASSERT_MESSAGE(0, "Cannot initialize cooperative animation. Init returned false.");
		CryLogAlways("Cannot initialize cooperative animation. Init returned false.");
		SAFE_DELETE(newAnimation);
		return false;
	}

	m_activeAnimations.push_back(newAnimation);

	return true;
}

void CCooperativeAnimationManager::Update(float dt)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	// if there are no active animations get right back
	if (m_activeAnimations.empty())
		return;

	TActiveAnimations forDeletion;

	// Go through all active animations an update them
	TActiveAnimations::iterator it = m_activeAnimations.begin();
	TActiveAnimations::iterator iend = m_activeAnimations.end();
	for (; it != iend; ++it)
	{
		// If the coop-animation returns false, it will be removed from the vector
		if (!(*it)->Update(dt))
		{
			// remember this one for deletion
			forDeletion.push_back((*it));
		}
	}

	// remove all that are to be deleted
	it = forDeletion.begin();
	iend = forDeletion.end();
	TActiveAnimations::iterator eraseMe;
	for (; it != iend; ++it)
	{
		CCooperativeAnimation* cAnim = (*it);
		eraseMe = std::find(m_activeAnimations.begin(), m_activeAnimations.end(), cAnim);
		SAFE_DELETE(cAnim);
		if (eraseMe != m_activeAnimations.end())
			m_activeAnimations.erase(eraseMe);
	}

}

bool CCooperativeAnimationManager::IsActorBusy(const IAnimatedCharacter* pActor, const CCooperativeAnimation* pIgnoreAnimation /*=NULL*/) const
{
	// Check if the actors is already in a cooperative animation first
	TActiveAnimations::const_iterator it = m_activeAnimations.begin();
	TActiveAnimations::const_iterator iend = m_activeAnimations.end();
	for (; it != iend; ++it)
	{
		if (*it && (*it) != pIgnoreAnimation && (*it)->IsUsingActor(pActor))
			return true;
	}

	return false;
}

bool CCooperativeAnimationManager::IsActorBusy(const EntityId entID) const
{
	// Check if the actors is already in a cooperative animation first
	TActiveAnimations::const_iterator it = m_activeAnimations.begin();
	TActiveAnimations::const_iterator iend = m_activeAnimations.end();
	for (; it != iend; ++it)
	{
		if (*it && (*it)->IsUsingActor(entID))
			return true;
	}

	return false;
}

bool CCooperativeAnimationManager::StopCooperativeAnimationOnActor(IAnimatedCharacter* pActor)
{
	bool retVal = false;

	TActiveAnimations::iterator it = m_activeAnimations.begin();
	TActiveAnimations::iterator iend = m_activeAnimations.end();
	for (; it != iend; ++it)
	{
		CCooperativeAnimation* pAnim = *it;
		if (pAnim->IsUsingActor(pActor))
		{
			pAnim->Stop();
			retVal = true;
		}
	}

	return retVal;
}

bool CCooperativeAnimationManager::StopCooperativeAnimationOnActor(IAnimatedCharacter* pActor1, IAnimatedCharacter* pActor2)
{
	bool retVal = false;

	TActiveAnimations::iterator it = m_activeAnimations.begin();
	TActiveAnimations::iterator iend = m_activeAnimations.end();
	for (; it != iend; ++it)
	{
		CCooperativeAnimation* pAnim = *it;
		if (pAnim->IsUsingActor(pActor1) && pAnim->IsUsingActor(pActor2))
		{
			pAnim->Stop();
			retVal = true;
		}
	}

	return retVal;
}

void CCooperativeAnimationManager::Reset()
{
	// stop all existing animations
	TActiveAnimations::iterator it = m_activeAnimations.begin();
	TActiveAnimations::iterator iend = m_activeAnimations.end();
	for (; it != iend; ++it)
		SAFE_DELETE(*it);

	stl::free_container(m_activeAnimations);
}
