// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements wheel based vehicle movement

-------------------------------------------------------------------------
History:
- Created by stan fichele

*************************************************************************/
#ifndef __VEHICLEMOVEMENTARCADEWHEELED_H__
#define __VEHICLEMOVEMENTARCADEWHEELED_H__

#include <SharedParams/ISharedParams.h>

#include "Network/NetActionSync.h"
#include "IVehicleSystem.h"
#include "VehicleMovementBase.h"
#include "Vehicle/VehicleUtils.h"
#include "VehicleSystem/VehicleNoiseGenerator.h"

// There is no need to net serialise

class CVehicleMovementArcadeWheeled;
class CNetworkMovementArcadeWheeled
{
public:
	CNetworkMovementArcadeWheeled();
	CNetworkMovementArcadeWheeled(CVehicleMovementArcadeWheeled *pMovement);

	typedef CVehicleMovementArcadeWheeled * UpdateObjectSink;

	bool operator == (const CNetworkMovementArcadeWheeled &rhs)
	{
		//return (abs(m_steer-rhs.m_steer)<0.001f) && (abs(m_pedal-rhs.m_pedal)<0.001f) && (m_brake==rhs.m_brake);
		return false;
	};

	bool operator != (const CNetworkMovementArcadeWheeled &rhs)
	{
		return !this->operator==(rhs);
	};

	void UpdateObject( CVehicleMovementArcadeWheeled *pMovement );
	void Serialize(TSerialize ser, EEntityAspects aspects);

	static const NetworkAspectType CONTROLLED_ASPECT = eEA_GameClientN;

private:
	float m_steer;
	float m_pedal;
	bool m_brake;
	bool m_boost;
};

/*
	Wheel
*/
struct SVehicleWheel
{
	inline SVehicleWheel()
	{
		offset           = Vec3(0.0f, 0.0f, 0.0f);
		frictionDir[0]   = Vec3(0.0f, 0.0f, 0.0f);
		frictionDir[1]   = Vec3(0.0f, 0.0f, 0.0f);
		worldOffset      = Vec3(0.0f, 0.0f, 0.0f);
		contactNormal    = Vec3(0.0f, 0.0f, 0.0f);
		wheelPart        = NULL;
		radius           = 0.0f;
		mass             = 0.0f;
		invMass          = 0.0f;
		inertia          = 0.0f;
		invInertia       = 0.0f;
		bottomOffset     = 0.0f;
		contact          = 0.0f;
		lastW            = 0.0f;
		w                = 0.0f;
		slipSpeed        = 0.0f;
		slipSpeedLateral = 0.f;
		suspLen          = 0.0f;
		compression      = 0.0f;
		waterLevel       = WATER_LEVEL_UNKNOWN;
		axleIndex        = 0;
		bCanLock         = 0;
		locked           = 0;
		chassisK0        = 0.f;
		chassisK1        = 0.f;
		invWheelK        = 0.f;
		chassisW0.zero();
		chassisW1.zero();
	}

	Vec3 offset;
	Vec3 frictionDir[2];			// [0] == inline friction direction, [1] == lateral friction direction
	Vec3 worldOffset;
	Vec3 contactNormal;

	// Solver constants
	// Responses: An impluse of 1.0 produces this velocity response, i.e. velocityChange = k * impulse
	float chassisK0;  // velocityChange = k0 * frictionDir[0] * impulseScale
	float chassisK1;  // velocityChange = k1 * frictionDir[1] * impulseScale
	Vec3 chassisW0;   // angVelocityChange = W0 * impulseScale,   if an impulse of frictionDir[0]*impulseScale is applied at the chassis
	Vec3 chassisW1;   // angVelocityChange = W1 * impulseScale,   if an impulse of frictionDir[1]*impulseScale is applied at the chassis
	float invWheelK;  // (inv) response at the contact point of the wheel with ground

	IVehiclePart* wheelPart;

	float radius;
	float mass;
	float invMass;
	float inertia;
	float invInertia;
	float bottomOffset;				// offset + (zAxis * bottomOffset) == bottom point of wheel
	float contact;
	float lastW;
	float w;
	float slipSpeed;
	float slipSpeedLateral;
	float suspLen;
	float compression;
	float waterLevel;

	int16 axleIndex;						// 0 == back wheel, 1 == front wheel
	int8 bCanLock;							// 0 or 1

	int8 locked;							// Handbraking
};


/*
	Chassis
*/
struct SVehicleChassis
{
	float radius;
	float mass;
	float inertia;

	float invMass;
	float invInertia;

	Vec3 vel;
	Vec3 angVel;

	int8 contactIdx0, contactIdx1, contactIdx2, contactIdx3;		// These are used to calculate the base contact normal, and are indexes to the wheels that used
};

/*
	Gears
*/
struct SVehicleGears
{
	enum { kMaxGears = 10 };

	enum { kReverse = 0, kNeutral, kFirst };

	float  averageWheelRadius;
	int8   curGear;
	int8   lastGear;
	int8   doGearChange;
	int8   changedUp;
	float  curRpm;            // Current rpm, normalised within 0-1
	float  targetRpm;         // Target RPM to lerp torwards
	float  timer;             // Timer for applying min gear changing times
	float  modulation;        // ramps from 0-1 on gear changes (a fake alternative for the clutch)
	float  rpmLoad;           // 0-1 time-filtered value representing the strain/load on the engine (given to FMOD)
};

struct SSharedVehicleGears
{
	float  ratios[SVehicleGears::kMaxGears];
	float  invRatios[SVehicleGears::kMaxGears];
	float  minChangeUpTime[SVehicleGears::kMaxGears];
	float  minChangeDownTime[SVehicleGears::kMaxGears];
	int    numGears;
	float  rpmRelaxSpeed;
	float  rpmInterpSpeed;
	float  gearChangeSpeed;                  // On a gear change the rpm is decreased to zero, and increased exponentially by 1-exp(-time*gearChangeSpeed),
	                                         //	1.0/gearChangeSpeed is exponential time constant of the increase
	float  gearChangeSpeed2;                 // The rpm change has a sin wave added to it, which is decayed by this exponential gaussian, exp(-sqr(time*gearChangeSpeed2))
	float  gearOscillationFrequency;         // This sets the frequency of the sin wave, should be kept low. (No larger than around 6-7 hz)
	float  gearOscillationAmp;               // This sets the amplitude of the sinwave added to the rpm_scale
	float  gearOscillationAmp2;              // This sets the amplitude of the sinwave for fmod 'clutch'

	// RPM loading
	float  rpmLoadFactor;
	float  rpmLoadChangeSpeedUp;
	float  rpmLoadChangeSpeedDown;
	
	float  rpmLoadDefaultValue;
	float  rpmLoadFromBraking;
	float  rpmLoadFromThrottle;
};


#if ENABLE_VEHICLE_DEBUG

/*
 * Purely for debug drawing
*/

struct IDebugHistoryManager;
struct IDebugHistory;

struct SVehicleDebugInfo
{
	CTimeSmoothedValue debugGearChange;

	// Debug graph 
	enum {
		k_graphNone=0,
		k_graphSlipSpeed,
		k_graphSlipSpeedLateral,
		k_graphCentrifugal,
		k_graphEffCentrifugal,
	};
	IDebugHistoryManager*  pDebugHistoryManager;
	IDebugHistory*         pDebugHistory;
	int                    graphMode;
};

#endif // ENABLE_VEHICLE_DEBUG


class CVehicleMovementArcadeWheeled
	: public CVehicleMovementBase
{
	friend class CNetworkMovementArcadeWheeled;
public:
	enum {maxWheels=18};
	enum {k_frictionNotSet=0, k_frictionUseLowLevel, k_frictionUseHiLevel}; // Friction State

public:
	CVehicleMovementArcadeWheeled();
	~CVehicleMovementArcadeWheeled();

	// IVehicleMovement
	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table);
	virtual void PostInit();
	virtual void Reset();
	virtual void Release();
	virtual void Physicalize();
	virtual void PostPhysicalize();

	virtual EVehicleMovementType GetMovementType() { return eVMT_Land; } 

	virtual bool StartDriving(EntityId driverId);  
	virtual void StopDriving();
	virtual void OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params);
	virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value);

	virtual void ProcessAI(const float deltaTime);
	virtual void ProcessMovement(const float deltaTime);
	virtual void Update(const float deltaTime);  
	virtual void UpdateSounds(const float deltaTime);
	virtual void PostUpdateView(SViewParams &viewParams, EntityId playerId);
	virtual int GetStatus(SVehicleMovementStatus* status);

	virtual bool RequestMovement(CMovementRequest& movementRequest);
	virtual void GetMovementState(SMovementState& movementState);
	
	virtual void EnableMovementProcessing(bool enable);

	virtual pe_type GetPhysicalizationType() const { return PE_WHEELEDVEHICLE; };
	virtual bool UseDrivingProxy() const { return true; };
	virtual int GetWheelContacts() const { return m_wheelContacts; }
	virtual int GetBlownTires() const { return m_blownTires; }

	virtual void GetMemoryUsage(ICrySizer * pSizer) const;

	virtual void Serialize(TSerialize ser, EEntityAspects aspects);
	virtual void SetAuthority( bool auth )
	{
		m_netActionSync.CancelReceived();
	};

	virtual SVehicleNetState GetVehicleNetState();
	virtual void SetVehicleNetState(const SVehicleNetState& state);
	// ~IVehicleMovement

	// IVehicleObject
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params);
	
	const SVehicleMovementLargeObjectInfo* GetLargeObjectInfo() { return &m_largeObjectInfo; }

protected:

	virtual bool InitPhysics(const CVehicleParams& table);
	virtual void InitSurfaceEffects();  

	virtual void UpdateSuspension(const float deltaTime);
	void UpdateSuspensionSound(const float deltaTime);
	void UpdateBrakes(const float deltaTime);

	virtual void UpdateSurfaceEffects(const float deltaTime);

	virtual void Boost(bool enable);
	virtual void ApplyBoost(float speed, float maxSpeed, float strength, float deltaTime);

	virtual bool DoGearSound();
	virtual float GetMinRPMSoundRatio() { return 0.f; }

#if ENABLE_VEHICLE_DEBUG
	virtual void DebugDrawMovement(const float deltaTime);
	void DebugDrawGraphs();
	void DebugCheat(float dt);
	void ReleaseDebugInfo();
	void RemoveDebugGraphs();
#endif

	float GetMaxSteer(float speedRel);
	float GetSteerSpeed(float speedRel);

	virtual float GetWheelCondition() const;
	void SetEngineRPMMult(float mult, int threadSafe=0);

	float CalcSteering(float steer, float speedRel, float rotateYaw, float dt);
	void TickGears(float dt, float averageWheelSpeed, float throttle, float forwardSpeed, float contact);
	void EnableLowLevelPhysics(int state, int bThreadSafe);
	void InternalPhysicsTick(float dt);
	void GetCurrentWheelStatus(IPhysicalEntity* pPhysics);

	void UpdateWaterLevels();
	void ResetWaterLevels();

protected:

	pe_params_car m_carParams;
	pe_status_vehicle m_vehicleStatus;
	pe_action_drive m_action;

	float m_steering;
	float m_steerMax;	// max steering angle in deg
	float m_lastBump, m_compressionMax;	
	float m_brakeTimer;
	float m_reverseTimer;
	bool  m_initialHandbreak;

	bool  m_stationaryHandbrake;
	float m_stationaryHandbrakeResetTimer;

	float m_suspDamping;
	float m_stabi;
	float m_speedSuspUpdated;    
	
	float m_topSpeedFactor; // factor applied to the defined topSpeed
	float m_accelFactor;

	// Network related
	CNetActionSync<CNetworkMovementArcadeWheeled> m_netActionSync;

	float m_lostContactTimer;
	float m_tireBlownTimer;
	float m_forceSleepTimer;
	bool  m_bForceSleep;

	SVehicleGears m_gears;

	int8 m_wheelContacts;
	int8 m_passengerCount;
	int8 m_blownTires;
	int8 m_frictionState;

	struct Handling
	{
		float  compressionScale;
		Vec3   contactNormal;
		float  lostContactOneSideTimer;
		float  viewRotate;
		Vec3   viewOffset;
		CVehicleNoiseValue fpViewVibration;
	};

	struct SSharedHandling
	{
		float  acceleration, decceleration, topSpeed, reverseSpeed, boostTopSpeed, boostAcceleration;
		float  reductionAmount, reductionRate;
		float  compressionBoost, compressionBoostHandBrake;
		float  backFriction, frontFriction, frictionOffset;
		float  grip1, grip2;       // Grip fraction at zero slip speed and grip fraction at high slip speed (usually 1.0f).
		float  gripK;              // 1.0f / slipSpeed.
		float  accelMultiplier1, accelMultiplier2;
		float  handBrakeDecceleration, handBrakeDeccelerationPowerLock;
		bool   handBrakeLockFront, handBrakeLockBack;
		float  handBrakeFrontFrictionScale, handBrakeBackFrictionScale;
		float  handBrakeAngCorrectionScale, handBrakeLateralCorrectionScale;
	};

	struct SSharedTankHandling
	{
		float additionalSteeringStationary;
		float additionalSteeringAtMaxSpeed;
		float additionalTilt;
	};

	struct SPowerSlide
	{
		float lateralSpeedFraction[2];   // Percentage of forward speed to transfer as side speed, [0] normal driving, [1] powerlock/handbrake turning
		float spring;                    // How quickly to apply that lerping
	};

	struct SStabilisation
	{
		float angDamping;          // Angular damping. Only affects tilt+roll but leaves the yaw speed unaffected 
		float rollDamping;         // Roll damping. Only added when contact on one side is lost - reduces the chance of the vehicle rolling on its side
		float upDamping;           // This damps the *upwards* velocity of vehicle when ground contact is lost
		float rollfixAir;
		float sinMaxTiltAngleAir;  // Max forward tilt angle before rollfixAir is enforced
		float cosMaxTiltAngleAir;  // Max forward tilt angle before rollfixAir is enforced
	};
	
	// First person view adjustments to add vehicle forces
	struct SViewAdjustment
	{
		float lateralDisp;	  // At top speed, and full steering
		float lateralCent;    // Due to centrifugal force
		float rotate;         // How much to rotate by due to steering+speed
		float rotateCent;     // How much to rotate by due to centrifugal force
		float maxLateralDisp; // Clamp the lateral disp to this
		float maxRotate;      // Clamp the rotation to this
		float rotateSpeed;
		float dispSpeed;
		float maxForwardDisp;
		float forwardResponse;
		float vibrationAmpSpeed;            // Perlin noise as a function of speed
		float vibrationSpeedResponse;
		float vibrationAmpAccel;            // Amplitude as a function of up/down vehicle acceleration
		float vibrationAccelResponse;
		float vibrationFrequency;
		float suspensionAmp;                // Amplitude as a response to suspension compression
		float suspensionResponse;           // Amplitude as a response to suspension compression
		float suspensionSharpness;          // Filter the vibration offset
	};

	struct Correction
	{
		float lateralSpring;
		float angSpring;
	};
	
	struct SSoundParams
	{
		float      airbrakeTime, bumpMinSusp, bumpMinSpeed, bumpIntensityMult;

		// Skidding/Slipping control
		float      skidLerpSpeed;              // Controls the filtering/smoothing speed of the skid factor
		float      skidCentrifugalFactor;      // How much the centrifugal force affects the slip parameter
		float      skidBrakeFactor;            // How much skidding from just appyling the brake action (alone)
		float      skidPowerLockFactor;        // How much skidding when hand-brake turning (i.e. pressing brake + accelerator at same time)
		float      skidLateralFactor;          // Multiple the lateral slip speed by this
		float      skidForwardFactor;          // Multiple the forward slip speed by this
	};

	BEGIN_SHARED_PARAMS(SSharedParams)

		bool                 isBreakingOnIdle;
		bool                 surfaceFXInFpView;
		float								 steerSpeed, steerSpeedMin;     // Steer speed at vMaxSteerMax and steer speed at v = 0.
		float                kvSteerMax;                    // Reduce steer max at vMaxSteerMax.
		float                v0SteerMax;                    // Max steering angle in deg at v = 0.
		float                steerSpeedScaleMin;            // Scale for sens at zero vel.
		float                steerSpeedScale;               // Scale for sens at vMaxSteerMax.
		float                steerRelaxation;               // Relaxation speed to center in degrees.
		float                vMaxSteerMax;                  // Speed at which entire kvSteerMax is subtracted from v0SteerMax.
		float                pedalLimitMax;                 // At vMaxSteerMax pedal is clamped to 1 - pedalLimitMax.
		float                suspDampingMin, suspDampingMax, suspDampingMaxSpeed;
		float                stabiMin, stabiMax;
		float                damagedWheelSpeedInfluenceFactor;
		float                ackermanOffset;
		float                gravity;
		SSharedVehicleGears  gears;
		SSharedHandling      handling;
		SSharedTankHandling  tankHandling;
		Correction           correction;
		SSoundParams         soundParams;
		SPowerSlide          powerSlide;
		SViewAdjustment      viewAdjustment[2];
		SStabilisation       stabilisation;

	END_SHARED_PARAMS

	Handling m_handling;

	Vec3 m_collisionNorm;
	float m_damageRPMScale;

	SVehicleChassis m_chassis;

	typedef std::vector<SVehicleWheel> TWheelArray;
	typedef std::vector<pe_status_wheel> TPEStatusWheelArray;

	TWheelArray          m_wheels;
	TPEStatusWheelArray  m_wheelStatus;
	size_t               m_iWaterLevelUpdate;
	
	volatile int    m_wheelStatusLock;
	volatile int    m_frictionStateLock;
	int             m_numWheels;
	float           m_invNumWheels;

	float m_invTurningRadius;
	struct SMovementInfo
	{
		float centrifugalAccel;            // Measured each frame by the internal physics tick
		float effectiveCentrifugalAccel;   // Measured each frame by the internal physics tick
		float averageSlipSpeedForward;
		float averageSlipSpeedLateral;
		float skidValue;                   // 0-1 value given to FMOD to represent the ccombined skid amount
	};

	SMovementInfo m_movementInfo;

	//------------------------------------------------------------------------------
	// AI related
	// PID controller for speed control.	

	//float	m_steering;
	float  m_prevAngle;

	CMovementRequest m_aiRequest;

	float m_submergedRatioMax;	// to avoid calling vehicle functions in ProcessMovement()

	bool m_netLerp;				// This is true when the vehicle is not locally controlled by the player

	SSharedParamsConstPtr	m_pSharedParams;

	// Kickable Car State
	SVehicleMovementLargeObjectInfo m_largeObjectInfo;
	int16 m_largeObjectInfoWheelIdx;
	int16 m_largeObjectFrictionType;
	
#if ENABLE_VEHICLE_DEBUG
	SVehicleDebugInfo* m_debugInfo;
	int  m_lastDebugFrame;
#endif
};

#endif
