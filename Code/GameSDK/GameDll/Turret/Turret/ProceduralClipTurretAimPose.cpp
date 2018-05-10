// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAnimationPoseModifier.h>

#include <ICryMannequin.h>

#include <CryExtension/CryCreateClassInstance.h>

#include <CryMath/Cry_Geo.h>
#include <CryMath/Cry_GeoDistance.h>

#include "ProceduralContextTurretAimPose.h"
#include "ICryMannequinDefs.h"
#include <Mannequin/Serialization.h>


struct STurretAimIKParams 
	: public IProceduralParams
{
	STurretAimIKParams()
		: blendTime(1.0f)
		, layer(4.0f)
		, horizontalAimSmoothTime(0.5f)
		, verticalAimSmoothTime(0.0f)
		, maxYawDegreesSecond(180.0f)
		, maxPitchDegreesSecond(50.0f)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(Serialization::Decorators::AnimationName<SAnimRef>(animRef), "Animation", "Animation");
		ar(blendTime, "BlendTime", "Blend Time");
		ar(layer, "Layer", "Layer");
		ar(horizontalAimSmoothTime, "HorizontalAimSmoothTime", "HorizontalAimSmoothTime");
		ar(verticalAimSmoothTime, "VerticalAimSmoothTime", "Vertical Aim Smooth Time");
		ar(maxYawDegreesSecond, "MaxYawDegreesSecond", "Max Yaw Degrees Second");
		ar(maxPitchDegreesSecond, "MaxPitchDegreesSecond", "Max Pitch Degrees Second");
	}

	SAnimRef animRef;

	float blendTime;
	float layer;

	float horizontalAimSmoothTime;
	float verticalAimSmoothTime;

	float maxYawDegreesSecond;
	float maxPitchDegreesSecond;
};

struct SProceduralClipWeightHelper
{
public:
	SProceduralClipWeightHelper()
		: m_weight( 0 )
		, m_weightChangeRate( 0 )
	{
	}

	void OnEnter( const float blendInSeconds )
	{
		assert( 0 <= blendInSeconds );
		if ( blendInSeconds == 0 )
		{
			m_weight = 1;
			m_weightChangeRate = 0;
		}
		else
		{
			m_weightChangeRate = ( 1 - m_weight ) / blendInSeconds;
		}
	}

	void Update( const float elapsedSeconds )
	{
		const float weightDelta = m_weightChangeRate * elapsedSeconds;
		const float newWeight = m_weight + weightDelta;
		m_weight = clamp_tpl( newWeight, 0.f, 1.f );
	}

	void OnExit( const float blendOutSeconds )
	{
		assert( 0 <= blendOutSeconds );
		if ( blendOutSeconds == 0 )
		{
			m_weight = 0;
			m_weightChangeRate = 0;
		}
		else
		{
			m_weightChangeRate = -m_weight / blendOutSeconds;
		}
	}

	float GetWeight() const
	{
		return m_weight;
	}

private:
	float m_weight;
	float m_weightChangeRate;
};


class CProceduralClipTurretAimPose
	: public TProceduralContextualClip< CProceduralContextTurretAimPose, STurretAimIKParams >
{
public:
	virtual void OnEnter( float blendTimeSeconds, float duration, const STurretAimIKParams& params )
	{
		m_weightHelper.OnEnter( blendTimeSeconds );

		const IAnimationSet* pAnimationSet = m_charInstance->GetIAnimationSet();
		assert( pAnimationSet != NULL );

		const AnimCRC aimAnimationCrc = params.animRef.crc;
		const int aimAnimationId = pAnimationSet->GetAnimIDByCRC( aimAnimationCrc );

		if ( aimAnimationId < 0 && aimAnimationCrc != ANIM_CRC_INVALID )
		{
			CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "ProceduralClipTurretAimPose: Requested an aim pose with name '%s' that doesn't exist for entity '%s'. Will use fallback vertical aiming behaviour, and it will look ugly!", GetParams().animRef.c_str(), m_entity->GetName() );
			return;
		}

		CryCharAnimationParams animParams;
		animParams.m_fTransTime = blendTimeSeconds;
		animParams.m_nLayerID = m_context->GetVerticalAimLayer();
		animParams.m_nFlags |= CA_LOOP_ANIMATION;
		animParams.m_nFlags |= CA_ALLOW_ANIM_RESTART;

		ISkeletonAnim* pSkeletonAnim = m_charInstance->GetISkeletonAnim();
		assert( pSkeletonAnim != NULL );

		pSkeletonAnim->StartAnimationById( aimAnimationId, animParams );
	}

	virtual void OnExit( float blendTimeSeconds )
	{
		m_weightHelper.OnExit( blendTimeSeconds );

		ISkeletonAnim* pSkeletonAnim = m_charInstance->GetISkeletonAnim();
		assert( pSkeletonAnim != NULL );

		const int32 verticalAimLayer = m_context->GetVerticalAimLayer();
		pSkeletonAnim->StopAnimationInLayer( verticalAimLayer, blendTimeSeconds );

		const float weight = m_weightHelper.GetWeight();

		const Vec3 targetWorldPosition = CalculateTargetWorldPosition();
		m_context->SetRequestedTargetWorldPosition( targetWorldPosition, weight, blendTimeSeconds );

		const STurretAimIKParams &params = GetParams();
		m_context->SetVerticalSmoothTime( params.verticalAimSmoothTime, weight, blendTimeSeconds );
		m_context->SetMaxYawDegreesPerSecond( params.maxYawDegreesSecond, weight, blendTimeSeconds );
		m_context->SetMaxPitchDegreesPerSecond( params.maxPitchDegreesSecond, weight, blendTimeSeconds );
	}

	virtual void Update( float timePassed )
	{
		m_weightHelper.Update( timePassed );
		const float weight = m_weightHelper.GetWeight();

		const Vec3 targetWorldPosition = CalculateTargetWorldPosition();
		m_context->SetRequestedTargetWorldPosition( targetWorldPosition, weight );

		const STurretAimIKParams &params = GetParams();
		m_context->SetVerticalSmoothTime( params.verticalAimSmoothTime, weight );
		m_context->SetMaxYawDegreesPerSecond( params.maxYawDegreesSecond, weight );
		m_context->SetMaxPitchDegreesPerSecond( params.maxPitchDegreesSecond, weight );
	}

	Vec3 CalculateTargetWorldPosition() const
	{
		QuatT targetWorldLocation;
		const bool hasTargetWorldLocationParam = GetParam( "TargetPos", targetWorldLocation );
		if ( hasTargetWorldLocationParam )
		{
			return targetWorldLocation.t;
		}

		const Vec3 targetWorldPosition = Vec3( 0, 0, 0 );
		return targetWorldPosition;
	}

private:
	SProceduralClipWeightHelper m_weightHelper;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipTurretAimPose, "TurretAimPose");
