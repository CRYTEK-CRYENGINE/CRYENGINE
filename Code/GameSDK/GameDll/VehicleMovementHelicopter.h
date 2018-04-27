// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(__VEHICLEMOVEMENTHELICOPTER_H__)
#define __VEHICLEMOVEMENTHELICOPTER_H__

#include "VehicleMovementBase.h"
#include "Vehicle/VehicleUtils.h"
#include "Vehicle/VehiclePhysicsHelicopter.h"
#include "VehicleSystem/VehicleNoiseGenerator.h"

class CVehicleMovementHelicopter;
class CVehicleActionLandingGears;

class CNetworkMovementHelicopterCrysis2
{
public:
	// Defines for CVehicleNetActionSync<T>
	typedef CVehicleMovementHelicopter ParentObject;
	static const NetworkAspectType CONTROLLED_ASPECT = eEA_GameClientB;

	CNetworkMovementHelicopterCrysis2();

	void Read(CVehicleMovementHelicopter* pMovement);
	void Write(CVehicleMovementHelicopter* pMovement);
	void Serialize(TSerialize ser, EEntityAspects aspects);
	
	ILINE uint16 GetFlags() { return m_netSyncFlags; }
	ILINE void SetFlags(uint16 f) { m_netSyncFlags = f; }

public:
	enum {
		k_syncPos = 0x1,
		k_syncLookMoveSpeed = 0x2,
		k_controlPos = 0x100,   // If set, then this will control the position of the vehicle, rather than the low level physics serialiation (which shuould be off)
	};

private:
	Vec3 m_lookTarget;
	Vec3 m_moveTarget;
	Vec3 m_currentPos;
	Quat m_currentRot;
	float m_desiredSpeed;
	uint16 m_netSyncFlags;
};

// Noise/Turbulence helper
class CVTolNoise
{
public:
	CVTolNoise();
	void Init(const Vec3& amplitude, const Vec3& frequency, const Vec3& rotAmplitude, const Vec3& rotFrequency, const float& startDamageRatio);
	void Update(float dt);

public:
	CVehicleNoiseValue3D m_posGen;
	CVehicleNoiseValue3D m_rotGen;

	// Settings
	Vec3 m_amplitude;
	Vec3 m_frequency;
	Vec3 m_rotAmplitude;
	Vec3 m_rotFrequency;

	Vec3 m_pos;
	Vec3 m_rot;
	Vec3 m_posDifference;
	Vec3 m_angDifference;

	float m_startDamageRatio;

	bool m_bEnabled;
};


class CVehicleMovementHelicopter
	: public CVehicleMovementBase
{
public:

	friend class CNetworkMovementHelicopterCrysis2;

	CVehicleMovementHelicopter();
	virtual ~CVehicleMovementHelicopter() {}

	// IVehicleMovement
	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table);
	virtual void PostInit();
	virtual void Reset();
	virtual void Release();
	virtual void Physicalize();

	virtual EVehicleMovementType GetMovementType() { return eVMT_Air; }
	virtual float GetDamageRatio() { return m_damage; }

	virtual bool StartDriving(EntityId driverId);
	virtual void StopDriving();

	virtual void OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params);
	virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value);

	virtual void ProcessMovement(const float deltaTime);
	virtual void ProcessActions(const float deltaTime);
	virtual void ProcessAI(const float deltaTime);

	virtual void Update(const float deltaTime);

	virtual bool RequestMovement(CMovementRequest& movementRequest);

	virtual void Serialize(TSerialize ser, EEntityAspects aspects);
	virtual void PostSerialize();
	virtual void SetAuthority( bool auth ) { m_netActionSync.ChangeAuthority(); }
	virtual void SetChannelId(uint16 id) {};

	virtual float GetAdditionalSlopePitch(const Vec3 &desiredMoveDir) const;

	// CVehicleMovementBase
	virtual float GetEnginePedal();

	void SetRotorPart(IVehiclePart* pPart) { m_pRotorPart = pPart; }

	virtual void PreProcessMovement(const float deltaTime);
	virtual void ResetActions();
	virtual void UpdateDamages(float deltaTime);
	virtual void UpdateEngine(float deltaTime);
	virtual void ProcessActions_AdjustActions(float deltaTime) {}

	virtual void GetMemoryUsage(ICrySizer * pSizer) const; 
	void GetMemoryUsageInternal(ICrySizer * pSizer) const; 	

	virtual void ProcessActionsLift(float deltaTime);

	//CVehicleMovementHelicopter
	virtual void UpdateNetworkError(const Vec3& netPosition, const Quat& netRotation);

	ILINE void SetYawResponseScalar (float val) { m_yawResponseScalar = val; }

protected:

	void InitMovementNoise(const CVehicleParams& noiseParams, CVTolNoise& noise);

	float GetEnginePower();
	float GetDamageMult();

	void  OccupyWeaponSeats();
	virtual float GetRollSpeed() const;
	virtual void CalculatePitch(const Ang3& worldAngles, const Vec3& desiredMoveDir, float currentSpeed2d, float desiredSpeed, float deltaTime);

	SHelicopterAction m_inputAction;
	CVehiclePhysicsHelicopter m_arcade;

	// animation stuff
	IVehicleAnimation			*m_HoverAnim;
	IVehiclePart				*m_pRotorPart;
	IVehicleHelper				*m_pRotorHelper;

	CVehicleActionLandingGears* m_pLandingGears;

	// movement settings (shouldn't be changed outside Init())
	float						m_engineWarmupDelay;

	// high-level controller abilities
	float 						m_yawPerRoll;

	// ramps from 0 to m_enginePowerMax during idle-up
	float						  m_enginePower;
	float						  m_enginePowerMax;
	float						  m_EnginePerformance;

	// movement states (shouldn't be changed outside ProcessMovement())
	float						  m_damageActual;

	// movement actions (to be modified by AI and player actions) and correspond 
	// to low-level "stick" inputs in the range -1 to 1
	float						  m_actionPitch;
	float						  m_actionYaw;
	float						  m_actionRoll;

	float						  m_maxRollAngle;
	float						  m_maxPitchAngle;

	float 						m_rollDamping;

	float						  m_yawResponseScalar;

	float						  m_CurrentSpeed;
	Vec3						  m_CurrentVel;
	Ang3              m_steeringDamage;

	bool							m_bExtendMoveTarget;
	bool							m_bApplyNoiseAsVelocity;

	// ai
	CMovementRequest			m_aiRequest;

	// Network related
	float m_sendTime;
	float m_sendTimer;
	NetworkAspectType m_updateAspects;
	Vec3 m_netPosAdjust;
	CVehicleNetActionSync<CNetworkMovementHelicopterCrysis2> m_netActionSync;

	// Network error adjustment settings
	static float k_netErrorPosScale;

	CVTolNoise m_defaultNoise;
	CVTolNoise* m_pNoise;
};


#endif // #if !defined(__VEHICLEMOVEMENTHELICOPTERCRYSIS2_H__)
