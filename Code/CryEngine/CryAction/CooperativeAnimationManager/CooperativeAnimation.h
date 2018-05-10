// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: synchronized animation playing on multiple actors

   -------------------------------------------------------------------------
   History:
   - August 5th, 2008: Created by Michelle Martin
   - February 5th, 2009: Moved to CryAction by David Ramos
*************************************************************************/

#ifndef __COOPERATIVEANIMATION_H__
#define __COOPERATIVEANIMATION_H__

#include <ICooperativeAnimationManager.h>

struct IAnimatedCharacter;

class CCooperativeAnimation : public IEntityEventListener
{
public:
	// constructor for two-person-animations
	CCooperativeAnimation();

	// destructor
	~CCooperativeAnimation();

	//! Update function to be called every frame
	//! When the animation is finished, it will return false;
	bool Update(float dt);

	//! Force cooperative animation to stop
	//! Doesn't do any fancy finish, just makes next update return false
	void Stop();

	// Check if an actor is in use by this coop-animation
	ILINE bool IsUsingActor(const IAnimatedCharacter* pActor) const { CRY_ASSERT(pActor); return pActor && IsUsingActor(pActor->GetEntityId()); }
	bool       IsUsingActor(const EntityId entID) const;

	// Check if all actors in coop-animation are still alive
	bool AreActorsAlive();

	// Check if all actors involved are still valid
	bool AreActorsValid() const;

	// retrieve the animation time for this entity's animation
	float GetAnimationNormalizedTime(const EntityId entID) const;

	// Check if we are in the vertical correction blendout phase (animations have finished, we're just reorienting characters now)
	bool DoingVerticalCorrectionBlendout() const;

	// Initialize the coop-animation
	bool Init(TCharacterParams& characterParams, const SCooperativeAnimParams& generalParams);
	bool Init(SCharacterParams& params1, SCharacterParams& params2, const SCooperativeAnimParams& generalParams);

	bool InitForOne(const SCharacterParams& params, const SCooperativeAnimParams& generalParams);

	//Keep track of entities involved in the animation to ensure they are still valid
	virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event);

private:

	enum ECoopAnimState
	{
		eCS_PlayAndSlide,
		eCS_WaitForAnimationFinish,
		eCS_ForceStopping,
	};

	//! Depending on parameters and positions this function determines a reference position
	//! to which all other animations are relative. Then it calculates Target positions for
	//! all characters.
	void DetermineReferencePositionAndCalculateTargets();

	//! Determines how much this character needs to slide
	//! to get into position. It sets the qSlideOffset value for the character
	//! and only needs to be called once.
	void GatherSlidingInformation(SCharacterParams& params) const;

	//! Pushes the characters over the last centimeters into position
	//! while the animation is already playing.
	//! Starts animation at the appropriate time (immediately or with a
	//! delay if start delay has been set
	void PlayAndSlideCharacters(float dt, bool& bAnimFailure, bool& bAllStarted, bool& bAllDone);

	//! Starts the animations for all characters
	bool StartAnimations();

	//! Starts the animation for the given character
	bool StartAnimation(SCharacterParams& params, int characterIndex);

	//! Returns true when the animation for all characters are finished
	bool AnimationsFinished();

	bool IsAnimationPlaying(const ISkeletonAnim* pISkeletonAnim, const CAnimation* pAnim);

	//! Cleans up after a character has finished playing his animation.
	//! Since this has to be done when either the animation is finished,
	//! forcefully stopped, or the CoopAnimation gets destroyed, it is
	//! safer to put everything in this separate function instead of
	//! maintaining duplicate code.
	void CleanupForFinishedCharacter(SCharacterParams& params);

	//! Returns the relative StartLocation for a specific character
	//! as it is stored in the animation, i.e. the offset of the
	//! character's root from the animator scene's origin
	QuatT GetStartOffset(SCharacterParams& params);

	//! To avoid code duplication, this function sets the necessary
	//! collider mode and movement control method for each character.
	void SetColliderModeAndMCM(SCharacterParams* params);

	//! Disables/Reenables collisions between first actor and the rest
	void IgnoreCollisionsWithFirstActor(SCharacterParams& characterParams, bool ignore);

	//! Handles streaming state of the involved anims
	bool UpdateAnimationsStreaming();

	bool                   m_isInitialized;
	bool                   m_bPauseAG;
	ECoopAnimState         m_activeState;
	QuatT                  m_refPoint;
	SCooperativeAnimParams m_generalParams;

	// saving the params for all actors involved in this cooperative animation
	TCharacterParams m_paramsList;

	// DEBUG OUTPUT STUFF
	Vec3  m_firstActorStartPosition;
	Vec3  m_firstActorStartDirection;
	Vec3  m_firstActorTargetPosition;
	Vec3  m_firstActorTargetDirection;
	float m_firstActorPushedDistance;
	// end of DEBUG OUTPUT STUFF

};

#endif
