// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   VehicleMovementMPVTOL.h
//  Version:     v1.00
//  Created:     02/09/2011
//  Compilers:   Visual Studio.NET
//  Description: Handles movement of the multiplayer version of the VTOL vehicle
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __VEHICLEMOVEMENTMPVTOL_H__
#define __VEHICLEMOVEMENTMPVTOL_H__

#include "VehicleMovementHelicopter.h"
#include "WaypointPath.h"

struct SVTOLPathPosParams
{
	SVTOLPathPosParams()
		: location(0.f)
		, pathId(0)
	{
	}

	void Serialize(TSerialize ser)
	{
		ser.Value("pathId", pathId, 'ui3');
		ser.Value("location", location, 'pLoc');
	}

	float location;	// Integer part is the Path Node index, decimal part is the distance between it and the next node.
	uint8 pathId;
};

class CVehicleMovementMPVTOL : public CVehicleMovementHelicopter
{
	typedef CVehicleMovementHelicopter BaseClass;
	#define MAX_NOISE_OVERRIDES 2
	static const NetworkAspectType k_vtolPathUpdate = eEA_GameClientC;

public:
	CVehicleMovementMPVTOL();
	virtual ~CVehicleMovementMPVTOL();

	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table);
	virtual void PostInit();
	virtual void Reset();
	virtual void Update(const float deltaTime);
	virtual void Serialize(TSerialize ser, EEntityAspects aspects);
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params);
  virtual void PostPhysicalize();
	virtual void ProcessAI(const float deltaTime);

	EntityId GetDestroyerId() const { return m_destroyerId; };

	//CVehicleMovementHelicopter
	virtual void UpdateNetworkError(const Vec3& netPosition, const Quat& netRotation);

	void UpdatePathSpeed( const float speed );
	void SetPathInfo( const struct SPathFollowingAttachToPathParameters& params, const CWaypointPath* pPath );
	void SnapToPathLoc( const SVTOLPathPosParams& data );
	bool FillPathingParams( struct SPathFollowingAttachToPathParameters& params );
	ILINE uint8 GetPathComplete ( ) const { return m_pathing.pathComplete; }
	ILINE void SetPathCompleteNotified ( ) { m_pathing.pathComplete|=0x2; }

protected:
	virtual float GetRollSpeed() const;
	virtual void CalculatePitch(const Ang3& worldAngles, const Vec3& desiredMoveDir, float currentSpeed2d, float desiredSpeed, float deltaTime);
	void ReceivedServerPathingData(const SVTOLPathPosParams& params);

private:
	float m_rollSpeedMultiplier;
	float m_prevDamageRatio;
	EntityId m_destroyerId;

	CVTolNoise m_noiseOverrides[MAX_NOISE_OVERRIDES];

	//Wing Rotation
	CVehicleNoiseValue m_leftWingNoise;
	CVehicleNoiseValue m_rightWingNoise;

	IVehiclePart* m_pLeftWing;
	IVehiclePart* m_pRightWing;

	float m_maxAngleRad; //The maximum angle to rotate the wings by (+/-)
	float m_maxAngleSpeed; //The speed at which the max angle can be reached
	float m_angularVelMult; //Wing angle [0-max] is based on angular velocity * this value.
	float m_angleSmoothTime; //How sluggish wing movement is
	float m_insideWingSpeedMult; //Adjusts the speed of the rotation of the wing on the inside of the turning circle
	float m_insideWingMaxAngleMult; //Adjusts the maximum angle of the rotation of the wing on the inside of the turning circle

	float m_leftWingRotationRad;
	float m_leftWingRotationSmoothRate;
	float m_rightWingRotationRad;
	float m_rightWingRotationSmoothRate;

	//Exhaust stuttering
	float m_exhaustsTimer; //Time until next toggle of exhausts on/off
	float m_stutterStartDamageRatio; //When the stuttering kicks in
	float m_invStutterDamageSpread;
	float m_minTimeBetweenStutters; //After a group of stuttering has happened wait at least this long before the next group
	float m_maxTimeBetweenStutters; //After a group of stuttering has happened wait at most this long before the next group
	float m_timeBetweenStutterDecayMult; //Reduces time between stutters based on damage ratio
	float m_stutterDurationOn; //During a stutter group wait this time between each toggle on
	float m_stutterDurationOff; //During a stutter group wait this time between each toggle off
	bool	m_exhaustsCutOut;
	int		m_baseStutters; //Stutter at least this many times in a row
	int		m_maxExtraStutters; //Randomly stutter up to this many extra times
	int		m_exhaustStuttersRemaining;

	//////////////////////////////////////////////////////////////////////////
	// Pathing Info.
	struct SPathing
	{
		SPathing()
			: pCachedPathPtr(NULL)
			, currentNode(-1)
			, interpolatedNode(-1)
			, speed(0.f)
			, targetSpeed(0.f)
			, accel(0.f)
			, defaultSpeed(0.f)
			, waitTime(0.f)
			, networkError(0.f)
			, shouldLoop(false)
			, pathComplete(0)
		{}
		~SPathing(){}

		void Reset();
		void SetPathInfo( const struct SPathFollowingAttachToPathParameters& params, const CWaypointPath* pPath );

		// Suppress passedByValue for tiny objects like SVTOLPathPosParams
		// cppcheck-suppress passedByValue
		bool SnapTo( const SVTOLPathPosParams data, QuatT& rLocation );
		void UpdatePathFollowing(const EntityId vtolId, const float dt, CVehicleMovementMPVTOL& rMovement, const SVehiclePhysicsStatus& physStatus, const Vec3& posNoise);

		SVTOLPathPosParams pathingData;
		const CWaypointPath* pCachedPathPtr;
		CWaypointPath::TNodeId currentNode;
		CWaypointPath::TNodeId interpolatedNode;
		float speed; //Current
		float targetSpeed;
		float accel;
		float defaultSpeed;
		float waitTime;
		float networkError;
		bool shouldLoop;
		uint8 pathComplete;
	};

	SPathing m_pathing;
};

#endif //__VEHICLEMOVEMENTMPVTOL_H__
