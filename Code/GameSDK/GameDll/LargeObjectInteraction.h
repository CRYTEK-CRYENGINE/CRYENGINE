// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:
	Part of Player that controls interaction with large objects

-------------------------------------------------------------------------
History:


*************************************************************************/

#pragma once

#ifndef __LARGEOBJECT_INTERACTION_H__
#define __LARGEOBJECT_INTERACTION_H__

class CPlayer;

class CLargeObjectInteraction
{
	enum BoostLevel
	{
		eBoost_None = 0,
		eBoost_Normal,
		eBoost_Maximum
	};

public:

	CLargeObjectInteraction()
	: m_pPlayer( NULL )
	, m_lastObjectId( 0 )
	, m_hitTargetId( 0 )
	, m_state( ST_IDLE )
	, m_timer( 0.f )
	, m_boostLevel(eBoost_None)
	, m_bSkipAnim(false)
	, m_interactionCount(0)
	{
	}

	void Init( CPlayer* pPlayer );
	ILINE bool IsBusy() const { return (m_state!=ST_IDLE); }
	void Enter( EntityId objectEntityId , const bool bSkipKickAnim = false);
	void Update( float frameTime );
	bool IsLargeObject( IEntity* pEntity, const SmartScriptTable& propertiesTable ) const;
	bool CanExecuteOn( IEntity* pEntity, bool objectWithinProximity ) const;
	void ReadXmlData(const IItemParamsNode* pRootNode, bool reloading = false);


	void NetSerialize(TSerialize ser);
	void NetEnterInteraction(EntityId entityId, const bool bSkipAnim = false);

	EntityId GetLastObjectId() const { return m_lastObjectId; } 
	const CTimeValue& GetLastObjectTime() const { return m_lastObjectTime; }

private:

	void ThrowObject();
	void Leave();
	
	EntityId GetBestTargetAndDirection(const Vec3& playerDirection2D, Vec3& outputTargetDirection) const;
	void AdjustObjectTrajectory(float frameTime);
	

private:

	enum EState { ST_IDLE, ST_WAITING_FOR_PUNCH, ST_WAITING_FOR_END_ANIM };

	CPlayer*	m_pPlayer;
	EntityId	m_lastObjectId;
	CTimeValue	m_lastObjectTime;
	EntityId	m_hitTargetId;
	EState		m_state;
	float		m_timer;
	bool		m_bSkipAnim; 
	uint8		m_boostLevel;
	uint8   m_interactionCount; 

	static const uint8	kInteractionCountMax = 4;

	struct TXmlParams
	{
		TXmlParams()
		: impulseDelay(0.f)
		, cosSideImpulseAngle(1.f)
		, impulseFactorNoPower(0.5f)
		, impulseFactorMaximumPower(1.25f)
		, interactionDistanceSP(1.5f)
		, interactionDistanceMP(1.5f)
		{}
		float impulseDelay;					
		float cosSideImpulseAngle;   // defines when is a 'side' or 'front' interaction
		float impulseFactorNoPower;
		float impulseFactorMaximumPower; // applied when brute force is upgraded to 'maximum'
		float interactionDistanceSP;
		float interactionDistanceMP;

		struct TImpulseParams
		{
			TImpulseParams()
			: tanVerticalAngle(0.f)
			, speed(0.f)
			, rotationFactor(0.f)
			, swingRotationFactor(0.f)
			{}
			
			float tanVerticalAngle;
			float speed;
			float rotationFactor;
			float swingRotationFactor;

			void ReadXmlData( const IItemParamsNode* pNode, const char* pChildNodeName );
		};

		struct TAssistanceParams
		{
			TAssistanceParams()
				: cos2DAngleThreshold(0.5f)
				, distanceThreshold(15.0f)
			{

			}

			float cos2DAngleThreshold;
			float distanceThreshold;
			void ReadXmlData( const IItemParamsNode* pNode, const char* pChildNodeName );
		};

		struct TMPParams
		{
			float cosSideImpulseAngle;   // defines when it's a 'side' interaction
			float cosFrontImpulseAngle;  // defines when it's a 'front' interaction
			float timer;                 // defines how long to turn off handbrake etc
			
			TMPParams()
			: cosSideImpulseAngle(0.f)
			, cosFrontImpulseAngle(0.f)
			, timer(0.f)
			{}

			void ReadXmlData( const IItemParamsNode* pNode, const char* pChildNodeName );
		};
		
		TImpulseParams frontImpulseParams;
		TImpulseParams sideImpulseParams;
		TAssistanceParams assistanceParams;

		// MP - kickable car params
		TMPParams mpParams;
		TImpulseParams mpKickableFrontParams;
		TImpulseParams mpKickableSideParams;
		TImpulseParams mpKickableDiagonalParams;
		
		void ReadXmlData( const IItemParamsNode* pNode );
	};
	
	static TXmlParams s_XmlParams;
	static bool s_XmlDataLoaded;
};


#endif // __LARGEOBJECT_INTERACTION_H__
