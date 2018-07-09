// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a standard boat vehicle movement

-------------------------------------------------------------------------
History:
- 30:05:2005: Created by Michael Rauh

*************************************************************************/
#ifndef __VEHICLEMOVEMENTSTDBOAT_H__
#define __VEHICLEMOVEMENTSTDBOAT_H__


#include "VehicleMovementBase.h"
#include "Network/NetActionSync.h"

class CVehicleMovementStdWheeled;
class CVehicleMovementArcadeWheeled;


class CVehicleMovementStdBoat;
class CNetworkMovementStdBoat
{
public:
  CNetworkMovementStdBoat();
  CNetworkMovementStdBoat(CVehicleMovementStdBoat *pMovement);

  typedef CVehicleMovementStdBoat * UpdateObjectSink;

  bool operator == (const CNetworkMovementStdBoat &rhs)
  { 
    return false;
  };

  bool operator != (const CNetworkMovementStdBoat &rhs)
  {
    return !this->operator==(rhs);
  };

  void UpdateObject( CVehicleMovementStdBoat *pMovement );
  void Serialize(TSerialize ser, EEntityAspects aspects);
  
  static const NetworkAspectType CONTROLLED_ASPECT = eEA_GameClientN;

private:
  float m_steer;
  float m_pedal;  
  bool  m_boost;      
};


class CVehicleMovementStdBoat
  : public CVehicleMovementBase
{
public:

  CVehicleMovementStdBoat();
  virtual ~CVehicleMovementStdBoat();

  // IVehicleMovement
  virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table);
  virtual void Reset();
  virtual void Release();
  virtual void PostPhysicalize();

  virtual EVehicleMovementType GetMovementType() { return eVMT_Sea; }

  virtual void Update(const float deltaTime);

	virtual void ProcessAI(const float deltaTime);
	virtual void ProcessMovement(const float deltaTime);
	virtual bool RequestMovement(CMovementRequest& movementRequest);

  virtual void OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params);
	virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value);

	virtual void Serialize(TSerialize ser, EEntityAspects aspects);
  virtual void SetAuthority( bool auth ) { m_netActionSync.CancelReceived(); }

	virtual void GetMemoryUsage(ICrySizer * pSizer) const;
  // ~IVehicleMovement

  friend class CNetworkMovementStdBoat;

protected:

  virtual bool GenerateWind() { return false; }
  virtual void UpdateRunSound(const float deltaTime);
  virtual void UpdateSurfaceEffects(const float deltaTime);  
  void Lift(bool lift);
  bool IsLifted();

#if ENABLE_VEHICLE_DEBUG
  void DrawImpulse(const pe_action_impulse& action, const Vec3& offset=Vec3(ZERO), float scale=1, const ColorB& col=ColorB(255,0,0,255));
#endif
  
  // driving
  float m_velMax;
  float m_velMaxReverse;
  float m_accel;
  float m_accelVelMax;
  float m_accelCoeff;
  Vec3  m_pushOffset;
  float m_pushTilt;
  float m_pedalLimitReverse;  
  float m_velLift;   
  float m_waterDensity;
  float m_factorMaxSpeed;
  float m_factorAccel;
  bool  m_lifted;

  // steering
  float m_turnRateMax;
  float m_turnAccel;
  float m_turnVelocityMult;
  float m_turnAccelCoeff;  
  float m_cornerForceCoeff;
  Vec3  m_cornerOffset;
  float m_cornerTilt;
  float m_turnDamping;
  float m_lateralDamping;
	float m_rollAccel;

  Vec3 m_Inertia;
  float m_gravity;
  Vec3 m_massOffset;

  // waves
  float m_waveTimer;  
  Vec3 m_lastWakePos;
  bool m_diving;
  int m_wakeSlot;
  Vec3 m_waveIdleStrength;
  float m_waveSpeedMult;
  float m_waveRandomMult;
  bool m_inWater;
  IVehicleHelper* m_pSplashPos;
  IParticleEffect* m_pWaveEffect;

  float m_waveSoundPitch;
  float m_waveSoundAmount;  
  int m_rpmPitchDir;

  CNetActionSync<CNetworkMovementStdBoat> m_netActionSync;
  bool m_bNetSync; 

	//------------------------------------------------------------------------------
	// AI related
	// PID controller for speed control.	

	float	m_prevAngle;

	CMovementRequest m_aiRequest;

};

#endif
