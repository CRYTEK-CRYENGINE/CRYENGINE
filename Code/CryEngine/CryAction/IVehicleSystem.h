// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntity.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryNetwork/ISerialize.h>
#include <CryAudio/IAudioSystem.h>
#include "IGameObjectSystem.h"
#include "IGameObject.h"
#include "IMovementController.h"
#include "VehicleSystem/VehicleParams.h"

struct IGameFramework;
struct SViewParams;
struct SOBJECTSTATE;
struct IVehiclePart;
struct IVehicleWheel;
struct IVehicleMovement;
class CMovementRequest;
struct SMovementState;
struct IVehicleEventListener;
struct SVehicleEventParams;
struct IFireController;
struct IVehicleSystem;
struct IVehicleSeat;
struct IVehicleAnimation;
struct IVehicleHelper;
struct IVehicleComponent;
struct IVehicleAnimationComponent;
struct IVehicleClient;
struct HitInfo;

// compile out debug draw code in release builds
#if defined(_RELEASE) || defined(DEDICATED_SERVER)
	#define ENABLE_VEHICLE_DEBUG 0
#else
	#define ENABLE_VEHICLE_DEBUG 1
#endif

// Summary
//   Type used to hold vehicle's ActionIds
// See Also
//   EVehicleActionIds
typedef int TVehicleActionId;

// Summary
//   ActionId constants
// Description
//   ActionId constants which represent the multiple ways that an actor can
//   interact while being on a vehicle seat.
// See Also
//   TVehicleActionId
enum EVehicleActionIds
{
	eVAI_Exit = 1,
	eVAI_ChangeSeat,
	eVAI_ChangeSeat1,
	eVAI_ChangeSeat2,
	eVAI_ChangeSeat3,
	eVAI_ChangeSeat4,
	eVAI_ChangeSeat5,
	eVAI_RotatePitch,
	eVAI_RotateYaw,
	eVAI_RotatePitchAimAssist,
	eVAI_RotateYawAimAssist,
	eVAI_RotateRoll,
	eVAI_XIRotatePitch,
	eVAI_XIRotateYaw,
	eVAI_MoveForward,
	eVAI_MoveBack,
	eVAI_XIMoveY,
	eVAI_TurnLeft,
	eVAI_TurnRight,
	eVAI_XIMoveX,
	eVAI_StrafeLeft,
	eVAI_StrafeRight,
	eVAI_XIStrafe,
	eVAI_RollLeft,
	eVAI_RollRight,
	eVAI_AfterBurner,
	eVAI_Brake,
	eVAI_MoveUp,
	eVAI_MoveDown,
	eVAI_XIMoveUpDown,
	eVAI_ChangeView,
	eVAI_ViewOption,
	eVAI_FireMode,
	eVAI_Attack1,
	eVAI_Attack2,
	eVAI_Attack3,
	eVAI_ToggleLights,
	eVAI_Horn,
	eVAI_ZoomIn,
	eVAI_ZoomOut,
	eVAI_DesirePitch,
	eVAI_Boost,
	eVAI_PitchUp,
	eVAI_PitchDown,
	eVAI_Debug_1,
	eVAI_Debug_2,
	eVAI_PhysicsStep,
	eVAI_XIAccelerate,
	eVAI_XIDeccelerate,
	eVAI_Others,
};

// Summary
//   Vehicle event types
// See Also
//   IVehicle::BroadcastVehicleEvent, IVehicle::RegisterVehicleEventListener, IVehicleEventListener
enum EVehicleEvent
{
	eVE_Collision = 0,
	eVE_Hit,
	eVE_Damaged,
	eVE_Destroyed,
	eVE_Repair,
	eVE_PassengerEnter,
	eVE_PassengerExit,
	eVE_PassengerChangeSeat,
	eVE_SeatFreed,
	eVE_PreVehicleDeletion,
	eVE_VehicleDeleted,
	eVE_ToggleDriverControlledGuns,
	eVE_Brake,
	eVE_Timer,
	eVE_EngineStopped,
	eVE_OpenDoors,
	eVE_CloseDoors,
	eVE_BlockDoors,
	eVE_ExtractGears,
	eVE_RetractGears,
	eVE_Indestructible,
	eVE_BeingFlipped,
	eVE_Reversing,
	eVE_WeaponRemoved,
	eVE_TryDetachPartEntity,
	eVE_OnDetachPartEntity,
	eVE_TryAttachPartEntityNetID,
	eVE_RequestDelayedDetachAllPartEntities,
	eVE_StartParticleEffect,
	eVE_StopParticleEffect,
	eVE_LockBoneRotation,
	eVE_VehicleMovementRotation,
	eVE_Hide,
	eVE_ViewChanged,
	eVE_Awake,
	eVE_Sleep,
	eVE_Last,
};

// Summary
//   Value type used to define the seat id in the vehicle system
// See Also
//   IVehicleSeat
typedef int TVehicleSeatId;
const TVehicleSeatId InvalidVehicleSeatId = 0;
const TVehicleSeatId FirstVehicleSeatId   = 1;

// Summary
//   Value type used to define the view id in the vehicle system
// See Also
//   IVehicleView
typedef int TVehicleViewId;
const TVehicleSeatId InvalidVehicleViewId = 0;
const TVehicleSeatId FirstVehicleViewId   = 1;

// Summary
//   Value type used to define the state id of a vehicle animation
// See Also
//   IVehicleAnimation
typedef int TVehicleAnimStateId;
const TVehicleAnimStateId InvalidVehicleAnimStateId = -1;

struct SVehicleStatus
{
	SVehicleStatus()
	{
		Reset();
	}

	void Reset()
	{
		vel.Set(0, 0, 0);
		speed = 0.f;
		health = 1.f;
		passengerCount = 0;
		altitude = 0.f;
		flipped = 0.f;
		contacts = 0;
		submergedRatio = 0.f;
		frameId = 0;
		beingFlipped = false;
		doingNetPrediction = false;
	}

	void Serialize(TSerialize ser, EEntityAspects aspects)
	{
		ser.Value("health", health);
		ser.Value("speed", speed);
		ser.Value("altitude", altitude);
		ser.Value("flipped", flipped);
	}

	Vec3  vel;
	float speed;
	float health;
	float altitude;
	float flipped;
	int   passengerCount;
	int   contacts;
	float submergedRatio;
	bool  beingFlipped;
	bool  doingNetPrediction;

	int   frameId;
};

// Vehicle Movement Status Interface
enum EVehicleMovementStatusType
{
	eVMS_invalid,
	eVMS_general,
	eVMS_wheeled,
};

struct SVehicleMovementStatus
{
	int typeId;
	SVehicleMovementStatus(int type = eVMS_invalid) : typeId(type) {}
};

// Gather general status
struct SVehicleMovementStatusGeneral : SVehicleMovementStatus
{
	SVehicleMovementStatusGeneral() : SVehicleMovementStatus(eVMS_general)
	{
		pos.zero();
		q.SetIdentity();
		centerOfMass.zero();
		vel.zero();
		angVel.zero();
		localVel.zero();
		localAngVel.zero();
		localAccel.zero();
		speed = 0.f;
		topSpeed = 0.f;
		submergedRatio = 0.f;
	}

	Vec3  pos;
	Quat  q;
	Vec3  centerOfMass;
	Vec3  vel;
	Vec3  angVel;
	Vec3  localVel;
	Vec3  localAngVel;
	Vec3  localAccel;
	float speed;
	float topSpeed;
	float submergedRatio;
};

// Gather wheeled status
struct SVehicleMovementStatusWheeled : SVehicleMovementStatus
{
	SVehicleMovementStatusWheeled() : SVehicleMovementStatus(eVMS_wheeled)
	{
		numWheels = 0;
		suspensionCompressionRate = 0.f;
	}

	int   numWheels;
	float suspensionCompressionRate;
};

struct SVehicleDamageParams
{
	float collisionDamageThreshold;

	float groundCollisionMinSpeed;
	float groundCollisionMaxSpeed;
	float groundCollisionMinMult;
	float groundCollisionMaxMult;

	float vehicleCollisionDestructionSpeed;

	float submergedRatioMax;
	float submergedDamageMult;

	float aiKillPlayerSpeed;
	float playerKillAISpeed;
	float aiKillAISpeed;  // only applies to AI of other faction

	SVehicleDamageParams()
	{
		collisionDamageThreshold = 50.f;

		groundCollisionMinMult = 1.f; // increases "falling" damage when > 1
		groundCollisionMaxMult = 1.f;
		groundCollisionMinSpeed = 15.f;
		groundCollisionMaxSpeed = 50.f;

		vehicleCollisionDestructionSpeed = -1;  // eg disabled.

		submergedRatioMax = 1.f;
		submergedDamageMult = 1.f;

		aiKillPlayerSpeed = 0.0f;
		playerKillAISpeed = 0.0f;
		aiKillAISpeed = 0.0f;
	}
};

struct SVehicleImpls
{
	void   Add(const string& impl) { vImpls.push_back(impl); }

	int    Count() const           { return vImpls.size(); }

	string GetAt(int idx) const
	{
		if (idx >= 0 && idx < (int)vImpls.size())
			return vImpls[idx];
		return "";
	}
private:
	std::vector<string> vImpls;
};

/** Exhaust particles parameter structure
 * Movement classes can use this to control exhausts.
 * Currently we support a start/stop effect and a running effect
 * with various parameters. It's up to the movement if/how
 * it makes use of it.
 */
struct SExhaustParams
{
	SExhaustParams()
	{
		runBaseSizeScale = 1.0f;
		runMinSpeed = runMaxSpeed = 0.0f;
		runMinPower = runMaxPower = 0.0f;
		runMinSpeedSizeScale = runMaxSpeedSizeScale = 1.0f;
		runMinSpeedCountScale = runMaxSpeedCountScale = 1.0f;
		runMinSpeedSpeedScale = runMaxSpeedSpeedScale = 1.0f;
		runMinPowerSizeScale = runMaxPowerSizeScale = 1.0f;
		runMinPowerSpeedScale = runMaxPowerSpeedScale = 1.0f;
		runMinPowerCountScale = 1.0f;
		runMaxPowerCountScale = 1.0f;
		insideWater = false;
		outsideWater = true;
		disableWithNegativePower = false;

		hasExhaust = false;
	}
	virtual ~SExhaustParams(){}
	virtual size_t          GetExhaustCount() const = 0;
	virtual IVehicleHelper* GetHelper(int idx) const = 0;

	virtual const char*     GetStartEffect() const = 0;
	virtual const char*     GetStopEffect() const = 0;
	virtual const char*     GetRunEffect() const = 0;
	virtual const char*     GetBoostEffect() const = 0;

	float                   runBaseSizeScale;
	float                   runMinSpeed;
	float                   runMinSpeedSizeScale;
	float                   runMinSpeedCountScale;
	float                   runMinSpeedSpeedScale;
	float                   runMaxSpeed;
	float                   runMaxSpeedSizeScale;
	float                   runMaxSpeedCountScale;
	float                   runMaxSpeedSpeedScale;
	float                   runMinPower;
	float                   runMinPowerSizeScale;
	float                   runMinPowerCountScale;
	float                   runMinPowerSpeedScale;
	float                   runMaxPower;
	float                   runMaxPowerSizeScale;
	float                   runMaxPowerCountScale;
	float                   runMaxPowerSpeedScale;

	bool                    insideWater;
	bool                    outsideWater;
	bool                    disableWithNegativePower;

	bool                    hasExhaust;
};

struct SDamageEffect
{
	string          effectName;
	IVehicleHelper* pHelper;
	bool            isGravityDirUsed;
	Vec3            gravityDir;
	float           pulsePeriod;

	void            GetMemoryUsage(ICrySizer* s) const
	{
		s->AddObject(effectName);
	}

	SDamageEffect()
		: pHelper(NULL),
		isGravityDirUsed(false),
		gravityDir(ZERO),
		pulsePeriod(0.f)
	{
	}
};

/** SEnvironmentParticles
 * Holds params for environment particles like dust, dirt..
 * A vehicle can use several layers to control different effects
 * with distinct parameters.
 */
struct SEnvironmentLayer
{
	SEnvironmentLayer()
	{
		minSpeed = 0;
		maxSpeed = 10;
		minSpeedSizeScale = maxSpeedSizeScale = 1.f;
		minSpeedCountScale = maxSpeedCountScale = 1.f;
		minSpeedSpeedScale = maxSpeedSpeedScale = 1.f;
		minPowerSizeScale = maxPowerSizeScale = 1.f;
		minPowerCountScale = maxPowerCountScale = 1.f;
		minPowerSpeedScale = maxPowerSpeedScale = 1.f;
		maxHeightSizeScale = maxHeightCountScale = 1.f;
		alignGroundHeight = 0;
		alignToWater = false;
		active = true;
	}
	virtual ~SEnvironmentLayer(){}

	virtual const char*     GetName() const = 0;

	virtual size_t          GetHelperCount() const = 0;
	virtual IVehicleHelper* GetHelper(int idx) const = 0;

	virtual size_t          GetGroupCount() const = 0;
	virtual size_t          GetWheelCount(int group) const = 0;
	virtual int             GetWheelAt(int group, int wheel) const = 0;

	virtual bool            IsGroupActive(int group) const = 0;
	virtual void            SetGroupActive(int group, bool active) = 0;

	float                   minSpeed;
	float                   minSpeedSizeScale;
	float                   minSpeedCountScale;
	float                   minSpeedSpeedScale;
	float                   maxSpeed;
	float                   maxSpeedSizeScale;
	float                   maxSpeedCountScale;
	float                   maxSpeedSpeedScale;
	float                   minPowerSizeScale;
	float                   minPowerCountScale;
	float                   minPowerSpeedScale;
	float                   maxPowerSizeScale;
	float                   maxPowerCountScale;
	float                   maxPowerSpeedScale;
	float                   alignGroundHeight;
	float                   maxHeightSizeScale;
	float                   maxHeightCountScale;
	bool                    alignToWater;
	bool                    active;
};

/** SEnvironmentParticles
 * Holds EnvironmentLayers
 */
struct SEnvironmentParticles
{
	virtual ~SEnvironmentParticles(){}
	virtual size_t                   GetLayerCount() const = 0;
	virtual const SEnvironmentLayer& GetLayer(int idx) const = 0;
	virtual const char*              GetMFXRowName() const = 0;
};

typedef std::map<string, SDamageEffect> TDamageEffectMap;

/** SParticleParams is the container for all particle structures
 */
struct SParticleParams
{
	virtual ~SParticleParams(){}
	virtual SExhaustParams*        GetExhaustParams() = 0;
	virtual const SDamageEffect*   GetDamageEffect(const char* pName) const = 0;
	virtual SEnvironmentParticles* GetEnvironmentParticles() = 0;
};

struct SVehicleEventParams
{
	EntityId entityId;

	bool     bParam;
	int      iParam;
	float    fParam, fParam2;
	Vec3     vParam;

	SVehicleEventParams()
		: entityId(0)
		, bParam(false)
		, iParam(0)
		, fParam(0.f)
		, fParam2(0.0f)
	{
		vParam.zero();
	}
};

struct SVehicleMovementEventParams
{
	Vec3               vParam; // param for hit pos or others
	float              fValue; // param for e.g. damage value
	int                iValue;
	bool               bValue;
	const char*        sParam;
	IVehicleComponent* pComponent; // optionally, vehicle component involved

	SVehicleMovementEventParams()
		: fValue(0.f)
		, iValue(0)
		, bValue(false)
		, pComponent(NULL)
		, sParam(NULL)
	{
		vParam.zero();
	}
};

// Sound event info
struct SVehicleSoundInfo
{
	string              name;      // event name
	IVehicleHelper*     pHelper;
	CryAudio::ControlId soundId;
	bool                isEngineSound;

	SVehicleSoundInfo() : isEngineSound(false), pHelper(NULL)
	{
		soundId = CryAudio::InvalidControlId;
	}
};
typedef int TVehicleSoundEventId;
const TVehicleSoundEventId InvalidSoundEventId = -1;

// Vehicle weapon info
struct SVehicleWeaponInfo
{
	EntityId entityId;
	bool     bCanFire;

	SVehicleWeaponInfo() : entityId(0), bCanFire(false) {}
};

struct IVehicleEventListener
{
	virtual ~IVehicleEventListener(){}
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) = 0;
};

struct IVehicleUsageEventListener
{
	virtual ~IVehicleUsageEventListener(){}
	virtual void OnStartUse(const EntityId playerId, IVehicle* pVehicle) = 0;
	virtual void OnEndUse(const EntityId playerId, IVehicle* pVehicle) = 0;
};

// Summary
//   Type for the id used to define the class of a vehicle object
// See Also
//   IVehicleObject
typedef unsigned int TVehicleObjectId;
const TVehicleObjectId InvalidVehicleObjectId = 0;

// Summary
//   Base interface used by many vehicle objects
struct IVehicleObject
	: public IVehicleEventListener
{
	// Summary
	//   Returns the id of the vehicle object
	virtual TVehicleObjectId GetId() const = 0;

	// Summary
	//   Serialize the vehicle object
	virtual void Serialize(TSerialize ser, EEntityAspects aspects) = 0;

	// Summary
	//   Update the vehicle object
	// Description
	//   Used to handle any time sensitive updates. In example, rotation for
	//   turret parts is implemented in this function.
	// Parameters
	//   deltaTime - total time for the update, in seconds
	virtual void Update(const float deltaTime) = 0;

	virtual void UpdateFromPassenger(const float deltaTime, EntityId playerId) {}

	// Summary
	//  Handle vehicle events
	// See Also
	//   EVehicleEvent
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) = 0;
};

// Summary
//   Enumeration of different event which can trigger a vehicle action to
//   perform some tasks.
// See Also
//   IVehicleAction
enum EVehicleActionEvent
{
	eVAE_IsUsable = 1,      // When the vehicle is being tested to be usuable by a player (by aiming the crossair).
	eVAE_OnUsed,            // When the vehicle is being used by a player (with the USE key).
	eVAE_OnGroundCollision, // When the vehicle collide with the ground.
	eVAE_OnEntityCollision, // When the vehicle collide with another entity.
	eVAE_Others,            // ot be used as starting index for game specific action events.
};

// Summary
//   Interface used to implement a special behavior that affect the vehicle
// Description
//   In example, landing gears on flying vehicles or the entering procedure
//   for the player are implemented as vehicle action.
struct IVehicleAction
	: public IVehicleObject
{
	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table) = 0;
	virtual void Reset() = 0;
	virtual void Release() = 0;

	virtual int  OnEvent(int eventType, SVehicleEventParams& eventParams) = 0;

	// IVehicleObject
	virtual TVehicleObjectId GetId() const override = 0;
	virtual void             Serialize(TSerialize ser, EEntityAspects aspects) override = 0;
	virtual void             Update(const float deltaTime) override = 0;
	// ~IVehicleObject
};

#define IMPLEMENT_VEHICLEOBJECT         \
  public:                               \
    static TVehicleObjectId m_objectId; \
    friend struct IVehicleSystem;       \
    virtual TVehicleObjectId GetId() const override { return m_objectId; }

#define DEFINE_VEHICLEOBJECT(obj) \
  TVehicleObjectId obj::m_objectId = InvalidVehicleObjectId;

#define CAST_VEHICLEOBJECT(type, objptr) \
  (objptr->GetId() == type::m_objectId) ? (type*)objptr : NULL

// Summary:
//   Vehicle implementation interface
// Description:
//   Interface used to implement a vehicle.
struct IVehicle : public IGameObjectExtension
{
	// Summary:
	//   Reset vehicle
	// Description:
	//   Will reset properly all components of the vehicle including the seats,
	//   components, parts and the movement.
	virtual void Reset(bool enterGame) = 0;

	// Summary:
	//   Gets the physics params
	// Description:
	//   It will return the physics params which have been sent to the entity
	//   system when the vehicle entity was spawned
	// Return value:
	//   a SEntityPhysicalizeParams structure
	virtual SEntityPhysicalizeParams& GetPhysicsParams() = 0;

	// Summary:
	//   Gets the status
	// Description:
	//   Will return a structure which hold several status information on the vehicle, including destroyed state and passenger count.
	// Return value:
	//   a SVehicleStatus structure
	virtual const SVehicleStatus& GetStatus() const = 0;

	// Summary:
	//   Updates view of a passenger
	// Description:
	//   Will update the SViewParams structure will the correct camera info for a specified passenger of the vehicle.
	// Parameters:
	//   viewParams - structure which is used to return the camera info
	//   playerId - entity id of the passenger
	virtual void UpdateView(SViewParams& viewParams, EntityId playerId = 0) = 0;

	virtual void UpdatePassenger(float frameTime, EntityId playerId) = 0;

	//FIXME:move this to the gameside, its not really needed here, plus add callerId to the IActionListener interface
	virtual void                        OnAction(const TVehicleActionId actionId, int activationMode, float value, EntityId callerId) = 0;
	virtual void                        OnHit(const HitInfo& hitInfo, IVehicleComponent* pHitComponent = NULL) = 0;

	virtual void                        SetUnmannedFlippedThresholdAngle(float angle) = 0;

	virtual float                       GetDamageRatio(bool onlyMajorComponents = false) const = 0;

	virtual void                        SetAmmoCount(IEntityClass* pAmmoType, int amount) = 0;
	virtual int                         GetAmmoCount(IEntityClass* pAmmoType) const = 0;

	virtual void                        SetOwnerId(EntityId ownerId) = 0;
	virtual EntityId                    GetOwnerId() const = 0;

	virtual int                         IsUsable(EntityId userId) = 0;
	virtual bool                        OnUsed(EntityId userId, int index) = 0;

	virtual bool                        AddSeat(const SmartScriptTable& seatParams) = 0;
	virtual bool                        AddHelper(const char* name, Vec3 position, Vec3 direction, IVehiclePart* pPart) = 0;

	virtual int                         GetPartCount() = 0;
	virtual IVehiclePart*               GetPart(unsigned int index) = 0;
	virtual void                        GetParts(IVehiclePart** parts, int nMax) = 0;

	virtual int                         GetComponentCount() const = 0;
	virtual IVehicleComponent*          GetComponent(int index) = 0;
	virtual IVehicleComponent*          GetComponent(const char* name) = 0;

	virtual IVehicleAnimationComponent& GetAnimationComponent() = 0;

	virtual IVehicleAnimation*          GetAnimation(const char* name) = 0;

	virtual int                         GetActionCount() = 0;
	virtual IVehicleAction*             GetAction(int index) = 0;

	// get the current movement controller
	virtual IMovementController* GetMovementController() = 0;

	virtual IFireController*     GetFireController(uint32 controllerNum = 0) = 0;

	// Get TVehicleSeatId of last seat
	virtual TVehicleSeatId GetLastSeatId() = 0;

	// Get Seat by SeatId, or NULL
	virtual IVehicleSeat* GetSeatById(const TVehicleSeatId seatId) const = 0;

	// Get amount of Seats
	virtual unsigned int GetSeatCount() = 0;

	// get seat interface for passengerId, or NULL
	virtual IVehicleSeat* GetSeatForPassenger(EntityId passengerId) const = 0;

	// Get SeatId by name
	virtual TVehicleSeatId GetSeatId(const char* pSeatName) = 0;

	// check if player/client is driving this vehicle
	virtual bool IsPlayerDriving(bool clientOnly = true) = 0;

	// check if there is friendlyPassenger
	virtual bool HasFriendlyPassenger(IEntity* pPlayer) = 0;

	// check if client is inside this vehicle
	virtual bool IsPlayerPassenger() = 0;

	// get total number of weapon entities on vehicle
	virtual int GetWeaponCount() const = 0;

	// Get EntityId by weapon index
	virtual EntityId GetWeaponId(int index) const = 0;

	// get EntityId for currently used weapon for passengerId, or 0
	virtual EntityId GetCurrentWeaponId(EntityId passengerId, bool secondary = false) const = 0;

	// get currently used weapon info for passengerId. Returns true if info was found.
	virtual bool GetCurrentWeaponInfo(SVehicleWeaponInfo& outInfo, EntityId passengerId, bool secondary = false) const = 0;

	// Summary:
	//   Indicates if an helper position is available
	// Description:
	//   Will find if a specified helper position exist on the vehicle.
	// Parameters:
	//   name - name of the helper position
	virtual bool              HasHelper(const char* pName) = 0;

	virtual IVehiclePart*     GetPart(const char* name) = 0;
	virtual IVehiclePart*     GetWeaponParentPart(EntityId weaponId) = 0;
	virtual IVehicleSeat*     GetWeaponParentSeat(EntityId weaponId) = 0;

	virtual IVehicleMovement* GetMovement() const = 0;

	// Summary:
	//   Returns the wheel count
	// Description:
	//   Will return the wheel count of the vehicle, if any are preset.
	// Return value:
	//   The wheel count, or 0 if none are available
	virtual int           GetWheelCount() = 0;

	virtual IVehiclePart* GetWheelPart(int idx) = 0;

	// Summary:
	//   Returns the mass
	// Description:
	//   Will return the mass of the vehicle.
	// Return value:
	//   The mass value, it should never be a negative value
	virtual float GetMass() const = 0;

	// Summary:
	//   Returns the altitude
	// Description:
	//   Will return the altitude of the vehicle.
	// Return value:
	//   The altitude value, it should be a positive value
	virtual float                GetAltitude() = 0;

	virtual IMovementController* GetPassengerMovementController(EntityId passenger) = 0;

	// Summary
	//   Sends a vehicle event to all the listeners
	// Parameters
	//   event - One of the event declared in EVehicleEvent
	//   params - optional parameter, see EVehicleEvent to know which member of this structure needs to be filled for the specific event
	// See Also
	//   UnregisterVehicleEventListener
	virtual void BroadcastVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) = 0;

	// Summary
	//   Registers an event listener
	// Parameters
	//   pEvenListener - a pointer to class which implements the IVehicleEventListener interface
	// See Also
	//   UnregisterVehicleEventListener
	virtual void RegisterVehicleEventListener(IVehicleEventListener* pEvenListener, const char* name) = 0;

	// Summary
	//   Unregisters an event listener
	// Parameters
	//   pEvenListener - a pointer to class which implements the IVehicleEventListener interface
	// See Also
	//   RegisterVehicleEventListener
	virtual void                        UnregisterVehicleEventListener(IVehicleEventListener* pEvenListener) = 0;

	virtual SParticleParams*            GetParticleParams() = 0;
	virtual const SVehicleDamageParams& GetDamageParams() const = 0;

	uint16                              GetChannelId()
	{
		return GetGameObject()->GetChannelId();
	}
	void SetChannelId(uint16 id)
	{
		GetGameObject()->SetChannelId(id);
	}

	enum EVehicleUpdateSlot
	{
		eVUS_Always = 0,    // used for vehicle elements which always need to be updated
		eVUS_EnginePowered, // used for vehicle elements which should only be updated when the engine is powered
		eVUS_PassengerIn,   // used for vehicle elements which should only when a passenger has entered the vehicle
		eVUS_Visible,       // currently unused
		eVUS_AIControlled,  // used for vehicle using new flight nav

		eVUS_Last = eVUS_AIControlled
	};

	enum EVehicleObjectUpdate
	{
		eVOU_NoUpdate = 0,
		eVOU_AlwaysUpdate,
		eVOU_PassengerUpdate,
		eVOU_Visible,
	};

	virtual void            SetObjectUpdate(IVehicleObject* pObject, EVehicleObjectUpdate updatePolicy) = 0;

	virtual IVehicleHelper* GetHelper(const char* pName) = 0;
	virtual void            Destroy() = 0;
	virtual bool            IsDestroyed() const = 0;
	virtual bool            IsFlipped(float maxSpeed = 0.f) = 0;
	// Summary:
	//   Enables/disables engine slot triggering by vehicle speed
	virtual void TriggerEngineSlotBySpeed(bool trigger) = 0;

	enum EVehicleNeedsUpdateFlags
	{
		eVUF_AwakePhysics = 1 << 0,
	};

	// Summary:
	//  Notify vehicle to that update is required
	virtual void NeedsUpdate(int flags = 0, bool bThreadSafe = false) = 0;

	// Summary:
	//  Register Entity Timer. If timerId == -1, timerId will be assigned
	virtual int SetTimer(int timerId, int ms, IVehicleObject* pObject) = 0;

	// Summary:
	//  Kill Entity Timer
	virtual int KillTimer(int timerId) = 0;

	// Summary:
	//  Get Actor on driver seat, or NULL
	virtual IActor* GetDriver() const = 0;

	// Summary:
	//	Finds a valid world position the actor could be placed at when they exit the vehicle.
	//	Based on seat helpers, but does additional checks if these are all blocked.
	//	NB: actor may not necessarily be a passenger (used for MP spawn trucks)
	//	Returns true if a valid position was found, false otherwise.
	//	extended==true -> allows two actors to exit from one seat (lining up away from the vehicle).
	virtual bool GetExitPositionForActor(IActor* pActor, Vec3& pos, bool extended = false) = 0;

	// Summary:
	//	Returns the physical entities attached to this vehicle, for use with physics RWI and PWI tests
	virtual int GetSkipEntities(IPhysicalEntity** pSkipEnts, int nMaxSkip) = 0;

	// Summary:
	//  Returns vehicle modifications as a comma separated list (with no spaces)
	virtual const char* GetModification() const = 0;

	// Request a passenger to leave the vehicle and spawn at pos
	virtual void ExitVehicleAtPosition(EntityId passengerId, const Vec3& pos) = 0;

	// Evacuate all passengers
	virtual void EvictAllPassengers() = 0;

	// Evacuate all passengers immediately on a client without having to ask the server for permission
	virtual void ClientEvictAllPassengers() = 0;

	// Evict specified passenger immediately on a client without having to ask the server for permission
	virtual void ClientEvictPassenger(IActor* pActor) = 0;

	// Check that an enemy is not already using this vehicle, before entering
	virtual bool IsCrewHostile(EntityId actorId) = 0;

	// Returns the name of the action map specified for this vehicle type in
	//	the xml file
	virtual const char* GetActionMap() const = 0;

	// Returns collision damage multiplayer
	virtual float GetSelfCollisionMult(const Vec3& velocity, const Vec3& normal, int partId, EntityId colliderId) const = 0;

	// Is vehicle probably distant from the player?
	virtual bool IsProbablyDistant() const = 0;

	virtual bool IsIndestructable() const = 0;
	// Sound parameters structure.

	virtual void OffsetPosition(const Vec3& delta) = 0;

	// Summary:
	//Used in MP for logically linking associated vehicles together when spawning
	virtual EntityId GetParentEntityId() const = 0;
	virtual void     SetParentEntityId(EntityId parentEntityId) = 0;
};

struct SVehicleNetState
{
	uint data[16];
};

struct SVehicleMovementAction
{
	SVehicleMovementAction()
	{
		Clear();
	};

	float power;
	float rotateYaw;
	float rotatePitch;
	float rotateRoll;
	bool  brake;
	bool  isAI;

	void  Clear(bool keepPressHoldControlledVars = false)
	{
		if (!keepPressHoldControlledVars)
		{
			power = 0.0f;
			brake = false;
			isAI = false;
			rotateYaw = 0.0f;
		}
		else
		{
			// only keep slight pedal (to allow rolling)
			power *= 0.1f;
		}

		rotateRoll = 0.0f;
		rotatePitch = 0.0f;
	}
};

struct IVehicleMovementActionFilter
{
	virtual ~IVehicleMovementActionFilter(){}
	virtual void OnProcessActions(SVehicleMovementAction& movementAction) = 0;
	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;
};

// Summary:
//   Interface for vehicle movement class
// Description:
//   Interface used to implement a movement class for vehicles.
struct IVehicleMovement
	: public IMovementController
{
	enum EVehicleMovementType
	{
		eVMT_Sea = 0,
		eVMT_Air,
		eVMT_Land,
		eVMT_Amphibious,
		eVMT_Walker,
		eVMT_Dummy,
		eVMT_Other
	};

	// Summary:
	//   Event values for the movement
	// Description:
	//   All event type possible to be sent to a movement.
	enum EVehicleMovementEvent
	{
		// the vehicle got hit and the movement should be damaged according to the
		// value which got passed
		eVME_Damage = 0,
		// the vehicle got hit and the movement steering should fail according to
		// the value which got passed
		eVME_DamageSteering,
		eVME_VehicleDestroyed,
		// Repair event. New damage ratio is passed.
		eVME_Repair,
		// tire destroyed
		eVME_TireBlown,
		// tires restored
		eVME_TireRestored,
		// sent when player enters/leaves seat
		eVME_PlayerEnterLeaveSeat,
		// sent when player enters/leaves the vehicle
		eVME_PlayerEnterLeaveVehicle,
		// sent when player switches view
		eVME_PlayerSwitchView,
		// sent on collision
		eVME_Collision,
		// sent on ground collision
		eVME_GroundCollision,
		// ?
		eVME_WarmUpEngine,
		// sent when vehicle toggles engine update slot
		eVME_ToggleEngineUpdate,
		// becoming visible
		eVME_BecomeVisible,
		eVME_SetMode,
		eVME_Turbulence,
		eVME_NW_Discharge,
		eVME_EnableHandbrake,
		eVME_PartDetached,
		eVME_SetFactorMaxSpeed,
		eVME_SetFactorAccel,
		eVME_StoodOnChange,
		eVME_Others,
	};

	// Summary:
	//   Initializes the movement
	// Description:
	//   Used to initialize a movement from a script table.
	// Parameters:
	//   pVehicle - pointer to the vehicle which will uses this movement instance
	//   table - table which hold all the movement parameters
	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table) = 0;

	// Summary:
	//   PostInit, e.g. for things that need full physicalization
	virtual void PostInit() = 0;

	// Summary:
	//	 Change physicalization of the vehicle.
	virtual void Physicalize() = 0;

	// Summary:
	//	 Post - physicalize.
	virtual void PostPhysicalize() = 0;

	// Summary:
	//   Resets the movement
	virtual void Reset() = 0;

	// Summary:
	//   Releases the movement
	virtual void Release() = 0;

	// Summary:
	//   Get movement type
	virtual EVehicleMovementType GetMovementType() = 0;

	// Summary:
	//	Resets any input previously active in the movement
	virtual void ResetInput() = 0;

	// Summary:
	//   Turn On the engine effects
	// Description:
	//   It will soon be replaced by an event passed with the OnEvent function.
	virtual bool StartEngine() = 0;

	// Summary:
	//   Turn Off the engine effects
	// Description:
	//   It will soon be replaced by an event passed with the OnEvent function.
	virtual void StopEngine() = 0;

	// Summary:
	//   The vehicle has started being driven.
	// Description:
	//   Called when the vehicle gets a new driver.
	virtual bool StartDriving(EntityId driverId) = 0;

	// Summary:
	//   The vehicle has stopped being driven.
	// Description:
	//   The vehicle is not being driven any more.
	virtual void  StopDriving() = 0;

	virtual bool  IsPowered() = 0;

	virtual float GetEnginePedal() = 0;

	// Summary
	//		Returns the damage ratio of the engine
	// Description
	//		Used to receive the damage ratio of the movement. The damage ratio of
	//		the movement may not reflect the damage ratio of the vehicle. The value
	//		is between 0 and 1.
	// Returns
	//		The damage ratio
	virtual float GetDamageRatio() = 0;

	// Summary
	//  Gets number of wheel contacts, 0 if n/a
	virtual int GetWheelContacts() const = 0;

	// Summary:
	//   Sends an event message to the vehicle movement
	// Description:
	//   Used by various code module of the vehicle system to notify the
	//   movement code of any appropriate change. A list of all the supported
	//   event can be found in the EVehicleMovementEvent enum.
	// Parameters:
	//   event  - event to be passed to the movement
	//   params - event parameters, e.g. damage value
	virtual void OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params) = 0;

	virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value) = 0;

	// Summary:
	//   Updates all physics related to the movement
	// Description:
	//   Will update the vehicle movement for a specified frame time. Unlike the
	//   Update function also present on the movement interface, this function is
	//   called from a callback function in CryPhysics rather than from
	//   IVehicle::Update. It's usually called at fixed interval and much more
	//   often than the Update function. This should only be used to update
	//   physics related attributes of the vehicle.
	// Parameters:
	//   deltaTime - time in seconds of the update
	virtual void ProcessMovement(const float deltaTime) = 0;

	// Summary:
	//   Enables/disables movement processing
	// Description:
	//   This allows disabling of the actual movement processing, while still
	//   have the engine running and process driver input, effects, etc.
	//   Useful for trackview sequences
	virtual void EnableMovementProcessing(bool enable) = 0;
	virtual bool IsMovementProcessingEnabled() = 0;

	// Summary:
	//   Enables/disables engine's ability to start
	virtual void DisableEngine(bool disable) = 0;

	// Summary:
	//   Updates the movement
	// Description:
	//   Will update the vehicle movement for a specified frame time. Unlike the
	//   ProcessMovement function, this function is called from the Update
	//   function of IVehicle.
	// Parameters:
	//   deltaTime - time in seconds of the update
	virtual void Update(const float deltaTime) = 0;

	// Summary:
	//   Allows updates by the movement system after the view has updated
	// Description:
	//   Optionally update the SViewParams structure will additional camera information
	// Parameters:
	//   viewParams - structure which is used to return the camera info
	//   playerId - entity id of the passenger
	virtual void PostUpdateView(SViewParams& viewParams, EntityId playerId) = 0;

	// Summary:
	//   Get a vehicle status,
	//   Can return 0 on failure as different vehicle movements will support different status information
	virtual int                    GetStatus(SVehicleMovementStatus* status) = 0;

	virtual void                   Serialize(TSerialize ser, EEntityAspects aspects) = 0;
	virtual void                   SetAuthority(bool auth) = 0;
	virtual void                   PostSerialize() = 0;

	virtual SVehicleMovementAction GetMovementActions() = 0;
	virtual void                   RequestActions(const SVehicleMovementAction& movementAction) = 0;
	virtual bool                   RequestMovement(CMovementRequest& movementRequest) = 0;
	virtual void                   GetMovementState(SMovementState& movementState) = 0;
	virtual SVehicleNetState       GetVehicleNetState() = 0;
	virtual void                   SetVehicleNetState(const SVehicleNetState& state) = 0;

	virtual pe_type                GetPhysicalizationType() const = 0;
	virtual bool                   UseDrivingProxy() const = 0;

	virtual void                   RegisterActionFilter(IVehicleMovementActionFilter* pActionFilter) = 0;
	virtual void                   UnregisterActionFilter(IVehicleMovementActionFilter* pActionFilter) = 0;

	virtual void                   ProcessEvent(const SEntityEvent& event) = 0;
	virtual CryCriticalSection*    GetNetworkLock() = 0;

	virtual void                   GetMemoryUsage(ICrySizer* s) const = 0;

	virtual IEntityAudioComponent* GetAudioProxy() const                      { return nullptr; }
	virtual CryAudio::ControlId    GetPrimaryWeaponAudioTrigger() const       { return CryAudio::InvalidControlId; }
	virtual CryAudio::ControlId    GetPrimaryWeaponAudioStopTrigger() const   { return CryAudio::InvalidControlId; }
	virtual CryAudio::ControlId    GetSecondaryWeaponAudioTrigger() const     { return CryAudio::InvalidControlId; }
	virtual CryAudio::ControlId    GetSecondaryWeaponAudioStopTrigger() const { return CryAudio::InvalidControlId; }
};

// Summary
//		Interface used to implement vehicle views
// Description
//		A vehicle view is a camera implementation. Default implementations of a
//		first person and third person camera are already available in the
//		CryAction Dll.
struct IVehicleView
	: public IVehicleObject
{
	virtual bool Init(IVehicleSeat* pSeat, const CVehicleParams& table) = 0;
	virtual void Reset() = 0;
	virtual void Release() = 0;
	virtual void ResetPosition() = 0;

	// Summary
	//		Returns the name of the camera view
	virtual const char* GetName() = 0;

	// Summary
	//		Indicates if the view implements a third person camera
	virtual bool IsThirdPerson() = 0;

	// Summary
	//		Indicates if the player body has been hidden
	virtual bool IsPassengerHidden() = 0;

	// Views defined via XML can be disabled in code, eg. per level
	virtual bool IsEnabled() const = 0;
	virtual void SetEnabled(bool enabled) = 0;

	// Summary
	//		Performs tasks needed when the client start using this view
	// Parameters
	//	playerId - EntityId of the player who will use the view
	virtual void OnStartUsing(EntityId playerId) = 0;

	// Summary
	//		Performs tasks needed when the client stop using this view
	virtual void             OnStopUsing() = 0;

	virtual void             OnAction(const TVehicleActionId actionId, int activationMode, float value) = 0;
	virtual void             UpdateView(SViewParams& viewParams, EntityId playerId = 0) = 0;

	virtual TVehicleObjectId GetId() const override = 0;
	virtual void             Serialize(TSerialize ser, EEntityAspects aspects) override = 0;

	// Summary
	//		Performs computation which depends of the frame rate
	virtual void Update(const float deltaTime) override = 0;

	virtual void SetDebugView(bool debug) = 0;
	virtual bool IsDebugView() = 0;

	virtual bool ShootToCrosshair() = 0;

	virtual bool IsAvailableRemotely() const = 0;

	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;

	virtual void OffsetPosition(const Vec3& delta) = 0;
};

// Summary:
//   Seat-dependent sound parameters
struct SSeatSoundParams
{
	CCryName moodName; //name of the soundmood to apply
	float    inout;    // [0..1], 0 means fully inside
	float    mood;     // [0..1], 1 means fully active (inside!)
	float    moodCurr; // current value

	SSeatSoundParams()
		: moodName("vehicle")
		, inout(0.f)
		, mood(0.f)
		, moodCurr(0.f)
	{
	}

};

// Summary:
//	Seat locking
enum EVehicleSeatLockStatus
{
	eVSLS_Unlocked,
	eVSLS_LockedForPlayers,
	eVSLS_LockedForAI,
	eVSLS_Locked,
};

struct IVehicleSeatAction;

// Summary:
//   Vehicle Seat interface
struct IVehicleSeat
{
	enum EVehicleTransition
	{
		eVT_None = 0,
		eVT_Entering,
		eVT_Exiting,
		eVT_ExitingWarped,
		eVT_Dying,
		eVT_SeatIsBorrowed,
		eVT_RemoteUsage,
	};
	virtual ~IVehicleSeat(){}
	virtual bool Init(IVehicle* pVehicle, TVehicleSeatId seatId, const CVehicleParams& paramsTable) = 0;
	virtual void PostInit(IVehicle* pVehicle) = 0;
	virtual void Reset() = 0;
	virtual void Release() = 0;

	// Summary
	//   Returns the seat name
	// Returns
	//   a string with the name
	virtual const char* GetSeatName() const = 0;

	// Summary
	//   Returns the id of the seat
	// Returns
	//   A seat id
	virtual TVehicleSeatId GetSeatId() const = 0;
	virtual EntityId       GetPassenger(bool remoteUser = false) const = 0;
	virtual int            GetCurrentTransition() const = 0;

	// Seat Views
	virtual IVehicleView*           GetView(TVehicleViewId viewId) const = 0;
	virtual bool                    SetView(TVehicleViewId viewId) = 0;
	virtual TVehicleViewId          GetCurrentViewId() const = 0;
	virtual IVehicleView*           GetCurrentView() const = 0;
	virtual TVehicleViewId          GetNextViewId(bool bExcludeDebug, bool bExcludeDisabled) const = 0;
	virtual TVehicleViewId          GetNextAvailableViewId() const = 0;
	virtual int                     GetViewCount() const = 0;

	virtual bool                    IsDriver() const = 0;
	virtual bool                    IsGunner() const = 0;
	virtual bool                    IsRemoteControlled() const = 0;
	virtual bool                    IsLocked(IActor* pActor) const = 0;

	virtual bool                    Enter(EntityId actorId, bool isTransitionEnabled = true) = 0;
	virtual bool                    Exit(bool isTransitionEnabled, bool force = false, Vec3 exitPos = ZERO) = 0;
	virtual void                    SetLocked(EVehicleSeatLockStatus status) = 0;
	virtual EVehicleSeatLockStatus  GetLockedStatus() const = 0;

	virtual void                    PrePhysUpdate(const float dt) = 0;

	virtual void                    OnPassengerDeath() = 0;
	virtual void                    UnlinkPassenger(bool ragdoll) = 0;
	virtual bool                    IsPassengerHidden() const = 0;
	virtual bool                    IsPassengerExposed() const = 0;

	virtual bool                    ProcessPassengerMovementRequest(const CMovementRequest& movementRequest) = 0;

	virtual IVehicle*               GetVehicle() const = 0;
	virtual IVehiclePart*           GetAimPart() const = 0;
	virtual IVehicleHelper*         GetSitHelper() const = 0;
	virtual IVehicleHelper*         GetAIVisionHelper() const = 0;
	virtual const SSeatSoundParams& GetSoundParams() const = 0;

	virtual void                    OnCameraShake(float& angleAmount, float& shiftAmount, const Vec3& pos, const char* source) const = 0;

	virtual void                    ForceFinishExiting() = 0;

	// Returns the name of the action map specified for this vehicle seat in
	//	the xml file (can be null if no specific seat actionmap)
	virtual const char*               GetActionMap() const = 0;

	virtual void                      ChangedNetworkState(NetworkAspectType aspects) {};

	virtual bool                      IsAutoAimEnabled()                             { return false; }

	virtual IVehicleSeatAction*       GetISeatActionWeapons() = 0;
	virtual const IVehicleSeatAction* GetISeatActionWeapons() const = 0;

	virtual void                      OffsetPosition(const Vec3& delta) = 0;
};

// Summary:
//   Vehicle Wheel interface
// Description:
//   Interface providing wheel-specific access
struct IVehicleWheel
{
	virtual ~IVehicleWheel(){}
	virtual int                     GetSlot() const = 0;
	virtual int                     GetWheelIndex() const = 0;
	virtual float                   GetTorqueScale() const = 0;
	virtual float                   GetSlipFrictionMod(float slip) const = 0;
	virtual const pe_cargeomparams* GetCarGeomParams() const = 0;
};

// Summary:
//   Vehicle Part interface
// Description:
//   Interface used to implement parts of a vehicle.
struct IVehiclePart
	: public IVehicleObject
{
	// Summary:
	//   Part event values
	// Description:
	//   Values used by the type variable in SVehiclePartEvent.
	enum EVehiclePartEvent
	{
		// the part got damaged, fparam will hold the damage ratio
		eVPE_Damaged = 0,
		// part gets repaired, fparam also holds damage ratio
		eVPE_Repair,
		// currently unused
		eVPE_Physicalize,
		// currently unused
		eVPE_PlayAnimation,
		// used to notify that the part geometry got modified and would need to be
		// reloaded next time the part is reset
		eVPE_GotDirty,
		// toggle hide the part, fparam specify the amount of time until the requested effect is fully completed
		eVPE_Fade,
		// toggle hide
		eVPE_Hide,
		// sent when driver entered the vehicle
		eVPE_DriverEntered,
		// sent when driver left
		eVPE_DriverLeft,
		// sent when part starts being used in turret (or similar) rotation
		eVPE_StartUsing,
		// sent when part starts being used in turret (or similar) rotation
		eVPE_StopUsing,
		// sent when rotations need to be blocked
		eVPE_BlockRotation,
		// sent when vehicle flips over. bParam is true when flipping, false when returning to normal orientation
		eVPE_FlippedOver,
		// used as a starting index for project specific part events
		eVPE_OtherEvents,
	};

	enum EVehiclePartType
	{
		eVPT_Base = 0,
		eVPT_Animated,
		eVPT_AnimatedJoint,
		eVPT_Static,
		eVPT_SubPart,
		eVPT_Wheel,
		eVPT_Tread,
		eVPT_Massbox,
		eVPT_Light,
		eVPT_Attachment,
		eVPT_Entity,
		eVPT_Last,
	};

	enum EVehiclePartState
	{
		eVGS_Default = 0,
		eVGS_Damaged1,
		eVGS_Destroyed,
		eVGS_Last,
	};

	enum EVehiclePartStateFlags
	{
		eVPSF_Physicalize = 1 << 0,
		eVPSF_Force       = 1 << 1,
	};

	// Summary:
	//   Part event structure
	// Description:
	//   This structure is used by the OnEvent function to pass message to
	//   vehicle parts.
	struct SVehiclePartEvent
	{
		// message type value, usually defined in EVehiclePartEvent
		EVehiclePartEvent type;
		// c string parameter (optional)
		const char*       sparam;
		// float parameter (optional)
		float             fparam;
		bool              bparam;
		void*             pData;

		SVehiclePartEvent()
		{
			sparam = 0;
			fparam = 0.f;
			bparam = false;
			pData = NULL;
			type = eVPE_Damaged;
		}
	};

	// Summary:
	//   Initializes the vehicle part
	// Description:
	//   Will initialize a newly allocated instance of the vehicle part class
	//   according to the parameter table which is passed. This function should only be called once.
	// Parameters:
	//   pVehicle - pointer to the vehicle instance which will own this part
	//   table - script table which hold all the part parameters
	//   pParent - pointer to a parent part, or NULL if there isn't any parent
	//             part specified
	// Return value:
	//   A boolean which indicate if the function succeeded
	//virtual bool Init(IVehicle* pVehicle, const SmartScriptTable &table, IVehiclePart* pParent = NULL) = 0;

	virtual void PostInit() = 0;

	virtual void PostSerialize() = 0;

	// Summary:
	//   Resets the vehicle part
	// Description:
	//   Used to reset a vehicle part to its initial state. Is usually being
	//   called when the Editor enter and exit the game mode.
	virtual void Reset() = 0;

	// Summary:
	//   Releases the vehicle part
	// Description:
	//   Used to release a vehicle part. A vehicle part implementation should
	//   then deallocate any memory or object that it dynamically allocated.
	//   In addition, the part implementation or its base class should include
	//   "delete this;".
	virtual void Release() = 0;

	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;

	// Summary
	//   Retrieves a pointer of the parent part
	// Return Value
	//   A pointer to the parent part, or NULL if there isn't any.
	virtual IVehiclePart* GetParent(bool root = false) = 0;

	// Summary
	//   Retrieves the name of the part
	virtual const char* GetName() = 0;

	// Summary
	//   Retrieves the entity which hold the vehicle part
	// Description
	//   In most of the cases, the entity holding the vehicle part will be the vehicle entity itself.
	virtual IEntity* GetEntity() = 0;

	// Summary:
	//   Sends an event message to the vehicle part
	// Description:
	//   Used to send different events to the vehicle part implementation. The
	//   EVehiclePartEvent enum lists several usual vehicle part events.
	// Parameters:
	//   event - event to be passed to the part
	virtual void OnEvent(const SVehiclePartEvent& event) = 0;

	// Summary:
	//   Loads the geometry/character needed by the vehicle part
	virtual bool ChangeState(EVehiclePartState state, int flags = 0) = 0;

	// Summary:
	//   Query the current state of the vehicle part
	virtual EVehiclePartState GetState() const = 0;

	// Summary:
	//   Sets material on the part
	virtual void SetMaterial(IMaterial* pMaterial) = 0;

	// Summary:
	//   Query the current state of the vehicle part
	// Description:
	//   Obsolete. Will be changed soon in favor of an event passed by OnEvent.
	virtual void Physicalize() = 0;

	// Summary:
	//   Gets final local transform matrix
	// Description:
	//   Will return the FINAL local transform matrix (with recoil etc) relative to parent part or vehicle space
	// Return value:
	//   a 3x4 matrix
	virtual Matrix34 GetLocalTM(bool relativeToParentPart, bool forced = false) = 0;

	// Summary:
	//   Gets the local base transform matrix
	// Description:
	//   Will return the local BASE transform matrix (without recoil etc) relative to parent part
	virtual Matrix34 GetLocalBaseTM() = 0;

	// Summary:
	//   Gets the initial base transform matrix
	// Description:
	//   Will return the local transform matrix from the initial state of the model as relative to parent part
	virtual Matrix34 GetLocalInitialTM() = 0;

	// Summary:
	//   Gets a world transform matrix
	// Description:
	//   Will return a transform matrix world space.
	// Return value:
	//   a 3x4 matrix
	virtual Matrix34 GetWorldTM() = 0;

	// Summary:
	//   Sets local transformation matrix relative to parent part
	// Description:
	//   This sets the FINAL local tm only. Usually you'll want to use SetLocalBaseTM
	virtual void SetLocalTM(const Matrix34& localTM) = 0;

	// Summary:
	//   Sets local base transformation matrix relative to parent part
	// Description:
	//   This sets the local base tm.
	virtual void SetLocalBaseTM(const Matrix34& localTM) = 0;

	// Summary:
	//   Gets a local bounding box
	// Description:
	//   Will return a local transform matrix relative to the vehicle space.
	// Return value:
	//   a 3x4 matrix
	virtual const AABB& GetLocalBounds() = 0;

	// Summary:
	//   Obsolete function
	// Description:
	//   Obsolete. Will be changed soon.
	virtual void RegisterSerializer(IGameObjectExtension* pGameObjectExt) = 0;

	// Summary:
	//   Retrieve type id
	virtual int GetType() = 0;

	// Summary:
	//   Retrieve IVehicleWheel interface, or NULL
	virtual IVehicleWheel* GetIWheel() = 0;

	// Summary:
	//   Add part to child list
	// Description:
	//   Used for part implementations that needs to keep track of their children
	virtual void AddChildPart(IVehiclePart* pPart) = 0;

	// Summary:
	//   Invalidate local transformation matrix
	virtual void InvalidateTM(bool invalidate) = 0;

	// Summary:
	//   Sets part as being moveable and/or rotateable
	virtual void             SetMoveable(bool allowTranslationMovement = false) = 0;

	virtual const Vec3&      GetDetachBaseForce() = 0;
	virtual float            GetMass() = 0;
	virtual int              GetPhysId() = 0;
	virtual int              GetSlot() = 0;

	virtual int              GetIndex() const = 0;

	virtual TVehicleObjectId GetId() const override = 0;
	virtual void             Update(const float deltaTime) override = 0;
	virtual void             Serialize(TSerialize ser, EEntityAspects aspects) override = 0;
};

// Summary:
//   Damage behavior events
// Description:
//   Values used by the OnDamageEvent function
enum EVehicleDamageBehaviorEvent
{
	// Used to transmit the hit values that the vehicle component took
	eVDBE_Hit = 0,
	// repair event
	eVDBE_Repair,
	// Sent once when max damage ratio is exceeded
	eVDBE_MaxRatioExceeded,
	// Obsolete, eVDBE_Hit should be used instead
	eVDBE_ComponentDestroyed,
	// Obsolete, eVDBE_Hit should be used instead
	eVDBE_VehicleDestroyed,
};

// Summary
//   Interface used to define different areas
// Description
//    The most important use for the vehicle components is to define different
//    region on the vehicle which have specific uses. A good example would be
//    to have components on the vehicles which react to hit damage in different
//    ways.
struct IVehicleComponent
{
	virtual ~IVehicleComponent(){}
	// Summary
	//   Gets the name of the component
	// Returns
	//   A c style string with the name of the component.
	virtual const char* GetComponentName() const = 0;

	// Summary
	//   Gets the number of vehicle parts which are linked to the vehicle
	// Returns
	//   The number of parts
	// See Also
	//   GetPart
	virtual unsigned int  GetPartCount() const = 0;

	virtual IVehiclePart* GetPart(unsigned int index) const = 0;

	// Summary:
	//    Get bounding box in vehicle space
	virtual const AABB& GetBounds() = 0;

	// Summary:
	//  Get current damage ratio
	virtual float GetDamageRatio() const = 0;

	// Summary:
	//	Set current damage ratio
	virtual void SetDamageRatio(float ratio) = 0;

	// Summary:
	//  Get max damage
	virtual float GetMaxDamage() const = 0;

	virtual void  GetMemoryUsage(ICrySizer* s) const = 0;
};

// Summary
//   Handles animations on the vehicle
struct IVehicleAnimationComponent
{
	enum EPriority
	{
		ePriority_SeatDefault = 1,
		ePriority_SeatTransition,
	};

	virtual ~IVehicleAnimationComponent(){}

	virtual IActionController* GetActionController() = 0;
	virtual CTagState*         GetTagState() = 0;
	virtual bool               IsEnabled() const = 0;
};

struct SVehicleDamageBehaviorEventParams
{
	EntityId           shooterId;
	Vec3               localPos;
	float              radius;
	float              hitValue;
	int                hitType;
	float              componentDamageRatio;
	float              randomness;
	IVehicleComponent* pVehicleComponent;

	SVehicleDamageBehaviorEventParams()
	{
		shooterId = 0;
		localPos.zero();
		radius = 0.f;
		hitValue = 0.f;
		hitType = 0;
		componentDamageRatio = 0.f;
		randomness = 0.f;
		pVehicleComponent = 0;
	}

	void Serialize(TSerialize ser, IVehicle* pVehicle)
	{
		ser.Value("shooterId", shooterId);
		ser.Value("localPos", localPos);
		ser.Value("radius", radius);
		ser.Value("hitValue", hitValue);
		ser.Value("componentDamageRatio", componentDamageRatio);
		ser.Value("randomness", randomness);
		string name;
		if (ser.IsWriting())
			name = pVehicleComponent->GetComponentName();
		ser.Value("componentName", name);
		if (ser.IsReading())
			pVehicleComponent = pVehicle->GetComponent(name.c_str());
	}
};

// Summary:
//   Vehicle Damage Behavior interface
// Description:
//   Interface used to implement a damage behavior for vehicles.
struct IVehicleDamageBehavior
	: public IVehicleObject
{
	// Summary:
	//   Initializes the damage behavior
	// Description:
	//   Will initialize a newly allocated instance of the damage behavior class
	//   according to the parameter table which is passed. This function should
	//   only be called once.
	// Parameters:
	//   pVehicle - pointer to the vehicle instance for which will own this
	//              damage behavior
	//   table - script table which hold all the parameters
	// Return value:
	//   A boolean which indicate if the function succeeded
	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table) = 0;

	// Summary:
	//   Resets the damage behavior
	// Description:
	//   Used to reset a damage behavior to its initial state. Is usually being
	//   called when the Editor enter and exit the game mode.
	virtual void Reset() = 0;

	// Summary:
	//   Releases the damage behavior
	// Description:
	//   Used to release a damage behavior, usually at the same time than the
	//   vehicle is being released. A damage behavior implementation should
	//   then deallocate any memory or object that it dynamically allocated.
	//   In addition, the part implementation or its base class should include
	//   "delete this;".
	virtual void             Release() = 0;

	virtual void             GetMemoryUsage(ICrySizer* pSizer) const = 0;
	virtual TVehicleObjectId GetId() const override = 0;
	virtual void             Serialize(TSerialize ser, EEntityAspects aspects) override = 0;
	virtual void             Update(const float deltaTime) override = 0;

	// Summary:
	//   Sends an event message to the damage behavior
	// Description:
	//   Used to send different events to the damage behavior implementation.
	//   The EVehicleDamageBehaviorEvent enum lists all the event which a damage
	//   behavior can receive.
	// Parameters:
	//   event - event type
	//   value - a float value used for the event
	virtual void OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& params) = 0;
};

// Summary
//   Interface used to implement interactive elements for the player passenger
// Description
//   The tank turret, headlights, vehicle weapons and steering wheels are all
//   examples of vehicle seat actions.
struct IVehicleSeatAction
	: public IVehicleObject
{
	virtual bool             Init(IVehicle* pVehicle, IVehicleSeat* pSeat, const CVehicleParams& table) = 0;
	virtual void             OnSpawnComplete() {}
	virtual void             Reset() = 0;
	virtual void             Release() = 0;

	virtual void             StartUsing(EntityId passengerId) = 0;
	virtual void             ForceUsage() = 0;
	virtual void             StopUsing() = 0;
	virtual void             OnAction(const TVehicleActionId actionId, int activationMode, float value) = 0;

	virtual void             PrePhysUpdate(const float dt) {}

	virtual void             GetMemoryUsage(ICrySizer* pSizer) const = 0;
	// IVehicleObject
	virtual TVehicleObjectId GetId() const override = 0;
	virtual void             Serialize(TSerialize ser, EEntityAspects aspects) override = 0;
	virtual void             PostSerialize() = 0;
	virtual void             Update(const float deltaTime) override = 0;
	// ~IVehicleObject

	virtual const char* GetName() const { return m_name.c_str(); }

	string m_name;
};

// Summary
//   Handles animations on the vehicle model
struct IVehicleAnimation
{
	virtual ~IVehicleAnimation(){}
	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table) = 0;
	virtual void Reset() = 0;
	virtual void Release() = 0;

	// Summary
	//   Triggers the animation to start
	// Returns
	//   A bool which indicates the success
	virtual bool StartAnimation() = 0;

	// Summary
	//   Triggers the animation to stop
	virtual void StopAnimation() = 0;

	// Summary
	//   Performs a change of state
	// Parameters
	//   stateId - An id of the state
	// Remarks
	//   The function GetStateId can return an appropriate stateId which can be
	//   used as parameter for this function.
	// Returns
	//   A bool which indicates if the state could be used
	// See Also
	//   GetState
	virtual bool ChangeState(TVehicleAnimStateId stateId) = 0;

	// Summary
	//   Returns the current animation state
	// Returns
	//   A stateId of the current animation state in use
	// See Also
	//   ChangeState
	virtual TVehicleAnimStateId GetState() = 0;

	virtual string              GetStateName(TVehicleAnimStateId stateId) = 0;
	virtual TVehicleAnimStateId GetStateId(const string& name) = 0;

	// Summary
	//   Changes the speed of the animation
	// Parameters
	//   speed - a value between 0 to 1
	virtual void SetSpeed(float speed) = 0;

	// Summary
	//   Returns the current time in the animation
	// Returns
	//   a value usually between 0 to 1
	virtual float GetAnimTime(bool raw = false) = 0;

	virtual void  ToggleManualUpdate(bool isEnabled) = 0;
	virtual bool  IsUsingManualUpdates() = 0;

	virtual void  SetTime(float time, bool force = false) = 0;

	virtual void  GetMemoryUsage(ICrySizer* pSizer) const = 0;
};

struct IVehicleDamagesGroup
{
	virtual ~IVehicleDamagesGroup(){}
	virtual bool ParseDamagesGroup(const CVehicleParams& table) = 0;
};

struct IVehicleDamagesTemplateRegistry
{
	virtual ~IVehicleDamagesTemplateRegistry(){}
	virtual bool Init(const string& damagesTemplatesPath) = 0;
	virtual void Release() = 0;

	virtual bool RegisterTemplates(const string& filename) = 0;
	virtual bool UseTemplate(const string& templateName, IVehicleDamagesGroup* pDamagesGroup) = 0;
};

struct IVehicleIterator
{
	virtual ~IVehicleIterator(){}
	virtual size_t    Count() = 0;
	virtual IVehicle* Next() = 0;
	virtual void      AddRef() = 0;
	virtual void      Release() = 0;
};
typedef _smart_ptr<IVehicleIterator> IVehicleIteratorPtr;

// Summary:
//   Vehicle System interface
// Description:
//   Interface used to implement the vehicle system.
struct IVehicleSystem
{
	DECLARE_GAMEOBJECT_FACTORY(IVehicleMovement);
	DECLARE_GAMEOBJECT_FACTORY(IVehicleView);
	DECLARE_GAMEOBJECT_FACTORY(IVehiclePart);
	DECLARE_GAMEOBJECT_FACTORY(IVehicleDamageBehavior);
	DECLARE_GAMEOBJECT_FACTORY(IVehicleSeatAction);
	DECLARE_GAMEOBJECT_FACTORY(IVehicleAction);

	virtual ~IVehicleSystem(){}
	virtual bool Init() = 0;
	virtual void Release() = 0;
	virtual void Reset() = 0;

	// Summary
	//   Performs the registration of all the different vehicle classes
	// Description
	//   The vehicle system will read all the xml files which are inside the
	//   directory "Scripts/Entities/Vehicles/Implementations/Xml/". All the
	//   valid vehicle classes will be registered as game object extension with
	//   IGameObjectSystem.
	//
	//   Several default vehicle parts, views, actions and
	//   damage behaviors classes are registered in this function as well.
	// Parameters
	//   pGameFramework - A pointer to the game framework
	virtual void RegisterVehicles(IGameFramework* pGameFramework) = 0;

	// Summary:
	//   Spawns a new vehicle instance
	// Description:
	//   Will spawn an entity based on the vehicle class requested. Usually
	//   called by the entity system when a vehicle entity is spawned.
	// Arguments:
	//   channelId - Id of the intented channel
	//   name - Name of the new vehicle instance to be spawned
	//   vehicleClass - Name of the vehicle class requested
	//   pos - Position of the new vehicle instance
	//   rot - Rotation of the new vehicle instance
	//   scale - Scale factor of the new vehicle instance
	//   id - Unused
	// Returns:
	//   A pointer to the correct vehicle proxy. The value 0 is returned in case
	//   that the specified vehicle couldn't be created.
	virtual IVehicle* CreateVehicle(uint16 channelId, const char* name, const char* vehicleClass, const Vec3& pos, const Quat& rot, const Vec3& scale, EntityId id = 0) = 0;

	// Summary:
	//   Gets the Vehicle proxy of an entity
	// Description:
	//   Will return the correct vehicle proxy of a vehicle entity.
	// Returns:
	//   A pointer to the correct vehicle proxy. The value 0 is returned in case
	//   that the proxy wasn't found.
	virtual IVehicle*                        GetVehicle(EntityId entityId) = 0;

	virtual IVehicle*                        GetVehicleByChannelId(uint16 channelId) = 0;

	virtual bool                             IsVehicleClass(const char* name) const = 0;

	virtual IVehicleMovement*                CreateVehicleMovement(const string& name) = 0;
	virtual IVehicleView*                    CreateVehicleView(const string& name) = 0;
	virtual IVehiclePart*                    CreateVehiclePart(const string& name) = 0;
	virtual IVehicleDamageBehavior*          CreateVehicleDamageBehavior(const string& name) = 0;
	virtual IVehicleSeatAction*              CreateVehicleSeatAction(const string& name) = 0;
	virtual IVehicleAction*                  CreateVehicleAction(const string& name) = 0;

	virtual IVehicleDamagesTemplateRegistry* GetDamagesTemplateRegistry() = 0;

	virtual void                             GetVehicleImplementations(SVehicleImpls& impls) = 0;
	virtual bool                             GetOptionalScript(const char* vehicleName, char* buf, size_t len) = 0;

	// Summary
	//   Registers a newly spawned vehicle in the vehicle system
	virtual void AddVehicle(EntityId entityId, IVehicle* pProxy) = 0;

	// Summary
	//   Registers a newly removed vehicle in the vehicle system
	virtual void             RemoveVehicle(EntityId entityId) = 0;

	virtual TVehicleObjectId AssignVehicleObjectId() = 0;
	virtual TVehicleObjectId AssignVehicleObjectId(const string& className) = 0;
	virtual TVehicleObjectId GetVehicleObjectId(const string& className) const = 0;

	// Summary
	//   Used to get the count of all vehicle instance
	// Returns
	//   The count of all the vehicle instance created by the vehicle system
	virtual uint32              GetVehicleCount() = 0;
	virtual IVehicleIteratorPtr CreateVehicleIterator() = 0;

	// Summary
	//   Returns the vehicle client class implementation
	// Returns
	//   A pointer to the current vehicle client implementation, or null
	//   if none as been registrated
	// See Also
	//   RegisterVehicleClient
	virtual IVehicleClient* GetVehicleClient() = 0;

	// Summary
	//   Performs the registration of vehicle client implementation
	// Parameters
	//   pVehicleClient - a pointer to the vehicle client implementation
	// Notes
	//   Only one vehicle client implementation can be registrated
	// See Also
	//   GetVehicleClient
	virtual void RegisterVehicleClient(IVehicleClient* pVehicleClient) = 0;

	// Summary
	//   Performs the registration of a class which wants to know when a specific player starts to use a vehicle
	// Parameters
	//   playerId  - the entity id of the player to listen for.
	//   pListener - a pointer to a listener object.
	// See Also
	//   IVehicle::RegisterVehicleEventListener()
	virtual void RegisterVehicleUsageEventListener(const EntityId playerId, IVehicleUsageEventListener* pEventListener) = 0;

	// Summary
	//   Unregisters class listening to the current player's vehicle.
	// Parameters
	//   playerId  - the entity id of the player that the listener is registered for.
	//   pListener - the pointer to be removed from the list of callbacks.
	// See Also
	//   IVehicle::RegisterVehicleEventListener()
	virtual void UnregisterVehicleUsageEventListener(const EntityId playerId, IVehicleUsageEventListener* pEventListener) = 0;

	// Summary
	//   Announces OnUse events for vehicles.
	// Parameters
	//   eventID   - One of the events declared in EVehicleEvent, currently only eVE_PassengerEnter and eVE_PassengerExit
	//   playerId  - the entity id of the player that the listener is registered for.
	//   pVehicle  - the vehicle which the player is interacting.
	// See Also
	//   RegisterVehicleUsageEventListener
	//   UnregisterVehicleEventListener
	virtual void BroadcastVehicleUsageEvent(const EVehicleEvent eventId, const EntityId playerId, IVehicle* pVehicle) = 0;

	virtual void Update(float deltaTime) = 0;
};

enum EVehicleDebugDraw
{
	eVDB_General       = 1,
	eVDB_Particles     = 2,
	eVDB_Parts         = 3,
	eVDB_View          = 4,
	eVDB_Sounds        = 5,
	eVDB_PhysParts     = 6,
	eVDB_PhysPartsExt  = 7,
	eVDB_Damage        = 8,
	eVDB_ClientPredict = 9,
	eVDB_Veed          = 10,
};

// Summary
//   Defines a position on a vehicle
// Description
//   The vehicle helper object is used to define a position on a vehicle.
//   Several components of the vehicle system use this object type to define
//   position on the vehicle (ie: seat position, firing position on vehicle
//   weapons). The Matrices are updated to reflect any changes in the position
//   or orientation parent part.
struct IVehicleHelper
{
	virtual ~IVehicleHelper(){}
	// Summary
	//   Releases the VehicleHelper instance
	// Notes
	//   Should usually be only used by the implementation of IVehicle
	virtual void Release() = 0;

	// Summary
	//   Returns the helper matrix in local space
	// Returns
	//   The Matrix34 holding the local space matrix
	// Notes
	//   The matrix is relative to the parent part
	// See Also
	//   GetParentPart, GetVehicleTM, GetWorldTM
	virtual const Matrix34& GetLocalTM() const = 0;

	// Summary
	//   Returns the helper matrix in vehicle space
	// Returns
	//   The Matrix34 holding the vehicle space matrix
	// See Also
	//   GetLocalTM, GetWorldTM
	virtual void GetVehicleTM(Matrix34& vehicleTM, bool forced = false) const = 0;

	// Summary
	//   Returns the helper matrix in world space
	// Returns
	//   The Matrix34 holding the world space matrix
	// See Also
	//   GetLocalTM, GetVehicleTM, GetReflectedWorldTM
	virtual void GetWorldTM(Matrix34& worldTM) const = 0;

	// Summary
	//	Returns the helper matrix in world space after reflecting the
	//	local translation across the yz plane (for left/right side of vehicle)
	// Returns
	//	The Matrix34 holding the world space matrix
	// See Also
	//	GetLocalTM, GetVehicleTM, GetWorldTM
	virtual void GetReflectedWorldTM(Matrix34& reflectedWorldTM) const = 0;

	// Summary
	//	Returns the helper translation in local space
	// Notes
	//   The matrix is relative to the parent part
	virtual Vec3 GetLocalSpaceTranslation() const = 0;

	// Summary
	//	Returns the helper translation in vehicle space
	virtual Vec3 GetVehicleSpaceTranslation() const = 0;

	// Summary
	//	Returns the helper translation in world space
	virtual Vec3 GetWorldSpaceTranslation() const = 0;

	// Summary
	//   Returns the parent part of the helper
	// Returns
	//   A pointer to the parent part, which is never null
	virtual IVehiclePart* GetParentPart() const = 0;

	// tmp(will be pure virtual soon)
	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;
};

// Summary
//   Handles game specific client/player tasks
// Description
//   The implementation of the vehicle client class is used to perform
//   game specific tasks related to the client player. For example,
//   it can be used to store the last view used by the player on specific
//   vehicle class or to filter game specific action types into vehicle
//   ActionIds.
// Remarks
//   Should be implemented inside the Game DLL. The game should call the
//   function IVehicleSystem::RegisterVehicleClient during intialization.
struct IVehicleClient
{
	virtual ~IVehicleClient(){}
	// Summary
	//   Initializes the vehicle client implementation
	// Returns
	//   A bool representing the success of the initialization
	virtual bool Init() = 0;

	virtual void Reset() = 0;

	// Summary
	//   Releases the vehicle client implementation
	// Note
	//   Should only be called when the game is shutdown
	virtual void Release() = 0;

	// Summary
	//   Filters client actions
	// Parameter
	//   pVehicle - vehicle instance used by the client
	//   actionId - ActionId sent
	//   activationMode - One of the different activation mode defined in EActionActivationMode
	//   value - value of the action
	virtual void OnAction(IVehicle* pVehicle, EntityId actorId, const ActionId& actionId, int activationMode, float value) = 0;

	// Summary
	//   Perform game specific tasks when the client enter a vehicle seat
	// Parameters
	//   pSeat - instance of the new vehicle seat assigned to the client
	// Note
	//   Is also called when the client switch to a different seats inside the
	//   same vehicle
	virtual void OnEnterVehicleSeat(IVehicleSeat* pSeat) = 0;

	// Summary
	//   Perform game specific tasks when the client exit a vehicle seat
	// Parameters
	//   pSeat - instance of the new vehicle seat exited by the client
	// Note
	//   Is also called when the client switch to a different seats inside the
	//   same vehicle
	virtual void OnExitVehicleSeat(IVehicleSeat* pSeat) = 0;
};
