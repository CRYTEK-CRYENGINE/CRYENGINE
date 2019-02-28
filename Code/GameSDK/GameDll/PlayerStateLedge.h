// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements the Player ledge state

-------------------------------------------------------------------------
History:
- 9.9.10: Created by Stephen M. North

*************************************************************************/
#pragma once

#ifndef __PlayerStateLedge_h__
#define __PlayerStateLedge_h__

#include <CryNetwork/ISerialize.h>
#include "PlayerStateEvents.h"
#include "Environment/LedgeManager.h"

class CPlayer;
struct SStateEventLedge;
struct SCharacterMoveRequest;

enum ELedgeType
{
	eLT_Any,
	eLT_Wide,
	eLT_Thin,
};

class CPlayerStateLedge
{
public:
	static void SetParamsFromXml(const IItemParamsNode* pParams) 
	{
		GetLedgeGrabbingParams().SetParamsFromXml( pParams );
	}

public:
	 CPlayerStateLedge();
	~CPlayerStateLedge();

	ILINE bool IsOnLedge() const { return (m_onLedge); }

	void OnEnter( CPlayer& player, const SStateEventLedge& ledgeEvent );
	void OnPrePhysicsUpdate( CPlayer& player, const SActorFrameMovementParams& movement, float frameTime );
	void OnAnimFinished( CPlayer &player );
	void OnExit( CPlayer& player );

	void Serialize( TSerialize serializer );
	void PostSerialize( CPlayer& player );

	static bool TryLedgeGrab( CPlayer& player, const float expectedJumpEndHeight, const float startJumpHeight, const bool bSprintJump, SLedgeTransitionData* pLedgeState, const bool ignoreCharacterMovementWhenFindingLedge );

	static bool CanGrabOntoLedge( const CPlayer& player );
	static void UpdateNearestGrabbableLedge( const CPlayer& player, SLedgeTransitionData* pLedgeState, const bool ignoreCharacterMovement );
	static SLedgeTransitionData::EOnLedgeTransition GetBestTransitionToLedge(const CPlayer& player,const Vec3& refPosition, const LedgeId& ledgeId, SLedgeTransitionData* pLedgeState);

	// Debug drawing of nearest ledge
#ifndef _RELEASE
	static void DebugDraw(const SLedgeTransitionData* pLedgeState);
#endif

private:

	float	m_ledgeSpeedMultiplier;
	float	m_lastTimeOnLedge;
	Vec3 m_exitVelocity;
	Vec3 m_ledgePreviousPosition;
	Vec3 m_ledgePreviousPositionDiff;
	Quat m_ledgePreviousRotation;
	LedgeId m_ledgeId;
	bool m_onLedge;
	bool m_enterFromGround;
	bool m_enterFromSprint;
	uint8 m_postSerializeLedgeTransition;

	// parameters to use while blending towards/from a ledge (also used to synch code with animation)
	struct SLedgeBlending
	{
		SLedgeBlending()
			:	
		m_qtTargetLocation(ZERO),
		m_forwardDir(ZERO)
		{
		}

		QuatT m_qtTargetLocation;
		Vec3  m_forwardDir;
	} m_ledgeBlending;

	// data structs used to collect parameters used for blending in ledge climbing
	struct SLedgeBlendingParams
	{
		SLedgeBlendingParams() 
			: m_fMoveDuration(0.0f)
			, m_fCorrectionDuration(0.0f)
			, m_vPositionOffset(ZERO)
			, m_vExitVelocity(0.f, 1.f, 1.f)
			, m_ledgeType(eLT_Any)
			, m_bIsVault(false)
			, m_bIsHighVault(false)
			, m_bKeepOrientation(false)
			, m_bEndFalling(false)
		{
		}

		void SetParamsFromXml(const IItemParamsNode* pParams);

		float		m_fMoveDuration;
		float		m_fCorrectionDuration;
		Vec3		m_vPositionOffset;
		Vec3		m_vExitVelocity;
		ELedgeType m_ledgeType;
		bool m_bIsVault;
		bool m_bIsHighVault;
		bool m_bKeepOrientation;
		bool m_bEndFalling;
	};

	struct SLedgeNearbyParams
	{
		SLedgeNearbyParams() : m_vSearchDir(ZERO), m_fMaxDistance(1.2f), m_fMaxAngleDeviationFromSearchDirInDegrees(45.0f), m_fMaxExtendedAngleDeviationFromSearchDirInDegrees(50.f) {}
		void SetParamsFromXml(const IItemParamsNode* pParams);

		Vec3 m_vSearchDir;
		float m_fMaxDistance;
		float m_fMaxAngleDeviationFromSearchDirInDegrees;
		float m_fMaxExtendedAngleDeviationFromSearchDirInDegrees;
	};

	struct SLedgeGrabbingParams
	{
		SLedgeGrabbingParams() : m_fNormalSpeedUp(1.0f), m_fMobilitySpeedUp(1.5f), m_fMobilitySpeedUpMaximum(1.5f) {}
		void SetParamsFromXml(const IItemParamsNode* pParams);

		float m_fNormalSpeedUp;
		float m_fMobilitySpeedUp;
		float m_fMobilitySpeedUpMaximum;
		SLedgeNearbyParams m_ledgeNearbyParams;
		SLedgeBlendingParams m_grabTransitionsParams[SLedgeTransitionData::eOLT_MaxTransitions];
	};

	static bool CanReachPlatform(const CPlayer& player, const Vec3& ledgePosition, const Vec3& refPosition);
	static bool IsCharacterMovingTowardsLedge(const CPlayer& player, const LedgeId& ledgeId);
	static bool IsLedgePointingTowardCharacter(const CPlayer& player, const LedgeId&	 ledgeId);
	QuatT CalculateLedgeOffsetLocation( const Matrix34& worldMat, const Vec3& vPositionOffset, const bool keepOrientation ) const;
	void StartLedgeBlending( CPlayer& player, const SLedgeBlendingParams &positionParams);
	float GetLedgeGrabSpeed( const CPlayer& player ) const;

private:

	static SLedgeGrabbingParams s_ledgeGrabbingParams;
	static SLedgeGrabbingParams& GetLedgeGrabbingParams();

};

#endif // __PlayerStateLedge_h__
