// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Contains the ray testing code extracted from CPlayer.

-------------------------------------------------------------------------
History:
- 20.10.10: Created by Stephen M. North

*************************************************************************/
#ifndef __PlayerStateSwim_WaterTestProxy_h__
#define __PlayerStateSwim_WaterTestProxy_h__

class CPlayer;

class CPlayerStateSwim_WaterTestProxy
{
	enum EProxyInternalState
	{
		eProxyInternalState_OutOfWater = 0,
		eProxyInternalState_PartiallySubmerged,
		eProxyInternalState_Swimming,
	};

public:
	CPlayerStateSwim_WaterTestProxy();
	~CPlayerStateSwim_WaterTestProxy();

	void Reset( bool bCancelRays );

	void OnEnterWater(const CPlayer& player);
	void OnExitWater(const CPlayer& player);

	void PreUpdateNotSwimming(const CPlayer& player, const float frameTime);
	void PreUpdateSwimming(const CPlayer& player, const float frameTime);
	void Update(const CPlayer& player, const float frameTime);
	void ForceUpdateBottomLevel(const CPlayer& player);

	ILINE bool ShouldSwim() const { return m_shouldSwim; }
	ILINE bool IsInWater() const { return m_internalState != eProxyInternalState_OutOfWater; }
	ILINE bool IsHeadUnderWater() const { return m_headUnderwater; }
	ILINE bool IsHeadComingOutOfWater() const { return m_headComingOutOfWater; }
	ILINE float GetRelativeBottomDepth() const { return m_relativeBottomLevel; }
	ILINE float GetWaterLevel() const { return m_waterLevel; }
	ILINE float GetRelativeWaterLevel() const { return m_playerWaterLevel; }
	ILINE float GetSwimmingTimer() const { return m_swimmingTimer; }
	ILINE float GetLastRaycastResult() const { return m_lastRayCastResult; }

	ILINE void	SetSubmergedFraction( const float submergedFraction ) { m_submergedFraction = submergedFraction; }
	ILINE float GetSubmergedFraction() const { return m_submergedFraction; }

	ILINE float GetWaterLevelTimeUpdated() const { return m_timeWaterLevelLastUpdated; }
	ILINE float GetOxygenLevel() const { return m_oxygenLevel; }

	ILINE static float GetRayLength() { return s_rayLength; }

private:

	void UpdateWaterLevel( const Vec3& worldReferencePos, const Vec3& playerWorldPos, IPhysicalEntity* piPhysEntity );


	void UpdateOutOfWater(const CPlayer& player, const float frameTime);
	void UpdateInWater(const CPlayer& player, const float frameTime);
	void UpdateSubmergedFraction(const float referenceHeight, const float playerHeight, const float waterLevel);

	static Vec3 GetLocalReferencePosition(const CPlayer& player);
	bool ShouldSwim(const float referenceHeight) const;
	void UpdateDrowningTimer(const float frameTime);
	void UpdateDrowning(const CPlayer& player);


	//Deferred raycast functions
	void OnRayCastBottomLevelDataReceived(const QueuedRayID& rayID, const RayCastResult& result);
	ILINE bool IsWaitingForBottomLevelResults() const { return (m_bottomLevelRayID != 0); }
	void RayTestBottomLevel( const CPlayer& player, const Vec3& referencePosition, float maxRelevantDepth );
	void CancelPendingRays();

	//Debug
#if !defined(_RELEASE)
	void DebugDraw(const CPlayer& player, const Vec3& referencePosition);
#endif

	EProxyInternalState m_internalState;
	EProxyInternalState m_lastInternalState;
	Vec3				m_lastWaterLevelCheckPosition;
	float				m_submergedFraction;
	float				m_bottomLevel;
	float				m_relativeBottomLevel;
	float				m_waterLevel;
	float				m_playerWaterLevel;
	float				m_swimmingTimer;
	float				m_drowningTimer;
	float				m_oxygenLevel;
	float				m_timeWaterLevelLastUpdated;
	float				m_lastRayCastResult;

	bool				m_headUnderwater;
	bool        m_headComingOutOfWater;
	bool				m_shouldSwim;

	QueuedRayID m_bottomLevelRayID;

	static float s_rayLength;
};


#endif // __PlayerStateSwim_WaterTestProxy_h__
