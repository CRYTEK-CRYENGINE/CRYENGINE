// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Central manager for synchronized animations on multiple animated characters/actors
   -------------------------------------------------------------------------
   History:
   - August 5th, 2008: Created by Michelle Martin
   - February 5th, 2009: Moved to CryAction by David Ramos
*************************************************************************/

#ifndef __I_COOPERATIVEANIMATIONMANAGER_H__
#define __I_COOPERATIVEANIMATIONMANAGER_H__

#include <CryEntitySystem/IEntity.h>
#include "IAnimatedCharacter.h"
#include "IActorSystem.h"

class CCooperativeAnimation;
class SCharacterParams;
struct SCooperativeAnimParams;
struct IAnimatedCharacter;

typedef DynArray<SCharacterParams> TCharacterParams;

/// Cooperative Animation Manager Common Interface
struct ICooperativeAnimationManager
{
	virtual ~ICooperativeAnimationManager(){}
	// update function for every frame
	// dt is the time passed since last frame
	virtual void Update(float dt) = 0;

	virtual void Reset() = 0;

	//! Start a new Cooperative animation
	//! Returns true if the animation was started
	//! If one of the specified actors is already part of a cooperative animation,
	//! the function will return false.
	virtual bool StartNewCooperativeAnimation(TCharacterParams& characterParams, const SCooperativeAnimParams& generalParams) = 0;

	//! Start a new Cooperative animation
	//! Returns true if the animation was started
	//! If one of the specified actors is already part of a cooperative animation,
	//! the function will return false.
	virtual bool StartNewCooperativeAnimation(SCharacterParams& params1, SCharacterParams& params2, const SCooperativeAnimParams& generalParams) = 0;

	//! This neat function allows to abuse the Cooperative Animation System for just a single
	//! character - taking advantage of the exact positioning
	virtual bool StartExactPositioningAnimation(const SCharacterParams& params, const SCooperativeAnimParams& generalParams) = 0;

	//! Stops all cooperative animations on this actor
	//! Returns true if there was at least one animation stopped, false otherwise.
	virtual bool StopCooperativeAnimationOnActor(IAnimatedCharacter* pActor) = 0;

	//! Same as above, but only stops cooperative animations that include both actors
	virtual bool StopCooperativeAnimationOnActor(IAnimatedCharacter* pActor1, IAnimatedCharacter* pActor2) = 0;

	virtual bool IsActorBusy(const IAnimatedCharacter* pActor, const CCooperativeAnimation* pIgnoreAnimation = NULL) const = 0;
	virtual bool IsActorBusy(const EntityId entID) const = 0;
};

//! Determines what information is to be used as reference
//! to align the actors for the animations
enum EAlignmentRef
{
	eAF_WildMatch = 0,      //! the best location that involves the least movement will be picked
	eAF_FirstActor,         //! the first actor will not move at all (but can be rotated), all others adjust to him
	eAF_FirstActorNoRot,    //! the first actor will not move or rotate at all, all others adjust to him
	eAF_FirstActorPosition, //! the target pos and rot of the first actor is specified in general params, all others adjust to that
	eAF_Location,           //! the reference location of the animation is specified in general params
	eAF_Max,
};

// Default values
const float animParamDefaultDistanceForSliding = 0.4f;
const float animParamDefaultSlidingDuration    = 0.15f;          // this should ideally be the same value that is set in the AG for transition time
const EAlignmentRef animParamDefaultAlignment  = eAF_WildMatch;
const QuatT animParamDefaultLoc                = QuatT(IDENTITY);
// END Default Values

//! Parameters needed per character
class SCharacterParams
{
	friend class CCooperativeAnimation;

public:
	IAnimatedCharacter* GetActor() const;
	bool                IsActorValid() const;
	void                SetStartDelay(float delay)             { fStartDelay = delay; }
	void                SetAllowHorizontalPhysics(bool bAllow) { bAllowHorizontalPhysics = bAllow; }
	QuatT               GetTargetQuatT()                       { return qTarget; }

	SCharacterParams(IAnimatedCharacter* actor, const char* signalName, bool allowHPhysics = false, float slidingDuration = animParamDefaultSlidingDuration) : deltaLocatorMovement(IDENTITY),
		collisionsDisabledWithFirstActor(0), animationState(AS_Unrequested), animFilepathCRC(0)
	{
		CRY_ASSERT_MESSAGE(actor != NULL, "Invalid parameter. The actor here may not be NULL");

		CRY_ASSERT_MESSAGE(signalName != NULL, "Invalid parameter. The signal name cannot be NULL");
		CRY_ASSERT_MESSAGE(strcmp(signalName, "") != 0, "Invalid parameter. The signal name cannot be empty");

		CRY_ASSERT_MESSAGE(slidingDuration > 0.0f, "Invalid parameter. The sliding duration cannot be <= 0.0f");

		bAnimationPlaying = false;
		bAllowHorizontalPhysics = allowHPhysics;
		entityId = actor->GetEntityId();
		pActor = actor;
		fSlidingDuration = (slidingDuration > 0.0f) ? slidingDuration : animParamDefaultSlidingDuration;
		fSlidingTimer = 0.0f;
		fStartDelay = 0.0f;
		fStartDelayTimer = 0.0f;

		qTarget = QuatT(Vec3(ZERO), IDENTITY);
		qSlideOffset = QuatT(Vec3(ZERO), IDENTITY);
		currPos = QuatT(Vec3(ZERO), IDENTITY);

		sSignalName = signalName;
	}

private:
	enum eAnimationState
	{
		AS_Unrequested,
		AS_NotReady,
		AS_Ready,
	};

	SCharacterParams() {}  // default constructor only available to CCooperativeAnimation

	// cached pointer to the actor
	mutable IAnimatedCharacter* pActor;

	// EntityId which owns the actor we need
	EntityId entityId;

	// time (in secs) this character has to slide into position while animation is already playing
	float fSlidingDuration;

	//! timer for when the character is moved over time (internal use in the CCooperativeAnim only)
	// Note: This a bit more complicated way of sliding is necessary to allow the character to move away
	// within the animation while still sliding, otherwise it locators have to be fixed in the animation
	// during the sliding duration and that constricts animators and is generally error-prone.
	float fSlidingTimer;

	// time (in secs) to delay starting this character's sliding and animation
	// Generally not needed, but useful for e.g. melee hit reactions, to start the animation of the character
	// being hit later than the one hitting
	float fStartDelay;

	//! timer for when the character animation should be started (internal use in the CCooperativeAnim only)
	float fStartDelayTimer;

	// name of the signal to the animation graph
	string sSignalName;

	// Target position and rotation of this character (internal use in the CCooperativeAnim only)
	QuatT qTarget;

	//! Value for the offset when sliding starts (internal use in the CCooperativeAnim only)
	// Needed because the animation will start to move the character away from his starting
	// position, so ::qTargetPos cannot be used as reference after the animation has started.
	// This relative offset will be applied over time.
	QuatT qSlideOffset;

	// Needed for internal sliding purposes
	QuatT currPos;

	//! Internal use only. Needed for the Play&Slide step, so locator movement is properly applied
	QuatT deltaLocatorMovement;

	//! Internal use only. Caches the FilePathCRC of the anim when using non-animgraph-based mode
	uint32 animFilepathCRC;

	// checking if the character has finished this coop action (he might already have started another)
	bool bAnimationPlaying;

	// if this is true, the animation will not clip into solid objects, but this can then affect
	// and falsify the relative positioning of the characters towards each other.
	bool bAllowHorizontalPhysics;

	//! Internal use only. For disabling collisions between first actor and the rest
	bool collisionsDisabledWithFirstActor;

	//! Internal use only. Reflects the state of the animation for this character when using non-animgraph-based mode
	uint8 animationState;
};

//! General Parameters for the animation
struct SCooperativeAnimParams
{
	//! Set to true if the animations shall be started no matter what.
	//! This can mean interrupting other coopAnims, and can result in animation snapping.
	bool bForceStart;

	//! true if this is supposed to be a looping animation
	bool bLooping;

	//! true if we don't want to abort animation if one of the characters dies (useful for e.g. melee killing moves)
	bool bIgnoreCharacterDeath;

	//! Defines the way the characters are aligned to each other
	EAlignmentRef eAlignment;

	//! Position and orientation that animation is to be started from (eAlignment will determine it's specific meaning)
	QuatT qLocation;

	//! true to enable the terrain elevation check
	bool bPreventFallingThroughTerrain;

	// disables collisions between first actor and the others
	bool bNoCollisionsBetweenFirstActorAndRest;

	SCooperativeAnimParams(bool forceStart = false, bool looping = false, EAlignmentRef alignment = animParamDefaultAlignment,
	                       const QuatT& location = animParamDefaultLoc, bool bPreventFallingThroughTerrain = true)
		: bForceStart(forceStart)
		, bLooping(looping)
		, bIgnoreCharacterDeath(false)
		, eAlignment(alignment)
		, qLocation(location)
		, bPreventFallingThroughTerrain(bPreventFallingThroughTerrain)
		, bNoCollisionsBetweenFirstActorAndRest(false)
	{}
};

//////////////////////////////////////////////////////////////////////////
ILINE IAnimatedCharacter* SCharacterParams::GetActor() const
{
	return pActor;
}

//////////////////////////////////////////////////////////////////////////
ILINE bool SCharacterParams::IsActorValid() const
{
	// No animated character or not Character instance makes an actor invalid
	return (pActor != NULL) && pActor->GetEntity() && (pActor->GetEntity()->GetCharacter(0) != NULL);
}

#endif //__I_COOPERATIVEANIMATIONMANAGER_H__
