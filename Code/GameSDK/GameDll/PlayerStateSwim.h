// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Audio/AudioSignalPlayer.h"

struct SActorFrameMovementParams;
class CPlayer;
class CPlayerStateSwim_WaterTestProxy;
class CPlayerStateSwim
{
public:
	static void SetParamsFromXml(const IItemParamsNode* pParams) 
	{
		GetSwimParams().SetParamsFromXml( pParams );
	}

	CPlayerStateSwim();

	void OnEnter( CPlayer& player );
	bool OnPrePhysicsUpdate( CPlayer& player, const SActorFrameMovementParams& movement, float frameTime );
	void OnUpdate( CPlayer& player, float frameTime );
	void OnExit( CPlayer& player );
	static void UpdateAudio(CPlayer const& player);
	bool DetectJump(CPlayer& player, const SActorFrameMovementParams& movement, float frameTime, float* pVerticalSpeedModifier) const;

private:

	Vec3 m_gravity;
	float m_lastWaterLevel;
	float m_lastWaterLevelTime;
	float m_verticalVelDueToSurfaceMovement; // e.g. waves.
	float m_headUnderWaterTimer;
	bool m_onSurface;
	bool m_bStillDiving;
	static CryAudio::ControlId m_submersionDepthParam;
	static float m_previousSubmersionDepth;

	struct CSwimmingParams
	{
		CSwimmingParams() 
			: m_swimSpeedSprintSpeedMul(2.5f)
			, m_swimUpSprintSpeedMul(2.0f)
			, m_swimSprintSpeedMul(1.4f)
			, m_stateSwim_animCameraFactor(0.25f)
			, m_swimDolphinJumpDepth(0.1f)
			,	m_swimDolphinJumpThresholdSpeed(3.0f)
			, m_swimDolphinJumpSpeedModification(0.0f)
		{}
		void SetParamsFromXml(const IItemParamsNode* pParams);

		float m_swimSpeedSprintSpeedMul;
		float m_swimUpSprintSpeedMul;
		float m_swimSprintSpeedMul;
		float m_stateSwim_animCameraFactor;

		float m_swimDolphinJumpDepth;
		float m_swimDolphinJumpThresholdSpeed;
		float m_swimDolphinJumpSpeedModification;
	};

	static CSwimmingParams s_swimParams;
	static CSwimmingParams& GetSwimParams() { return s_swimParams; }

	// DO NOT IMPLEMENT!
	CPlayerStateSwim( const CPlayerStateSwim& );
	CPlayerStateSwim& operator=( const CPlayerStateSwim& );
};
