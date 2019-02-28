// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryParticleSystem/ParticleParams.h>
#include <IVehicleSystem.h>
#include <IForceFeedbackSystem.h>
#include "Actor.h"
#include <CryAction/IMaterialEffects.h>

#define ENGINESOUND_MAX_DIST 150.f


struct IGroundEffect;

/** exhaust status structure
*/
struct SExhaustStatus
{
	IVehicleHelper* pHelper;
	int startStopSlot;
	int runSlot;
	int boostSlot;
	int enabled;

	SExhaustStatus()
		: pHelper(nullptr)
		, startStopSlot(-1)
		, runSlot(-1)
		, boostSlot(-1)
		, enabled(1)
	{    
	}
};

struct TEnvEmitter
{
	TEnvEmitter()
	{ 
		layer = -1;      
		slot = -1;
		matId = -1;
		group = -1;
		bContact = false;
		pGroundEffect = 0;    
		active = true;
		quatT.SetIdentity();
	}

	QuatT quatT;  // local tm
	int layer; // layer idx the emitter belongs to    
	int slot;  // emitter slot
	int matId; // last surface idx    
	int group; // optional     
	IGroundEffect* pGroundEffect;
	bool bContact; // optional  
	bool active; 
};

struct SEnvParticleStatus
{
	bool initalized;

	typedef std::vector<TEnvEmitter> TEnvEmitters;
	TEnvEmitters emitters;  //maps emitter slots to user-defined index

	SEnvParticleStatus() : initalized(false)
	{}

	void Serialize(TSerialize ser, EEntityAspects aspects)
	{    
		ser.BeginGroup("EnvParticleStatus");   

		int readCount = emitters.size();
		ser.Value("NumEnvEmitters", readCount);
		const int count = min(readCount, static_cast<int>(emitters.size()));

		char name[16] = "slot_x";
		for (int i=0; i<count; ++i)
		{
			itoa(i, &name[5], 10);
			ser.ValueWithDefault(name, emitters[i].slot, -1);
		}

		ser.EndGroup();
	}
};


/** particle status structure
*/
struct SParticleStatus
{  
	std::vector<SExhaustStatus> exhaustStats; // can hold multiple exhausts
	SEnvParticleStatus envStats;

	SParticleStatus()
	{ 
	}

	void Serialize(TSerialize ser, EEntityAspects aspects)
	{
		ser.BeginGroup("ExhaustStatus");   

		uint32 count = exhaustStats.size();
		ser.Value("NumExhausts", count);

		if (ser.IsWriting())
		{
			for(std::vector<SExhaustStatus>::iterator it = exhaustStats.begin(); it != exhaustStats.end(); ++it)      
			{
				ser.BeginGroup("ExhaustRunSlot");
				ser.Value("slot", it->runSlot);
				ser.EndGroup();
			}
		}
		else if (ser.IsReading())
		{
			//assert(count == exhaustStats.size());
			for (size_t i=0; i<count&&i<exhaustStats.size(); ++i)
			{
				ser.BeginGroup("ExhaustRunSlot");
				ser.Value("slot", exhaustStats[i].runSlot); 
				ser.EndGroup();
			}
		}   
		ser.EndGroup();

		envStats.Serialize(ser, aspects);
	}
};

struct SSurfaceSoundStatus
{
	int matId; 
	float slipRatio;
	float slipTimer;
	int scratching;

	SSurfaceSoundStatus()
	{
		Reset();
	}

	void Reset()
	{
		matId = 0;
		slipRatio = 0.f;
		slipTimer = 0.f;
		scratching = 0;
	}

	void Serialize(TSerialize ser, EEntityAspects aspects)
	{
	}
};

// sounds
enum EVehicleMovementSound
{
	eSID_Start = 0,
	eSID_Run,
	eSID_Stop,
	eSID_Ambience,
	eSID_Bump,
	eSID_Splash,
	eSID_Gear,
	eSID_Slip,                  // Deprecated
	eSID_Acceleration,
	eSID_Boost,
	eSID_Damage,
	eSID_StopDamage,

	// EXTRA IDS
	eSID_VehicleRPM,
	eSID_VehicleSpeed,
	eSID_VehicleDamage,
	eSID_VehicleSlip,
	eSID_VehicleINOUT,
	eSID_VehicleSurface,
	eSID_VehicleStroke,
	eSID_TankTurnTurret,
	eSID_VehiclePrimaryWeapon,
	eSID_VehicleStopPrimaryWeapon,
	eSID_VehicleSecondaryWeapon,
	eSID_VehicleStopSecondaryWeapon,
	eSID_Max,
};

enum EVehicleMovementAnimation
{
	eVMA_Engine = 0,
	eVMA_Max,
};

struct SMovementSoundStatus
{
	SMovementSoundStatus(){ Reset(); }

	void Reset()
	{    
		for (int i=0; i<eSID_Max; ++i)    
		{      
			//sounds[i] = INVALID_SOUNDID;
			lastPlayed[i].SetValue(0);
		}

		inout = 1.f;
	}

	//tSoundID sounds[eSID_Max];  
	CTimeValue lastPlayed[eSID_Max];
	float inout;
};

struct SVehiclePhysicsStatus
{
	SVehiclePhysicsStatus()
	{
		pos = Vec3Constants<float>::fVec3_Zero;
		q.SetIdentity();
		v = Vec3Constants<float>::fVec3_Zero;
		w = Vec3Constants<float>::fVec3_Zero;
		centerOfMass = Vec3Constants<float>::fVec3_Zero;
		submergedFraction = 0;
		mass = 0;
	}
	Vec3 pos;                // position
	Quat q;                  // rot
	Vec3 v;                  // velocity
	Vec3 w;                  // ang velocity
	Vec3 centerOfMass;
	float submergedFraction;
	float mass;
};



#if ENABLE_VEHICLE_DEBUG
namespace 
{
	bool DebugParticles()
	{
		static ICVar* pVar = gEnv->pConsole->GetCVar("v_debugdraw");
		return pVar->GetIVal() == eVDB_Particles;
	}
}
#endif

// enum continuation of CryAction EVehicleMovementEvent for Game-Specific events
enum EVehicleMovementEventGame
{
	eVME_StartLargeObject = IVehicleMovement::eVME_Others,
};

struct SVehicleMovementEventLargeObject
{
	enum {k_KickFront, k_KickDiagonal, k_KickSide};

	Vec3 impulseDir;
	float speed;
	float rotationFactor;
	float swingRotationFactor;
	float boostFactor;
	Vec3 eyePos;
	Quat viewQuat;
	EntityId kicker;
	int kickType;
	float timer;
};

// Get information about any large object interaction
// that this vehicle is having
struct SVehicleMovementLargeObjectInfo
{
	Vec3 impulseDir;
	EntityId kicker;
	float countDownTimer;     // A timer counting down to zero. When zero is reached vehicle is no longer being 'interacted' with
	float startTime;         // The initial time to count down
	float invStartTime;       // Recip. of initial time

	void SetZero()
	{
		impulseDir.zero();
		kicker = 0;
		countDownTimer = 0;
		startTime = 0;
		invStartTime = 0;
	}
};


//=================================================
// Params for driving a simulation of the vehicle
// NB: this is optional for vehicles to support it
//=================================================
struct SVehicleMovementSimulate
{
	float dt;
	// These are updated at the simulaton progresses
	Vec3 entityPos;
	Quat entityRot;
	Vec3 vel;
	Vec3 angVel;
};



class CVehicleMovementBase : 
	public IVehicleMovement, 
	public IVehicleObject/*, 
	public ISoundEventListener*/
{
	IMPLEMENT_VEHICLEOBJECT
public:  
	CVehicleMovementBase();
	virtual ~CVehicleMovementBase();

	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table) override;
	virtual void PostInit() override;
	virtual void Release() override;
	virtual void Reset() override;
	virtual void Physicalize() override;
	virtual void PostPhysicalize() override;

	virtual void ResetInput() override;

	virtual EVehicleMovementType GetMovementType() override { return eVMT_Other; }

	virtual bool StartDriving(EntityId driverId) override;
	virtual void StopDriving() override;

	virtual bool IsPowered() override { return m_isEnginePowered; }
	virtual void DisableEngine(bool disable) override;

	virtual float GetDamageRatio() override { return m_damage; }

	virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value) override;
	virtual void OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params) override;
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override;

	virtual void ProcessMovement(const float deltaTime) override;
	virtual void ProcessActions(const float deltaTime) {}
	virtual void ProcessAI(const float deltaTime) {}
	virtual void Update(const float deltaTime) override;
	virtual void PostUpdateView(SViewParams &viewParams, EntityId playerId) override {}
	
	virtual int GetStatus(SVehicleMovementStatus* status) override;

	// Optional debug for simulation
	virtual void SimulateProcessMovement(SVehicleMovementSimulate* simulate) {}

	virtual void Serialize(TSerialize ser, EEntityAspects aspects) override;
	virtual void SetChannelId(uint16 id) {}
	virtual void SetAuthority(bool auth) override{}
	virtual void PostSerialize() override;

	virtual void OnEngineCompletelyStarted();
	virtual void OnEngineCompletelyStopped();

	virtual SVehicleMovementAction GetMovementActions() override { return m_movementAction; }
	virtual void RequestActions(const SVehicleMovementAction& movementAction) override;
	virtual bool RequestMovement(CMovementRequest& movementRequest) override;
	virtual void GetMovementState(SMovementState& movementState) override;
	virtual bool GetStanceState(const SStanceStateQuery& query, SStanceState& state) override;
	virtual SVehicleNetState GetVehicleNetState() override { return SVehicleNetState(); }
	virtual void SetVehicleNetState(const SVehicleNetState& state) override {}

	virtual pe_type GetPhysicalizationType() const override { return PE_RIGID; }
	virtual bool UseDrivingProxy() const override { return false; }
	virtual int GetWheelContacts() const override { return 0; }

	virtual void RegisterActionFilter(IVehicleMovementActionFilter* pActionFilter) override;
	virtual void UnregisterActionFilter(IVehicleMovementActionFilter* pActionFilter) override;

	//virtual void OnSoundEvent(ESoundCallbackEvent event,ISound *pSound);

	virtual void EnableMovementProcessing(bool enable) override { m_bMovementProcessingEnabled = enable; }
	virtual bool IsMovementProcessingEnabled() override { return m_bMovementProcessingEnabled; }

	virtual void ProcessEvent(const SEntityEvent& event) override;
	virtual CryCriticalSection* GetNetworkLock() override { return &m_networkLock; }

	virtual float GetEnginePedal() override { return m_movementAction.power; }

	const SVehiclePhysicsStatus& GetPhysicsStatus(unsigned int thread) { CRY_ASSERT(thread<k_numThreads); return m_physStatus[thread]; }

	void SetRemotePilot(bool on) { m_remotePilot = on; }
	SParticleStatus& GetParticleStats() { return m_paStats; }
	SParticleParams* GetParticleParams() { return m_pPaParams; }

	virtual const SVehicleMovementLargeObjectInfo* GetLargeObjectInfo() { return NULL; }

	virtual void GetMemoryUsage(ICrySizer* pSizer) const override
	{
		pSizer->AddObject(this, sizeof(*this));
		GetMemoryUsageInternal(pSizer);
	}

	void GetMemoryUsageInternal(ICrySizer * pSizer) const
	{
		for( int i = 0 ; i < eSID_Max ; ++i )
			pSizer->AddObject(m_audioControlIDs[i]);

		pSizer->AddObject(m_actionFilters);
		pSizer->AddObject(m_damageComponents);
	}

	virtual IEntityAudioComponent* GetAudioProxy() const override;
	virtual CryAudio::ControlId GetPrimaryWeaponAudioTrigger() const override { return m_audioControlIDs[eSID_VehiclePrimaryWeapon]; }
	virtual CryAudio::ControlId GetPrimaryWeaponAudioStopTrigger() const override { return m_audioControlIDs[eSID_VehicleStopPrimaryWeapon]; }
	virtual CryAudio::ControlId GetSecondaryWeaponAudioTrigger() const override { return m_audioControlIDs[eSID_VehicleSecondaryWeapon]; }
	virtual CryAudio::ControlId GetSecondaryWeaponAudioStopTrigger() const override { return m_audioControlIDs[eSID_VehicleStopSecondaryWeapon]; }

protected:

	virtual bool StartEngine() override;
	virtual void StopEngine() override;

	ILINE IPhysicalEntity* GetPhysics() const { return m_pVehicle->GetEntity()->GetPhysics(); }
	bool IsProfilingMovement();

	// sound methods
	void ExecuteTrigger(EVehicleMovementSound eSID);
	void StopTrigger(EVehicleMovementSound eSID);
	void StopAllTriggers();
	void ResetAudioParams();
	const CryAudio::ControlId& GetAudioControlID(EVehicleMovementSound eSID);
	void SetSoundParam(EVehicleMovementSound eSID, const char* param, float value);

#if ENABLE_VEHICLE_DEBUG
	void DebugDraw(const float deltaTime);
#endif

	// animation methiods
	void StartAnimation(EVehicleMovementAnimation eAnim);
	void StopAnimation(EVehicleMovementAnimation eAnim);
	void SetAnimationSpeed(EVehicleMovementAnimation eAnim, float speed);

	void SetDamage(float damage, bool fatal);
	virtual void UpdateDamage(const float deltaTime);

	virtual void UpdateRunSound(const float deltaTime);
	virtual void UpdateSpeedRatio(const float deltaTime);

	virtual void Boost(bool enable);
	virtual bool Boosting() { return m_boost; }
	virtual void UpdateBoost(const float deltaTime);
	virtual void ResetBoost();

	void EjectionTest(float deltaTime);
	void ApplyAirDamp(float angleMin, float angVelMin, float deltaTime, int threadSafe);
	void UpdateGravity(float gravity);

	// surface particle/sound methods
	virtual void InitExhaust();
	virtual void InitSurfaceEffects();
	virtual void ResetParticles();
	virtual void UpdateSurfaceEffects(const float deltaTime);  
	virtual void RemoveSurfaceEffects();
	virtual void GetParticleScale(const SEnvironmentLayer& layer, float speed, float power, float& countScale, float& sizeScale, float& speedScale);
	virtual void EnableEnvEmitter(TEnvEmitter& emitter, bool enable);
	virtual void UpdateExhaust(const float deltaTime);  
	void FreeEmitterSlot(int& slot);
	void FreeEmitterSlot(const int& slot);
	void StartExhaust(bool ignition=true, bool reload=true);
	void StopExhaust();  
	float GetWaterMod(SExhaustStatus& exStatus);
	float GetSoundDamage();
	void FreeExhaustParticles();

	SMFXResourceListPtr GetEffectNode(int matId);
	const char* GetEffectByIndex(int matId, const char* username);

	virtual bool GenerateWind() { return true; }
	void InitWind();
	void UpdateWind(const float deltaTime);
	void SetWind(const Vec3& wind);
	Vec3 GetWindPos(Vec3& posRad, Vec3& posLin);
	// ~surface particle/sound methods

	static void UpdatePhysicsStatus(SVehiclePhysicsStatus* status, const pe_status_pos* psp, const pe_status_dynamics* psd);
	void CacheAudioControlIDs();

public:
	enum { k_mainThread=0, k_physicsThread=1, k_numThreads };

protected:
	IVehicle* m_pVehicle;
	IEntity* m_pEntity;
	static IGameTokenSystem* m_pGameTokenSystem;
	static IVehicleSystem* m_pVehicleSystem;
	static IActorSystem* m_pActorSystem;

	SVehicleMovementAction m_movementAction;

	bool m_keepEngineOn;	// If true, engine won't turn off if the driver leaves.
	bool m_isEngineDisabled;
	bool m_isEngineStarting;
	bool m_isEngineGoingOff;
	uint8 m_isProbablyDistant : 1;
	uint8 m_isProbablyVisible : 1;

	float m_engineStartup;
	float m_engineIgnitionTime;
	uint32 m_engineDisabledTimerId;


	uint32 m_engineDisabledFXId;


	bool m_bMovementProcessingEnabled;
	bool m_isEnginePowered;
	bool m_isExhaustActivated;
	bool m_remotePilot;
	float m_damage;
	bool m_bFirstHit;

	CryAudio::ControlId m_audioControlIDs[eSID_Max];
	
	Vec3 m_enginePos;
	float m_runSoundDelay;  
	float m_rpmScale, m_rpmScaleSgn;
	float m_rpmPitchSpeed;

	bool m_boost;
	bool m_wasBoosting;
	float m_boostEndurance;
	float m_boostRegen;  
	float m_boostStrength;
	float m_boostCounter;

	IVehicleAnimation* m_animations[eVMA_Max];

	SParticleParams* m_pPaParams;
	SParticleStatus m_paStats;
	SSurfaceSoundStatus m_surfaceSoundStats;
	SMovementSoundStatus m_soundStats;

	EntityId m_actorId;

	SVehiclePhysicsStatus m_physStatus[k_numThreads];

	float m_speed;				// Gets updated once per update on the main thread
	Vec3 m_localSpeed;			// Gets updated once per update on the main thread
	Vec3 m_localAccel;

	float m_maxSpeed; 
	float m_speedRatio;
	float m_speedRatioUnit;

	float m_measureSpeedTimer;
	Vec3 m_lastMeasuredVel;

	// flight stabilization
	Vec3 m_dampAngle;
	Vec3 m_dampAngVel;  

	float m_ejectionDotProduct;
	float m_ejectionTimer;
	float m_ejectionTimer0;

	IPhysicalEntity* m_pWind[2];

	typedef std::list<IVehicleMovementActionFilter*> TVehicleMovementActionFilterList;
	TVehicleMovementActionFilterList m_actionFilters;

	typedef std::vector<IVehicleComponent*> TComponents;
	TComponents m_damageComponents;  

	static float		m_sprintTime;

	static const float	ms_engineSoundIdleRatio, ms_engineSoundOverRevRatio;

	ForceFeedbackFxId	m_engineStartingForceFeedbackFxId;

	ForceFeedbackFxId	m_enginePoweredForceFeedbackFxId;

	ForceFeedbackFxId	m_collisionForceFeedbackFxId;

	CryCriticalSection	m_lock;
	CryCriticalSection	m_networkLock;
};


ILINE CVehicleMovementBase* StaticCast_CVehicleMovementBase(IVehicleMovement* pMovement)
{
	return (pMovement->GetMovementType()!=IVehicleMovement::eVMT_Dummy) ? static_cast<CVehicleMovementBase*>(pMovement) : NULL;
}


struct SPID
{
	ILINE SPID() : m_kP( 0 ),	m_kD( 0 ), m_kI( 0 ),	m_prevErr( 0 ),	m_intErr( 0 ){}
	ILINE void	Reset(){m_prevErr = 0; m_intErr = 0;}
	float	Update( float inputVal, float setPoint, float clampMin, float clampMax );
	void  Serialize(TSerialize ser);
	float	m_kP;
	float	m_kD;
	float	m_kI;
	float	m_prevErr;
	float	m_intErr;
};

#define MOVEMENT_VALUE_OPT(name, var, t) \
	t.getAttr(name, var);

#define MOVEMENT_VALUE_REQ(name, var, t) \
	if (!t.getAttr(name, var)) \
{ \
	CryLog("Movement Init (%s) - failed to init due to missing <%s> parameter", m_pVehicle->GetEntity()->GetClass()->GetName(), name); \
	return false; \
}

#define MOVEMENT_VALUE(name, var) \
	if (!table.getAttr(name, var)) \
{ \
	CryLog("Movement Init (%s) - failed to init due to missing <%s> parameter", m_pVehicle->GetEntity()->GetClass()->GetName(), name); \
	return false; \
}
