// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a standard wheel based vehicle movements

-------------------------------------------------------------------------
History:
- 04:04:2005: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEMOVEMENTSTDWHEELED_H__
#define __VEHICLEMOVEMENTSTDWHEELED_H__


#include "Network/NetActionSync.h"
#include "IVehicleSystem.h"
#include "VehicleMovementBase.h"



class CVehicleMovementStdWheeled;
class CNetworkMovementStdWheeled
{
public:
	CNetworkMovementStdWheeled();
	CNetworkMovementStdWheeled(CVehicleMovementStdWheeled *pMovement);

	typedef CVehicleMovementStdWheeled * UpdateObjectSink;

	bool operator == (const CNetworkMovementStdWheeled &rhs)
	{
		//return (abs(m_steer-rhs.m_steer)<0.001f) && (abs(m_pedal-rhs.m_pedal)<0.001f) && (m_brake==rhs.m_brake);
		return false;
	};

	bool operator != (const CNetworkMovementStdWheeled &rhs)
	{
		return !this->operator==(rhs);
	};

	void UpdateObject( CVehicleMovementStdWheeled *pMovement );
	void Serialize(TSerialize ser, EEntityAspects aspects);

	static const NetworkAspectType CONTROLLED_ASPECT = eEA_GameClientDynamic;

private:
	float m_steer;
	float m_pedal;
	bool	m_brake;	
  bool  m_boost;
};

struct SWheelStats
{
  float pullMod;
  float lateralSlip;
  float suspLen;
  float compression;
  float torqueScale;
  bool handBraking;
  bool bContact;
  
  // for debugging
  float friction;

  SWheelStats()
  {
    pullMod = 0.f;
    lateralSlip = 0.f;
    friction = 0.f;
    suspLen = 0.f;
    compression = 0.f;
    torqueScale = 0.f;
    handBraking = false;
    bContact = false;
  }

	void GetMemoryUsage( ICrySizer *pSizer ) const{}

};


class CVehicleMovementStdWheeled
	: public CVehicleMovementBase
{
	friend class CNetworkMovementStdWheeled;
public:
	CVehicleMovementStdWheeled();
	~CVehicleMovementStdWheeled();

	// IVehicleMovement
	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table);
  virtual void PostInit();
	virtual void Reset();
	virtual void Release();
	virtual void Physicalize();
  virtual void PostPhysicalize();

  virtual EVehicleMovementType GetMovementType() { return eVMT_Land; } 
    
  virtual bool StartEngine();  
	virtual void StopEngine();
	virtual void OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params);
	virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value);

	virtual void ProcessAI(const float deltaTime);
	virtual void ProcessMovement(const float deltaTime);
	virtual void Update(const float deltaTime);  
  virtual void UpdateSounds(const float deltaTime);

	virtual bool RequestMovement(CMovementRequest& movementRequest);
	virtual void GetMovementState(SMovementState& movementState);

	virtual pe_type GetPhysicalizationType() const { return PE_WHEELEDVEHICLE; };
  virtual bool UseDrivingProxy() const { return true; };
  virtual int GetWheelContacts() const { return m_wheelContacts; }
	virtual int GetBlownTires() const { return m_blownTires; }
  
	virtual void GetMemoryUsage(ICrySizer * pSizer) const;
	void GetMemoryUsageInternal(ICrySizer * pSizer) const;
	virtual void Serialize(TSerialize ser, EEntityAspects aspects);
	virtual void SetAuthority( bool auth )
	{
		m_netActionSync.CancelReceived();
	};
	// ~IVehicleMovement

  // IVehicleObject
  virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params);

protected:

	virtual bool InitPhysics(const CVehicleParams& table);
  virtual void InitSurfaceEffects();  
  
  virtual void UpdateSuspension(const float deltaTime);
  void UpdateSuspensionSound(const float deltaTime);
  virtual void UpdateAxleFriction(float pedal, bool backwardMax, const float deltaTime);
  void UpdateBrakes(const float deltaTime);
  
  virtual void UpdateSurfaceEffects(const float deltaTime);
  
  virtual void Boost(bool enable);
  virtual void ApplyBoost(float speed, float maxSpeed, float strength, float deltaTime);
  
  virtual bool DoGearSound();
  virtual float GetMinRPMSoundRatio() { return 0.f; }
    
#if ENABLE_VEHICLE_DEBUG
  virtual void DebugDrawMovement(const float deltaTime);
#endif

  float GetMaxSteer(float speedRel);
  float GetSteerSpeed(float speedRel);
  
  virtual float GetWheelCondition() const;
  void SetEngineRPMMult(float mult, int threadSafe=0);

protected:

  pe_params_car m_carParams;
  pe_status_vehicle m_vehicleStatus;	
	pe_action_drive m_action;

	float m_engineMaxRPM, m_engineIdleRPM, m_engineShiftDownRPM;
  		 
	float m_steerMax;             // max steering angle in deg
	float m_steerSpeedMin;        // steer speed at v=0
  float m_steerSpeed;           // steer speed at vMaxSteerMax	
	float m_v0SteerMax;           // max steering angle in deg at v=0
  float m_kvSteerMax;           // reduces steer max at vMaxSteerMax
	float m_steerSpeedScaleMin;   // scale for sens at zero vel
  float m_steerSpeedScale;      // scale for sens at vMaxSteerMax
	
	float m_steerRelaxation;      // relaxation speed to center in degrees
  float m_vMaxSteerMax;         // speed at which entire kvSteerMax is substracted from v0SteerMax		
  float m_pedalLimitMax;        // at vMaxSteerMax pedal is clamped to 1-pedalLimitMax
		
  float m_rpmTarget;
  float m_rpmRelaxSpeed, m_rpmInterpSpeed, m_rpmGearShiftSpeed;	
  float m_lastBump, m_compressionMax;
  float m_bumpMinSusp, m_bumpMinSpeed, m_bumpIntensityMult;
  float m_load, m_rpmScalePrev;
  float m_airbrakeTime;
  float m_brakeTimer;
	float m_reverseTimer;
  float m_initialHandbreak;

  int m_currentGear;
  bool m_gearSoundPending;
	bool m_isBreakingOnIdle;  
  float m_maxBrakingFriction;
  float m_brakeImpulse;
  float m_axleFriction, m_axleFrictionMin, m_axleFrictionMax;
  float m_suspDampingMin, m_suspDampingMax, m_suspDamping, m_suspDampingMaxSpeed;  
  float m_stabiMin, m_stabiMax, m_stabi;
  float m_speedSuspUpdated;    
	    
	// Network related
	CNetActionSync<CNetworkMovementStdWheeled> m_netActionSync;
  
  typedef std::vector<IVehiclePart*> TWheelParts;
  TWheelParts m_wheelParts;
  
  typedef std::vector<SWheelStats> TWheelStats;
  TWheelStats m_wheelStats;
  
  float m_lostContactTimer;
  float m_tireBlownTimer;
  int   m_blownTires;
  float m_forceSleepTimer;
  bool  m_bForceSleep;

	float m_gearRatios[16];
  float m_latFriction;
  float m_avgLateralSlip;  
  float m_avgWheelRot;
  int   m_wheelContacts;
  int   m_wheelContactsLeft;
  int   m_wheelContactsRight;
  int	m_passengerCount;
  int   m_lastDebugFrame;

	float m_massCW;	// CW = counter-weight
	int m_partidCW;
	Vec3 m_pos0CW;
	float m_kzCW;
	float m_maxxCW;
	float m_posxCW, m_poszCW, m_velxCW;
	float m_maxxCWcam;
	Vec3 m_offsCWcam;
	float m_posxCWhist[8],m_dtHist;
	float m_ktiltCW[3], m_stiltCW;
	float m_minVelSteerLean;

	float m_dampingy;
	float m_velWheelDisable;
	int m_maskWheelDisable;
	int m_uprightOnMount;
	Quat m_qPrev;

	//------------------------------------------------------------------------------
	// AI related
	// PID controller for speed control.	

	float	m_steering;
	float	m_prevAngle;

	CMovementRequest m_aiRequest;

	float m_submergedRatioMax;	// to avoid calling vehicle functions in ProcessMovement()
	pe_status_dynamics m_statusDyn; // gets updated once per update
	pe_status_pos m_statusPos; // gets updated once per update
	pe_status_dynamics m_PhysDyn; // gets updated once per phys update
	pe_status_pos	m_PhysPos; // gets updated once per phys update
	float m_maxSoundSlipSpeed;  
};


#endif
