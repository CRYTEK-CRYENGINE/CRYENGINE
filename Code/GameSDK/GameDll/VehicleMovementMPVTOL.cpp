// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleMovementMPVTOL.h"
#include "VTOLVehicleManager/VTOLVehicleManager.h"
#include "PersistantStats.h"
#include "GamePhysicsSettings.h"
#include "WaypointPath.h"
//#include "Utility/CryWatch.h"

CVehicleMovementMPVTOL::CVehicleMovementMPVTOL() 
	: m_rollSpeedMultiplier(0.f)
	, m_destroyerId(0)
	, m_exhaustsCutOut(false)
	, m_exhaustsTimer(0.f)
	, m_stutterStartDamageRatio(0.f)
	, m_invStutterDamageSpread(1.f)
	, m_minTimeBetweenStutters(0.f)
	, m_maxTimeBetweenStutters(0.f)
	, m_timeBetweenStutterDecayMult(0.f)
	, m_stutterDurationOn(0.f)
	, m_stutterDurationOff(0.f)
	, m_baseStutters(0)
	, m_maxExtraStutters(0)
	, m_exhaustStuttersRemaining(0)
	, m_prevDamageRatio(0.f)
	, m_maxAngleRad(DEG2RAD(90.f))
	, m_maxAngleSpeed(5.f)
	, m_angularVelMult(5.f)
	, m_angleSmoothTime(0.5f)
	, m_insideWingSpeedMult(1.f)
	, m_insideWingMaxAngleMult(1.f)
	, m_leftWingRotationRad(0.f)
	, m_leftWingRotationSmoothRate(0.f)
	, m_rightWingRotationRad(0.f)
	, m_rightWingRotationSmoothRate(0.f)
	, m_pLeftWing(NULL)
	, m_pRightWing(NULL)
{
}

CVehicleMovementMPVTOL::~CVehicleMovementMPVTOL()
{
	m_pVehicle->UnregisterVehicleEventListener(this); 
}

bool CVehicleMovementMPVTOL::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	bool success = BaseClass::Init(pVehicle, table);

	//ToDo: Any VTOL animations/effects setup
	MOVEMENT_VALUE("rollSpeedMultiplier", m_rollSpeedMultiplier);
	
	// Control what needs to be network synced
	int vtolmanagerControlled = 0;
	table.getAttr("vtolmanagerControlled", vtolmanagerControlled);
	if (vtolmanagerControlled && g_pGame->GetGameRules()->GetVTOLVehicleManager())
	{
		// No need to do low level physics synchronisation
		m_pVehicle->GetGameObject()->DontSyncPhysics();
		// Disable some of the additional network serialisation
		// Disable all pos, rot serialization. All Network syncing is done via catchup along the path, handled by the manager.
		// The only data sent is the pathId, and the distance along the path (pathLoc)
		//m_netActionSync.m_actionRep.SetFlags(CNetworkMovementHelicopterCrysis2::k_syncPos|CNetworkMovementHelicopterCrysis2::k_controlPos);
		m_sendTime = g_pGameCVars->v_MPVTOLNetworkSyncFreq;
		m_updateAspects |= k_vtolPathUpdate;
	}

	if(CVehicleParams stutterParams = table.findChild("EngineStuttering"))
	{
		stutterParams.getAttr("stutterStartDamageRatio", m_stutterStartDamageRatio);
		if(m_stutterStartDamageRatio == 1.f)
		{
			GameWarning("CVehicleMovementMPVTOL::Init: stutterStartDamageRatio XML param == 1.0f. Will cause divide by 0 error in release.");
			m_stutterStartDamageRatio = 0.f;
		}
		m_invStutterDamageSpread = __fres(1.f - m_stutterStartDamageRatio);
		stutterParams.getAttr("minStutters", m_baseStutters);
		stutterParams.getAttr("maxStutters", m_maxExtraStutters);
		m_maxExtraStutters -= m_baseStutters;
		stutterParams.getAttr("minTimeBetweenStutters", m_minTimeBetweenStutters);
		stutterParams.getAttr("maxTimeBetweenStutters", m_maxTimeBetweenStutters);
		stutterParams.getAttr("timeBetweenStutterDecayMult", m_timeBetweenStutterDecayMult);
		stutterParams.getAttr("stutterDurationOn", m_stutterDurationOn);
		stutterParams.getAttr("stutterDurationOff", m_stutterDurationOff);
	}

	if(CVehicleParams wingRotationParams = table.findChild("WingRotation"))
	{
		float maxAngleDeg;
		if(wingRotationParams.getAttr("maxAngleDeg", maxAngleDeg))
		{
			m_maxAngleRad = DEG2RAD(maxAngleDeg);
		}
		wingRotationParams.getAttr("canReachMaxAngleAtSpeed", m_maxAngleSpeed);
		CRY_ASSERT_MESSAGE(m_maxAngleSpeed, "CVehicleMovementMPVTOL::Init - canReachMaxAngleAtSpeed is 0. This will result in divide by 0 error during update.");
		wingRotationParams.getAttr("angleAngularVelMult", m_angularVelMult);
		wingRotationParams.getAttr("smoothTime", m_angleSmoothTime);
		wingRotationParams.getAttr("insideWingSpeedMult", m_insideWingSpeedMult);
		wingRotationParams.getAttr("insideWingMaxAngleMult", m_insideWingMaxAngleMult);

		float noiseAmp = 0.f;
		float noiseFreq = 0.f;
		wingRotationParams.getAttr("noiseAmplitude", noiseAmp);
		wingRotationParams.getAttr("noiseFrequency", noiseFreq);

		m_leftWingNoise.Setup(noiseAmp, noiseFreq, 0x473845);
		m_rightWingNoise.Setup(noiseAmp, noiseFreq, 0x287463);
	}

	if(CVehicleParams noiseOverrideParams = table.findChild("NoiseOverrides"))
	{
		int count = noiseOverrideParams.getChildCount();
		for(int i = 0; i < count; ++i)
		{
			if(i == MAX_NOISE_OVERRIDES)
			{
				GameWarning("CVehicleMovementMPVTOL::Init - %i noise overrides specified in XML but only %i currently supported. Needs code change to support more.", count, MAX_NOISE_OVERRIDES);
				break;
			}

			InitMovementNoise(noiseOverrideParams.getChild(i), m_noiseOverrides[i]);
		}
	}

	pVehicle->RegisterVehicleEventListener(this, "CVehicleMovementMPVTOL"); 

	return success;
}

void CVehicleMovementMPVTOL::PostPhysicalize()
{
	BaseClass::PostPhysicalize();

	if(IPhysicalEntity* pPhys = m_pVehicle->GetEntity()->GetPhysics())
	{
		if(CGamePhysicsSettings* pGPS = g_pGame->GetGamePhysicsSettings())
		{
			// Set the Collision Class flags.
			pGPS->SetCollisionClassFlags( *pPhys, gcc_vtol );

			// Make it NOT have vehicle type Physics.
			pe_params_part ppart;
			ppart.flagsColliderAND = ~geom_colltype_vehicle;
			pPhys->SetParams(&ppart);
		}
	}
}

void CVehicleMovementMPVTOL::Reset()
{
	BaseClass::Reset();

	m_destroyerId = 0;
	m_exhaustsCutOut = false;
	m_exhaustsTimer = 0.f;
	m_exhaustStuttersRemaining = 0;
	m_prevDamageRatio = 0.f;
	m_leftWingRotationRad = 0.f;
	m_leftWingRotationSmoothRate = 0.f;
	m_rightWingRotationRad = 0.f;
	m_rightWingRotationSmoothRate = 0.f;
}

void CVehicleMovementMPVTOL::Update(const float deltaTime)
{
	BaseClass::Update(deltaTime);

	//Update wing rotation based on linear and angular velocities
	if(m_pLeftWing && m_pRightWing)
	{
		const float angVelZ = m_physStatus[k_mainThread].w.z;
		const float speedRatio = min(1.f, __fres(m_maxAngleSpeed)*m_CurrentSpeed);

		//Move inside and outside wings (in terms of turning circle) at different speeds
		const bool leftWingIsInside = angVelZ > 0.f;

		const float maxRadAtCurrentSpeedOutside = m_maxAngleRad * speedRatio * clamp_tpl(m_angularVelMult * angVelZ, -1.f, 1.f);
		const float maxRadAtCurrentSpeedInside = maxRadAtCurrentSpeedOutside * m_insideWingMaxAngleMult;

		const float insideSmoothRate = m_angleSmoothTime * (2.f - speedRatio);
		const float outsideSmoothRate = insideSmoothRate * m_insideWingSpeedMult;

		if(leftWingIsInside)
		{
			SmoothCD(m_leftWingRotationRad, m_leftWingRotationSmoothRate, deltaTime, maxRadAtCurrentSpeedInside, insideSmoothRate);
			SmoothCD(m_rightWingRotationRad, m_rightWingRotationSmoothRate, deltaTime, maxRadAtCurrentSpeedOutside, outsideSmoothRate);
		}
		else
		{
			SmoothCD(m_leftWingRotationRad, m_leftWingRotationSmoothRate, deltaTime, maxRadAtCurrentSpeedOutside, outsideSmoothRate);
			SmoothCD(m_rightWingRotationRad, m_rightWingRotationSmoothRate, deltaTime, maxRadAtCurrentSpeedInside, insideSmoothRate);
		}

		const float leftWingNoise = m_leftWingNoise.Update(deltaTime);
		const float rightWingNoise = m_rightWingNoise.Update(deltaTime);

		const Quat leftRotation = Quat::CreateRotationX(m_leftWingRotationRad+leftWingNoise);
		const Quat rightRotation = Quat::CreateRotationX(-m_rightWingRotationRad+rightWingNoise);

		m_pLeftWing->SetLocalBaseTM(Matrix34(leftRotation));
		m_pRightWing->SetLocalBaseTM(Matrix34(rightRotation));
	}
	
	//Exhausts periodically cut out whilst damaged
	if(m_exhaustsCutOut || m_exhaustsTimer)
	{
		m_exhaustsTimer -= deltaTime;
		if(m_exhaustsTimer < 0.f)
		{
			if(m_exhaustsCutOut)
			{
				StartExhaust();
				if(--m_exhaustStuttersRemaining > 0)
				{
					m_exhaustsTimer = m_stutterDurationOn;
				}
				else
				{
					m_exhaustsTimer = cry_random(m_minTimeBetweenStutters, m_maxTimeBetweenStutters);

					//Time between stutters decreases linearly as more damage is received
					float progress = m_pVehicle->GetDamageRatio() - m_stutterStartDamageRatio;
					m_exhaustsTimer *= 1.f - ((progress * m_invStutterDamageSpread) * m_timeBetweenStutterDecayMult);
					
					m_exhaustStuttersRemaining = m_baseStutters + cry_random(0, m_maxExtraStutters); //Number of stutters next time
				}
			}
			else
			{
				StopExhaust();
				m_exhaustsTimer = m_stutterDurationOff;
			}
			m_exhaustsCutOut = !m_exhaustsCutOut;
		}
	}

#ifndef _RELEASE
	// Keep updating this, in case it is changed mid-game.
	m_sendTime = g_pGameCVars->v_MPVTOLNetworkSyncFreq;
#endif
}

void CVehicleMovementMPVTOL::Serialize(TSerialize ser, EEntityAspects aspects)
{
	CVTOLVehicleManager* pVTOLManager = g_pGame->GetGameRules()->GetVTOLVehicleManager();

	if(pVTOLManager && ser.GetSerializationTarget()==eST_Network)
	{
		// Don't call CVehicleMovementHelicopter::Serialize();
		CVehicleMovementBase::Serialize(ser, aspects);
		if( (aspects&k_vtolPathUpdate)!=0 )
		{
			SVTOLPathPosParams data(m_pathing.pathingData);

			// Serialize.
			data.Serialize(ser);

			// Apply results.
			if(ser.IsReading() && !gEnv->bServer)
			{
				ReceivedServerPathingData(data);
			}
		}
	}
	else
	{
		BaseClass::Serialize(ser, aspects);
	}

}

float CVehicleMovementMPVTOL::GetRollSpeed() const
{
	return gf_PI * m_rollSpeedMultiplier;
}

void CVehicleMovementMPVTOL::CalculatePitch( const Ang3& worldAngles, const Vec3& desiredMoveDir, float currentSpeed2d, float desiredSpeed, float deltaTime )
{
	float desiredPitch = m_CurrentVel.z;
	Limit(desiredPitch, -m_maxPitchAngle, m_maxPitchAngle);

	m_actionPitch = worldAngles.x + (desiredPitch - worldAngles.x) * deltaTime;
}

void CVehicleMovementMPVTOL::OnVehicleEvent( EVehicleEvent event, const SVehicleEventParams& params )
{
	if(event == eVE_Damaged)
	{
		//track stat of damage done to vehicle by local player

		if( params.entityId == gEnv->pGameFramework->GetClientActorId() )
		{
			CPersistantStats* pStats = CPersistantStats::GetInstance();

			pStats->IncrementClientStats( EFPS_DamageToFlyers, params.fParam );
		}


		if(params.fParam2 < 1.0f)
		{
			if(params.fParam2 >= m_stutterStartDamageRatio && m_prevDamageRatio < m_stutterStartDamageRatio)
			{
				m_exhaustsTimer = 0.5f;
				m_exhaustStuttersRemaining = m_baseStutters + cry_random(0, m_maxExtraStutters);
			}

			for(int i = 0; i < MAX_NOISE_OVERRIDES; ++i)
			{
				CVTolNoise& noise = m_noiseOverrides[i];
				if(params.fParam2 >= noise.m_startDamageRatio && m_prevDamageRatio < noise.m_startDamageRatio)
				{
					m_pNoise = &noise;
				}
			}
		}
		else
		{
			m_destroyerId = params.entityId;			
		}
		m_prevDamageRatio = params.fParam2;
	}

	BaseClass::OnVehicleEvent(event, params);
}

void CVehicleMovementMPVTOL::UpdateNetworkError( const Vec3& netPosition, const Quat& netRotation )
{
	if( !g_pGame->GetGameRules()->GetVTOLVehicleManager() )
	{
		BaseClass::UpdateNetworkError(netPosition,netRotation);
	}
	else
	{
		m_netPosAdjust.zero();
	}
}

void CVehicleMovementMPVTOL::PostInit()
{
	BaseClass::PostInit();

	for (int i = 0; i < m_pVehicle->GetPartCount(); i++)
	{
		IVehiclePart* pPart = m_pVehicle->GetPart(i);
		
		if(!pPart)
			continue;

		if (m_pLeftWing == NULL && strcmpi(pPart->GetName(), "wing_left_pivot") == 0)
		{
			m_pLeftWing = pPart;
			pPart->SetMoveable();
			m_pVehicle->SetObjectUpdate(pPart, IVehicle::eVOU_AlwaysUpdate);
		}
		else if(m_pRightWing == NULL && strcmpi(pPart->GetName(), "wing_right_pivot") == 0)
		{
			m_pRightWing = pPart;
			pPart->SetMoveable();
			m_pVehicle->SetObjectUpdate(pPart, IVehicle::eVOU_AlwaysUpdate);
		}
	}

	CRY_ASSERT_MESSAGE(m_pLeftWing && m_pRightWing, "CVehicleMovementMPVTOL::PostInit - Wing parts not found");
}



//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementMPVTOL::ProcessAI( const float deltaTime )
{
	CryAutoCriticalSection lk(m_lock);
	const SVehiclePhysicsStatus& physStatus = m_physStatus[k_physicsThread];

	if(!m_bApplyNoiseAsVelocity)
	{
		m_pNoise->Update(deltaTime);
	}

	m_pathing.UpdatePathFollowing(m_pVehicle->GetEntityId(), deltaTime, *this, physStatus, m_bApplyNoiseAsVelocity?Vec3(ZERO):m_pNoise->m_pos);

	BaseClass::ProcessAI(deltaTime);
}

void CVehicleMovementMPVTOL::SetPathInfo( const SPathFollowingAttachToPathParameters& params, const CWaypointPath* pPath )
{
	CryAutoCriticalSection lk(m_lock);

	m_pathing.SetPathInfo( params, pPath );

	// Snap if needed.
	int snapToNode = -1;
	if( gEnv->bServer && params.shouldStartAtInitialNode )
	{
		snapToNode = 0;
	}
	else if(params.forceSnap)
	{
		snapToNode = params.nodeIndex;
	}
	if( snapToNode>=0 )
	{
		SVTOLPathPosParams snap;
		snap.pathId = params.pathIndex;
		snap.location = (float)snapToNode;
		SnapToPathLoc(snap);
	}
}

void CVehicleMovementMPVTOL::SnapToPathLoc( const SVTOLPathPosParams& data )
{
	QuatT transform(IDENTITY);
	if(m_pathing.SnapTo(data, transform))
	{
		m_pVehicle->GetEntity()->SetWorldTM(Matrix34(transform));
	}
}

void CVehicleMovementMPVTOL::ReceivedServerPathingData( const SVTOLPathPosParams& data )
{
	CryAutoCriticalSection lk(m_lock);

	//CryLog( "[VTOL] CLUPDATE: path[%d] loc[%f]", data.pathId, data.location );

	float netError = 0.f;
	if( m_pathing.pathingData.pathId==data.pathId && m_pathing.pCachedPathPtr )
	{
		netError = m_pathing.pCachedPathPtr->GetDistance( m_pathing.pathingData.location, data.location, m_pathing.shouldLoop );
		if(fabs_tpl(netError) > g_pGameCVars->v_MPVTOLNetworkSnapThreshold)
		{
			SnapToPathLoc(data);
			netError = 0.0f;
		}
	}
	// Let it out of a wait, if the server VTOL is moving on (avoids a snap)
	if(netError>(g_pGameCVars->v_MPVTOLNetworkSnapThreshold*0.4f) && m_pathing.waitTime)
	{
		m_pathing.waitTime = 0.f;
		m_pathing.targetSpeed = m_pathing.defaultSpeed;
	}
	m_pathing.networkError = netError;
}

void CVehicleMovementMPVTOL::UpdatePathSpeed( const float speed )
{
	CryAutoCriticalSection lk(m_lock);

	m_pathing.defaultSpeed = m_pathing.speed = speed;
}

bool CVehicleMovementMPVTOL::FillPathingParams( struct SPathFollowingAttachToPathParameters& params )
{
	if(!m_pathing.pCachedPathPtr)
		return false;

	CryAutoCriticalSection lk(m_lock);

	params.pathIndex = m_pathing.pathingData.pathId;
	params.nodeIndex = m_pathing.currentNode;
	params.interpNodeIndex = m_pathing.interpolatedNode;
	params.speed = m_pathing.speed;
	params.defaultSpeed = m_pathing.defaultSpeed;
	params.shouldLoop = m_pathing.shouldLoop;
	params.waitTime = m_pathing.waitTime;
	return true;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleMovementMPVTOL::SPathing::Reset()
{
	pathingData.location = 0.f;
	pathingData.pathId = 0;
	pCachedPathPtr = NULL;
	currentNode = -1;
	interpolatedNode = -1;
	shouldLoop = false;
	speed = 0.f;
	targetSpeed = 0.f;
	accel = 0.f;
	defaultSpeed = 0.f;
	waitTime = 0.f;
	networkError = 0.f;
	pathComplete = 0;
}

void CVehicleMovementMPVTOL::SPathing::SetPathInfo( const SPathFollowingAttachToPathParameters& params, const CWaypointPath* pPath )
{
	// Set the new path info.
	pCachedPathPtr = pPath;
	pathingData.pathId = params.pathIndex;
	const float fNodeIndex = (float)params.nodeIndex;	// Not using CIntToFloat as will load it directly from memory.
	pathingData.location = max(fNodeIndex, 0.f);
	interpolatedNode = currentNode = max(0,params.nodeIndex);
	shouldLoop = params.shouldLoop;
	targetSpeed = speed = params.speed;
	defaultSpeed = params.defaultSpeed;
	waitTime = params.waitTime;
	networkError = 0.f;
	pathComplete = 0;
}

// Suppress passedByValue for tiny objects like SVTOLPathPosParams
// cppcheck-suppress passedByValue
bool CVehicleMovementMPVTOL::SPathing::SnapTo( const SVTOLPathPosParams data, QuatT& rLocation )
{
	if(pCachedPathPtr && pathingData.pathId==data.pathId)
	{
		CWaypointPath::TNodeId node = 0;
		pCachedPathPtr->GetPathLoc(data.location, rLocation, node);

		pathingData.location = data.location;
		currentNode = node;
		interpolatedNode = node;
		waitTime = 0.f;
		networkError = 0.f;
		pathComplete = 0;
		return true;
	}
	return false;
}

void CVehicleMovementMPVTOL::SPathing::UpdatePathFollowing( const EntityId vtolId, const float dt, CVehicleMovementMPVTOL& rMovement, const SVehiclePhysicsStatus& physStatus, const Vec3& posNoise )
{
	//CryWatch("VTOL: id[%d] path[%p] loc[%f] error[%f] wait[%f]", vtolId, pCachedPathPtr, pathingData.location, networkError, waitTime );

	if(!pCachedPathPtr)
		return;

	networkError *= max(0.f, 1.f-(dt*2.0f));
	float networkAdj = networkError;

	if(waitTime > 0.f)
	{
		waitTime -= dt;
		if(waitTime < 0.f)
		{
			waitTime = 0.f;
			targetSpeed = defaultSpeed;
		}
		else
		{
			networkAdj = 0.f;
			targetSpeed = 0.0f;
		}
	}

	CWaypointPath::TNodeId currentInerpolatedNode = interpolatedNode;
	Vec3 targetPos;
	if(pCachedPathPtr->GetPosAfterDistance(currentNode, physStatus.centerOfMass, g_pGameCVars->g_VTOLPathingLookAheadDistance, targetPos, interpolatedNode, currentNode, pathingData.location, shouldLoop))
	{
		QuatT location;
		CWaypointPath::TNodeId locationNode = -1;
		pCachedPathPtr->GetPathLoc(pathingData.location, location, locationNode);
		const Vec3 forward(targetPos-location.t);
		const Vec3 side(forward.y, -forward.x, 0.f);
		Vec3 noise(side.GetNormalizedSafe(physStatus.q.GetColumn0())*posNoise.x);
		noise.z = posNoise.z;

		targetPos += noise;

		if(waitTime<=0.f && currentInerpolatedNode!=interpolatedNode)
		{
			float fValueOut = 0.f;
			CWaypointPath::E_NodeDataType dataTypeOut;
			if(pCachedPathPtr->HasDataAtNode(interpolatedNode, dataTypeOut, fValueOut))
			{
				if(dataTypeOut == CWaypointPath::ENDT_Speed)
				{
					targetSpeed = fValueOut;
				}
				else if(dataTypeOut == CWaypointPath::ENDT_Wait && fValueOut > 0.f)
				{
					targetSpeed = speed = 0.f;
					waitTime = fValueOut;
				}
			}
			else
			{
				targetSpeed = defaultSpeed;
			}
		}

		SmoothCD(speed, accel, dt, targetSpeed, 0.2f);
		const float finalSpeed = max(0.f, speed + clamp_tpl( networkAdj, -speed, g_pGameCVars->v_MPVTOLNetworkCatchupSpeedLimit ));
		const float scalar = clamp_tpl( (speed>0.1f) ? finalSpeed/speed : 1.f, 1.f, 3.f );

		CMovementRequest movementRequest;
		movementRequest.SetDesiredSpeed(finalSpeed);
		movementRequest.SetMoveTarget(targetPos);	
		movementRequest.SetBodyTarget(targetPos);
		movementRequest.SetLookTarget(targetPos);

		rMovement.RequestMovement(movementRequest);
		rMovement.SetYawResponseScalar(scalar);
	}
	else
	{
		pathComplete |= 0x1;
	}
}
