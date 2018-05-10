// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PROCEDURAL_CONTEXT_TURRET_AIM_POSE__H__
#define __PROCEDURAL_CONTEXT_TURRET_AIM_POSE__H__

#include <ICryMannequin.h>

#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAnimationPoseModifier.h>

#include <CryExtension/CryCreateClassInstance.h>
#include <CryExtension/ClassWeaver.h>
#include <CryExtension/ICryFactoryRegistryImpl.h>
#include <CryExtension/RegFactoryNode.h>



template< typename T >
struct SContextBlendableValue
{
public:
	SContextBlendableValue( const T& zeroValue )
		: m_value( zeroValue )
		, m_weight( 0 )
	{
	}

	void AddValue( const T& value, const float weight )
	{
		assert( 0 <= weight );
		assert( weight <= 1 );
		
		m_value += value * weight;
		m_weight += weight;

		assert( m_weight < 1.001f );
	}

	const T EvaluateAndReset( const T& defaultValue, const T& zeroValue )
	{
		const T finalValue = Evaluate( defaultValue );
		Reset( zeroValue );
		return finalValue;
	}

	const T Evaluate( const T& defaultValue ) const
	{
		const T finalValue = ( m_value * m_weight ) + ( defaultValue * ( 1.f - m_weight ) );
		return finalValue;
	}

	const T& GetValue() const
	{
		return m_value;
	}

	float GetWeight() const
	{
		return m_weight;
	}

	void Reset( const T& zeroValue )
	{
		m_value = zeroValue;
		m_weight = 0;
	}

private:
	T m_value;
	float m_weight;
};



template< typename T >
struct SContextBlendableValueQueue
{
public:
	SContextBlendableValueQueue( const T& zeroValue )
		: m_value( zeroValue )
		, m_weight( 0 )
	{
		m_outQueue.reserve( 2 );
	}

	void AddValue( const T& value, const float weight )
	{
		m_value += value * weight;
		m_weight += weight;
	}

	void AddTimedValue( const T& value, const float weight, const float blendOutSeconds )
	{
		assert( 0 <= blendOutSeconds );
		assert( 0 <= weight );
		if ( blendOutSeconds <= 0 || weight <= 0 )
		{
			AddValue( value, weight );
		}
		else
		{
			SBlendOutEntry entry;
			entry.m_value = value;
			entry.m_weight = weight;
			entry.m_weightChangeRate = -weight / blendOutSeconds;

			m_outQueue.push_back( entry );
		}
	}

	T Update( const float elapsedSeconds, const T& zeroValue, const T& defaultValue )
	{
		UpdateQueue( elapsedSeconds );

		T finalValue;
		if ( m_weight < 1 )
		{
			finalValue = ( m_value * m_weight ) + ( defaultValue * ( 1.f - m_weight ) );
		}
		else
		{
			const float invWeight = 1.f / m_weight;
			finalValue = m_value * invWeight;
		}

		m_value = zeroValue;
		m_weight = 0;

		return finalValue;
	}

	void ClearQueue()
	{
		m_outQueue.clear();
	}

private:
	void UpdateQueue( const float elapsedSeconds )
	{
		typename TBlendOutQueue::iterator it = m_outQueue.begin();
		while ( it != m_outQueue.end() )
		{
			SBlendOutEntry& entry = *it;
			entry.m_weight += entry.m_weightChangeRate * elapsedSeconds;
			if ( 0 < entry.m_weight )
			{
				AddValue( entry.m_value, entry.m_weight );
				++it;
			}
			else
			{
				it = m_outQueue.erase( it );
			}
		}
	}

private:
	struct SBlendOutEntry
	{
		T m_value;
		float m_weight;
		float m_weightChangeRate;
	};

	float m_weight;
	T m_value;
	typedef std::vector< SBlendOutEntry > TBlendOutQueue;
	TBlendOutQueue m_outQueue;
};


class CProceduralContextTurretAimPose
	: public IProceduralContext
{
public:
	PROCEDURAL_CONTEXT(CProceduralContextTurretAimPose, "ProceduralContextTurretAimPose", "c47e5db7-3d57-4ae5-8efe-04de2442ed8f"_cry_guid);

	CProceduralContextTurretAimPose();
	virtual ~CProceduralContextTurretAimPose() {}

	virtual void Initialise( IEntity& entity, IActionController& actionController ) override;
	virtual void Update( float timePassedSeconds ) override;

	void SetHorizontalAimJointName( const char* horizontalAimJointName );
	void SetWeaponJointName( const char* weaponJointName );
	void SetAimPosesOriginJointName( const char* aimPosesOriginJointName );

	void SetHorizontalAimJointId( const int16 horizontalAimJointId );
	void SetWeaponJointId( const int16 weaponJointId );
	void SetAimPosesOriginJointId( const int16 aimPosesOriginJointId );

	void SetRequestedTargetWorldPosition( const Vec3& targetWorldPosition, const float weight, const float blendOutTime = 0 );

	void SetVerticalSmoothTime( const float verticalSmoothTimeSeconds, const float weight, const float blendOutTime = 0 );

	void SetMaxYawDegreesPerSecond( const float maxYawDegreesPerSecond, const float weight, const float blendOutTime = 0 );
	void SetMaxPitchDegreesPerSecond( const float maxPitchDegreesPerSecond, const float weight, const float blendOutTime = 0 );

	int GetVerticalAimLayer() const;

protected:
	bool InitialiseCharacter( const int characterSlot );
	void InitialiseHorizontalAim();
	void InitialiseVerticalAim();

	void ResetTargetWorldPosition();

	void UpdateHorizontalAim( const Vec3& targetLocalPosition, const float timePassedSeconds );
	void UpdateVerticalAim( const Vec3& targetWorldPosition, const float timePassedSeconds );
	void UpdateFallbackAim( const Vec3& targetLocalPosition );

	void UpdateSmoothedTargetWorldPosition( const float timePassedSeconds );

	float VelocityClampYaw( const float yaw, const float targetYaw, const float threshold, const float velocity, const float elapsedSeconds );
	float VelocityClampPitch( const float pitch, const float targetPitch, const float velocity, const float elapsedSeconds );

private:
	ICharacterInstance* m_pCharacterInstance;
	ISkeletonAnim* m_pSkeletonAnim;
	ISkeletonPose* m_pSkeletonPose;
	IDefaultSkeleton* m_pIDefaultSkeleton;
	
	typedef IAnimationOperatorQueuePtr TOperatorQueuePtr;
	TOperatorQueuePtr m_pHorizontalAim;

	typedef IAnimationPoseBlenderDir* TPoseBlenderAimPtr;
	TPoseBlenderAimPtr m_pVerticalAim;

	uint32 m_horizontalAimLayer;
	uint32 m_verticalAimLayer;

	SContextBlendableValueQueue< float > m_horizontalAimSmoothTime;
	SContextBlendableValueQueue< float > m_verticalAimSmoothTime;

	SContextBlendableValueQueue< float > m_maxYawDegreesPerSecond;
	SContextBlendableValueQueue< float > m_maxPitchDegreesPerSecond;

	int16 m_horizontalAimJointId;
	int16 m_weaponJointId;
	int16 m_aimPosesOriginJointId;

	float m_targetMinDistanceClamp;
	float m_minDistanceClampHeight;

	struct STurretOrientation
	{
		STurretOrientation()
			: yawRadians( 0 )
			, pitchRadians( 0 )
			, yawDistance( 0 )
			, pitchDistance( 0 )
		{
		}

		STurretOrientation( const float yawRadians_, const float pitchRadians_, const float yawDistance_, const float pitchDistance_ )
			: yawRadians( yawRadians_ )
			, pitchRadians( pitchRadians_ )
			, yawDistance( yawDistance_ )
			, pitchDistance( pitchDistance_ )
		{
		}

		STurretOrientation operator * ( const float s ) const
		{
			return STurretOrientation( yawRadians * s, pitchRadians * s, yawDistance * s, pitchDistance * s );
		}

		STurretOrientation operator + ( const STurretOrientation& rhs ) const
		{
			return STurretOrientation( yawRadians + rhs.yawRadians, pitchRadians + rhs.pitchRadians, yawDistance + rhs.yawDistance, pitchDistance + rhs.pitchDistance );
		}

		void operator += ( const STurretOrientation& rhs )
		{
			yawRadians += rhs.yawRadians;
			pitchRadians += rhs.pitchRadians;
			yawDistance += rhs.yawDistance;
			pitchDistance += rhs.pitchDistance;
		}

		float yawRadians;
		float pitchRadians;
		float yawDistance;
		float pitchDistance;
	};

	SContextBlendableValueQueue< STurretOrientation > m_requestedTurretOrientation;

	Vec3 m_smoothedTargetWorldPosition;
	float m_pitchRadians;
	float m_yawRadians;

	Matrix34 m_invertedWorldTM;
};


#endif
