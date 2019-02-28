// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
// -------------------------------------------------------------------------
//
// Description: 
//  AI-specific functions from CPlayer (which one day should find their way
//  into their own class)
//
////////////////////////////////////////////////////////////////////////////
#ifndef __PLAYER_AI_H__
#define __PLAYER_AI_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "ICryMannequin.h"
#include "AnimActionAIMovement.h"


//////////////////////////////////////////////////////////////////////////
struct SMannequinAIStateParams;

class CAIAnimationState
{
public:
	CAIAnimationState();
	~CAIAnimationState();

	void Init( SAnimationContext* pContext );
	void Reset();

	void SetRequestedStance( const EStance stance );
	EStance GetRequestedStance() const;

	void SetStance( const EStance stance );
	EStance GetStance() const;

	void SetRequestedCoverBodyDirection( const ECoverBodyDirection coverBodyDirection );
	ECoverBodyDirection GetRequestedCoverBodyDirection() const;

	void SetInProgressCoverBodyDirection( const ECoverBodyDirection coverBodyDirection );
	ECoverBodyDirection GetInProgressCoverBodyDirection() const;

	void SetCoverBodyDirection( const ECoverBodyDirection coverBodyDirection );
	ECoverBodyDirection GetCoverBodyDirection() const;

	void SetRequestedCoverAction( const ECoverBodyDirection coverBodyDirection, const char* const actionName );
	void SetRequestClearCoverAction();

	bool HasPendingCoverActionChangeRequest() const;

	void SetRequestedActionCoverBodyDirection( const ECoverBodyDirection coverBodyDirection );
	ECoverBodyDirection GetRequestedActionCoverBodyDirection() const;

	void SetRequestedCoverActionName( const char* const actionName );
	const string& GetRequestedCoverActionName() const;

	void SetCoverActionName( const char* const actionName );
	const string& GetCoverActionName() const;

	const bool IsInCoverAction() const;

	void SetCoverLocation( const QuatT& location );
	void ClearCoverLocation();
	const QuatT* GetCoverLocation() const; // returns NULL when no coverlocation has been set

	void SetPseudoSpeed( float pseudoSpeed ) { m_pseudoSpeed = pseudoSpeed; }
	float GetPseudoSpeed() const { return m_pseudoSpeed; }

private:
	SAnimationContext* m_pContext;
	const SMannequinAIStateParams* m_pAiUserParams;

	EStance m_requestedStance;
	EStance m_stance;

	ECoverBodyDirection m_requestedCoverBodyDirection;
	ECoverBodyDirection m_inProgressCoverBodyDirection;
	ECoverBodyDirection m_coverBodyDirection;

	ECoverBodyDirection m_requestedActionCoverBodyDirection;
	string m_requestedCoverActionName;
	string m_coverActionName;
	QuatT m_coverLocation;
	bool m_hasCoverLocation;

	float m_pseudoSpeed;
};


//////////////////////////////////////////////////////////////////////////
class CAIAnimationComponent
{
public:
	CAIAnimationComponent( IScriptTable* pScriptTable );
	~CAIAnimationComponent();

	// Call InitMannequin to make sure all Actions are properly created.
	// Can safely be called every frame or multiple times in a row.
	//
	// Returns true on success.  On failure this CAIAnimationComponent should
	// not be used.
	bool InitMannequin( IActionController& actionController, const SAnimActionAIMovementSettings& animActionAIMovementSettings );

	void ResetMannequin();

	void RequestAIMovementDetail( CAnimActionAIDetail::EMovementDetail movementDetail );

	CAIAnimationState& GetAnimationState() { return m_animationState; }
	const CAIAnimationState& GetAnimationState() const { return m_animationState; }

	void UpdateAnimationStateRequests( CPlayer& player, const SActorFrameMovementParams &frameMovementParams );
	void UpdateStanceAndCover( CPlayer& player, IActionController* pActionController );

	void UpdateAimingState( ISkeletonPose* pSkeletonPose, const bool aimEnabled, const Vec3& aimTarget, const uint32 aimIkLayer, const float aimIkFadeoutTime );

	bool UpdateLookingState( const bool lookEnabled, const Vec3& lookTarget );

	// Returns true (the default) when:
	// - the locator is always pointing into the wall
	// - the character is aligned to the wall by 'entity driven' rotation.
	// Returns false when:
	// - the locator is pointing 'forward' in the animations.
	// - When within cover typically animation driven motion is picked
	// - Alignment to the wall is done using procedural adjustment clips in Mannequin
	bool GetUseLegacyCoverLocator() const { return m_useLegacyCoverLocator; }

	void ForceStanceTo( CPlayer& player, const EStance targetStance );
	void ForceStanceInAIActorTo( CPlayer& player, const EStance targetStance );

private:
	_smart_ptr< class CAnimActionAIMovement > m_pAnimActionAiMovement;
	_smart_ptr< class CAnimActionAIDetail > m_pAnimActionAiDetail;

	CAIAnimationState m_animationState;

	class CProceduralContextAim* m_pProceduralContextAim;
	class CProceduralContextLook* m_pProceduralContextLook;
	bool m_useLegacyCoverLocator;

	string m_forcedTagList;
};



#endif // __PLAYER_AI_H__