// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/****************************************************************************************
----------------------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements common physics movement code for the helicopters

----------------------------------------------------------------------------------------
History:
- Created: Stan Fichele

****************************************************************************************/

#ifndef __VEHICLE_PHYSICS_HELIcopter__H__
#define __VEHICLE_PHYSICS_HELIcopter__H__

struct SHelicopterAction
{
	float rawInputUp;
	float rawInputDown;
	float upDown;
	float forwardBack;
	float leftRight;
	float yaw;

	void Reset()
	{
		rawInputUp=0.f;
		rawInputDown=0.f;
		upDown=0.f;
		forwardBack=0.f;
		leftRight=0.f;
		yaw=0.f;
	}
};

struct SHelicopterHandling
{
	SHelicopterHandling()
	{
		acceleration=0.f;
		accelerationStrength=0.f;
		maxSpeedForward=0.f;
		maxSpeedBackward=0.f;
		maxSpeedLeftRight=0.f;
		maxSpeedUpDown=0.f;
		maxYawSpeed=0.f;
		autoPitchForward=0.f;
		autoPitchBack=0.f;
		autoRoll=0.f;
		autoPitchChangeSpeed=0.f;
		alignToForwardVel = 0.f;
		gravityCorrection=1.f;
		horizontalFraction=1.f;
		damping=0.f;
		angDamping=0.f;
		instability=0.f;
		angInstability=0.f;
		aiYawResponse=0.f;
	}

	float acceleration;
	float accelerationStrength;
	float maxSpeedForward;
	float maxSpeedBackward;
	float maxSpeedLeftRight;
	float maxSpeedUpDown;
	float maxYawSpeed;
	float autoPitchForward;      // Auto pitch angle when moving forward at full speed
	float autoPitchBack;         // Auto pitch angle when moving backward at full speed
	float autoRoll;              // Auto roll/tilt angle when strafing at full speed
	float autoPitchChangeSpeed;  // How fast the tilt can change
	float alignToForwardVel;     // 0.0 autoPitchForward is used to control pitch based on horizontal forward velocity. 1.0 the helicopter will align with the velocity (like a plane)
	float damping;               // Damping strength
	float angDamping;            // Damping strength
	float instability;           // How much to oscillate when "stationary" in the air
	float angInstability;        // How much to oscillate when "stationary" in the air
	float gravityCorrection;     // How mush to compensate gravity -> 0-1.0
	float horizontalFraction;    // Are the lateral forces horizontal (1.0) or directed along the helicpoter's prinicple axes
	float aiYawResponse;
};

struct SHelicopterChassis
{
	Vec3 angVel;
	Vec3 targetZ;
	float timer;
	Vec3 angVelAdded;
};

struct SVehiclePhysicsStatus;

struct SVehiclePhysicsHelicopterProcessParams
{
	IPhysicalEntity* pPhysics;
	SVehiclePhysicsStatus* pPhysStatus;
	SHelicopterAction* pInputAction;
	float dt;
	bool haveDriver;
	bool isAI;
	Vec3 aiRequiredVel;

	SVehiclePhysicsHelicopterProcessParams()
	{
		pPhysics=NULL;
		pPhysStatus=NULL;
		pInputAction=NULL;
		dt = 0.f;
		haveDriver = false;
		isAI = false;
		aiRequiredVel = Vec3(0.f);
	}
};

struct SVehiclePhysicsHelicopterProcessAIParams
{
	SVehiclePhysicsStatus* pPhysStatus;
	SHelicopterAction* pInputAction;
	CMovementRequest* pAiRequest;
	float dt;
	float aiCurrentSpeed;
	float aiYawResponseScalar;
	Vec3 aiRequiredVel;

	SVehiclePhysicsHelicopterProcessAIParams()
	{
		pPhysStatus=NULL;
		pInputAction=NULL;
		pAiRequest = NULL;
		dt = 0.f;
		aiCurrentSpeed = 0.f;
		aiYawResponseScalar = 1.f;
		aiRequiredVel = Vec3(0.f);
	}
};


class CVehiclePhysicsHelicopter
{
public:
	CVehiclePhysicsHelicopter();
	~CVehiclePhysicsHelicopter();

	bool Init(IVehicle* pVehicle, const CVehicleParams& table);
	void PostInit();
	void Reset(IVehicle* pVehicle, IEntity* pEntity);
	
	// Process the AI requests (on the physics thread)
	void ProcessAI(SVehiclePhysicsHelicopterProcessAIParams& params);

	/// Process the physics movement (on the physics thread)
	void ProcessMovement(SVehiclePhysicsHelicopterProcessParams& params);

public:
	// Handling
	SHelicopterHandling m_handling;
	SHelicopterChassis m_chassis;
};


#endif // __VEHICLE_PHYSICS_HELIcopter__H__
