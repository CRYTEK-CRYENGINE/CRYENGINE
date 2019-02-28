// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TURRET_SOUND_MANAGER__H__
#define __TURRET_SOUND_MANAGER__H__


enum ETurretSounds
{
	eTS_VerticalMovementStart,
	eTS_VerticalMovementStop,
	eTS_VerticalMovementLoop,

	eTS_HorizontalMovementStart,
	eTS_HorizontalMovementStop,
	eTS_HorizontalMovementLoop,

	eTS_Alerted,
	eTS_LockedOnTarget,

	eTS_SoundCount,
};


struct STurretSoundInfo
{
	STurretSoundInfo()
		: /*soundId( INVALID_SOUNDID )
		,*/ speedParamValue( FLT_MAX )
	{
	}

	CCryName soundName;
	//tSoundID soundId;
	float speedParamValue;
};

class CTurretSoundManagerHelper
{
public:
	CTurretSoundManagerHelper( IEntityAudioComponent* pIEntityAudioComponent )
		: m_pIEntityAudioComponent( pIEntityAudioComponent )
	{
		assert( m_pIEntityAudioComponent != NULL );
	}

	~CTurretSoundManagerHelper()
	{
		StopAllSounds();
	}

	void SetSoundName( const size_t soundId, const char* const soundName )
	{
		assert( soundId < eTS_SoundCount );
		STurretSoundInfo& soundInfo = m_sounds[ soundId ];
		soundInfo.soundName = soundName;
	}

	void StopAllSounds()
	{
		//m_pIEntityAudioComponent->StopAllSounds();
		REINST("needs verification!");
		for ( size_t i = 0; i < eTS_SoundCount; ++i )
		{
			STurretSoundInfo& soundInfo = m_sounds[ i ];
			//soundInfo.soundId = INVALID_SOUNDID;
			soundInfo.speedParamValue = FLT_MAX;
		}
	}

	void StopSound( const size_t soundId )
	{
		/*assert( soundId < eTS_SoundCount );
		STurretSoundInfo& soundInfo = m_sounds[ soundId ];
		m_pIEntityAudioComponent->StopSound( soundInfo.soundId );
		soundInfo.soundId = INVALID_SOUNDID;*/
	}

	bool IsLoopingSoundStarted( const size_t soundId ) const
	{
		/*assert( soundId < eTS_SoundCount );
		const STurretSoundInfo& soundInfo = m_sounds[ soundId ];
		return ( soundInfo.soundId != INVALID_SOUNDID );*/
		return true;
	}

	void PlaySound( const size_t soundId )
	{
		assert( soundId < eTS_SoundCount );
		STurretSoundInfo& soundInfo = m_sounds[ soundId ];
		const char* const soundName = soundInfo.soundName.c_str();
		const bool validSoundName = ( soundName[ 0 ] != 0 );
		if ( validSoundName )
		{
			/*const Vec3 offset( 0, 0, 0 );
			const Vec3 direction( 0, 0, 0 );
			soundInfo.soundId = m_pIEntityAudioComponent->PlaySound( soundName, offset, direction, FLAG_SOUND_DEFAULT_3D, 0, eSoundSemantic_Mechanic_Entity );*/
			soundInfo.speedParamValue = FLT_MAX;
		}
	}

	void SetSoundParam( const size_t soundId, const char* const paramName, const float value )
	{
		assert( soundId < eTS_SoundCount );
		const STurretSoundInfo& soundInfo = m_sounds[ soundId ];
		/*_smart_ptr< ISound > pSound = m_pIEntityAudioComponent->GetSound( soundInfo.soundId );
		if ( pSound )
		{
			const bool logError = false;
			pSound->SetParam( paramName, value, logError );
		}*/
	}

	void SetSoundSpeedParam( const size_t soundId, const float value )
	{
		assert( soundId < eTS_SoundCount );
		STurretSoundInfo& soundInfo = m_sounds[ soundId ];
		const float thresholdDeltaValue = DEG2RAD( 5.0f );
		if ( soundInfo.speedParamValue == FLT_MAX || ( thresholdDeltaValue < abs( value - soundInfo.speedParamValue ) ) )
		{
			SetSoundParam( soundId, "speed", value );
			soundInfo.speedParamValue = value;
		}
	}

private:
	IEntityAudioComponent* m_pIEntityAudioComponent;
	STurretSoundInfo m_sounds[ eTS_SoundCount ];
};


class CTurretSoundManager
{
public:
	CTurretSoundManager( SmartScriptTable pSoundPropertiesTable, ICharacterInstance* pCharacterInstance, IEntityAudioComponent* pIEntityAudioComponent )
		: m_soundManagerHelper( pIEntityAudioComponent )
		, m_oldYawRadians( 0 )
		, m_oldPitchRadians( 0 )
		, m_movingHorizontal( false )
		, m_movingVertical( false )
		, m_movementStartSoundThresholdRadiansSecond( DEG2RAD( 10.0f ) )
		, m_movementStopSoundThresholdRadiansSecond( DEG2RAD( 5.0f ) )
		, m_horizontalLoopingSoundDelaySeconds( 0.3f )
		, m_verticalLoopingSoundDelaySeconds( 0.3f )
		, m_yawJointId( -1 )
		, m_pitchJointId( -1 )
		, m_alertedSoundTimeoutSeconds( 10.0f )
		, m_waitingForLockOnTarget( false )
		, m_lockOnTargetSeconds( 0.5f )
		, m_resourcesCached( false )
		, m_gameHintName( "" )
	{
		m_gameHintName = GetProperty< const char* >( pSoundPropertiesTable, "GameHintName", m_gameHintName.c_str() );
		CacheResources();

		InitSound( eTS_VerticalMovementStart, pSoundPropertiesTable, "soundVerticalMovementStart" );
		InitSound( eTS_VerticalMovementStop, pSoundPropertiesTable, "soundVerticalMovementStop" );
		InitSound( eTS_VerticalMovementLoop, pSoundPropertiesTable, "soundVerticalMovementLoop" );
		InitSound( eTS_HorizontalMovementStart, pSoundPropertiesTable, "soundHorizontalMovementStart" );
		InitSound( eTS_HorizontalMovementStop, pSoundPropertiesTable, "soundHorizontalMovementStop" );
		InitSound( eTS_HorizontalMovementLoop, pSoundPropertiesTable, "soundHorizontalMovementLoop" );
		InitSound( eTS_Alerted, pSoundPropertiesTable, "soundAlerted" );
		InitSound( eTS_LockedOnTarget, pSoundPropertiesTable, "soundLockedOnTarget" );

		m_movementStartSoundThresholdRadiansSecond = DEG2RAD( abs( GetProperty< float >( pSoundPropertiesTable, "fMovementStartSoundThresholdDegreesSecond", m_movementStartSoundThresholdRadiansSecond ) ) );
		m_movementStopSoundThresholdRadiansSecond = DEG2RAD( abs( GetProperty< float >( pSoundPropertiesTable, "fMovementStopSoundThresholdDegreesSecond", m_movementStopSoundThresholdRadiansSecond ) ) );
		m_horizontalLoopingSoundDelaySeconds = abs( GetProperty< float >( pSoundPropertiesTable, "fHorizontalLoopingSoundDelaySeconds", m_horizontalLoopingSoundDelaySeconds ) );
		m_verticalLoopingSoundDelaySeconds = abs( GetProperty< float >( pSoundPropertiesTable, "fVerticalLoopingSoundDelaySeconds", m_verticalLoopingSoundDelaySeconds ) );
		m_alertedSoundTimeoutSeconds = abs( GetProperty< float >( pSoundPropertiesTable, "fAlertedTimeoutSeconds", m_alertedSoundTimeoutSeconds ) );
		m_lockOnTargetSeconds = abs( GetProperty< float >( pSoundPropertiesTable, "fLockOnTargetSeconds", m_lockOnTargetSeconds ) );

		InitJointIds( pSoundPropertiesTable, pCharacterInstance );
	}

	~CTurretSoundManager()
	{
		ReleaseResources();
	}

	void ProcessEvent( const SEntityEvent& event )
	{
		switch( event.event )
		{
		case ENTITY_EVENT_HIDE:
			OnHide();
			break;

		case ENTITY_EVENT_UNHIDE:
			OnUnhide();
			break;

		case ENTITY_EVENT_DEACTIVATED:
			OnDeactivated();
			break;

		case ENTITY_EVENT_ACTIVATED:
			OnActivated();
			break;
		}
	}

	void UpdateHorizontalMovement( const CTimeValue& timeNow, const bool moving )
	{
		UpdateMovementSounds( timeNow, moving, m_movingHorizontal, eTS_HorizontalMovementStart, eTS_HorizontalMovementLoop, eTS_HorizontalMovementStop, m_startHorizontalLoopingSoundTime, m_horizontalLoopingSoundDelaySeconds );
	}

	void UpdateMovingVertical( const CTimeValue& timeNow, const bool moving )
	{
		UpdateMovementSounds( timeNow, moving, m_movingVertical, eTS_VerticalMovementStart, eTS_VerticalMovementLoop, eTS_VerticalMovementStop, m_startVerticalLoopingSoundTime, m_verticalLoopingSoundDelaySeconds );
	}

	void Update( ICharacterInstance* pCharacterInstance, const bool isActive, const EntityId targetEntityId, const float frameTimeSeconds )
	{
		if ( ! isActive )
		{
			m_soundManagerHelper.StopSound( eTS_VerticalMovementLoop );
			m_soundManagerHelper.StopSound( eTS_HorizontalMovementLoop );
			return;
		}

		UpdateAlertedSound( targetEntityId );
		UpdateLockOnTargetSound();

		if ( pCharacterInstance == NULL )
		{
			return;
		}

		ISkeletonPose* pSkeletonPose = pCharacterInstance->GetISkeletonPose();
		if ( pSkeletonPose == NULL )
		{
			return;
		}

		float yawRadians = m_oldYawRadians;
		if ( 0 <= m_yawJointId )
		{
			const QuatT horizontalLocation = pSkeletonPose->GetRelJointByID( m_yawJointId );
			const Ang3 horizontalAngles( horizontalLocation.q );
			//yawRadians = horizontalAngles.z;
			yawRadians = horizontalAngles.x + horizontalAngles.y + horizontalAngles.z;
		}

		float pitchRadians = m_oldPitchRadians;
		if ( 0 <= m_pitchJointId )
		{
			const QuatT verticalLocation = pSkeletonPose->GetRelJointByID( m_pitchJointId );
			const Ang3 verticalAngles( verticalLocation.q );
			//yawRadians = horizontalAngles.x;
			pitchRadians = verticalAngles.x + verticalAngles.y + verticalAngles.z;
		}

		Update( yawRadians, pitchRadians, frameTimeSeconds );
	}

	void Update( const float yawRadians, const float pitchRadians, const float frameTimeSeconds )
	{
		if ( frameTimeSeconds <= 0.0f )
		{
			return;
		}

		const CTimeValue timeNow = gEnv->pTimer->GetAsyncTime();

		{
			const float deltaYawRadians = abs( yawRadians - m_oldYawRadians );
			const float horizontalRadiansPerSecond = deltaYawRadians / frameTimeSeconds;

			if ( m_movingHorizontal )
			{
				const bool movingHorizontal = ( m_movementStopSoundThresholdRadiansSecond <= horizontalRadiansPerSecond );
				UpdateHorizontalMovement( timeNow, movingHorizontal );
				if ( m_soundManagerHelper.IsLoopingSoundStarted( eTS_HorizontalMovementLoop ) )
				{
					m_soundManagerHelper.SetSoundSpeedParam( eTS_HorizontalMovementLoop, horizontalRadiansPerSecond );
				}
				else
				{
					m_soundManagerHelper.SetSoundSpeedParam( eTS_HorizontalMovementStart, horizontalRadiansPerSecond );
				}
			}
			else
			{
				const bool movingHorizontal = ( m_movementStartSoundThresholdRadiansSecond <= horizontalRadiansPerSecond );
				UpdateHorizontalMovement( timeNow, movingHorizontal );
				m_soundManagerHelper.SetSoundSpeedParam( eTS_HorizontalMovementStop, horizontalRadiansPerSecond );
			}
		}

		{
			const float deltaPitchRadians = abs( pitchRadians - m_oldPitchRadians );
			const float verticalRadiansPerSecond = deltaPitchRadians / frameTimeSeconds;

			if ( m_movingVertical )
			{
				const bool movingVertical = ( m_movementStopSoundThresholdRadiansSecond <= verticalRadiansPerSecond );
				UpdateMovingVertical( timeNow, movingVertical );
				if ( m_soundManagerHelper.IsLoopingSoundStarted( eTS_HorizontalMovementLoop ) )
				{
					m_soundManagerHelper.SetSoundSpeedParam( eTS_VerticalMovementLoop, verticalRadiansPerSecond );
				}
				else
				{
					m_soundManagerHelper.SetSoundSpeedParam( eTS_VerticalMovementStart, verticalRadiansPerSecond );
				}
			}
			else
			{
				const bool movingVertical = ( m_movementStartSoundThresholdRadiansSecond <= verticalRadiansPerSecond );
				UpdateMovingVertical( timeNow, movingVertical );
				m_soundManagerHelper.SetSoundSpeedParam( eTS_VerticalMovementStop, verticalRadiansPerSecond );
			}
		}

		m_oldYawRadians = yawRadians;
		m_oldPitchRadians = pitchRadians;
	}

	void NotifyPreparingToFire( const CTimeValue& nextFiringTime )
	{
		m_waitingForLockOnTarget = true;
		m_nextLockOnTargetTime = nextFiringTime - CTimeValue( m_lockOnTargetSeconds );
	}

	void NotifyCancelPreparingToFire()
	{
		m_waitingForLockOnTarget = false;
	}

private:
	void InitSound( const size_t soundId, SmartScriptTable pSoundPropertiesTable, const char* const soundPropertyName )
	{
		const char* const soundName = GetProperty< const char* >( pSoundPropertiesTable, soundPropertyName, "" );
		m_soundManagerHelper.SetSoundName( soundId, soundName );
	}

	void InitJointIds( SmartScriptTable pSoundPropertiesTable, ICharacterInstance* pCharacterInstance )
	{
		if ( pCharacterInstance == NULL )
		{
			return;
		}

		IDefaultSkeleton& rIDefaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();
		{
			const char* const yawJointName = GetProperty< const char* >( pSoundPropertiesTable, "HorizontalJointName", "" );
			m_yawJointId = rIDefaultSkeleton.GetJointIDByName( yawJointName );
		}

		{
			const char* const pitchJointName = GetProperty< const char* >( pSoundPropertiesTable, "VerticalJointName", "" );
			m_pitchJointId = rIDefaultSkeleton.GetJointIDByName( pitchJointName );
		}
	}

	void UpdateMovementSounds( const CTimeValue& timeNow, const bool moving, bool& movingOut, const size_t startSoundId, const size_t loopSoundId, const size_t stopSoundId, CTimeValue& loopTimeValueOut, const float delayLoopSeconds )
	{
		if ( movingOut )
		{
			if ( moving )
			{
				const bool playingLoopSound = m_soundManagerHelper.IsLoopingSoundStarted( loopSoundId );
				if ( ! playingLoopSound )
				{
					if ( loopTimeValueOut <= timeNow )
					{
						m_soundManagerHelper.PlaySound( loopSoundId );
					}
				}
			}
			else
			{
				m_soundManagerHelper.StopSound( loopSoundId );
				m_soundManagerHelper.PlaySound( stopSoundId );

				movingOut = false;
			}
		}
		else
		{
			if ( moving )
			{
				m_soundManagerHelper.StopSound( loopSoundId );
				m_soundManagerHelper.PlaySound( startSoundId );

				loopTimeValueOut = timeNow + CTimeValue( delayLoopSeconds );
				movingOut = true;
			}
		}
	}

	void UpdateAlertedSound( const EntityId targetEntityId )
	{
		if ( targetEntityId == 0 )
		{
			return;
		}

		const CTimeValue timeNow = gEnv->pTimer->GetAsyncTime();
		if ( m_nextAlertedTime <= timeNow )
		{
			m_soundManagerHelper.PlaySound( eTS_Alerted );
			m_nextAlertedTime = timeNow + CTimeValue( m_alertedSoundTimeoutSeconds );
		}
	}

	void UpdateLockOnTargetSound()
	{
		if ( ! m_waitingForLockOnTarget )
		{
			return;
		}

		const CTimeValue timeNow = gEnv->pTimer->GetAsyncTime();
		if ( m_nextLockOnTargetTime <= timeNow )
		{
			m_soundManagerHelper.PlaySound( eTS_LockedOnTarget );
			m_waitingForLockOnTarget = false;
		}
	}

	void CacheResources()
	{
		assert( gEnv );
		if ( gEnv->IsEditor() )
		{
			return;
		}

		if ( m_resourcesCached )
		{
			return;
		}

		assert( gEnv->pAudioSystem );
		//gEnv->pSoundSystem->CacheAudioFile( m_gameHintName.c_str(), eAFCT_GAME_HINT );
		m_resourcesCached = true;
	}

	void ReleaseResources()
	{
		assert( gEnv );
		if ( gEnv->IsEditor() )
		{
			return;
		}

		if ( ! m_resourcesCached )
		{
			return;
		}

		assert( gEnv->pAudioSystem );
		//gEnv->pSoundSystem->RemoveCachedAudioFile( m_gameHintName.c_str(), false );
		m_resourcesCached = false;
	}


	void OnHide()
	{
		m_soundManagerHelper.StopAllSounds();
		ReleaseResources();
	}

	void OnUnhide()
	{
		CacheResources();
	}

	void OnActivated()
	{
	}

	void OnDeactivated()
	{
		m_soundManagerHelper.StopAllSounds();
	}


private:
	CTurretSoundManagerHelper m_soundManagerHelper;

	float m_oldYawRadians;
	float m_oldPitchRadians;
	float m_movementStartSoundThresholdRadiansSecond;
	float m_movementStopSoundThresholdRadiansSecond;

	bool m_movingHorizontal;
	bool m_movingVertical;

	CTimeValue m_startHorizontalLoopingSoundTime;
	CTimeValue m_startVerticalLoopingSoundTime;

	float m_horizontalLoopingSoundDelaySeconds;
	float m_verticalLoopingSoundDelaySeconds;

	int16 m_yawJointId;
	int16 m_pitchJointId;

	float m_alertedSoundTimeoutSeconds;
	CTimeValue m_nextAlertedTime;

	bool m_waitingForLockOnTarget;
	CTimeValue m_nextLockOnTargetTime;
	float m_lockOnTargetSeconds;

	bool m_resourcesCached;
	CCryName m_gameHintName;
};

#endif