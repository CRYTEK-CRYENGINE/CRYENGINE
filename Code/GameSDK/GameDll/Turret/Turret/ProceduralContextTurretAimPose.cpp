// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProceduralContextTurretAimPose.h"
#include "TurretHelpers.h"

CRYREGISTER_CLASS( CProceduralContextTurretAimPose );


#define DEFAULT_HORIZONTAL_AIM_LAYER 4
#define DEFAULT_VERTICAL_AIM_LAYER ( DEFAULT_HORIZONTAL_AIM_LAYER + 1 )
#define DEFAULT_CHARACTER_SLOT 0

#define HARDCODED_WEAPON_JOINT_NAME "weaponjoint"
#define HARDCODED_HORIZONTAL_AIM_JOINT_NAME "arcjoint"
#define HARDCODED_AIMPOSES_ORIGIN_JOINT_NAME "weaponjoint_ref"
#define ABS_PITCH_LIMIT_RADIANS ( DEG2RAD( 89 ) )
#define YAW_THRESHOLD_RADIANS ( DEG2RAD( 10 ) )

namespace
{
	float ClampValueDelta( const float value, const float targetValue, const float absMaxDeltaValue )
	{
		const float deltaValue = targetValue - value;
		const float absDeltaValue = abs( deltaValue );

		const float absClampedDeltaValue = min( absMaxDeltaValue, absDeltaValue );

		const float signDeltaValue = ( 0 <= deltaValue ) ? 1.0f : -1.0f;

		const float clampedDeltaValue = signDeltaValue * absClampedDeltaValue;

		const float clampedValue = value + clampedDeltaValue;
		return clampedValue;
	}


	float VelocityClampValue( const float value, const float targetValue, const float velocity, const float elapsedSeconds )
	{
		assert( 0 <= velocity );
		assert( 0 <= elapsedSeconds );

		const float absMaxDeltaValue = velocity * elapsedSeconds;
		const float clampedValue = ClampValueDelta( value, targetValue, absMaxDeltaValue );

		return clampedValue;
	}
}


CProceduralContextTurretAimPose::CProceduralContextTurretAimPose()
: m_pCharacterInstance( NULL )
, m_pSkeletonAnim( NULL )
, m_pSkeletonPose( NULL )
, m_pIDefaultSkeleton( NULL )
, m_pVerticalAim( NULL )
, m_horizontalAimLayer( DEFAULT_HORIZONTAL_AIM_LAYER )
, m_verticalAimLayer( DEFAULT_VERTICAL_AIM_LAYER )
, m_horizontalAimSmoothTime( 0 )
, m_verticalAimSmoothTime( 0 )
, m_maxYawDegreesPerSecond( 0 )
, m_maxPitchDegreesPerSecond( 0 )
, m_horizontalAimJointId( -1 )
, m_weaponJointId( -1 )
, m_aimPosesOriginJointId( -1 )
, m_targetMinDistanceClamp( 0 )
, m_minDistanceClampHeight( 0 )
, m_requestedTurretOrientation( STurretOrientation() )
, m_smoothedTargetWorldPosition( 0, 0, 0 )
, m_pitchRadians( 0 )
, m_yawRadians( 0 )
, m_invertedWorldTM( IDENTITY )
{
}


void CProceduralContextTurretAimPose::Initialise( IEntity& entity, IActionController& actionController )
{
	IProceduralContext::Initialise( entity, actionController );

	const Matrix34& worldTM = m_entity->GetWorldTM();
	m_invertedWorldTM = worldTM.GetInverted();

	const bool initialiseCharacterSuccess = InitialiseCharacter( DEFAULT_CHARACTER_SLOT );
	if ( ! initialiseCharacterSuccess )
	{
		CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "ProceduralContext TurretAimPose: Could not initialise character in slot '%d' for entity with name '%s'.", DEFAULT_CHARACTER_SLOT, entity.GetName() );
		return;
	}

	InitialiseHorizontalAim();
	InitialiseVerticalAim();

	SetWeaponJointName( HARDCODED_WEAPON_JOINT_NAME );
	SetHorizontalAimJointName( HARDCODED_HORIZONTAL_AIM_JOINT_NAME );
	SetAimPosesOriginJointName( HARDCODED_AIMPOSES_ORIGIN_JOINT_NAME );

	m_smoothedTargetWorldPosition = m_entity->GetWorldPos() + m_entity->GetForwardDir();
}


void CProceduralContextTurretAimPose::Update( float timePassedSeconds )
{
	if ( m_pSkeletonAnim == NULL || m_pSkeletonPose == NULL )
	{
		return;
	}

	const Matrix34& worldTM = m_entity->GetWorldTM();
	m_invertedWorldTM = worldTM.GetInverted();

	UpdateSmoothedTargetWorldPosition( timePassedSeconds );

	const Vec3& targetWorldPosition = m_smoothedTargetWorldPosition;
	const Vec3 targetLocalPosition = m_invertedWorldTM * targetWorldPosition;

	UpdateHorizontalAim( targetLocalPosition, timePassedSeconds );
	UpdateVerticalAim( targetWorldPosition, timePassedSeconds );
	UpdateFallbackAim( targetLocalPosition );
}


float CProceduralContextTurretAimPose::VelocityClampYaw( const float yaw, const float targetYaw, const float threshold, const float velocity, const float elapsedSeconds )
{
	using namespace TurretHelpers;

	assert( -gf_PI <= yaw );
	assert( yaw <= gf_PI );
	assert( -gf_PI <= targetYaw );
	assert( targetYaw <= gf_PI );
	assert( 0 <= velocity );
	assert( 0 <= elapsedSeconds );

	const float deltaYawA = Wrap( targetYaw - yaw, -gf_PI, gf_PI );
	const float deltaYawB = Wrap( yaw - targetYaw, -gf_PI, gf_PI );

	const float absDeltaYawA = abs( deltaYawA );
	const float absDeltaYawB = abs( deltaYawB );
	const float absDeltaYawBWithThreshold = absDeltaYawB + threshold;

	const float deltaYaw = ( absDeltaYawA <= absDeltaYawBWithThreshold ) ? deltaYawA : deltaYawB;

	const float absDeltaYaw = abs( deltaYaw );
	const float absMaxDeltaYaw = velocity * elapsedSeconds;
	const float absClampedDeltaYaw = min( absMaxDeltaYaw, absDeltaYaw );

	const float signDeltaYaw = ( 0 <= deltaYaw ) ? 1.0f : -1.0f;

	const float clampedDeltaYaw = signDeltaYaw * absClampedDeltaYaw;

	const float clampedYaw = yaw + clampedDeltaYaw;

	const float wrappedClampYaw = Wrap( clampedYaw, -gf_PI, gf_PI );
	return wrappedClampYaw;
}


float CProceduralContextTurretAimPose::VelocityClampPitch( const float pitch, const float targetPitch, const float velocity, const float elapsedSeconds )
{
	const float clampedTargetPitch = clamp_tpl( targetPitch, -ABS_PITCH_LIMIT_RADIANS, ABS_PITCH_LIMIT_RADIANS );
	const float clampedPitch = VelocityClampValue( pitch, clampedTargetPitch, velocity, elapsedSeconds );
	return clampedPitch;
}


void CProceduralContextTurretAimPose::UpdateSmoothedTargetWorldPosition( const float timePassedSeconds )
{
	assert( m_pSkeletonPose != NULL );

	const int16 originJointId = m_aimPosesOriginJointId;
	const QuatT originJointLocation = m_pSkeletonPose->GetAbsJointByID( originJointId );

	const Matrix34& worldTM = m_entity->GetWorldTM();

	const Vec3 oldTargetLocalPosition = m_invertedWorldTM * m_smoothedTargetWorldPosition;

	const float oldYawRadians = m_yawRadians;
	const float oldPitchRadians = m_pitchRadians;

	static const STurretOrientation s_zeroTurretOrientation;
	static const STurretOrientation s_defaultTurretOrientation;
	const STurretOrientation turretOrientation = m_requestedTurretOrientation.Update( timePassedSeconds, s_zeroTurretOrientation, s_defaultTurretOrientation );
	const float yawRadians = turretOrientation.yawRadians;
	const float pitchRadians = turretOrientation.pitchRadians;
	const float pitchDistance = turretOrientation.pitchDistance;
	const float yawDistance = turretOrientation.yawDistance;

	const float uncheckedMaxYawDegreesPerSecond = m_maxYawDegreesPerSecond.Update( timePassedSeconds, 0, 0 );
	const float uncheckedMaxPitchDegreesPerSecond = m_maxPitchDegreesPerSecond.Update( timePassedSeconds, 0, 0 );

	const float maxYawDegreesPerSecond = max( 0.0f, uncheckedMaxYawDegreesPerSecond );
	const float maxPitchDegreesPerSeconds = max( 0.0f, uncheckedMaxPitchDegreesPerSecond );

	const float clampedYawRadians = VelocityClampYaw( oldYawRadians, yawRadians, YAW_THRESHOLD_RADIANS, DEG2RAD( maxYawDegreesPerSecond ), timePassedSeconds );
	const float clampedPitchRadians = VelocityClampPitch( oldPitchRadians, pitchRadians, DEG2RAD( maxPitchDegreesPerSeconds ), timePassedSeconds );

	const Vec3 clampedTargetLocalPosition = TurretHelpers::CalculateTargetLocalPosition( originJointLocation, clampedYawRadians, clampedPitchRadians, yawDistance, pitchDistance );
	const Vec3 clampedTargetWorldPosition = worldTM.TransformPoint( clampedTargetLocalPosition );

#ifdef DEBUGDRAW_PROCEDURAL_CONTEXT_TURRET_AIM_POSE
	{
		const float color[ 4 ] = { 1, 1, 1, 1 };

		const Vec3 reconstructedTargetLocalPosition = TurretHelpers::CalculateTargetLocalPosition( originJointLocation, yawRadians, pitchRadians, yawDistance, pitchDistance );
		const Vec3 reconstructedTargetWorldPosition = worldTM.TransformPoint( reconstructedTargetLocalPosition );
		
		float textY = 10;
		IRenderAuxText::Draw2dLabel( 5, textY, 1.2f, color, false, "turret <%f, %f > <%f, %f>", RAD2DEG( pitchRadians ), RAD2DEG( yawRadians ), RAD2DEG( clampedPitchRadians ), RAD2DEG( clampedYawRadians ) ); textY += 15;
		IRenderAuxText::Draw2dLabel( 5, textY, 1.2f, color, false, "reconstruct world pos <%f, %f, %f>", reconstructedTargetWorldPosition.x, reconstructedTargetWorldPosition.y, reconstructedTargetWorldPosition.z ); textY += 15;
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere( reconstructedTargetWorldPosition, 0.5f, Col_Red );
		IRenderAuxText::Draw2dLabel( 5, textY, 1.2f, color, false, "clamped world pos <%f, %f, %f>", clampedTargetWorldPosition.x, clampedTargetWorldPosition.y, clampedTargetWorldPosition.z ); textY += 15;
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere( clampedTargetWorldPosition, 0.5f, Col_Blue );
	}
#endif


	m_smoothedTargetWorldPosition = clampedTargetWorldPosition;
	m_pitchRadians = clampedPitchRadians;
	m_yawRadians = clampedYawRadians;
}


bool CProceduralContextTurretAimPose::InitialiseCharacter( const int characterSlot )
{
	if ( m_entity == NULL )
	{
		return false;
	}

	ICharacterInstance* pCharacterInstance = m_entity->GetCharacter( characterSlot );
	if ( pCharacterInstance == NULL )
	{
		return false;
	}

	ISkeletonAnim* pSkeletonAnim = pCharacterInstance->GetISkeletonAnim();
	if ( pSkeletonAnim == NULL )
	{
		return false;
	}

	ISkeletonPose* pSkeletonPose = pCharacterInstance->GetISkeletonPose();
	if ( pSkeletonPose == NULL )
	{
		return false;
	}
	IDefaultSkeleton& rIDefaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();

	m_pCharacterInstance = pCharacterInstance;
	m_pSkeletonAnim = pSkeletonAnim;
	m_pSkeletonPose = pSkeletonPose;
	m_pIDefaultSkeleton = &rIDefaultSkeleton;

	return true;
}


void CProceduralContextTurretAimPose::InitialiseHorizontalAim()
{
	CryCreateClassInstanceForInterface(cryiidof<IAnimationOperatorQueue>(), m_pHorizontalAim);
}


void CProceduralContextTurretAimPose::InitialiseVerticalAim()
{
	assert( m_pSkeletonPose != NULL );

	m_pVerticalAim = m_pSkeletonPose->GetIPoseBlenderAim();

	if ( m_pVerticalAim == NULL )
	{
		return;
	}

	const Vec3 defaultTargetWorldPosition = m_entity->GetWorldPos() + m_entity->GetForwardDir();
	m_pVerticalAim->SetTarget( defaultTargetWorldPosition );
	m_pVerticalAim->SetPolarCoordinatesMaxRadiansPerSecond( Vec2( gf_PI * 10.f, gf_PI * 10.f ) ); // Yaw/pitch smoothing is handled outside
	m_pVerticalAim->SetPolarCoordinatesSmoothTimeSeconds( 1 );
	m_pVerticalAim->SetState( false );
	m_pVerticalAim->SetLayer( m_verticalAimLayer );
}


void CProceduralContextTurretAimPose::SetRequestedTargetWorldPosition( const Vec3& targetWorldPosition, const float weight, const float blendOutTime )
{
	const int16 originJointId = m_aimPosesOriginJointId;
	const QuatT& originJointLocation = m_pSkeletonPose->GetAbsJointByID( originJointId );

	const Vec3 targetLocalPosition = m_invertedWorldTM * targetWorldPosition;

	STurretOrientation targetOrientation;
	TurretHelpers::CalculateTargetOrientation( targetLocalPosition, originJointLocation, targetOrientation.yawRadians, targetOrientation.pitchRadians, targetOrientation.yawDistance, targetOrientation.pitchDistance );

	m_requestedTurretOrientation.AddTimedValue( targetOrientation, weight, blendOutTime );
}


void CProceduralContextTurretAimPose::SetVerticalSmoothTime( const float verticalSmoothTimeSeconds, const float weight, const float blendOutTime )
{
	m_verticalAimSmoothTime.AddTimedValue( verticalSmoothTimeSeconds, weight, blendOutTime );
}


void CProceduralContextTurretAimPose::SetMaxYawDegreesPerSecond( const float maxYawDegreesPerSecond, const float weight, const float blendOutTime )
{
	m_maxYawDegreesPerSecond.AddTimedValue( maxYawDegreesPerSecond, weight, blendOutTime );
}


void CProceduralContextTurretAimPose::SetMaxPitchDegreesPerSecond( const float maxPitchDegreesPerSecond, const float weight, const float blendOutTime )
{
	m_maxPitchDegreesPerSecond.AddTimedValue( maxPitchDegreesPerSecond, weight, blendOutTime );
}


void CProceduralContextTurretAimPose::UpdateHorizontalAim( const Vec3& targetLocalPosition, const float timePassedSeconds )
{
	assert( m_pSkeletonAnim != NULL );

	if ( ! m_pHorizontalAim )
	{
		return;
	}

	if ( m_horizontalAimJointId < 0 )
	{
		return;
	}

	m_pHorizontalAim->Clear();

	const Vec3 forwardLocalDirection( 0, 1, 0 );

	const Vec3 targetLocalDirection = targetLocalPosition.GetNormalizedSafe( forwardLocalDirection );
	const Vec3 targetDirection2d = Vec3( targetLocalDirection.x, targetLocalDirection.y, 0 ).GetNormalizedSafe( forwardLocalDirection );

	const Quat desiredOrientation = Quat::CreateRotationV0V1( forwardLocalDirection, targetDirection2d );

	m_pHorizontalAim->PushOrientation( m_horizontalAimJointId, IAnimationOperatorQueue::eOp_Override, desiredOrientation );

	m_pSkeletonAnim->PushPoseModifier( m_horizontalAimLayer, cryinterface_cast< IAnimationPoseModifier >( m_pHorizontalAim ), "TurretAimPose" );
}


void CProceduralContextTurretAimPose::UpdateVerticalAim( const Vec3& targetWorldPosition, const float timePassedSeconds )
{
	assert( m_pSkeletonAnim != NULL );
	assert( m_pSkeletonPose != NULL );

	if ( m_pVerticalAim == NULL )
	{
		return;
	}

	const float verticalAimSmoothTime = m_verticalAimSmoothTime.Update( timePassedSeconds, 0, 0 );

	m_pVerticalAim->SetPolarCoordinatesSmoothTimeSeconds( verticalAimSmoothTime );
	m_pVerticalAim->SetState( true );
	m_pVerticalAim->SetLayer( m_verticalAimLayer );
	m_pVerticalAim->SetTarget( targetWorldPosition );
}


void CProceduralContextTurretAimPose::UpdateFallbackAim( const Vec3& targetLocalPosition )
{
	assert( m_pSkeletonPose != NULL );

	const bool needFallbackAim = ( m_pVerticalAim == NULL );
	if ( ! needFallbackAim )
	{
		return;
	}

	if ( ! m_pHorizontalAim )
	{
		return;
	}

	const int16 fallbackJointId = m_weaponJointId;
	if ( fallbackJointId < 0 )
	{
		return;
	}

	const Vec3 forwardLocalDirection( 0, 1, 0 );

	const QuatT& lastFrameJointLocation = m_pSkeletonPose->GetAbsJointByID( fallbackJointId );
	const Vec3 targetJointLocalPosition = targetLocalPosition - lastFrameJointLocation.t;
	const Vec3 targetJointLocalDirection = targetJointLocalPosition.GetNormalizedSafe( forwardLocalDirection );

	const Quat desiredOrientation = Quat::CreateRotationV0V1( forwardLocalDirection, targetJointLocalDirection );

	m_pHorizontalAim->PushOrientation( fallbackJointId, IAnimationOperatorQueue::eOp_Override, desiredOrientation );
}


void CProceduralContextTurretAimPose::SetHorizontalAimJointName( const char* horizontalAimJointName )
{
	assert( horizontalAimJointName );
	assert( horizontalAimJointName[ 0 ] != 0 );
	assert( m_pSkeletonPose != NULL );
	if ( m_pSkeletonPose == NULL )
	{
		CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "ProceduralContext TurretAimPose: Could not initialise horizontal joint with name '%s' for entity named '%s'.", horizontalAimJointName, m_entity->GetName() );
		SetHorizontalAimJointId( -1 );
		return;
	}

	const int16 horizontalAimJointId = m_pIDefaultSkeleton->GetJointIDByName( horizontalAimJointName );
	SetHorizontalAimJointId( horizontalAimJointId );
}


void CProceduralContextTurretAimPose::SetWeaponJointName( const char* weaponJointName )
{
	assert( weaponJointName );
	assert( weaponJointName[ 0 ] != 0 );
	assert( m_pSkeletonPose != NULL );
	if ( m_pSkeletonPose == NULL )
	{
		CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "ProceduralContext TurretAimPose: Could not initialise weapon joint with name '%s' for entity named '%s'.", weaponJointName, m_entity->GetName() );
		SetWeaponJointId( -1 );
		return;
	}

	const int16 weaponJointId = m_pIDefaultSkeleton->GetJointIDByName( weaponJointName );
	SetWeaponJointId( weaponJointId );
}


void CProceduralContextTurretAimPose::SetAimPosesOriginJointName( const char* aimPosesOriginJointName )
{
	assert( aimPosesOriginJointName );
	assert( aimPosesOriginJointName[ 0 ] != 0 );
	assert( m_pSkeletonPose != NULL );
	if ( m_pSkeletonPose == NULL )
	{
		CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "ProceduralContext TurretAimPose: Could not initialise aim poses origin joint with name '%s' for entity named '%s'.", aimPosesOriginJointName, m_entity->GetName() );
		SetAimPosesOriginJointId( -1 );
		return;
	}

	const int16 aimPosesOriginJointId = m_pIDefaultSkeleton->GetJointIDByName( aimPosesOriginJointName );
	SetAimPosesOriginJointId( aimPosesOriginJointId );
}


void CProceduralContextTurretAimPose::SetHorizontalAimJointId( const int16 horizontalAimJointId )
{
	m_horizontalAimJointId = horizontalAimJointId;
}


void CProceduralContextTurretAimPose::SetWeaponJointId( const int16 weaponJointId )
{
	m_weaponJointId = weaponJointId;
}


void CProceduralContextTurretAimPose::SetAimPosesOriginJointId( const int16 aimPosesOriginJointId )
{
	m_aimPosesOriginJointId = aimPosesOriginJointId;
}


int CProceduralContextTurretAimPose::GetVerticalAimLayer() const
{
	return m_verticalAimLayer;
}