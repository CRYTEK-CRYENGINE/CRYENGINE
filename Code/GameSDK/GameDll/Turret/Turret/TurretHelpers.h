// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TURRET_HELPERS__H__
#define __TURRET_HELPERS__H__

#include "Game.h"
#include "IGameRulesSystem.h"
#include "Turret.h"

class CTacticalManager;

namespace TurretHelpers
{
	ILINE CTurret* FindTurret( const EntityId entityId )
	{
		assert( g_pGame != NULL );

		IGameFramework* pGameFramework = g_pGame->GetIGameFramework();
		assert( pGameFramework );

		IGameObject* pGameObject = pGameFramework->GetGameObject( entityId );
		IGameObjectExtension* pTurretExtension = pGameObject ? pGameObject->QueryExtension( "Turret" ) : NULL;
		CTurret* pTurret = pTurretExtension ? static_cast< CTurret* >( pTurretExtension ) : NULL;
		return pTurret;
	}


	ILINE IGameRules* GetCurrentGameRules()
	{
		assert( g_pGame != NULL );

		IGameFramework* pGameFramework = g_pGame->GetIGameFramework();
		if ( pGameFramework == NULL )
		{
			return NULL;
		}

		IGameRulesSystem* pGameRulesSystem = pGameFramework->GetIGameRulesSystem();
		if ( pGameRulesSystem == NULL )
		{
			return NULL;
		}

		IGameRules* pGameRules = pGameRulesSystem->GetCurrentGameRules();
		return pGameRules;
	}


	ILINE IMannequin* GetMannequinInterface()
	{
		assert( g_pGame != NULL );

		IGameFramework* pGameFramework = g_pGame->GetIGameFramework();
		if ( pGameFramework == NULL )
		{
			return NULL;
		}

		IMannequin& mannequinInterface = pGameFramework->GetMannequinInterface();
		return &mannequinInterface;
	}


	ILINE IItemSystem* GetItemSystem()
	{
		assert( g_pGame != NULL );

		IGameFramework* pGameFramework = g_pGame->GetIGameFramework();
		if ( pGameFramework == NULL )
		{
			return NULL;
		}

		IItemSystem* pItemSystem = pGameFramework->GetIItemSystem();
		return pItemSystem;
	}


	ILINE IEntitySystem* GetEntitySystem()
	{
#if !defined(SYS_ENV_AS_STRUCT)
		assert( gEnv != NULL );
#endif 
		IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
		return pEntitySystem;
	}


	ILINE float GetFrameTimeSeconds()
	{
#if !defined(SYS_ENV_AS_STRUCT)
		assert( gEnv != NULL );
#endif
		ITimer* pTimer = gEnv->pTimer;
		assert( pTimer != NULL );

		const float frameTimeSeconds = pTimer->GetFrameTime();
		return frameTimeSeconds;
	}


	ILINE CTimeValue GetTimeNow()
	{
#if !defined(SYS_ENV_AS_STRUCT)
		assert( gEnv != NULL );
#endif
		ITimer* pTimer = gEnv->pTimer;
		assert( pTimer != NULL );

		return pTimer->GetAsyncTime();
	}


	template< typename T >
	ILINE T GetProperty( SmartScriptTable pPropertiesTable, const char* propertyName, const T& defaultValue )
	{
		if ( ! pPropertiesTable )
		{
			return defaultValue;
		}

		T propertyValue = defaultValue;
		const bool hasProperty = pPropertiesTable->GetValue( propertyName, propertyValue );
		if ( ! hasProperty )
		{
			return defaultValue;
		}

		return propertyValue;
	}


	template< typename T >
	ILINE T GetEntityProperty( const IEntity* pEntity, const char* propertyName, const T& defaultValue )
	{
		assert( pEntity != NULL );

		IScriptTable* pScriptTable = pEntity->GetScriptTable();
		if ( pScriptTable == NULL )
		{
			return defaultValue;
		}

		SmartScriptTable pPropertiesTable;
		const bool hasPropertiesTable = pScriptTable->GetValue( "Properties", pPropertiesTable );
		if ( ! hasPropertiesTable )
		{
			return defaultValue;
		}

		const T propertyValue = GetProperty( pPropertiesTable, propertyName, defaultValue );
		return propertyValue;
	}

	template< typename T >
	ILINE T GetEntityInstanceProperty( const IEntity* pEntity, const char* propertyName, const T& defaultValue )
	{
		assert( pEntity != NULL );

		IScriptTable* pScriptTable = pEntity->GetScriptTable();
		if ( pScriptTable == NULL )
		{
			return defaultValue;
		}

		SmartScriptTable pPropertiesInstanceTable;
		const bool hasPropertiesInstanceTable = pScriptTable->GetValue( "PropertiesInstance", pPropertiesInstanceTable );
		if ( ! hasPropertiesInstanceTable )
		{
			return defaultValue;
		}

		const T propertyValue = GetProperty( pPropertiesInstanceTable, propertyName, defaultValue );
		return propertyValue;
	}


	ILINE SmartScriptTable GetSubPropertiesTable( const IEntity* pEntity, const char* subTableName )
	{
		assert( pEntity != NULL );
		assert( subTableName != NULL );
		assert( subTableName[ 0 ] != 0 );

		IScriptTable* pScriptTable = pEntity->GetScriptTable();
		if ( pScriptTable == NULL )
		{
			return SmartScriptTable();
		}

		SmartScriptTable pPropertiesTable;
		const bool hasPropertiesTable = pScriptTable->GetValue( "Properties", pPropertiesTable );
		if ( ! hasPropertiesTable )
		{
			return SmartScriptTable();
		}

		SmartScriptTable pSubPropertiesTable;
		pPropertiesTable->GetValue( subTableName, pSubPropertiesTable );

		return pSubPropertiesTable;
	}


	ILINE SmartScriptTable GetSubScriptTable( SmartScriptTable pScriptTable, const char* name )
	{
		assert( name != NULL );
		assert( name[ 0 ] != 0 );

		if ( ! pScriptTable )
		{
			return SmartScriptTable();
		}

		SmartScriptTable pSubScriptTable;
		const bool hasSubScriptTable = pScriptTable->GetValue( name, pSubScriptTable );
		if ( ! hasSubScriptTable )
		{
			return SmartScriptTable();
		}

		return pSubScriptTable;
	}


	ILINE SmartScriptTable GetVisionPropertiesTable( const IEntity* pEntity )
	{
		SmartScriptTable pVisionPropertiesTable = GetSubPropertiesTable( pEntity, "Vision" );
		return pVisionPropertiesTable;
	}


	ILINE SmartScriptTable GetMannequinPropertiesTable( const IEntity* pEntity )
	{
		SmartScriptTable pVisionPropertiesTable = GetSubPropertiesTable( pEntity, "Mannequin" );
		return pVisionPropertiesTable;
	}


	ILINE SmartScriptTable GetBehaviorPropertiesTable( const IEntity* pEntity )
	{
		SmartScriptTable pBehaviorPropertiesTable = GetSubPropertiesTable( pEntity, "Behavior" );
		return pBehaviorPropertiesTable;
	}


	ILINE SmartScriptTable GetRateOfDeathPropertiesTable( const IEntity* pEntity )
	{
		SmartScriptTable pRateOfDeathPropertiesTable = GetSubPropertiesTable( pEntity, "RateOfDeath" );
		return pRateOfDeathPropertiesTable;
	}


	ILINE SmartScriptTable GetDamagePropertiesTable( const IEntity* pEntity )
	{
		SmartScriptTable pDamagePropertiesTable = GetSubPropertiesTable( pEntity, "Damage" );
		return pDamagePropertiesTable;
	}


	ILINE SmartScriptTable GetLaserPropertiesTable( const IEntity* pEntity )
	{
		SmartScriptTable pLaserPropertiesTable = GetSubPropertiesTable( pEntity, "Laser" );
		return pLaserPropertiesTable;
	}


	ILINE SmartScriptTable GetAutoAimParamsTable( const IEntity* pEntity )
	{
		SmartScriptTable pAutoAimParamsTable = GetSubPropertiesTable( pEntity, "AutoAimTargetParams" );
		return pAutoAimParamsTable;
	}


	ILINE CScriptBind_Turret* GetScriptBindTurret()
	{
		assert( g_pGame != NULL );

		CScriptBind_Turret* pScriptBindTurret = g_pGame->GetTurretScriptBind();
		return pScriptBindTurret;
	}

	
	ILINE CTacticalManager* GetTacticalManager()
	{
		assert( g_pGame != NULL );

		CTacticalManager* pTacticalManager = g_pGame->GetTacticalManager();
		return pTacticalManager;
	}


	ILINE void CalculateTargetYaw( const Vec3& targetLocalPosition, float& yawRadiansOut, float& yawDistanceOut )
	{
		const Vec2 targetLocalPositionXY( targetLocalPosition.x, targetLocalPosition.y );
		const Vec2 normalisedTargetLocalPositionXY = targetLocalPositionXY.GetNormalizedSafe( Vec2( 0, 1 ) );
		yawRadiansOut = atan2_tpl( normalisedTargetLocalPositionXY.x, normalisedTargetLocalPositionXY.y );
		yawDistanceOut = targetLocalPositionXY.GetLength();
	}


	ILINE void CalculateTargetPitch( const Vec3& targetLocalPosition, const QuatT& pitchJointLocation, float& pitchRadiansOut, float& pitchDistanceOut )
	{
		const QuatT invertedPitchJointLocation = pitchJointLocation.GetInverted();
		const Vec3 pitchJointLocalTargetPosition = invertedPitchJointLocation * targetLocalPosition;

		pitchDistanceOut = pitchJointLocalTargetPosition.GetLength();

		if ( pitchDistanceOut != 0 )
		{
			pitchRadiansOut = ( gf_PI * 0.5f ) - acos( pitchJointLocalTargetPosition.x / pitchDistanceOut );
		}
		else
		{
			pitchRadiansOut = 0;
		}
	}


	ILINE void CalculateTargetOrientation( const Vec3& targetLocalPosition, const QuatT& pitchJointLocation, float& yawRadiansOut, float& pitchRadiansOut, float& yawDistanceOut, float& pitchDistanceOut )
	{
		CalculateTargetYaw( targetLocalPosition, yawRadiansOut, yawDistanceOut );
		CalculateTargetPitch( targetLocalPosition, pitchJointLocation, pitchRadiansOut, pitchDistanceOut );
	}


	ILINE Vec2 CalculateTargetLocalPositionXY( const float yawRadians, const float yawDistance )
	{
		const float sinYaw = sin( yawRadians );
		const float cosYaw = cos( yawRadians );

		const float x = sinYaw * yawDistance;
		const float y = cosYaw * yawDistance;

		return Vec2( x, y );
	}


	ILINE float CalculateTargetLocalPositionZ( const QuatT& pitchJointLocation, const float pitchRadians, const float pitchDistance )
	{
		const float sinPitch = sin( pitchRadians );
		const float z = ( sinPitch * pitchDistance ) + pitchJointLocation.t.z;

		return z;
	}


	ILINE Vec3 CalculateTargetLocalPosition( const QuatT& pitchJointLocation, const float yawRadians, const float pitchRadians, const float yawDistance, const float pitchDistance )
	{
		const Vec2 xy = CalculateTargetLocalPositionXY( yawRadians, yawDistance );
		const float z = CalculateTargetLocalPositionZ( pitchJointLocation, pitchRadians, pitchDistance );

		return Vec3( xy.x, xy.y, z );
	}


	template< typename T >
	T Mod( const T x, const T y )
	{
		if ( y == 0 )
			return x;

		return x - ( y * floor( x / y ) );
	}


	template< typename T >
	T Wrap( const T value, const T lowerBound, const T upperBound )
	{
		return Mod( value - lowerBound, upperBound - lowerBound ) + lowerBound;
	}
}

#endif