// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a standard wheel based vehicle movements

-------------------------------------------------------------------------
History:
- Created : stan fichele, an attempt to provide different
            handling from the engine default

*************************************************************************/

#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"
#include <CryAction/IDebugHistory.h>

#include "VehicleMovementArcadeWheeled.h"

#include "IVehicleSystem.h"
#include "Network/NetActionSync.h"
#include <CryAISystem/IAgent.h>
#include <CryGame/GameUtils.h>
#include <CryGame/IGameTokens.h>
#include "Player.h"
#include "NetInputChainDebug.h"

DEFINE_SHARED_PARAMS_TYPE_INFO(CVehicleMovementArcadeWheeled::SSharedParams);

#define THREAD_SAFE 1
#define LOAD_RAMP_TIME 0.2f
#define LOAD_RELAX_TIME 0.25f


/*
============================================================================================================================
	- MISC PHYSICS - 
============================================================================================================================
*/

#ifndef XYZ
#define XYZ(v) (v).x, (v).y, (v).z
#endif

struct ClampedImpulse
{
	float min;
	float max;
	float applied;
};

static void clampedImpulseInit(ClampedImpulse* c, float mn, float mx)
{
	c->applied = 0.f;
	c->max = mx;	// max should be >= 0.f
	c->min = mn;	// min should be <= 0.f
}

static void clampedImpulseInit(ClampedImpulse* c, float mx)
{
	c->applied = 0.f;
	c->max = +fabsf(mx);
	c->min = -fabsf(mx);
}

static float clampedImpulseApply(ClampedImpulse* c, float impulse)
{
	float prev = c->applied;
	float total = c->applied + impulse;
	c->applied = clamp_tpl(total, c->min, c->max);
	return c->applied - prev;
}

static ILINE float computeDenominator(float invMass, float invInertia, const Vec3& offset, const Vec3& norm)
{
    // If you apply an impulse of 1.0f in the direction of 'norm'
    // at position specified by 'offset' then the point will change
    // velocity by the amount calculated here
	Vec3 cross = offset.cross(norm);
	cross = cross * invInertia;	// Assume sphere inertia for now
	cross = cross.cross(offset);
    return norm.dot(cross) + invMass;
}

static ILINE void addImpulseAtOffset(Vec3& vel, Vec3& angVel, float invMass, float invInertia, const Vec3& offset, const Vec3& impulse)
{
	vel = vel + impulse * invMass;
	angVel = angVel + (offset.cross(impulse) * invInertia);
}

// Same as above but the angular change has been flattened into dW for a given offset and direction
static ILINE void addImpulse(Vec3& vel, Vec3& angVel, float invMass, const Vec3& dW, float impulse, const Vec3& dir)
{
	vel += (invMass * impulse)*dir;
	angVel += impulse * dW;
}

struct Vehicle3Settings
{
	bool useNewSystem;
	Vehicle3Settings()
	{
		useNewSystem = false;
	}
};

/*
============================================================================================================================
	- CVehicleMovementArcadeWheeled -
============================================================================================================================
*/
CVehicleMovementArcadeWheeled::CVehicleMovementArcadeWheeled()
{
	m_iWaterLevelUpdate = 0;
	m_wheelStatusLock = 0;
	m_frictionStateLock = 0;

	m_passengerCount = 0;
	m_steerMax = 20.0f;
	m_speedSuspUpdated = -1.f;
	m_suspDamping = 0.f;
	m_stabi = 0.f;
	m_lastBump = 0.f;
	m_prevAngle = 0.0f;  
	m_lostContactTimer = 0.f;
	m_brakeTimer = 0.f;
	m_reverseTimer = 0.0f;
	m_tireBlownTimer = 0.f;
	m_boostEndurance = 7.5f;
	m_boostRegen = m_boostEndurance;
	m_boostStrength = 6.f;
	m_netActionSync.PublishActions( CNetworkMovementArcadeWheeled(this) );  	  
	m_blownTires = 0;
	m_bForceSleep = false;
	m_forceSleepTimer = 0.0f;
	m_submergedRatioMax = 0.0f;
	m_initialHandbreak = true;
	
	m_topSpeedFactor = 1.f;
	m_accelFactor = 1.f;
	
	m_stationaryHandbrake						= true;
	m_stationaryHandbrakeResetTimer	= 0.0f;

	m_handling.compressionScale = 1.f;

	m_chassis.radius = 1.f;

	m_damageRPMScale = 1.f;

	m_frictionState = k_frictionNotSet;
	m_netLerp = false;

	// Gear defaults
	
	m_gears.curGear = SVehicleGears::kNeutral;
	m_gears.curRpm = 0.f;
	m_gears.targetRpm = 0.f;
	m_gears.modulation = 0.f;
	m_gears.timer = 0.f;
	m_gears.averageWheelRadius = 1.f;

	m_handling.contactNormal.zero();
	m_largeObjectInfo.SetZero();
	m_largeObjectInfoWheelIdx = 0;
	m_largeObjectFrictionType = 0;

#if ENABLE_VEHICLE_DEBUG
	m_debugInfo = NULL;
	m_lastDebugFrame = 0;
#endif
}

//------------------------------------------------------------------------
CVehicleMovementArcadeWheeled::~CVehicleMovementArcadeWheeled()
{ 
	m_wheelStatusLock = 0;
	m_frictionStateLock = 0;
#if ENABLE_VEHICLE_DEBUG
	ReleaseDebugInfo();
#endif
}

//------------------------------------------------------------------------

namespace
{
	CVehicleParams GetWheeledTable( const CVehicleParams& table )
	{
		CVehicleParams wheeledLegacyTable = table.findChild("WheeledLegacy");

		if (!wheeledLegacyTable)
			return table.findChild("Wheeled");

		return wheeledLegacyTable;
	}
}

bool CVehicleMovementArcadeWheeled::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	if (!CVehicleMovementBase::Init(pVehicle, table))
	{
		assert(0);
		return false;
	}

	m_largeObjectInfo.SetZero();
	m_largeObjectInfoWheelIdx = 0;
	m_largeObjectFrictionType = 0;

	CryFixedStringT<256>	sharedParamsName;

	sharedParamsName.Format("%s::%s::CVehicleMovementArcadeWheeled", pVehicle->GetEntity()->GetClass()->GetName(), pVehicle->GetModification());

	ISharedParamsManager	*pSharedParamsManager = gEnv->pGameFramework->GetISharedParamsManager();

	CRY_ASSERT(pSharedParamsManager);

	m_pSharedParams = CastSharedParamsPtr<SSharedParams>(pSharedParamsManager->Get(sharedParamsName));

	if(!m_pSharedParams)
	{
		SSharedParams	sharedParams;

		const float boostSpeedScale = 1.5f;
		const float boostAccScale = 1.5f;

		sharedParams.isBreakingOnIdle      = false;
		sharedParams.surfaceFXInFpView     = false;
		sharedParams.steerSpeed            = 40.0f;
		sharedParams.steerSpeedMin         = 90.0f;
		sharedParams.kvSteerMax            = 10.0f;
		sharedParams.v0SteerMax            = 30.0f;
		sharedParams.steerSpeedScaleMin    = 1.0f;
		sharedParams.steerSpeedScale       = 0.8f;
		sharedParams.steerRelaxation       = 90.0f;
		sharedParams.vMaxSteerMax          = 20.f;
		sharedParams.pedalLimitMax         = 0.3f; 
		sharedParams.suspDampingMin        = 0.0f;
		sharedParams.suspDampingMax        = 0.0f;  
		sharedParams.suspDampingMaxSpeed   = 0.0f;
		sharedParams.damagedWheelSpeedInfluenceFactor = 0.5f;
		sharedParams.stabiMin              = 0.0f;
		sharedParams.stabiMax              = 0.0f;
		sharedParams.ackermanOffset        = 0.0f;
		sharedParams.gravity               = 9.81f;


		sharedParams.gears.ratios[0]                 = -1.0f;   // Reverse gear.
		sharedParams.gears.invRatios[0]              = -1.0f;   // Reverse gear.
		sharedParams.gears.ratios[1]                 = 0.0f;    // Neutral.
		sharedParams.gears.invRatios[1]              = 0.0f;    // Neutral.
		sharedParams.gears.ratios[2]                 = 1.0f;    // First gear.
		sharedParams.gears.invRatios[2]              = 1.0f;    // First gear.
		sharedParams.gears.numGears                  = 3;
		sharedParams.gears.rpmRelaxSpeed             = 2.0f;
		sharedParams.gears.rpmInterpSpeed            = 4.0f;
		sharedParams.gears.gearOscillationFrequency  = 5.0f;
		sharedParams.gears.gearOscillationAmp        = 1.0f;
		sharedParams.gears.gearOscillationAmp2       = 2.0f;
		sharedParams.gears.gearChangeSpeed           = 10.0f;   // This is effectively the inverse time it takes to do a gear change noise
		sharedParams.gears.gearChangeSpeed2          = 3.0f;    // This should always be lower than gearChangeSpeed
		sharedParams.gears.rpmLoadFactor             = 4.f;
		sharedParams.gears.rpmLoadChangeSpeedUp      = 1.f;
		sharedParams.gears.rpmLoadChangeSpeedDown    = 1.f;
		sharedParams.gears.rpmLoadDefaultValue       = 0.2f;
		sharedParams.gears.rpmLoadFromBraking        = 0.3f;
		sharedParams.gears.rpmLoadFromThrottle       = 0.3f;

		sharedParams.handling.acceleration                     = 5.0f;
		sharedParams.handling.boostAcceleration                = sharedParams.handling.acceleration * boostAccScale;
		sharedParams.handling.decceleration                    = 5.0f;
		sharedParams.handling.topSpeed                         = 10.0f;
		sharedParams.handling.boostTopSpeed                    = sharedParams.handling.topSpeed * boostSpeedScale;
		sharedParams.handling.reverseSpeed                     = 5.0f;
		sharedParams.handling.reductionAmount                  = 0.0f;
		sharedParams.handling.reductionRate                    = 0.0f;
		sharedParams.handling.compressionBoost                 = 0.0f;
		sharedParams.handling.compressionBoostHandBrake        = 0.0f;
		sharedParams.handling.backFriction                     = 100.f;
		sharedParams.handling.frontFriction                    = 100.f;
		sharedParams.handling.frictionOffset                   = -0.05f;
		sharedParams.handling.grip1                            = 0.8f;
		sharedParams.handling.grip2                            = 1.0f;
		sharedParams.handling.gripK                            = 1.0f;
		sharedParams.handling.accelMultiplier1                 = 1.0f;
		sharedParams.handling.accelMultiplier2                 = 1.0f;
		sharedParams.handling.handBrakeDecceleration           = 30.f;
		sharedParams.handling.handBrakeDeccelerationPowerLock  = 0.0f;
		sharedParams.handling.handBrakeLockFront               = true;
		sharedParams.handling.handBrakeLockBack                = true;
		sharedParams.handling.handBrakeFrontFrictionScale      = 1.0f;
		sharedParams.handling.handBrakeBackFrictionScale       = 1.0f;
		sharedParams.handling.handBrakeAngCorrectionScale      = 1.0f;
		sharedParams.handling.handBrakeLateralCorrectionScale  = 1.0f;

		sharedParams.stabilisation.angDamping                  = 3.0f;
		sharedParams.stabilisation.rollDamping                 = 10.0f;
		sharedParams.stabilisation.rollfixAir                  = 0.0f;
		sharedParams.stabilisation.upDamping                   = 1.0f;
		sharedParams.stabilisation.sinMaxTiltAngleAir          = sinf(DEG2RAD(25.f));
		sharedParams.stabilisation.cosMaxTiltAngleAir          = cosf(DEG2RAD(25.f));
		
		sharedParams.tankHandling.additionalSteeringStationary = 0.f;
		sharedParams.tankHandling.additionalSteeringAtMaxSpeed = 0.f;
		sharedParams.tankHandling.additionalTilt = 0.f;
		
		sharedParams.powerSlide.lateralSpeedFraction[0]        = 0.f;
		sharedParams.powerSlide.lateralSpeedFraction[1]        = 0.f;
		sharedParams.powerSlide.spring                         = 0.f;

		sharedParams.correction.lateralSpring   = 0.0f;
		sharedParams.correction.angSpring       = 0.0f;
		
		sharedParams.soundParams.bumpMinSusp              = 0.0f;
		sharedParams.soundParams.bumpMinSpeed             = 0.0f;
		sharedParams.soundParams.bumpIntensityMult        = 1.0f;
		sharedParams.soundParams.airbrakeTime             = 0.0f;

		sharedParams.soundParams.skidLerpSpeed            = 1.0f;
		sharedParams.soundParams.skidCentrifugalFactor    = 1.0f;
		sharedParams.soundParams.skidBrakeFactor          = 1.0f;
		sharedParams.soundParams.skidPowerLockFactor      = 1.0f;
		sharedParams.soundParams.skidForwardFactor        = 1.0f;
		sharedParams.soundParams.skidLateralFactor        = 1.0f;

		for (int i=0; i<2; i++)
		{
			sharedParams.viewAdjustment[i].maxLateralDisp              = 0.f;
			sharedParams.viewAdjustment[i].maxRotate                   = 0.f;
			sharedParams.viewAdjustment[i].lateralDisp                 = 0.f;
			sharedParams.viewAdjustment[i].lateralCent                 = 0.f;
			sharedParams.viewAdjustment[i].rotate                      = 0.f;
			sharedParams.viewAdjustment[i].rotateCent                  = 0.f;
			sharedParams.viewAdjustment[i].rotateSpeed                 = 1.f;
			sharedParams.viewAdjustment[i].dispSpeed                   = 1.f;
			sharedParams.viewAdjustment[i].maxForwardDisp              = 0.f;
			sharedParams.viewAdjustment[i].forwardResponse             = 0.f;
			sharedParams.viewAdjustment[i].vibrationAmpSpeed           = 0.f;
			sharedParams.viewAdjustment[i].vibrationSpeedResponse      = 0.f;
			sharedParams.viewAdjustment[i].vibrationAmpAccel           = 0.f;
			sharedParams.viewAdjustment[i].vibrationAccelResponse      = 0.f;
			sharedParams.viewAdjustment[i].vibrationFrequency          = 0.f;
			sharedParams.viewAdjustment[i].suspensionAmp               = 0.f;
			sharedParams.viewAdjustment[i].suspensionResponse          = 0.f;
			sharedParams.viewAdjustment[i].suspensionSharpness         = 0.f;
		}

		table.getAttr("isBreakingOnIdle", sharedParams.isBreakingOnIdle);
		table.getAttr("surfaceFXInFpView", sharedParams.surfaceFXInFpView);
		table.getAttr("steerSpeed", sharedParams.steerSpeed);
		table.getAttr("steerSpeedMin", sharedParams.steerSpeedMin);
		table.getAttr("kvSteerMax", sharedParams.kvSteerMax);
		table.getAttr("v0SteerMax", sharedParams.v0SteerMax);
		table.getAttr("steerSpeedScaleMin", sharedParams.steerSpeedScaleMin);
		table.getAttr("steerSpeedScale", sharedParams.steerSpeedScale);
		table.getAttr("steerRelaxation", sharedParams.steerRelaxation);
		table.getAttr("vMaxSteerMax", sharedParams.vMaxSteerMax);
		table.getAttr("pedalLimitMax", sharedParams.pedalLimitMax);
		table.getAttr("ackermanOffset", sharedParams.ackermanOffset);
		table.getAttr("gravity", sharedParams.gravity);

		if(CVehicleParams wheeledTable = GetWheeledTable(table))
		{
			wheeledTable.getAttr("suspDampingMin", sharedParams.suspDampingMin);
			wheeledTable.getAttr("suspDampingMax", sharedParams.suspDampingMax);
			wheeledTable.getAttr("suspDampingMaxSpeed", sharedParams.suspDampingMaxSpeed);
			wheeledTable.getAttr("stabiMin", sharedParams.stabiMin);
			wheeledTable.getAttr("stabiMax", sharedParams.stabiMax);
			wheeledTable.getAttr("damagedWheelSpeedInfluenceFactor", sharedParams.damagedWheelSpeedInfluenceFactor);
		}

		if(CVehicleParams soundParams = table.findChild("SoundParams"))
		{
			soundParams.getAttr("roadBumpMinSusp", sharedParams.soundParams.bumpMinSusp);
			soundParams.getAttr("roadBumpMinSpeed", sharedParams.soundParams.bumpMinSpeed);
			soundParams.getAttr("roadBumpIntensity", sharedParams.soundParams.bumpIntensityMult);
			soundParams.getAttr("airbrake", sharedParams.soundParams.airbrakeTime);
			
			if(CVehicleParams skidParams = soundParams.findChild("Skidding"))
			{
				skidParams.getAttr("skidLerpSpeed", sharedParams.soundParams.skidLerpSpeed);
				skidParams.getAttr("skidCentrifugalFactor", sharedParams.soundParams.skidCentrifugalFactor);
				skidParams.getAttr("skidBrakeFactor", sharedParams.soundParams.skidBrakeFactor);
				skidParams.getAttr("skidPowerLockFactor", sharedParams.soundParams.skidPowerLockFactor);
				skidParams.getAttr("skidForwardFactor", sharedParams.soundParams.skidForwardFactor);
				skidParams.getAttr("skidLateralFactor", sharedParams.soundParams.skidLateralFactor);
			}
			// Convert to a sensible scale
			sharedParams.soundParams.skidForwardFactor *= 0.1f;
			sharedParams.soundParams.skidLateralFactor *= 0.1f;
			sharedParams.soundParams.skidCentrifugalFactor *= 0.1f;
		}
		
		CVehicleParams viewParams[2] = { table.findChild("ViewAdjustment"), table.findChild("ViewAdjustmentPowerLock") };
		if (viewParams[0])
		{
			for (int i=0; i<2; i++)
			{
				if (i>0) sharedParams.viewAdjustment[i] = sharedParams.viewAdjustment[0];
				if (!viewParams[i].IsValid()) continue;
				viewParams[i].getAttr("lateral", sharedParams.viewAdjustment[i].lateralDisp);
				viewParams[i].getAttr("lateralCent", sharedParams.viewAdjustment[i].lateralCent);
				viewParams[i].getAttr("rotate", sharedParams.viewAdjustment[i].rotate);
				viewParams[i].getAttr("rotateCent", sharedParams.viewAdjustment[i].rotateCent);
				viewParams[i].getAttr("maxLateralDisp", sharedParams.viewAdjustment[i].maxLateralDisp);
				viewParams[i].getAttr("maxRotate" , sharedParams.viewAdjustment[i].maxRotate);
				viewParams[i].getAttr("rotateSpeed", sharedParams.viewAdjustment[i].rotateSpeed);
				viewParams[i].getAttr("dispSpeed", sharedParams.viewAdjustment[i].dispSpeed);
				viewParams[i].getAttr("maxForwardDisp", sharedParams.viewAdjustment[i].maxForwardDisp);
				viewParams[i].getAttr("forwardResponse", sharedParams.viewAdjustment[i].forwardResponse);
				viewParams[i].getAttr("vibrationAmpSpeed", sharedParams.viewAdjustment[i].vibrationAmpSpeed);
				viewParams[i].getAttr("vibrationSpeedResponse", sharedParams.viewAdjustment[i].vibrationSpeedResponse);
				viewParams[i].getAttr("vibrationAmpAccel", sharedParams.viewAdjustment[i].vibrationAmpAccel);
				viewParams[i].getAttr("vibrationAccelResponse", sharedParams.viewAdjustment[i].vibrationAccelResponse);
				viewParams[i].getAttr("vibrationFrequency", sharedParams.viewAdjustment[i].vibrationFrequency);
				viewParams[i].getAttr("suspensionAmp", sharedParams.viewAdjustment[i].suspensionAmp);
				viewParams[i].getAttr("suspensionResponse", sharedParams.viewAdjustment[i].suspensionResponse);
				viewParams[i].getAttr("suspensionSharpness", sharedParams.viewAdjustment[i].suspensionSharpness);
			}
			for (int i=0; i<2; i++)
			{
				sharedParams.viewAdjustment[i].rotate = DEG2RAD(sharedParams.viewAdjustment[i].rotate);
				sharedParams.viewAdjustment[i].maxRotate = DEG2RAD(sharedParams.viewAdjustment[i].maxRotate);
				sharedParams.viewAdjustment[i].lateralCent *= 0.1f;
				sharedParams.viewAdjustment[i].rotateCent *= 0.1f;
			}
		}
		
		if(CVehicleParams fakeGearsParams = table.findChild("FakeGears"))
		{
			// Global min change up/down times [legacy]
			float minChangeUpTime = 0.6f;
			float minChangeDownTime = 0.3f;
			fakeGearsParams.getAttr("minChangeUpTime", minChangeUpTime);
			fakeGearsParams.getAttr("minChangeDownTime", minChangeDownTime);

			fakeGearsParams.getAttr("rpmRelaxSpeed", sharedParams.gears.rpmRelaxSpeed);
			fakeGearsParams.getAttr("rpmInterpSpeed", sharedParams.gears.rpmInterpSpeed);
			fakeGearsParams.getAttr("gearOscillationFrequency", sharedParams.gears.gearOscillationFrequency);
			fakeGearsParams.getAttr("gearOscillationAmp", sharedParams.gears.gearOscillationAmp);
			fakeGearsParams.getAttr("gearOscillationAmp2", sharedParams.gears.gearOscillationAmp2);
			fakeGearsParams.getAttr("gearChangeSpeed", sharedParams.gears.gearChangeSpeed);
			fakeGearsParams.getAttr("gearChangeSpeed2", sharedParams.gears.gearChangeSpeed2);

			fakeGearsParams.getAttr("rpmLoadFactor", sharedParams.gears.rpmLoadFactor);
			fakeGearsParams.getAttr("rpmLoadChangeSpeedUp", sharedParams.gears.rpmLoadChangeSpeedUp);
			fakeGearsParams.getAttr("rpmLoadChangeSpeedDown", sharedParams.gears.rpmLoadChangeSpeedDown);
			fakeGearsParams.getAttr("rpmLoadDefaultValue", sharedParams.gears.rpmLoadDefaultValue);
			fakeGearsParams.getAttr("rpmLoadFromBraking", sharedParams.gears.rpmLoadFromBraking);
			fakeGearsParams.getAttr("rpmLoadFromThrottle", sharedParams.gears.rpmLoadFromThrottle);

			// Clamp to sensible values
			sharedParams.gears.rpmInterpSpeed = max(0.1f, sharedParams.gears.rpmInterpSpeed);
			sharedParams.gears.rpmRelaxSpeed = max(0.1f, sharedParams.gears.rpmRelaxSpeed);

			if(CVehicleParams ratios = fakeGearsParams.findChild("Ratios"))
			{
				int	count = min(ratios.getChildCount(), SVehicleGears::kMaxGears - 3);
				
				if(count > 0)
				{
					sharedParams.gears.numGears = 2;

					for(int i = 0; i < count; ++ i)
					{	
						float	ratio = 0.0f;

						if(CVehicleParams gearRef = ratios.getChild(i))
						{
							gearRef.getAttr("value", ratio);

							if(ratio > 0.0f)
							{
								gearRef.getAttr("minChangeDownTime", minChangeDownTime);
								gearRef.getAttr("minChangeUpTime", minChangeUpTime);
								sharedParams.gears.ratios[sharedParams.gears.numGears] = ratio;
								sharedParams.gears.minChangeDownTime[sharedParams.gears.numGears] = minChangeDownTime;
								sharedParams.gears.minChangeUpTime[sharedParams.gears.numGears] = minChangeUpTime;
								sharedParams.gears.numGears++;
							}
						}
					}
				}

				for(int i = 0; i < sharedParams.gears.numGears; ++ i)
				{
					const float	ratio = sharedParams.gears.ratios[i];

					sharedParams.gears.invRatios[i] = (ratio != 0.0f) ? 1.0f / ratio : 0.f;
				}
			}
		}

		if(CVehicleParams	handlingParams = table.findChild("Handling"))
		{
			if(CVehicleParams powerParams = handlingParams.findChild("Power"))
			{
				powerParams.getAttr("acceleration", sharedParams.handling.acceleration);
				powerParams.getAttr("decceleration", sharedParams.handling.decceleration);
				powerParams.getAttr("topSpeed", sharedParams.handling.topSpeed);
				powerParams.getAttr("reverseSpeed", sharedParams.handling.reverseSpeed);
				// Default the boost setting to something sensible incase the xml does not contain any values
				sharedParams.handling.boostTopSpeed = sharedParams.handling.topSpeed * boostSpeedScale;
				sharedParams.handling.boostAcceleration = sharedParams.handling.acceleration * boostAccScale;
				powerParams.getAttr("boostTopSpeed", sharedParams.handling.boostTopSpeed);
				powerParams.getAttr("boostAcceleration", sharedParams.handling.boostAcceleration);
			}

			if(CVehicleParams reductionParams = handlingParams.findChild("SpeedReduction"))
			{
				reductionParams.getAttr("reductionAmount", sharedParams.handling.reductionAmount);
				reductionParams.getAttr("reductionRate", sharedParams.handling.reductionRate);
			}

			if(CVehicleParams compressionParams = handlingParams.findChild("Compression"))
			{
				compressionParams.getAttr("frictionBoost", sharedParams.handling.compressionBoost);
				compressionParams.getAttr("frictionBoostHandBrake", sharedParams.handling.compressionBoostHandBrake);
			}

			if(CVehicleParams frictionParams = handlingParams.findChild("Friction"))
			{
				frictionParams.getAttr("back", sharedParams.handling.backFriction);
				frictionParams.getAttr("front", sharedParams.handling.frontFriction);
				frictionParams.getAttr("offset", sharedParams.handling.frictionOffset);
			}

			if(CVehicleParams powerSlideParams = handlingParams.findChild("PowerSlide"))
			{
				powerSlideParams.getAttr("lateralSpeedFraction", sharedParams.powerSlide.lateralSpeedFraction[0]);
				powerSlideParams.getAttr("lateralSpeedFractionHB", sharedParams.powerSlide.lateralSpeedFraction[1]);
				powerSlideParams.getAttr("spring", sharedParams.powerSlide.spring);
			}

			if(CVehicleParams wheelSpin = handlingParams.findChild("WheelSpin"))
			{
				wheelSpin.getAttr("grip1", sharedParams.handling.grip1);
				wheelSpin.getAttr("grip2", sharedParams.handling.grip2);
				wheelSpin.getAttr("gripRecoverSpeed", sharedParams.handling.gripK);
				wheelSpin.getAttr("accelMultiplier1", sharedParams.handling.accelMultiplier1);
				wheelSpin.getAttr("accelMultiplier2", sharedParams.handling.accelMultiplier2);

				if(sharedParams.handling.gripK > 0.0f)
				{
					sharedParams.handling.gripK = 1.0f / sharedParams.handling.gripK;
				}
			}

			if(CVehicleParams handBrakeParams = handlingParams.findChild("HandBrake"))
			{
				handBrakeParams.getAttr("decceleration", sharedParams.handling.handBrakeDecceleration);
				handBrakeParams.getAttr("deccelerationPowerLock", sharedParams.handling.handBrakeDeccelerationPowerLock);
				handBrakeParams.getAttr("lockBack", sharedParams.handling.handBrakeLockBack);
				handBrakeParams.getAttr("lockFront", sharedParams.handling.handBrakeLockFront);
				handBrakeParams.getAttr("frontFrictionScale", sharedParams.handling.handBrakeFrontFrictionScale);
				handBrakeParams.getAttr("backFrictionScale", sharedParams.handling.handBrakeBackFrictionScale);
				handBrakeParams.getAttr("angCorrectionScale", sharedParams.handling.handBrakeAngCorrectionScale);
				handBrakeParams.getAttr("latCorrectionScale", sharedParams.handling.handBrakeLateralCorrectionScale);
			}

			if(CVehicleParams correctionParams = handlingParams.findChild("Correction"))
			{
				correctionParams.getAttr("lateralSpring", sharedParams.correction.lateralSpring);
				correctionParams.getAttr("angSpring", sharedParams.correction.angSpring);
			}

			if(CVehicleParams stabilisationParams = handlingParams.findChild("Stabilisation"))
			{
				stabilisationParams.getAttr("angDamping", sharedParams.stabilisation.angDamping);
				stabilisationParams.getAttr("rollDamping", sharedParams.stabilisation.rollDamping);
				stabilisationParams.getAttr("rollfixAir", sharedParams.stabilisation.rollfixAir);
				stabilisationParams.getAttr("upDamping", sharedParams.stabilisation.upDamping);
				float mta;
				if (stabilisationParams.getAttr("maxTiltAngleAir", mta))
				{
					mta = clamp_tpl(mta, 0.f, 90.f);
					sharedParams.stabilisation.sinMaxTiltAngleAir = sinf(DEG2RAD(mta));
					sharedParams.stabilisation.cosMaxTiltAngleAir = cosf(DEG2RAD(mta));
				}
			}
		}
		
		if(CVehicleParams	handlingParams = table.findChild("TankHandling"))
		{
			handlingParams.getAttr("additionalSteeringStationary", sharedParams.tankHandling.additionalSteeringStationary);
			handlingParams.getAttr("additionalSteeringAtMaxSpeed", sharedParams.tankHandling.additionalSteeringAtMaxSpeed);
			handlingParams.getAttr("additionalTilt", sharedParams.tankHandling.additionalTilt);
		}

		m_pSharedParams = CastSharedParamsPtr<SSharedParams>(pSharedParamsManager->Register(sharedParamsName, sharedParams));
	}

	CRY_ASSERT(m_pSharedParams.get());

	m_carParams.enginePower = 0.f;
	m_carParams.kStabilizer = 0.f;  
	m_carParams.engineIdleRPM = 0.f;
	m_carParams.engineMinRPM = m_carParams.engineMaxRPM = 0.f;     
	m_carParams.engineShiftDownRPM = m_carParams.engineShiftUpRPM = 0.f;
	m_carParams.steerTrackNeutralTurn = 0.f;  

	if (CVehicleParams handlingParams = table.findChild("Handling"))
	{
		/* Inertia */
		if (CVehicleParams inertiaParams = handlingParams.findChild("Inertia"))
		{
			inertiaParams.getAttr("radius", m_chassis.radius);
		}
	}

	m_action.steer = 0.0f;
	m_action.pedal = 0.0f;
	m_action.dsteer = 0.0f;
	m_action.ackermanOffset = m_pSharedParams->ackermanOffset;

	// Initialise the steering history.
	m_prevAngle = 0.0f;

	m_rpmScale = 0.0f;
	m_compressionMax = 0.f;
	m_wheelContacts = 0;
	m_topSpeedFactor = 1.f;
	m_accelFactor = 1.f;

	return InitPhysics(table);
}

//------------------------------------------------------------------------

#define ARCADE_WHEELED_MOVEMENT_VALUE_REQ(name, var, t)																																								\
if(!t.getAttr(name, var))																																																							\
{																																																																			\
	CryLog("Movement Init (%s) - failed to init due to missing <%s> parameter", m_pVehicle->GetEntity()->GetClass()->GetName(), name);	\
																																																																			\
	return false;																																																												\
}

bool CVehicleMovementArcadeWheeled::InitPhysics(const CVehicleParams& table)
{
	CVehicleParams wheeledTable = GetWheeledTable( table );

	if (!wheeledTable)
		return false;

	m_carParams.maxTimeStep = 0.02f;

	ARCADE_WHEELED_MOVEMENT_VALUE_REQ("damping", m_carParams.damping, wheeledTable);
	ARCADE_WHEELED_MOVEMENT_VALUE_REQ("engineIdleRPM", m_carParams.engineIdleRPM, wheeledTable);
	ARCADE_WHEELED_MOVEMENT_VALUE_REQ("engineMaxRPM", m_carParams.engineMaxRPM, wheeledTable);
	ARCADE_WHEELED_MOVEMENT_VALUE_REQ("engineMinRPM", m_carParams.engineMinRPM, wheeledTable);
	ARCADE_WHEELED_MOVEMENT_VALUE_REQ("stabilizer", m_carParams.kStabilizer, wheeledTable);
	
	wheeledTable.getAttr("integrationType", m_carParams.iIntegrationType);
	wheeledTable.getAttr("maxTimeStep", m_carParams.maxTimeStep);
	wheeledTable.getAttr("minEnergy", m_carParams.minEnergy);

	m_stabi = m_carParams.kStabilizer;

	if (g_pGameCVars->pVehicleQuality->GetIVal()==1)
		m_carParams.maxTimeStep = max(m_carParams.maxTimeStep, 0.04f);

	{
		// Set up the Arcade Mass
		m_chassis.mass = 1.f;
		m_chassis.invMass = 1.f/m_chassis.mass;
		m_chassis.inertia = 0.4f*m_chassis.radius*m_chassis.radius*m_chassis.mass;
		m_chassis.invInertia = 1.f/m_chassis.inertia;

		// Kill some of the low level physics params
		m_carParams.enginePower=0.f;
		m_carParams.brakeTorque=0.f;
		m_carParams.clutchSpeed=0.f;
		//m_carParams.damping=0.f;
		m_carParams.minBrakingFriction=0.f;
		m_carParams.maxBrakingFriction=0.f;
		m_carParams.kDynFriction=0.f;
		m_carParams.axleFriction=0.f;
		m_carParams.pullTilt=0.f;
		m_carParams.nGears = 0;

		// Better Collisions. Only make wheels part of chassis
		// when contact normal is over 85 degrees
		m_carParams.maxTilt = sinf(DEG2RAD(85.f));
		m_carParams.bKeepTractionWhenTilted = 1;

		m_maxSpeed = m_pSharedParams->handling.topSpeed;
	}

	return true;
}

#undef ARCADE_WHEELED_MOVEMENT_VALUE_REQ

//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::PostInit()
{
	CVehicleMovementBase::PostInit();

	// This needs to be called from two places due to the init order on server and client
	Reset();
}

//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::Physicalize()
{
	CVehicleMovementBase::Physicalize();

	SEntityPhysicalizeParams physicsParams(m_pVehicle->GetPhysicsParams());	

	physicsParams.type = PE_WHEELEDVEHICLE;	  
	m_carParams.nWheels = m_pVehicle->GetWheelCount();
	m_numWheels = m_carParams.nWheels;
	m_wheels.resize(m_numWheels);
	m_wheelStatus.resize(m_numWheels);
	m_invNumWheels = 1.f/(float)(m_numWheels|iszero(m_numWheels));

	pe_params_car carParams(m_carParams);  
	physicsParams.pCar = &carParams;

	m_pVehicle->GetEntity()->Physicalize(physicsParams);

	IPhysicalEntity *pPhysEnt = GetPhysics();
	if (pPhysEnt)
	{
		if (g_pGameCVars->pVehicleQuality->GetIVal()==1 && m_carParams.steerTrackNeutralTurn)
		{
			pe_params_flags pf; pf.flagsOR = wwef_fake_inner_wheels;
			pe_params_foreign_data pfd; pfd.iForeignFlagsOR = PFF_UNIMPORTANT;
			pPhysEnt->SetParams(&pf);
			pPhysEnt->SetParams(&pfd);
		}
		GetCurrentWheelStatus(pPhysEnt);

		if (m_pSharedParams->gravity>0.f)
		{
			pe_simulation_params paramsSet;
			paramsSet.gravity = Vec3(0.f, 0.f, -m_pSharedParams->gravity);
			paramsSet.gravityFreefall = Vec3(0.f, 0.f, -m_pSharedParams->gravity);
			pPhysEnt->SetParams(&paramsSet);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::PostPhysicalize()
{
	CVehicleMovementBase::PostPhysicalize();
	
	// This needs to be called from two places due to the init order on server and client
	if(!gEnv->pSystem->IsSerializingFile()) //don't do this while loading a savegame, it will overwrite the engine
	Reset();

	EnableLowLevelPhysics(k_frictionUseLowLevel, 0);
	
	/*IPhysicalEntity* pPhysics = GetPhysics();
	if (pPhysics && m_numWheels>0)
	{
		pe_params_car car;
		pPhysics->GetParams(&car);
		if (car.nWheels != m_numWheels)		// martinsh: disabled because occasionally this can fail if the wheel geometry got queued by the physics
		{
			// Asset is probably missing, reset number of wheels
			IEntity* pEntity = m_pVehicle->GetEntity();
			GameWarning("Vehicle '%s' has broken setup! Have %d physics wheels, but xml claims %d wheels", pEntity?pEntity->GetName():"", car.nWheels, m_numWheels);
			
			m_numWheels = 0;
			m_wheels.clear();
			m_wheelStatus.clear();
			m_invNumWheels = 1.f;
		}
	}*/
}

//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::InitSurfaceEffects()
{ 
	IPhysicalEntity* pPhysics = GetPhysics();
	pe_status_nparts tmpStatus;
	int numParts = pPhysics->GetStatus(&tmpStatus);
	int numWheels = m_pVehicle->GetWheelCount();

	m_paStats.envStats.emitters.clear();

	// for each wheelgroup, add 1 particleemitter. the position is the wheels' 
	// center in xy-plane and minimum on z-axis
	// direction is upward
	SEnvironmentParticles* envParams = m_pPaParams->GetEnvironmentParticles();

	for (int iLayer=0; iLayer < (int)envParams->GetLayerCount(); ++iLayer)
	{
		const SEnvironmentLayer& layer = envParams->GetLayer(iLayer);

		m_paStats.envStats.emitters.reserve(m_paStats.envStats.emitters.size() + layer.GetGroupCount());

		for (int i=0; i < (int)layer.GetGroupCount(); ++i)
		{ 
			Matrix34 tm(IDENTITY);

			if (layer.GetHelperCount() == layer.GetGroupCount() && layer.GetHelper(i))
			{
				// use helper pos if specified
				if (IVehicleHelper* pHelper = layer.GetHelper(i))
					pHelper->GetVehicleTM(tm);
			}
			else
			{
				// else use wheels' center
				Vec3 pos(ZERO);

				for (int w=0; w < (int)layer.GetWheelCount(i); ++w)
				{       
					int ipart = numParts - numWheels + layer.GetWheelAt(i,w)-1; // wheels are last

					if (ipart < 0 || ipart >= numParts)
					{
						CryLog("%s invalid wheel index: %i, maybe asset/setup mismatch", m_pEntity->GetName(), ipart);
						continue;
					}

					pe_status_pos spos;
					spos.ipart = ipart;
					if (pPhysics->GetStatus(&spos))
					{
						spos.pos.z += spos.BBox[0].z;
						pos = (pos.IsZero()) ? spos.pos : 0.5f*(pos + spos.pos);        
					}
				}
				tm = Matrix34::CreateRotationX(DEG2RAD(90.f));      
				tm.SetTranslation( m_pEntity->GetWorldTM().GetInverted().TransformPoint(pos) );
			}     

			TEnvEmitter emitter;
			emitter.layer = iLayer;        
			emitter.slot = -1;
			emitter.group = i;
			emitter.active = layer.IsGroupActive(i);
			emitter.quatT = QuatT(tm);
			m_paStats.envStats.emitters.push_back(emitter);

#if ENABLE_VEHICLE_DEBUG
			if (DebugParticles())
			{
				const Vec3 loc = tm.GetTranslation();
				CryLog("WheelGroup %i local pos: %.1f %.1f %.1f", i, loc.x, loc.y, loc.z);        
			}      
#endif
		}
	}

	m_paStats.envStats.initalized = true;  
}

//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::Reset()
{
	CVehicleMovementBase::Reset();


	m_steering = 0.f;
	m_prevAngle = 0.0f;
	m_action.pedal = 0.f;
	m_action.steer = 0.f;
	m_rpmScale = 0.0f;
	m_tireBlownTimer = 0.f; 
	m_wheelContacts = 0;
	m_topSpeedFactor = 1.f;
	m_accelFactor = 1.f;
		
	m_handling.lostContactOneSideTimer = 0.f;
	m_handling.viewRotate = 0.f;
	m_handling.viewOffset.zero();
	m_handling.fpViewVibration.Setup(0.02f, 10.f);

	if (m_blownTires)
		SetEngineRPMMult(1.0f);
	m_blownTires = 0;

	// needs to be after base reset (m_maxSpeed is overwritten by tweak groups)
	m_maxSpeed = m_pSharedParams->handling.topSpeed;
	
	IPhysicalEntity* pPhysics = GetPhysics();

	if (m_bForceSleep)
	{
		if (pPhysics)
		{
			pe_params_car params;
			params.minEnergy = m_carParams.minEnergy;
			pPhysics->SetParams(&params);
		}
	}
	m_bForceSleep = false;
	m_forceSleepTimer = 0.0f;
	m_passengerCount = 0;
	m_initialHandbreak = true;
	
	m_largeObjectInfo.SetZero();
	m_largeObjectInfoWheelIdx = 0;
	m_largeObjectFrictionType = 0;

	m_movementInfo.centrifugalAccel = 0.f;
	m_movementInfo.effectiveCentrifugalAccel = 0.f;
	m_movementInfo.skidValue = 0.f;
	m_movementInfo.averageSlipSpeedForward = 0.f;
	m_movementInfo.averageSlipSpeedLateral = 0.f;

	m_damageRPMScale = 1.f;
	m_collisionNorm.zero();

	m_chassis.vel.zero();
	m_chassis.angVel.zero();
	
	m_gears.averageWheelRadius = 1.f;
	m_gears.changedUp = 0;
	
	// Store numWheels once
	m_numWheels = m_pVehicle->GetWheelCount();
	m_wheels.resize(m_numWheels);
	m_wheelStatus.resize(m_numWheels);
	m_invNumWheels = 1.f/(float)(m_numWheels|iszero(m_numWheels));

	assert(m_numWheels<=maxWheels);

	if (m_numWheels > maxWheels)
	{
		GameWarning("Vehicle '%s' has broken setup! Number of wheels: %d exceeds maximum allowed number of wheels: %d", m_pEntity->GetName(), m_numWheels, maxWheels);
	}

	int n = 0;
	const int nParts = m_pVehicle->GetPartCount();
	for (int i=0; i<nParts; ++i)
	{      
		IVehiclePart* pPart = m_pVehicle->GetPart(i);
		if (pPart->GetIWheel())
		{ 
			m_wheels[n++].wheelPart = pPart;
		}
	}
	assert(n == m_numWheels);

	if (pPhysics && m_numWheels>0)
	{		
		pe_status_pos pos;
		pe_status_dynamics dyn;
		pPhysics->GetStatus(&pos);
		pPhysics->GetStatus(&dyn);
		Vec3 offset = (dyn.centerOfMass - pos.pos)*pos.q;		// Offset of centre of mass, in car space, from the physical-entity origin

		m_chassis.contactIdx0 = m_chassis.contactIdx1 = m_chassis.contactIdx2 = m_chassis.contactIdx3 = 0;
		float bestOffset[4] = {0.f, 0.f, 0.f, 0.f};

		float averageRadius = 0.f;

		GetCurrentWheelStatus(pPhysics);

		const int numWheels = m_numWheels;
		for (int i=0; i<numWheels; i++)
		{
			SVehicleWheel* w = &m_wheels[i];
			IVehiclePart* pPart = m_wheels[i].wheelPart;
			
			w->offset = pPart->GetLocalInitialTM().GetTranslation() - offset;
			
			w->radius = m_wheelStatus[i].r;
			w->mass = m_chassis.mass*0.05f;
			w->invMass = 1.f/w->mass;
			w->inertia = 0.4f*w->radius*w->radius*w->mass;
			if (w->inertia != 0)
				w->invInertia = 1.f/w->inertia;
			else
				w->invInertia = 1.0f;
			w->w = 0.f;
			w->lastW = 0.f;
			w->suspLen = m_wheelStatus[i].suspLen;
			w->compression = 0.f;
			w->slipSpeed = 0.f;
			w->slipSpeedLateral = 0.f;

			w->bottomOffset = w->offset.z - w->radius;
			w->offset.z = m_pSharedParams->handling.frictionOffset;
			w->bottomOffset -= w->offset.z;
			w->axleIndex = (w->offset.y > 0.f) ? 1 : 0;
		
			// Wheel's velocity response to a solver impulse
			w->invWheelK = 1.f/(w->invMass + w->radius * w->radius * w->invInertia);

			w->bCanLock = ((w->axleIndex == 0) && m_pSharedParams->handling.handBrakeLockBack) ? 1 : 0;
			w->bCanLock |= ((w->axleIndex == 1) && m_pSharedParams->handling.handBrakeLockFront) ? 1 : 0;

			// contactIdx0 = left-forward
			// contactIdx1 = left-back
			// contactIdx2 = right-forward
			// contactIdx3 = right-back
			// Manhatten distance used to find the best choice
			if ((w->offset.x < 0.f) && ((+w->offset.y - w->offset.x) >= bestOffset[0])) { bestOffset[0] = +w->offset.y - w->offset.x; m_chassis.contactIdx0 = i; }
			if ((w->offset.x < 0.f) && ((-w->offset.y - w->offset.x) >= bestOffset[1])) { bestOffset[1] = -w->offset.y - w->offset.x; m_chassis.contactIdx1 = i; }
			if ((w->offset.x >= 0.f) && ((+w->offset.y + w->offset.x) >= bestOffset[2])) { bestOffset[2] = +w->offset.y + w->offset.x; m_chassis.contactIdx2 = i; }
			if ((w->offset.x >= 0.f) && ((-w->offset.y + w->offset.x) >= bestOffset[3])) { bestOffset[3] = -w->offset.y + w->offset.x; m_chassis.contactIdx3 = i; }

			averageRadius += w->radius;
		}

		m_gears.averageWheelRadius = averageRadius*m_invNumWheels;

		EnableLowLevelPhysics(k_frictionUseLowLevel, 0);
	}

	ResetWaterLevels();
}

//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::Release()
{
	CVehicleMovementBase::Release();

	delete this;
}

//------------------------------------------------------------------------
bool CVehicleMovementArcadeWheeled::StartDriving(EntityId driverId)
{
	if (!CVehicleMovementBase::StartDriving(driverId))
		return false;

	m_brakeTimer = 0.f;
	m_reverseTimer = 0.f;
	m_action.pedal = 0.f;
	m_action.steer = 0.f;
	m_initialHandbreak =false;
	m_steering = 0.f;

	// Gears
	m_gears.curGear = SVehicleGears::kNeutral;
	m_gears.lastGear = m_gears.curGear;
	m_gears.curRpm = 0.1f;
	m_gears.modulation = 0.f;
	m_gears.targetRpm = 0.1f;
	m_gears.timer = 0.f;
	m_gears.rpmLoad = 0.f;

	return true;
}

//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::StopDriving()
{
	CVehicleMovementBase::StopDriving();
	m_movementAction.Clear(true);

	UpdateBrakes(0.f);
	EnableLowLevelPhysics(k_frictionUseLowLevel, 0);
}

//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params)
{
	CVehicleMovementBase::OnEvent(event, params);
		
	if (event == eVME_Collision)
	{
		int otherIdx=0;
		IPhysicalEntity* pPhysics = GetPhysics();
		EventPhysCollision* pCollision = (EventPhysCollision*)params.sParam;
		if (sqr(pCollision->normImpulse)>m_collisionNorm.GetLengthSquared())
		{
			Vec3 n;
			if (pPhysics==pCollision->pEntity[0])
			{
				otherIdx = 1;
				n = -pCollision->n;
			}
			else
			{
				n = pCollision->n;
			}
			IPhysicalEntity* otherPhys = pCollision->pEntity[otherIdx]; 
			if (pCollision->mass[otherIdx]>1000.f && otherPhys)
			{
				if (IEntity* otherEntity = (IEntity*)otherPhys->GetForeignData(PHYS_FOREIGN_ID_ENTITY))
				{
					if (IVehicle* pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(otherEntity->GetId()))
					{
						m_collisionNorm = n * pCollision->normImpulse;
					}
				}
			}
		}
	}
	
	if (event==eVME_SetFactorMaxSpeed)
	{
		m_topSpeedFactor = max(0.001f, params.fValue );
	}

	if (event==eVME_SetFactorAccel)
	{
		m_accelFactor = max(0.001f, params.fValue );
	}

	if (event == eVME_TireBlown || event == eVME_TireRestored)
	{		
		int wheelIndex = params.iValue;

		if (m_carParams.steerTrackNeutralTurn == 0.f)
		{
			int blownTiresPrev = m_blownTires;    
			m_blownTires = max(0, min(m_pVehicle->GetWheelCount(), event==eVME_TireBlown ? m_blownTires+1 : m_blownTires-1));

			// reduce speed based on number of tyres blown out        
			if (m_blownTires != blownTiresPrev)
			{	
				SetEngineRPMMult(GetWheelCondition());
			}
		}

		// handle particles (sparks etc.)
		SEnvironmentParticles* envParams = m_pPaParams->GetEnvironmentParticles();    

		SEnvParticleStatus::TEnvEmitters::iterator emitterIt = m_paStats.envStats.emitters.begin();
		SEnvParticleStatus::TEnvEmitters::iterator emitterItEnd = m_paStats.envStats.emitters.end();
		for (; emitterIt != emitterItEnd; ++emitterIt)
		{ 
			// disable this wheel in layer 0, enable in layer 1            
			if (emitterIt->group >= 0)
			{
				const SEnvironmentLayer& layer = envParams->GetLayer(emitterIt->layer);

				for (int i=0; i < (int)layer.GetWheelCount(emitterIt->group); ++i)
				{
					if (layer.GetWheelAt(emitterIt->group, i)-1 == wheelIndex)
					{
						bool bEnable = !strcmp(layer.GetName(), "rims");
						if (event == eVME_TireRestored)
							bEnable=!bEnable;
						EnableEnvEmitter(*emitterIt, bEnable);
						emitterIt->active = bEnable;
					}
				} 
			}
		}     
	} 

	if (event == static_cast<EVehicleMovementEvent>(eVME_StartLargeObject))
	{
		// Begin Large object interaction - kickable cars
		// NB: This code path is used in MP only at the moment
		const SVehicleMovementEventLargeObject* info = (SVehicleMovementEventLargeObject*)params.sParam;

		m_largeObjectInfo.countDownTimer = info->timer;
		m_largeObjectInfo.startTime = info->timer;
		m_largeObjectInfo.invStartTime = 1.f/info->timer;
		m_largeObjectInfo.impulseDir = info->impulseDir;
		m_largeObjectInfo.kicker = info->kicker;

		pe_action_set_velocity asv;

		AABB worldBounds;
		IEntity *vehicleEntity = m_pVehicle->GetEntity();
		vehicleEntity->GetWorldBounds(worldBounds);

		Vec3 diff = info->eyePos - (worldBounds.IsEmpty() ? vehicleEntity->GetPos() : worldBounds.GetCenter());
		diff.z = 0.f;
		asv.v = info->impulseDir * (info->speed * info->boostFactor);
		asv.w = diff^asv.v*info->swingRotationFactor;
		
		// Choose one wheel to adjust friction on
		m_largeObjectInfoWheelIdx = -1;

		// 50 - 50 chance of which friction method to use
		m_largeObjectFrictionType = cry_random(0, 1);

		// Side Hit
		if (info->kickType == SVehicleMovementEventLargeObject::k_KickSide)
		{
			const Matrix34& worldTM = m_pVehicle->GetEntity()->GetWorldTM();
			const Vec3 xAxis = worldTM.GetColumn0();
			const Vec3 yAxis = worldTM.GetColumn1();
			float dot = xAxis * info->impulseDir;
			if (fabsf(dot)>0.8f)
			{
				Vec3 playerXAxis = info->viewQuat.GetColumn0();
				float speed = info->rotationFactor * sgnnz(playerXAxis*yAxis);
				asv.w = yAxis * -speed;
			}
		}
		else if (m_largeObjectFrictionType==0)
		{
			// Choose just one wheel which to keep friction high 
			const Matrix34& worldTM = m_pVehicle->GetEntity()->GetWorldTM();
			Matrix33 inv = Matrix33(worldTM);
			inv.Invert();
			diff = inv * diff;

			// Find the wheel that is behind
			const int c0 = m_chassis.contactIdx0;
			const int c1 = m_chassis.contactIdx1;
			const int c2 = m_chassis.contactIdx2;
			const int c3 = m_chassis.contactIdx3;
			if (diff.x*m_wheels[c0].offset.x<0.f && diff.y*m_wheels[c0].offset.y < 0.f)
				m_largeObjectInfoWheelIdx = c0;
			if (diff.x*m_wheels[c1].offset.x< 0.f && diff.y*m_wheels[c1].offset.y < 0.f)
				m_largeObjectInfoWheelIdx = c1;
			if (diff.x*m_wheels[c2].offset.x< 0.f && diff.y*m_wheels[c2].offset.y < 0.f)
				m_largeObjectInfoWheelIdx = c2;
			if (diff.x*m_wheels[c3].offset.x< 0.f && diff.y*m_wheels[c3].offset.y < 0.f)
				m_largeObjectInfoWheelIdx = c3;
		}

		EnableLowLevelPhysics(k_frictionUseLowLevel, 0);

		if (gEnv->bServer)
		{
			IPhysicalEntity* pPhysics = GetPhysics();
			pPhysics->Action(&asv);
		}
	}

	if(event == eVME_EnableHandbrake)
	{
		// Set stationary handbrake and wake physics if necessary.

		m_stationaryHandbrake = params.bValue;

		if(!m_stationaryHandbrake)
		{
			m_stationaryHandbrakeResetTimer	= params.fValue;

			m_pVehicle->NeedsUpdate(IVehicle::eVUF_AwakePhysics);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{

	CryAutoCriticalSection netlk(m_networkLock);
	CryAutoCriticalSection lk(m_lock);

	CVehicleMovementBase::OnAction(actionId, activationMode, value);

}

//------------------------------------------------------------------------
float CVehicleMovementArcadeWheeled::GetWheelCondition() const
{
	// for a 4-wheel vehicle, want to reduce speed by 20% for each wheel shot out. So I'm assuming that for an 8-wheel
	//	vehicle we'd want to reduce by 10% per wheel.
	return  1.0f - ((float)m_blownTires*m_invNumWheels*m_pSharedParams->damagedWheelSpeedInfluenceFactor);
}

//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::SetEngineRPMMult(float mult, int threadSafe)
{
	m_damageRPMScale = mult;	
}

//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	CVehicleMovementBase::OnVehicleEvent(event,params);
}

//------------------------------------------------------------------------
float CVehicleMovementArcadeWheeled::GetMaxSteer(float speedRel)
{
	// reduce max steering angle with increasing speed
	m_steerMax = m_pSharedParams->v0SteerMax - (m_pSharedParams->kvSteerMax * speedRel);
	//  m_steerMax = 45.0f;
	return DEG2RAD(m_steerMax);
}


//------------------------------------------------------------------------
float CVehicleMovementArcadeWheeled::GetSteerSpeed(float speedRel)
{
	// reduce steer speed with increasing speed
	float steerDelta = m_pSharedParams->steerSpeed - m_pSharedParams->steerSpeedMin;
	float steerSpeed = m_pSharedParams->steerSpeedMin + steerDelta * speedRel;

	// additionally adjust sensitivity based on speed
	float steerScaleDelta = m_pSharedParams->steerSpeedScale - m_pSharedParams->steerSpeedScaleMin;
	float sensivity = m_pSharedParams->steerSpeedScaleMin + steerScaleDelta * speedRel;

	return steerSpeed * sensivity;
}

//----------------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::ApplyBoost(float speed, float maxSpeed, float strength, float deltaTime)
{
	IPhysicalEntity* pPhysics = GetPhysics();
	if (pPhysics==NULL) return;

	SVehiclePhysicsStatus* physStatus = &m_physStatus[k_physicsThread];
	const Vec3 yAxis = physStatus->q.GetColumn1();
	speed = yAxis.dot(m_chassis.vel);
	if (m_action.pedal > 0.01f && m_wheelContacts >= 0.5f*(float)m_numWheels)
	{
		float error = maxSpeed - speed;
		if (error>0.f)
		{
			const Vec3 zAxis = physStatus->q.GetColumn2();
			const float angle = DEG2RAD(30.f);
			Vec3 dir = (yAxis * cosf(angle)) - (zAxis*sinf(angle));
			float spring = 1.f;//approxOneExp(deltaTime);
			error = error*spring;
			if (error>10.f*deltaTime)	// Cap top a maximum of 10m/s^2 acceleration
				error = 10.f*deltaTime;
			m_chassis.vel += dir*error;
			pe_action_set_velocity setVelocity;
			setVelocity.v = m_chassis.vel;
			pPhysics->Action(&setVelocity, THREAD_SAFE);
		}
	}
}

#if ENABLE_VEHICLE_DEBUG
//----------------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::ReleaseDebugInfo()
{
	if (m_debugInfo)
	{
		RemoveDebugGraphs();
		SAFE_DELETE(m_debugInfo);
	}
}

void CVehicleMovementArcadeWheeled::RemoveDebugGraphs()
{
	if (m_debugInfo)
	{
		SAFE_RELEASE(m_debugInfo->pDebugHistoryManager);
		m_debugInfo->pDebugHistory = NULL;
		m_debugInfo->graphMode = SVehicleDebugInfo::k_graphNone;
	}
}

//----------------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::DebugDrawMovement(const float deltaTime)
{
	if (!IsProfilingMovement())
	{
		ReleaseDebugInfo();
		return;
	}

	if (g_pGameCVars->v_profileMovement==3 || g_pGameCVars->v_profileMovement==1 && m_lastDebugFrame == gEnv->pRenderer->GetFrameID())
		return;


	IPhysicalEntity* pPhysics = GetPhysics();
	IRenderer* pRenderer = gEnv->pRenderer;
	static float color[4] = {1,1,1,1};
	float green[4] = {0,1,0,1};
	float red[4] = {1,0,0,1};
	static ColorB colRed(255,0,0,255);
	static ColorB colBlue(0,0,255,255);
	static ColorB colWhite(255,255,255,255);
	ColorB colGreen(0,255,0,255);
	ColorB col1(255,255,0,255);
	float x, y = 50.f, step1 = 15.f, step2 = 20.f, size=1.3f, sizeL=1.5f;
	
	m_lastDebugFrame = gEnv->pRenderer->GetFrameID();
	
#if 0
	{
		IPhysicalEntity* pPhysics = GetPhysics();
		pe_status_vehicle psv;
		pPhysics->GetStatus(&psv);
    	static float drawColor[4] = {1,1,1,1};
		const Matrix34& worldTM = m_pVehicle->GetEntity()->GetWorldTM();
		IRenderAuxText::DrawLabelExF(worldTM.GetTranslation(), 2.0f, drawColor, true, true, "<%s> friction=%s,  enabled=%s, HB=%d", m_pVehicle->GetEntity()->GetName(), m_frictionState==k_frictionUseLowLevel ? "Low" : "Game", m_bMovementProcessingEnabled ? "on" : "off", psv.bHandBrake);
	}
#endif

	//=======================================
	// Create Auxillary Debug Information
	//=======================================
	if (m_debugInfo==NULL)
	{
		m_debugInfo = new SVehicleDebugInfo;
		m_debugInfo->debugGearChange.m_invTimeConstant = 3.f;
		m_debugInfo->pDebugHistoryManager = NULL;
		m_debugInfo->pDebugHistory = NULL;
		m_debugInfo->graphMode = SVehicleDebugInfo::k_graphNone;
	}
	
	SVehiclePhysicsStatus* physStatus = &m_physStatus[k_mainThread];

	const int width  = pRenderer->GetOverlayWidth();
	const int height = pRenderer->GetOverlayHeight();


	Matrix33 bodyRot( physStatus->q );
	Matrix34 bodyPose( bodyRot, physStatus->centerOfMass );
	const Vec3 xAxis = bodyPose.GetColumn0();
	const Vec3 yAxis = bodyPose.GetColumn1();
	const Vec3 zAxis = bodyPose.GetColumn2();
	const Vec3 chassisPos = bodyPose.GetColumn3();

	float speedMs = physStatus->v.dot(yAxis);
	float speed = m_vehicleStatus.vel.len();
	float speedRel = min(speed, m_pSharedParams->vMaxSteerMax) / m_pSharedParams->vMaxSteerMax;
	float steerMax = GetMaxSteer(speedRel);
	float steerSpeed = GetSteerSpeed(speedRel);  
	int percent = (int)(speed / m_maxSpeed * 100.f);
	Vec3 localVel = m_localSpeed;

	// Get the view name
	IActor* pActor = m_pVehicle->GetDriver();
	IVehicleSeat* pSeat = pActor ? m_pVehicle->GetSeatForPassenger(pActor->GetEntityId()) : NULL;
	IVehicleView* pView = pSeat ? pSeat->GetCurrentView() : NULL;
	const char* viewName = pView ? pView->GetName() : "";

	pe_params_car carparams;
	pPhysics->GetParams(&carparams);
	
	IRenderAuxText::Draw2dLabel(5.0f,   y, sizeL, color, false, "Car movement");
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size, color, false, "fake gear: %d", m_gears.curGear-1);

	IRenderAuxText::Draw2dLabel(5.0f, y += step2, size, color, false, "invTurningRadius: %.1f", m_invTurningRadius);
	IRenderAuxText::Draw2dLabel(5.0f, y += step2, size, color, false, "Speed: %.1f (%.1f km/h) (%i) (%f m/s)", speed, speed*3.6f, percent, speedMs);
	IRenderAuxText::Draw2dLabel(5.0f, y += step1, size, color, false, "localVel: %.1f %.1f %.1f", localVel.x, localVel.y, localVel.z);
	IRenderAuxText::Draw2dLabel(5.0f, y += step1, size, color, false, "Dampers:  %.2f", m_suspDamping);
	IRenderAuxText::Draw2dLabel(5.0f, y += step1, size, color, false, "Stabi:  %.2f", m_stabi);
	//IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "BrakeTime:  %.2f", m_brakeTimer);
	//IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "compressionScale:  %.2f", m_handling.compressionScale);

	//if (m_statusDyn.submergedFraction > 0.f)
	//	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "Submerged:  %.2f", m_statusDyn.submergedFraction);

	//if (m_damage > 0.f)
	//	IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size, color, false, "Damage:  %.2f", m_damage);  

	//if (Boosting())
	//	pRenderer->Draw2dLabel(5.0f,  y+=step1, size, green, false, "Boost:  %.2f", m_boostCounter);

	IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, sizeL, color, false, "using View: %s", viewName);

	IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, sizeL, color, false, "Driver input");
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size, color, false, "power: %.2f", m_movementAction.power);
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "steer: %.2f", m_movementAction.rotateYaw); 
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "brake: %i", m_movementAction.brake);

	IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, sizeL, color, false, "Car action");
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size, color, false, "pedal: %.2f", m_action.pedal);
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "steer: %.2f (max %.2f)", RAD2DEG(m_steering), RAD2DEG(steerMax)); 
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "steerFrac: %.2f", m_steering / DEG2RAD(m_steerMax));
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "brake: %i", m_action.bHandBrake);

	IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size, color, false, "steerSpeed: %.2f", steerSpeed);

	const Matrix34& worldTM = m_pVehicle->GetEntity()->GetWorldTM();

	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags flags = pAuxGeom->GetRenderFlags();
	SAuxGeomRenderFlags oldFlags = pAuxGeom->GetRenderFlags();
	flags.SetDepthWriteFlag(e_DepthWriteOff);
	flags.SetDepthTestFlag(e_DepthTestOff);
	pAuxGeom->SetRenderFlags(flags);

	pAuxGeom->DrawSphere(physStatus->centerOfMass, 0.1f, colGreen);

	pAuxGeom->DrawLine(chassisPos, colGreen, chassisPos + 2.f*m_handling.contactNormal, col1);

	pe_status_wheel ws;
	pe_status_pos wp;
	pe_params_wheel wparams;

	pe_status_nparts tmpStatus;
	int numParts = pPhysics->GetStatus(&tmpStatus);

	int count = m_numWheels;
	float tScaleTotal = 0.f;
		
		
	// wheel-specific
	for (int i=0; i<count; ++i)
	{
		{
			IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size, color, false, "slip speed: %.2f", m_wheels[i].slipSpeed);
		}

		if (0)
		{
			Vec3 pos;
			pos = physStatus->centerOfMass + physStatus->q * m_wheels[i].offset;
			pAuxGeom->DrawSphere(pos, 0.05f, colGreen);
			pAuxGeom->DrawLine(pos, colGreen, pos + m_wheels[i].frictionDir[0], colGreen);
			pAuxGeom->DrawLine(pos, colGreen, pos + m_wheels[i].frictionDir[1], colGreen);
		}


		ws.iWheel = i;
		wp.ipart = numParts - count + i;
		wparams.iWheel = i;

		int ok = pPhysics->GetStatus(&ws);
		ok &= pPhysics->GetStatus(&wp);
		ok &= pPhysics->GetParams(&wparams);

		if (!ok)
			continue;

		// slip
		if (g_pGameCVars->v_draw_slip)
		{
			if (ws.bContact)
			{ 
				Vec3 nrmLineStart;
				Vec3 nrmLineEnd;
				nrmLineStart = ws.ptContact;
				nrmLineEnd = nrmLineStart;
				nrmLineEnd += ws.normContact * 0.25f;

				pAuxGeom->DrawSphere(ws.ptContact, 0.05f, colRed);
				pAuxGeom->DrawLine(nrmLineStart, colBlue, nrmLineEnd, colBlue);

				float slip = ws.velSlip.len();        
				if (ws.bSlip>0)
				{ 
					pAuxGeom->DrawLine(wp.pos, colRed, wp.pos+ws.velSlip, colRed);
				}        
				pAuxGeom->DrawLine(wp.pos, colRed, wp.pos+ws.normContact, colRed);
			}
		}    

		// suspension    
		if (g_pGameCVars->v_draw_suspension)
		{
			ColorB col(255,0,0,255);

			Vec3 lower = m_wheels[i].wheelPart->GetLocalTM(false).GetTranslation();
			lower.x += fsgnf(lower.x) * 0.5f;

			Vec3 upper(lower);
			upper.z += ws.suspLen;

			lower = worldTM.TransformPoint(lower);
			pAuxGeom->DrawSphere(lower, 0.1f, col);              

			upper = worldTM.TransformPoint(upper);
			pAuxGeom->DrawSphere(upper, 0.1f, col);

			//pAuxGeom->DrawLine(lower, col, upper, col);
		}

		// Draw Wheel Markers (Helps for looking at wheel rotations)
		{
			const Matrix34 wheelMat = m_wheels[i].wheelPart->GetLocalTM(false);
			Vec3 pos1 = worldTM*(wheelMat*Vec3(0.f,0.f,0.f));
			Vec3 pos2 = worldTM*(wheelMat*Vec3(0.f,0.2f,0.f));
			pAuxGeom->DrawLine(pos1, colRed, pos2, colRed);
		}
	}

	if (tScaleTotal != 0.f)
	{
		IRenderAuxText::DrawLabelF(worldTM.GetTranslation(),1.3f,"tscale: %.2f",tScaleTotal);
	}

	if (m_pWind[0])
	{
		pe_params_buoyancy buoy;
		pe_status_pos pos;
		if (m_pWind[0]->GetParams(&buoy) && m_pWind[0]->GetStatus(&pos))
		{
			IRenderAuxText::DrawLabelF(pos.pos, 1.3f, "rad: %.1f", buoy.waterFlow.len());
		}
		if (m_pWind[1]->GetParams(&buoy) && m_pWind[1]->GetStatus(&pos))
		{
			IRenderAuxText::DrawLabelF(pos.pos, 1.3f, "lin: %.1f", buoy.waterFlow.len());
		}
	}

	//========================
	// Draw Gears and RPM dial
	//========================
	pAuxGeom->SetOrthographicProjection(true, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f);
	{
		float radius = 40.f;
		Vec3 centre (0.2f*(float)width, 0.8f*(float)height, 0.f);
		Vec3 centre1 = centre - Vec3(1.3f*radius, 0.f, 0.f);
		Vec3 centre2 = centre + Vec3(1.3f*radius, 0.f, 0.f);
		Vec3 circle0[32];
		Vec3 circle1[32];
		Vec3 circle2[32];
		float a = 0.f;
		float da = gf_PI2/32.f;
		for (int i=0; i<32; i++)
		{
			circle0[i].Set(radius*cosf(a), radius*sinf(a), 0.f);
			circle1[i] = circle0[i] + centre1; 
			circle2[i] = circle0[i] + centre2;
			a += da;
		}

		float angleOffset1 = gf_PI * 0.75f;
		float angleOffset2 = gf_PI * 2.25f;
		float angleDiff = angleOffset2 - angleOffset1;

		Vec3 minMarker(cosf(angleOffset1), sinf(angleOffset1), 0.f);
		Vec3 maxMarker(cosf(angleOffset2), sinf(angleOffset2), 0.f);

		float topSpeed = m_gears.curGear==SVehicleGears::kReverse ? -m_pSharedParams->handling.reverseSpeed : m_pSharedParams->handling.topSpeed;
		topSpeed *= m_topSpeedFactor;
		float speedNorm = angleOffset1 + clamp_tpl(speedMs/topSpeed, 0.f, 1.f) * angleDiff;
		float rpm = angleOffset1 + m_rpmScale * angleDiff;
		Vec3 speedDial(cosf(speedNorm), sinf(speedNorm), 0.f);
		Vec3 rpmDial(cosf(rpm), sinf(rpm), 0.f);

		pAuxGeom->DrawPolyline(circle1, 32, true, ColorB(255,255,255,255));
		pAuxGeom->DrawLine(centre1 + minMarker*0.9f*radius, colWhite, centre1 + minMarker*1.1f*radius, colWhite);
		pAuxGeom->DrawLine(centre1 + maxMarker*0.9f*radius, colWhite, centre1 + maxMarker*1.1f*radius, colWhite);
		pAuxGeom->DrawLine(centre1, colRed, centre1 + speedDial*radius*0.9f, colRed);

		pAuxGeom->DrawPolyline(circle2, 32, true, ColorB(255,255,255,255));
		pAuxGeom->DrawLine(centre2 + minMarker*0.9f*radius, colWhite, centre2 + minMarker*1.1f*radius, colWhite);
		pAuxGeom->DrawLine(centre2 + maxMarker*0.9f*radius, colWhite, centre2 + maxMarker*1.1f*radius, colWhite);
		pAuxGeom->DrawLine(centre2, colRed, centre2 + rpmDial*radius*0.9f, colRed);
		
		IRenderAuxText::Draw2dLabel(centre1.x-radius*0.3f, centre1.y+radius*1.4f, 1.3f, color, false, "Speed");
		IRenderAuxText::Draw2dLabel(centre2.x-radius*0.3f, centre2.y+radius*1.4f, 1.3f, color, false, "Rpm");

	}

	//========================
	DebugDrawGraphs();
	//========================

	//========================================
	// Draw Sound Value Bars
	//========================================
	{
		CTimeSmoothedBar debugBar(0.f, 1.f);
		CTimeSmoothedValue value;

		// Draw Gear Change Bar (GREEN)
		x = 0.9f*(float)width;
		y = 0.8f*(float)height;
		debugBar.m_colourBar.set(0,255,0,255);
		debugBar.Draw(m_debugInfo->debugGearChange, x, y, 20.f, 90.f);
		IRenderAuxText::Draw2dLabel(x-25.f, y+20.f, 1.3f, color, false, "GearChange");

		// Draw the RPM-engine strain gauage (BLUE)
		x-=130.f;
		debugBar.m_colourBar.set(0,0,255,255);
		value.Reset(fabsf(m_gears.rpmLoad));
		debugBar.Draw(value, x, y, 20.f, 90.f);
		IRenderAuxText::Draw2dLabel(x-20.f, y+20.f, 1.3f, color, false, "RPM Load");

		// Draw Forward Slip Fraction (RED-ISH)
		x-=130.f;
		debugBar.m_colourBar.set(150,50,50,255);
		value.Reset(fabsf(m_movementInfo.averageSlipSpeedForward)*m_pSharedParams->soundParams.skidForwardFactor);
		debugBar.Draw(value, x, y, 20.f, 90.f);
		IRenderAuxText::Draw2dLabel(x-20.f, y+20.f, 1.3f, color, false, "Fwd SlipRatio");

		// Draw Side Slip Fraction (RED-ISH)
		x-=130.f;
		debugBar.m_colourBar.set(150,50,50,255);
		value.Reset(fabsf(m_movementInfo.averageSlipSpeedLateral)*m_pSharedParams->soundParams.skidLateralFactor);
		debugBar.Draw(value, x, y, 20.f, 90.f);
		IRenderAuxText::Draw2dLabel(x-20.f, y+20.f, 1.3f, color, false, "Side Slip");
		
		// Draw Centrifugal accel (YELLOW)
		x = 0.9f*(float)width;
		y = 0.8f*(float)height - 140.f;
		debugBar.m_colourBar.set(255,255,0,255);
		value.Reset(fabsf(m_movementInfo.centrifugalAccel)*m_pSharedParams->soundParams.skidCentrifugalFactor);
		debugBar.Draw(value, x, y, 20.f, 90.f);
		IRenderAuxText::Draw2dLabel(x-20.f, y+20.f, 1.3f, color, false, "Centrif");

		// Draw Effective Centrifugal accel (YELLOW)
		x-=130.f;
		debugBar.m_colourBar.set(255,255,0,255);
		value.Reset(fabsf(m_movementInfo.effectiveCentrifugalAccel)*m_pSharedParams->soundParams.skidCentrifugalFactor);
		debugBar.Draw(value, x, y, 20.f, 90.f);
		IRenderAuxText::Draw2dLabel(x-20.f, y+20.f, 1.3f, color, false, "Eff. Centif");
		
		// Draw FMOD Skid Value (RED)
		x-=130.f;
		debugBar.m_colourBar.set(255,0,0,255);
		value.Reset(m_movementInfo.skidValue);
		debugBar.Draw(value, x, y, 20.f, 90.f);
		IRenderAuxText::Draw2dLabel(x-20.f, y+20.f, 1.3f, color, false, "FMOD Skid Value");
	}

	pAuxGeom->SetOrthographicProjection(false);
	pAuxGeom->SetRenderFlags(oldFlags);
}


static int ChooseGraphMode(SVehicleDebugInfo* info, const char* cvarName, const char* graphName, int mode, float value, float min1, float max1, float min2, float max2)
{
	if (strcmp(cvarName, graphName)==0)
	{
		if (info->graphMode!=mode)
		{
			info->pDebugHistory->SetVisibility(false);
			info->pDebugHistory->ClearHistory();
			info->pDebugHistory->SetupScopeExtent(min1, max1, min2, max2);
			info->pDebugHistory->SetName(graphName);
			info->pDebugHistory->SetVisibility(true);
			info->graphMode = mode;
		}
		info->pDebugHistory->AddValue(value);
		return 1;
	}
	return 0;
}


void CVehicleMovementArcadeWheeled::DebugDrawGraphs()
{
	const char* graphName = g_pGameCVars->v_profile_graph;
	if (graphName[0]==0)
	{
		RemoveDebugGraphs();
		return;
	}

	///////// Create Graph /////////////////////////
	if (m_debugInfo->pDebugHistoryManager==NULL)
	{
		static float leftX = 0.6f, topY = 0.1f, width = 0.4f, height = 0.4f, margin = 0.f;
		m_debugInfo->pDebugHistoryManager = g_pGame->GetIGameFramework()->CreateDebugHistoryManager();
		CRY_ASSERT(m_debugInfo->pDebugHistoryManager);
		m_debugInfo->pDebugHistory = m_debugInfo->pDebugHistoryManager->CreateHistory("vehicle-graph", "");
		m_debugInfo->pDebugHistory->SetupLayoutRel(leftX, topY, width, height, margin);
		m_debugInfo->pDebugHistory->SetVisibility(false);
		m_debugInfo->graphMode = SVehicleDebugInfo::k_graphNone;
	}

	
	////////// Choose which graph data to plot //////////////////////////////
	int showGraph=0;
	showGraph |= ChooseGraphMode(m_debugInfo, graphName, "slip-speed", SVehicleDebugInfo::k_graphSlipSpeed, m_movementInfo.averageSlipSpeedForward, -100.f, +100.f, -1.f, +1.f);
	showGraph |= ChooseGraphMode(m_debugInfo, graphName, "slip-speed-lateral", SVehicleDebugInfo::k_graphSlipSpeedLateral, m_movementInfo.averageSlipSpeedLateral, -100.f, +100.f, -1.f, +1.f);
	showGraph |= ChooseGraphMode(m_debugInfo, graphName, "centrif", SVehicleDebugInfo::k_graphEffCentrifugal, m_movementInfo.effectiveCentrifugalAccel, -100.f, +100.f, -1.f, +1.f);
	showGraph |= ChooseGraphMode(m_debugInfo, graphName, "ideal-centrif", SVehicleDebugInfo::k_graphCentrifugal, m_movementInfo.centrifugalAccel, -100.f, +100.f, -1.f, +1.f);
	if (showGraph==0)
	{
		m_debugInfo->pDebugHistory->ClearHistory();
		m_debugInfo->pDebugHistory->SetVisibility(false);
		m_debugInfo->graphMode = SVehicleDebugInfo::k_graphNone;
	}
}
#endif

void CVehicleMovementArcadeWheeled::GetCurrentWheelStatus(IPhysicalEntity* pPhysics)
{
	// Cache the current physics status once, rather than
	// getting it over-and-over again in every member function
	WriteLock lock(m_wheelStatusLock);
	const int numWheels = m_numWheels;
	for (int i=0; i<numWheels; i++)
	{
		m_wheelStatus[i].iWheel = i;
		if (pPhysics->GetStatus(&m_wheelStatus[i]) == 0)
		{
			new (&m_wheelStatus[i]) pe_status_wheel;
			m_wheelStatus[i].bContact = 0;
			m_wheelStatus[i].bSlip = 0;
			m_wheelStatus[i].contactSurfaceIdx = 0;
			m_wheelStatus[i].friction = 0.0f;
			m_wheelStatus[i].iWheel = i;
			m_wheelStatus[i].normContact = Vec3Constants<float>::fVec3_Zero;
			m_wheelStatus[i].partid = 0;
			m_wheelStatus[i].pCollider = nullptr;
			m_wheelStatus[i].ptContact = Vec3Constants<float>::fVec3_Zero;
			m_wheelStatus[i].r = 1.0f;
			m_wheelStatus[i].steer = 0.0f;
			m_wheelStatus[i].suspLen = 0.0f;
			m_wheelStatus[i].suspLen0 = 0.0f;
			m_wheelStatus[i].suspLenFull = 0.0f;
			m_wheelStatus[i].torque = 0.0f;
			m_wheelStatus[i].type = EPE_Status::ePE_status_wheel;
			m_wheelStatus[i].velSlip = Vec3Constants<float>::fVec3_Zero;
			m_wheelStatus[i].w = 0.0f;
		}
	}
}


//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::Update(const float deltaTime)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	IEntity* pEntity = m_pVehicle->GetEntity();
	IPhysicalEntity* pPhysics = GetPhysics();
	if(!pPhysics)
	{
		assert(0 && "[CVehicleMovementArcadeWheeled::Update]: PhysicalEntity NULL!");
		return;
	}

	const SVehicleStatus& status = m_pVehicle->GetStatus();
	m_passengerCount = status.passengerCount;

	if (!pPhysics->GetStatus(&m_vehicleStatus))
		return;

	if (m_largeObjectInfo.countDownTimer>0.f)
	{
		m_largeObjectInfo.countDownTimer -= deltaTime;
		if (m_largeObjectInfo.countDownTimer<=0.f)
		{
			m_largeObjectInfo.countDownTimer = 0.f;
			m_largeObjectInfo.kicker = 0;
		}
		EnableLowLevelPhysics(k_frictionUseLowLevel, 0);
	}

	CVehicleMovementBase::Update(deltaTime);
	UpdateWaterLevels();
	UpdateSounds(deltaTime);
	UpdateBrakes(deltaTime);


	int notDistant = m_isProbablyDistant^1;

	if (notDistant && m_blownTires && m_carParams.steerTrackNeutralTurn == 0.f)
		m_tireBlownTimer += deltaTime;       

#if ENABLE_VEHICLE_DEBUG
	DebugDrawMovement(deltaTime);
#endif

	// update reversing
	if(notDistant && IsPowered() && m_actorId)
	{
		const float vel = m_localSpeed.y;

		if(vel < -0.1f && m_action.pedal < -0.1f)
		{
			if (m_reverseTimer == 0.f)
			{
				SVehicleEventParams params;
				params.bParam = true;
				m_pVehicle->BroadcastVehicleEvent(eVE_Reversing, params);
			}

			m_reverseTimer += deltaTime;
		}
		else
		{
			if(m_reverseTimer > 0.0f)
			{
				SVehicleEventParams params;
				params.bParam = false;
				m_pVehicle->BroadcastVehicleEvent(eVE_Reversing, params);
			}

			m_reverseTimer = 0.f;
		}
	}
	
	if (IsMovementProcessingEnabled()==false || m_isEnginePowered==false )
		EnableLowLevelPhysics(k_frictionUseLowLevel, 0);

	const SVehicleDamageParams& damageParams = m_pVehicle->GetDamageParams();
	m_submergedRatioMax = damageParams.submergedRatioMax;

	if (gEnv->bMultiplayer)
	{
		IActor* pActor = m_pVehicle->GetDriver();
		if (pActor)
		{
			m_netLerp = ((CActor*)pActor)->IsRemote();
		}
		else
		{
			m_netLerp = false;
		}
	}

	// Reset stationary handbrake?

	if(m_stationaryHandbrakeResetTimer > 0.0f)
	{
		m_stationaryHandbrakeResetTimer -= deltaTime;

		if(m_stationaryHandbrakeResetTimer <= 0.0f)
		{
			m_stationaryHandbrake = true;
		}
	}
}

void CVehicleMovementArcadeWheeled::PostUpdateView(SViewParams &viewParams, EntityId playerId)
{
	IActor* pActor = m_pVehicle->GetDriver();
	if (pActor==NULL)
		return;

	IVehicleSeat* pSeat = m_pVehicle->GetSeatForPassenger(pActor->GetEntityId());
	if (pSeat==NULL)
		return;

	IVehicleView* pView = pSeat->GetCurrentView();
	if (pView==NULL)
		return;
	
	bool bThirdPerson = pView->IsThirdPerson();
	if (bThirdPerson)
		return;

	float dt = gEnv->pTimer->GetFrameTime();
	float throttle = m_isEnginePowered ? m_movementAction.power : 0.f;
	int viewIndex = (throttle>0.1f && m_action.bHandBrake) ? 1 : 0;
	const SViewAdjustment* va = &m_pSharedParams->viewAdjustment[viewIndex];

	// Update the view position using steering and centrifugal force
	float steering = m_steering / DEG2RAD(m_steerMax);
	Quat vehicleWorldRot = m_pVehicle->GetEntity()->GetWorldRotation();
	const Vec3 xAxis = vehicleWorldRot.GetColumn0();
	const Vec3 yAxis = vehicleWorldRot.GetColumn1();

	const float topSpeed = m_pSharedParams->handling.topSpeed;
	const float invTopSpeed = 1.f/topSpeed;
	const float speed = m_localSpeed.y;
	const float speedNorm = clamp_tpl(speed*invTopSpeed, 0.f, 1.f);
	const float linearAdjustment = steering*speedNorm;

	//float cent = 0.5f*(m_movementInfo.centrifugalAccel+m_movementInfo.effectiveCentrifugalAccel);
	float cent = m_movementInfo.centrifugalAccel;
	float angle = clamp_tpl(-linearAdjustment*va->rotate + cent*va->rotateCent, -va->maxRotate, +va->maxRotate);
	float lateral = clamp_tpl(linearAdjustment*va->lateralDisp + cent*va->lateralCent, -va->maxLateralDisp, +va->maxLateralDisp);
	float forward = clamp_tpl(-speedNorm*va->forwardResponse, -va->maxForwardDisp, +va->maxForwardDisp);

	m_handling.viewRotate += (angle - m_handling.viewRotate) * approxOneExp(dt*va->rotateSpeed);
	m_handling.viewOffset += (Vec3(lateral, forward, 0.f) - m_handling.viewOffset) * approxOneExp(dt*va->dispSpeed);
	
	// Vibration
	float accelNorm = min(1.f, m_localAccel.GetLengthSquared()*sqr(va->vibrationAccelResponse));
	float amp = va->vibrationAmpSpeed * max(speedNorm, accelNorm);
	float freq = va->vibrationFrequency * min(1.f, (topSpeed+m_speed)*0.5f*invTopSpeed);
	m_handling.fpViewVibration.SetAmpFreq(amp, freq);
	
	// Suspension Compression
	float compression = 0.f;
	for (int i=0; i<m_numWheels; i++)
		compression += m_wheels[i].compression;
	compression *= m_invNumWheels;
	m_handling.viewOffset.z += (va->suspensionAmp*clamp_tpl(compression*va->suspensionResponse,-1.f, +1.f) - m_handling.viewOffset.z)*approxOneExp(va->suspensionSharpness*dt);

	// Finally adjust the view settings
	Quat dq = Quat::CreateRotationZ(m_handling.viewRotate);
	viewParams.rotation *= dq;
	viewParams.position += xAxis * m_handling.viewOffset.x;
	viewParams.position += yAxis * m_handling.viewOffset.y;
	viewParams.position.z += m_handling.fpViewVibration.Update(dt) + m_handling.viewOffset.z;
}
	
//------------------------------------------------------------------------
int CVehicleMovementArcadeWheeled::GetStatus(SVehicleMovementStatus* status)
{
	if (status->typeId==eVMS_wheeled)
	{
		SVehicleMovementStatusWheeled* wheeled = (SVehicleMovementStatusWheeled*)status;
		wheeled->numWheels = m_numWheels;
		wheeled->suspensionCompressionRate = 0.f;
		for (int i=0; i<m_numWheels; i++)
			wheeled->suspensionCompressionRate += m_wheels[i].compression;
		wheeled->suspensionCompressionRate *= m_invNumWheels;
		return 1;
	}

	return CVehicleMovementBase::GetStatus(status);
}
	
//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::UpdateSounds(const float deltaTime)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );
	
	if (m_isProbablyDistant)
		return;

	if (m_pVehicle->GetStatus().health > 0.f)
		UpdateSuspensionSound(deltaTime);

	if (!gEnv->IsClient())
		return;

	auto pIEntityAudioComponent = GetAudioProxy();
	
	if (m_isEnginePowered && !m_isEngineGoingOff)
	{
		float oscillation = m_gears.changedUp ? expf(-sqr(m_gears.timer*m_pSharedParams->gears.gearChangeSpeed2))*sinf(m_gears.timer*m_pSharedParams->gears.gearOscillationFrequency*gf_PI2) : 0.f;
		m_rpmScale = clamp_tpl(m_gears.curRpm + m_pSharedParams->gears.gearOscillationAmp*oscillation, 0.f, 1.f);
		if (pIEntityAudioComponent)
			pIEntityAudioComponent->SetParameter(m_audioControlIDs[eSID_VehicleRPM], m_rpmScale);
		//SetSoundParam(eSID_Run, "rpm_scale", m_rpmScale);
		SetSoundParam(eSID_Run, "rpm_load", m_gears.rpmLoad);
		SetSoundParam(eSID_Run, "clutch", clamp_tpl(m_gears.modulation + m_pSharedParams->gears.gearOscillationAmp2*oscillation, 0.f, 1.f));
		SetSoundParam(eSID_Run, "gear_oscillation", oscillation);

		if (DoGearSound() && m_gears.doGearChange)
		{ 
			ExecuteTrigger(eSID_Gear);
		}
	
		if (m_action.bHandBrake)
			StopTrigger(eSID_Boost);

#if ENABLE_VEHICLE_DEBUG
		// Update the debug gear-change bar
		if (m_debugInfo)
		{
			if (m_gears.doGearChange)
				m_debugInfo->debugGearChange.Reset(1.f);
			else
				m_debugInfo->debugGearChange.Update(0.f, deltaTime);
			m_debugInfo->debugGearChange.m_value = m_debugInfo->debugGearChange.m_filteredValue;
		}
#endif
	}
	
	m_gears.doGearChange = 0;
	
	//SetSoundParam(eSID_Run, "surface", m_surfaceSoundStats.surfaceParam); 

	//=================================================
	//  Skid Amount
	//=================================================
	float throttle = m_isEnginePowered ? m_movementAction.power : 0.f;
	float skidBrakes = 0.f;
	float skidCentIdeal = fabsf(m_movementInfo.centrifugalAccel)*m_pSharedParams->soundParams.skidCentrifugalFactor;
	float skidCentEff = fabsf(m_movementInfo.effectiveCentrifugalAccel)*m_pSharedParams->soundParams.skidCentrifugalFactor;
	float skidCent = max(skidCentIdeal, skidCentEff);
	float skidForward = fabsf(m_movementInfo.averageSlipSpeedForward)*m_pSharedParams->soundParams.skidForwardFactor;
	float skidLateral = fabsf(m_movementInfo.averageSlipSpeedLateral)*m_pSharedParams->soundParams.skidLateralFactor;
	if (m_action.bHandBrake)
		skidBrakes = (fabsf(throttle)>0.f) ? m_pSharedParams->soundParams.skidPowerLockFactor : m_pSharedParams->soundParams.skidBrakeFactor;
		
	// Combine and Morph the skid-value
	float totalSkidValue = sqrtf(sqr(max(skidCent, skidLateral)) + sqr(skidForward)) + skidBrakes;
	m_movementInfo.skidValue += (totalSkidValue - m_movementInfo.skidValue) * approxOneExp(deltaTime*m_pSharedParams->soundParams.skidLerpSpeed);
	m_movementInfo.skidValue = clamp_tpl(m_movementInfo.skidValue, 0.f, 1.f);

	if (pIEntityAudioComponent)
		pIEntityAudioComponent->SetParameter(m_audioControlIDs[eSID_VehicleSlip], m_movementInfo.skidValue);

	// tire slip sound
	{
		//ISound* pSound = GetSound(eSID_Slip);    

		if (m_surfaceSoundStats.slipRatio > 0.08f)
		{ 
			float slipTimerPrev = m_surfaceSoundStats.slipTimer;
			m_surfaceSoundStats.slipTimer += deltaTime;
			REINST("needs verification!");
			/*const static float slipSoundMinTime = 0.12f;
			if (!pSound && slipTimerPrev <= slipSoundMinTime && m_surfaceSoundStats.slipTimer > slipSoundMinTime)
			{
				pSound = PlaySound(eSID_Slip);
			} */     
		}
		else if (m_surfaceSoundStats.slipRatio < 0.03f && m_surfaceSoundStats.slipTimer > 0.f)
		{
			m_surfaceSoundStats.slipTimer -= deltaTime;

			if (m_surfaceSoundStats.slipTimer <= 0.f)
			{
				StopTrigger(eSID_Slip);
				m_surfaceSoundStats.slipTimer = 0.f;
			}      
		}

		/*if (pSound)
		{
		SetSoundParam(eSID_Slip, "slip_speed", m_movementInfo.skidValue);
		SetSoundParam(eSID_Slip, "surface", m_surfaceSoundStats.surfaceParam);
		SetSoundParam(eSID_Slip, "scratch", (float)m_surfaceSoundStats.scratching);
		SetSoundParam(eSID_Slip, "in_out", m_soundStats.inout);
		}  */ 
	}
}


//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::UpdateSuspension(const float deltaTime)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	float dt = max( deltaTime, 0.005f);

	IPhysicalEntity* pPhysics = GetPhysics();

	// update suspension and friction, if needed      
	bool bSuspUpdate = false;

	float diffSusp = m_pSharedParams->suspDampingMax - m_pSharedParams->suspDampingMin;    
	float diffStabi = m_pSharedParams->stabiMax - m_pSharedParams->stabiMin;

	if ((fabsf(diffSusp) + fabsf(diffStabi)) != 0.f) // if (diffSusp || diffStabi)
	{
		const float speed = m_physStatus[k_mainThread].v.len();
		if (fabsf(m_speedSuspUpdated-speed) > 0.25f) // only update when speed changes
		{
			float maxSpeed = (float)__fsel(-m_pSharedParams->suspDampingMaxSpeed, 0.15f*m_maxSpeed, m_pSharedParams->suspDampingMaxSpeed);
			float speedNorm = min(1.f, speed/maxSpeed);

			if (diffSusp)
			{
				m_suspDamping = m_pSharedParams->suspDampingMin + (speedNorm * diffSusp);
				bSuspUpdate = true;
			}           

			if (diffStabi)
			{
				m_stabi = m_pSharedParams->stabiMin + (speedNorm * diffStabi);

				pe_params_car params;
				params.kStabilizer = m_stabi;
				pPhysics->SetParams(&params, 1);        
			}

			m_speedSuspUpdated = speed;    
		}
	}

	m_compressionMax = 0.f;
	int numRot = 0;  
	m_surfaceSoundStats.scratching = 0;

	ReadLock lock(m_wheelStatusLock);

	const int numWheels = m_numWheels;
	for (int i=0; i<numWheels; ++i)
	{ 
		pe_params_wheel wheelParams;
		bool bUpdate = bSuspUpdate;
		IVehicleWheel* pWheel = m_wheels[i].wheelPart->GetIWheel();

		const pe_status_wheel &ws = m_wheelStatus[i];

		numRot += ws.bContact;

		if (bSuspUpdate)
		{
			wheelParams.iWheel = i;      
			wheelParams.kDamping = m_suspDamping;
			pPhysics->SetParams(&wheelParams, THREAD_SAFE);
		}

		// check for hard bump
		if (ws.suspLen0 > 0.01f)
		{ 
			// compression as fraction of relaxed length over time
			m_wheels[i].compression = ((m_wheels[i].suspLen-ws.suspLen)/ws.suspLen0) / dt;
			m_compressionMax = max(m_compressionMax, m_wheels[i].compression);
		}
		m_wheels[i].suspLen = ws.suspLen;
	}  

	m_wheelContacts = numRot;
}

//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::UpdateBrakes(const float deltaTime)
{
	if (m_movementAction.brake || m_pVehicle->GetStatus().health <= 0.f)
		m_action.bHandBrake = 1;
	else
		m_action.bHandBrake = 0;

	if (m_pSharedParams->isBreakingOnIdle && fabsf(m_movementAction.power)<0.1f)
	{
		const bool isTank = m_pSharedParams->tankHandling.additionalSteeringAtMaxSpeed > 0.f;
		m_action.bHandBrake = (!isTank || fabsf(m_movementAction.rotateYaw)<0.1f) ? 1 : 0;
	}

	// brake lights should come on if... (+ engine is on, and actor is driving)
	//	- handbrake is activated 
	//	- pedal is pressed, and the vehicle is moving in the opposite direction
	if (IsPowered() && m_actorId )
	{
		const float forwardSpeed = m_localSpeed.y;

		if ((fabsf(m_action.pedal) > 0.1f && fabsf(forwardSpeed) > 0.1f && (forwardSpeed*m_action.pedal) < 0.f)
			|| m_action.bHandBrake)
		{
			if (m_brakeTimer == 0.f)
			{
				SVehicleEventParams params;
				params.bParam = true;
				m_pVehicle->BroadcastVehicleEvent(eVE_Brake, params);
			}

			m_brakeTimer += deltaTime;  
		}
		else
		{
			if (m_brakeTimer > 0.f)
			{
				SVehicleEventParams params;
				params.bParam = false;
				m_pVehicle->BroadcastVehicleEvent(eVE_Brake, params);

				// airbrake sound
				REINST("needs verification!");
				/*if (m_pSharedParams->soundParams.airbrakeTime > 0.f)
				{ 
					if (m_brakeTimer > m_pSharedParams->soundParams.airbrakeTime)
					{
						char name[256];
						cry_sprintf(name, "sounds/vehicles:%s:airbrake", m_pVehicle->GetEntity()->GetClass()->GetName());
						m_pIEntityAudioComponent->PlaySound(name, Vec3(0), FORWARD_DIRECTION, FLAG_SOUND_DEFAULT_3D, 0, eSoundSemantic_Vehicle);                
					}          
				}*/
			}
			
			m_brakeTimer = 0.f;  
		}
	}
}

//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::UpdateSuspensionSound(const float deltaTime)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	ReadLock lock(m_wheelStatusLock);

	const float speed = m_speed;
	const int numWheels = m_numWheels;
	const SSoundParams* soundParams = &m_pSharedParams->soundParams;

	int soundMatId = 0;

	m_lostContactTimer += deltaTime;
	for (int i=0; i<numWheels; ++i)
	{
		const pe_status_wheel* ws = &m_wheelStatus[i];

		if (ws->bContact)
		{ 
			// sound-related                   
			if (!m_surfaceSoundStats.scratching && soundMatId==0 && speed > 0.001f)
			{
				if(m_wheels[i].waterLevel > (ws->ptContact.z + 0.02f))
				{        
					soundMatId = gEnv->pPhysicalWorld->GetWaterMat();
					m_lostContactTimer = 0;
				}
				else if (ws->contactSurfaceIdx > 0 /*&& soundMatId != gEnv->pPhysicalWorld->GetWaterMat()*/)
				{   
					if (m_wheels[i].wheelPart->GetState() == IVehiclePart::eVGS_Damaged1)
						m_surfaceSoundStats.scratching = 1;

					soundMatId = ws->contactSurfaceIdx;
					m_lostContactTimer = 0;
				}
			}      
		}
	}

	m_lastBump += deltaTime;
	if (m_pVehicle->GetStatus().speed > soundParams->bumpMinSpeed && m_lastBump > 1.f)
	{ 
		if (m_compressionMax > soundParams->bumpMinSusp)
		{	
			const float fStroke = min(1.f, soundParams->bumpIntensityMult*m_compressionMax/soundParams->bumpMinSusp);
			auto pIEntityAudioComponent = GetAudioProxy();
			if (pIEntityAudioComponent)
				pIEntityAudioComponent->SetParameter(m_audioControlIDs[eSID_VehicleStroke], fStroke);
			ExecuteTrigger(eSID_Bump);
			m_lastBump = 0;    
		}            
	}   

	// set surface sound type
	if (soundMatId != m_surfaceSoundStats.matId)
	{ 
		if (m_lostContactTimer == 0.f || m_lostContactTimer > 3.f)
		{  
			m_surfaceSoundStats.matId = soundMatId;
		}
	} 

}


//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementArcadeWheeled::ProcessAI(const float deltaTime)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	float dt = max( deltaTime,0.005f);
	SVehiclePhysicsStatus* physStatus = &m_physStatus[k_physicsThread];

	m_movementAction.brake = false;
	m_movementAction.rotateYaw = 0.0f;
	m_movementAction.power = 0.0f;
	m_action.bHandBrake = 0;

	float inputSpeed = 0.0f;
	{
		if (m_aiRequest.HasDesiredSpeed())
			inputSpeed = m_aiRequest.GetDesiredSpeed();
		Limit(inputSpeed, -m_maxSpeed, m_maxSpeed);
	}

	Vec3 vMove(ZERO);
	{
		if (m_aiRequest.HasMoveTarget())
			vMove = ( m_aiRequest.GetMoveTarget() - physStatus->pos ).GetNormalizedSafe();
	}

	//start calculation
	if ( fabsf( inputSpeed ) < 0.0001f || m_tireBlownTimer > 1.5f )
	{
		m_movementAction.brake = true;
		m_action.bHandBrake = 1;
	}
	else
	{

		Matrix33 entRotMatInvert( physStatus->q );
		entRotMatInvert.Invert();
		float currentAngleSpeed = RAD2DEG(-physStatus->w.z);

		const float maxSteer = RAD2DEG(gf_PI/4.f); // fix maxsteer, shouldn't change  
		Vec3 vVel = physStatus->v;
		Vec3 vVelR = entRotMatInvert * vVel;
		float currentSpeed =vVel.GetLength();
		vVelR.NormalizeSafe();
		if ( vVelR.Dot( FORWARD_DIRECTION ) < 0 )
			currentSpeed *= -1.0f;

		// calculate pedal
		const float accScale = 0.5f;
		m_movementAction.power = (inputSpeed - currentSpeed) * accScale;
		Limit( m_movementAction.power, -1.0f, 1.0f);

		// calculate angles
		Vec3 vMoveR = entRotMatInvert * vMove;
		Vec3 vFwd	= FORWARD_DIRECTION;

		vMoveR.z =0.0f;
		vMoveR.NormalizeSafe();

		float cosAngle = vFwd.Dot(vMoveR);
		float angle = RAD2DEG( acos_tpl(cosAngle));
		if ( vMoveR.Dot( Vec3( 1.0f,0.0f,0.0f ) )< 0 )
			angle = -angle;

//		int step =0;
		m_movementAction.rotateYaw = angle * 1.75f/ maxSteer; 

		// implementation 1. if there is enough angle speed, we don't need to steer more
		if ( fabsf(currentAngleSpeed) > fabsf(angle) && angle*currentAngleSpeed > 0.0f )
		{
			m_movementAction.rotateYaw = m_steering*0.995f; 
//			step =1;
		}

		// implementation 2. if we can guess we reach the distination angle soon, start counter steer.
		float predictDelta = inputSpeed < 0.0f ? 0.1f : 0.07f;
		float dict = angle + predictDelta * ( angle - m_prevAngle) / dt ;
		if ( dict*currentAngleSpeed<0.0f )
		{
			if ( fabsf( angle ) < 2.0f )
			{
				m_movementAction.rotateYaw = angle* 1.75f/ maxSteer;
//				step =3;
			}
			else
			{
				m_movementAction.rotateYaw = currentAngleSpeed < 0.0f ? 1.0f : -1.0f; 
//				step =2;
			}
		}

		if ( fabsf( angle ) > 20.0f && currentSpeed > 7.0f ) 
		{
			m_movementAction.power *= 0.0f ;
//			step =4;
		}

		// for more tight condition to make curve.
		if ( fabsf( angle ) > 40.0f && currentSpeed > 3.0f ) 
		{
			m_movementAction.power *= 0.0f ;
//			step =5;
		}

		//		m_prevAngle =  angle;
		//		char buf[1024];
		///		cry_sprintf(buf, "steering	%4.2f	%4.2f %4.2f	%4.2f	%4.2f	%4.2f	%d\n", deltaTime,currentSpeed,angle,currentAngleSpeed, m_movementAction.rotateYaw,currentAngleSpeed-m_prevAngle,step);
		//		OutputDebugString( buf );
	}

}

//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementArcadeWheeled::ProcessMovement(const float deltaTime)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	m_netActionSync.UpdateObject(this);

	CryAutoCriticalSection lk(m_lock);

	CVehicleMovementBase::ProcessMovement(deltaTime);
	
	IPhysicalEntity* pPhysics = GetPhysics();
	if (pPhysics==NULL) 
		return;

	SVehiclePhysicsStatus* physStatus = &m_physStatus[k_physicsThread];
	
	NETINPUT_TRACE(m_pVehicle->GetEntityId(), m_action.pedal);
	
	float speed = physStatus->v.len();
	bool isDestroyed = m_pVehicle->IsDestroyed();

	bool canApplyAction = (gEnv->bServer || !m_netLerp);	// Dont apply an Action unless this is a player controlled vehicle or it's the server

	bool isAIDisabled=false;
	IEntity* pDriver = gEnv->pEntitySystem->GetEntity(m_actorId);
	if (pDriver)
	{
		if (m_movementAction.isAI)
		{
			IAIObject	*pAI=pDriver->GetAI();
			isAIDisabled=pAI ? !pAI->IsEnabled() : true;
		}
	}

	if (isAIDisabled || !((m_actorId || m_remotePilot) && m_isEnginePowered) || isDestroyed || IsMovementProcessingEnabled()==false)
	{
		const float sleepTime = 3.0f;

		if ( m_passengerCount > 0 || ( isDestroyed && m_bForceSleep == false ))
		{
			UpdateSuspension(deltaTime);
		}

		//if (m_frictionState!=k_frictionUseLowLevel)
			EnableLowLevelPhysics(k_frictionUseLowLevel, THREAD_SAFE);

		if (canApplyAction)
		{
			m_action.bHandBrake = m_stationaryHandbrake;
			m_action.pedal = 0;
			pPhysics->Action(&m_action, THREAD_SAFE);
		}

		if ( isDestroyed && m_bForceSleep == false )
		{
			int numContact= 0;
			const int numWheels = m_numWheels;
			for (int i=0; i<numWheels; ++i)
			{
				if (m_wheelStatus[i].bContact)
					numContact ++;
			}
			if ( numContact > numWheels/2 || speed<0.2f )
				m_forceSleepTimer += deltaTime;
			else
				m_forceSleepTimer = max(0.0f,m_forceSleepTimer-deltaTime);

			if ( m_forceSleepTimer > sleepTime )
			{
				pe_params_car params;
				params.minEnergy = 0.05f;
				pPhysics->SetParams(&params, 1);
				m_bForceSleep = true;
			}
		}
		return;

	}

	// Reset stationary handbrake.
	m_stationaryHandbrake = true;

	// moved to main thread
	UpdateSuspension(deltaTime);   	
	//UpdateBrakes(deltaTime);
		
	float damageMul = 1.0f - 0.4f*m_damage;  
	bool bInWater = physStatus->submergedFraction > 0.01f;
	if (canApplyAction)
	{
		pe_action_drive prevAction = m_action; 
		
		// speed ratio    
		float speedRel = min(speed, m_pSharedParams->vMaxSteerMax) / m_pSharedParams->vMaxSteerMax;  

		// reduce actual pedal with increasing steering and velocity
		float maxPedal = 1 - (speedRel * fabsf(m_movementAction.rotateYaw) * m_pSharedParams->pedalLimitMax);  
		float submergeMul = 1.0f;  
		float totalMul = 1.0f;  
		if ( GetMovementType()!=IVehicleMovement::eVMT_Amphibious && bInWater )
		{
			submergeMul = max( 0.0f, 0.04f - physStatus->submergedFraction ) * 10.0f;
			submergeMul *=submergeMul;
			submergeMul = max( 0.2f, submergeMul );
		}

		totalMul = max( 0.4f, damageMul *  submergeMul );
		m_action.pedal = clamp_tpl(m_movementAction.power, -maxPedal, maxPedal ) * totalMul;

		// make sure cars can't drive under water
		if(GetMovementType()!=IVehicleMovement::eVMT_Amphibious && physStatus->submergedFraction >= m_submergedRatioMax && m_damage >= 0.99f)
		{
			m_action.pedal = 0.0f;
		}
		
		m_steering = CalcSteering(m_steering, speedRel, m_movementAction.rotateYaw, deltaTime);
		const bool isTank = m_pSharedParams->tankHandling.additionalSteeringAtMaxSpeed > 0.f;
		m_action.steer = isTank ? 0.f : m_steering;

		if ((!is_unused(m_action.pedal) && !is_unused(prevAction.pedal) && abs(m_action.pedal-prevAction.pedal)>FLT_EPSILON) || 
			(!is_unused(m_action.steer) && !is_unused(prevAction.steer) && abs(m_action.steer-prevAction.steer)>FLT_EPSILON) || 
			(!is_unused(m_action.clutch) && !is_unused(prevAction.clutch) && abs(m_action.clutch-prevAction.clutch)>FLT_EPSILON) || 
			m_action.bHandBrake != prevAction.bHandBrake || 
			m_action.iGear != prevAction.iGear)
		{
			pPhysics->Action(&m_action, THREAD_SAFE);
		}
	}

	Vec3 vUp(physStatus->q.GetColumn2());
	Vec3 vUnitUp(0.0f,0.0f,1.0f);

	float slopedot = vUp.Dot( vUnitUp );
	bool bSteep =  fabsf(slopedot) < 0.7f;
	{ //fix for 30911
		if ( bSteep && speed > 7.5f )
		{
			Vec3 vVelNorm = physStatus->v.GetNormalizedSafe();
			if ( vVelNorm.Dot(vUnitUp)> 0.0f )
			{
				pe_action_impulse imp;
				imp.impulse = -physStatus->v;
				imp.impulse *= deltaTime * physStatus->mass*5.0f;      
				imp.point = physStatus->centerOfMass;
				imp.iApplyTime = 0;
				pPhysics->Action(&imp, THREAD_SAFE);
			}
		}
	}
	
	{
		// fix driving on steep angles DT: 1570, tip the vehicle over
		Vec3 xAxis(physStatus->q.GetColumn0());
		Vec3 yAxis(physStatus->q.GetColumn1());
		if (fabsf(xAxis.z)>0.75f && vUp.z>0.f)	// Threshold angle reached
		{
			pe_action_impulse imp;
			imp.impulse = vUp * deltaTime * physStatus->mass * 5.f;
			imp.angImpulse = fsgnf(xAxis.z)*yAxis * deltaTime * physStatus->mass * -9.f;
			pPhysics->Action(&imp, THREAD_SAFE);
		}
	}
	
	if (m_numWheels)
	{
		if (m_frictionState!=(int8)k_frictionUseHiLevel)
			EnableLowLevelPhysics(k_frictionUseHiLevel, THREAD_SAFE);
		if (deltaTime>0.f)
			InternalPhysicsTick(deltaTime);
	
		if ( !bSteep && Boosting() )
			ApplyBoost(speed, m_pSharedParams->handling.boostTopSpeed*GetWheelCondition()*damageMul, m_boostStrength, deltaTime);  
	}

	EjectionTest(deltaTime);

	if (!m_pVehicle->GetStatus().doingNetPrediction)
	{
		if (m_netActionSync.PublishActions( CNetworkMovementArcadeWheeled(this) ))
			CHANGED_NETWORK_STATE(m_pVehicle,  CNetworkMovementArcadeWheeled::CONTROLLED_ASPECT );
	}
}
	
void CVehicleMovementArcadeWheeled::EnableLowLevelPhysics(int state, int bThreadSafe)
{
	WriteLock lock(m_frictionStateLock);
	IPhysicalEntity* pPhysics = GetPhysics();
	if(pPhysics)
	{
		pe_params_car carParams;
		pe_params_wheel wheelParams;
		const int numWheels = m_numWheels;

		float kLatFriction0 = 1.f;
		float kLatFriction = 0.f;

		if (state==k_frictionUseLowLevel)
		{
			kLatFriction = 1.f - m_largeObjectInfo.countDownTimer*m_largeObjectInfo.invStartTime;
			carParams.minBrakingFriction = 0.f;
			carParams.maxBrakingFriction = 100.f;
			carParams.kDynFriction = 1.f;
			wheelParams.minFriction = 0.f;
			wheelParams.maxFriction = 100.0f;
			wheelParams.bCanBrake = (m_largeObjectInfo.countDownTimer>0.f) ? 0 : 1;
			wheelParams.bDriving = 0;
			// Make sure the HB is ON!
			m_action.bHandBrake = 1;
			m_action.pedal = 0;
			pPhysics->Action(&m_action, bThreadSafe);
		}
		else // Assume Hi Level, game-side friction
		{
			carParams.minBrakingFriction = 0.f;
			carParams.maxBrakingFriction = 0.f;
			carParams.kDynFriction = 0.f;
			wheelParams.minFriction = 0.f;
			wheelParams.maxFriction = 0.f;
			wheelParams.bCanBrake = 0;
			wheelParams.bDriving = 0;
		}

		pPhysics->SetParams(&carParams, bThreadSafe);

		if (m_largeObjectInfo.countDownTimer==0.f)
		{
			for (int i=0; i<numWheels; i++)
			{
				// Kill the low level friction
				wheelParams.iWheel = i;      
				pPhysics->SetParams(&wheelParams, bThreadSafe);
			}
		}
		else
		{
			if (m_largeObjectFrictionType!=0 && m_localSpeed.GetLengthSquared()>1.f)
			{
				// Find the wheel that is behind
				const int c0 = m_chassis.contactIdx0;
				const int c1 = m_chassis.contactIdx1;
				const int c2 = m_chassis.contactIdx2;
				const int c3 = m_chassis.contactIdx3;
				if (m_localSpeed.x * m_wheels[c0].offset.x > 0.f && m_localSpeed.y*m_wheels[c0].offset.y < 0.f)
					m_largeObjectInfoWheelIdx = c0;
				if (m_localSpeed.x * m_wheels[c1].offset.x > 0.f && m_localSpeed.y*m_wheels[c1].offset.y < 0.f)
					m_largeObjectInfoWheelIdx = c1;
				if (m_localSpeed.x * m_wheels[c2].offset.x > 0.f && m_localSpeed.y*m_wheels[c2].offset.y < 0.f)
					m_largeObjectInfoWheelIdx = c2;
				if (m_localSpeed.x * m_wheels[c3].offset.x > 0.f && m_localSpeed.y*m_wheels[c3].offset.y < 0.f)
					m_largeObjectInfoWheelIdx = c3;
			}
			for (int i=0; i<numWheels; i++)
			{
				// Kill the low level friction
				wheelParams.iWheel = i;      
				if (m_largeObjectInfoWheelIdx == i)
				{
					wheelParams.kLatFriction = kLatFriction0;
				}
				else
				{
					wheelParams.kLatFriction = kLatFriction;
				}
				pPhysics->SetParams(&wheelParams, bThreadSafe);
			}
		}

		assert(state!=k_frictionNotSet);
		m_frictionState = (int8)state;
	}
}

static ILINE void SolveFriction(Vec3& dVel, Vec3& dAngVel, const Vec3& vel, const Vec3& angVel, SVehicleChassis* c, SVehicleWheel* w, ClampedImpulse* maxTractionImpulse, ClampedImpulse* maxLateralImpulse, float solverERP, float dt)
{
	Vec3 wheelVel = vel + angVel.cross(w->worldOffset);

	// Inline
	if (!w->locked)
	{
		float slipSpeed = -w->w * w->radius + wheelVel.dot(w->frictionDir[0]);
		float impulse = clampedImpulseApply(maxTractionImpulse, solverERP * slipSpeed * w->invWheelK);
		w->w += impulse * w->radius * w->invInertia;
		float velChange = - impulse * w->invMass;	// This is the vel change imparted on just the wheel
		// Bring velChange back to zero by applying impulse to chassis
		float denom = w->invMass + w->chassisK0;
		impulse = velChange / denom;
		addImpulse(dVel, dAngVel, c->invMass, w->chassisW0, impulse, w->frictionDir[0]);
	}
	else
	{
		float slipSpeed = wheelVel.dot(w->frictionDir[0]);
		float denom = w->chassisK0;
		float impulse = clampedImpulseApply(maxTractionImpulse, -solverERP*slipSpeed / denom);
		addImpulse(dVel, dAngVel, c->invMass, w->chassisW0, impulse, w->frictionDir[0]);
	}

	// Lateral
	{
		float errorV = wheelVel.dot(w->frictionDir[1]);
		float denom = w->chassisK1;
		float impulse0 = -solverERP * errorV / denom;
		float impulse = clampedImpulseApply(maxLateralImpulse, impulse0);
		addImpulse(dVel, dAngVel, c->invMass, w->chassisW1, impulse, w->frictionDir[1]);
	}
}

float CVehicleMovementArcadeWheeled::CalcSteering(float steer, float speedRel, float rotateYaw, float dt)
{
	float steerMax = GetMaxSteer(speedRel);
	float steeringFrac = rotateYaw;    
	float steerTarget = steeringFrac * steerMax;
	steerTarget = (float)__fsel(fabsf(steerTarget)-0.01f, steerTarget, 0.f);
	float steerError = steerTarget - steer;
	steerError = (float)__fsel(fabsf(steerError)-0.01f, steerError, 0.f);

	if ((steerError*steer) < 0.f)
	{
		// Decreasing, therefore use steerRelaxation speed
		// to relax towards central position, and calc any remaining dt
		float absSteer = fabsf(steer);
		float correction = dt * DEG2RAD(m_pSharedParams->steerRelaxation);
		absSteer = absSteer - correction;
		dt = (float)__fsel(absSteer, 0.f, absSteer/DEG2RAD(m_pSharedParams->steerRelaxation)); // Calculate any overshoot in dt
		steer = (float)__fsel(absSteer, (float)__fsel(steer, +absSteer, -absSteer), 0.f);
		steerError = steerTarget - steer;
	} 

	if (dt>0.f)
	{
		float steerSpeed = DEG2RAD(GetSteerSpeed(speedRel));
		steer = steer + fsgnf(steerError) * min(fabsf(steerError), steerSpeed * dt);
		//steer = clamp_tpl(steer, -steerMax, steerMax);
	}

	return steer;
}

void CVehicleMovementArcadeWheeled::InternalPhysicsTick(float dt)
{
	IPhysicalEntity* pPhysics = GetPhysics();
	if (pPhysics==NULL)
		return;

	GetCurrentWheelStatus(pPhysics);

	const int numWheels = m_numWheels;

	SVehicleChassis* c = &m_chassis;
	SVehiclePhysicsStatus* physStatus = &m_physStatus[k_physicsThread];

	Matrix33 bodyRot( physStatus->q );
	const Vec3 xAxis = bodyRot.GetColumn0();
	const Vec3 yAxis = bodyRot.GetColumn1();
	const Vec3 zAxis = bodyRot.GetColumn2();
	const Vec3 pos = physStatus->centerOfMass;

	Vec3 vel = physStatus->v;
	Vec3 angVel = physStatus->w;

#if ENABLE_VEHICLE_DEBUG
	DebugCheat(dt);
#endif

	const bool isTank = m_pSharedParams->tankHandling.additionalSteeringAtMaxSpeed > 0.f;
	//=============================================
	// Collision Mitigation 
	//=============================================
	float collisionThrottleScale = 0.2f*clamp_tpl(1.f-3.f*(fsgnf(m_movementAction.power)*m_collisionNorm.dot(yAxis)/physStatus->mass), 0.5f, 1.f) + 0.8f;
	vel *= collisionThrottleScale;
	if (isTank)
	{
		angVel = angVel - yAxis*(0.8f*(1.f-collisionThrottleScale)*(angVel.dot(yAxis)));
	}

	//=============================================
	// Get Wheel Status
	//=============================================

	int contactMap[2][2]={{0,0},{0,0}};
	int numContacts = 0;
	assert(numWheels <= maxWheels);

	if (m_numWheels > maxWheels)
	{
		GameWarning("Vehicle '%s' has broken setup! Number of wheels: %d exceeds maximum allowed number of wheels: %d - Skipping internal physics tick.", m_pEntity->GetName(), m_numWheels, maxWheels);
		return;
	}

	const float invNumWheels = m_invNumWheels;

	Vec3 contacts[maxWheels];
	float suspensionExtension = 0.f;
	float suspensionVelocity = 0.f;
	bool tankHasWheelContact = (zAxis.z>0.1f && fabsf(xAxis.z)<0.8f);

	for (int i=0; i<numWheels; ++i)
	{ 
		SVehicleWheel* w = &m_wheels[i];
		pe_status_wheel* ws = &m_wheelStatus[i];
		if (!isTank && ws->bContact)
		{
			contacts[i] = ws->ptContact;
			w->contact = 1.f;
			contactMap[w->axleIndex][w->offset.x>0.f]=1;
		}
		else
		{
			// Pretend that the contact was at the bottom of the wheel
			Vec3 bottom = w->offset;
			bottom.z += w->bottomOffset + ws->suspLen0 - ws->suspLen;
			contacts[i] = pos + bodyRot * bottom;
			w->contact = tankHasWheelContact ?  1.f : 0.f;
		}
		if (ws->suspLenFull != 0)
		{
			float extension = (ws->suspLen - ws->suspLen0)/ws->suspLenFull;
			//if (extension > 0.f) extension = 0.f;
			extension = (float)__fsel(extension, 0.f, extension);
			suspensionExtension += extension;
		}
	}
	suspensionExtension *= invNumWheels;

	numContacts = isTank ? (tankHasWheelContact ? 4 : 0) : (contactMap[0][0] + contactMap[0][1] + contactMap[1][0] + contactMap[1][1]);

	//==============================================
	// Scale friction and traction by the average 
	// suspension compression and velocity
	// BUT only for handbrake
	//==============================================
	if (m_action.bHandBrake)
	{
		m_handling.compressionScale = (1.f - m_pSharedParams->handling.compressionBoostHandBrake * suspensionExtension);
	}
	else
	{
		m_handling.compressionScale = (1.f - m_pSharedParams->handling.compressionBoost * suspensionExtension);
	}
	Limit(m_handling.compressionScale, 1.0f, 10.f);

	// Work out the base contact normal
	// This is somewhat faked, but works well enough
	if (numWheels>=4)
	{
		const int c0 = c->contactIdx0;
		const int c1 = c->contactIdx1;
		const int c2 = c->contactIdx2;
		const int c3 = c->contactIdx3;
		m_handling.contactNormal = (contacts[c3] - contacts[c0]).cross(contacts[c2] - contacts[c1]);
		m_handling.contactNormal.normalize();
	}
	else
	{
		m_handling.contactNormal = zAxis;
	}

	float speed = yAxis.dot(vel);
	float absSpeed = fabsf(speed);
	int bPowerLock = 0;

	//=============================================
	// Calculate invR from the steering wheels
	//=============================================
	float steering = m_steering / DEG2RAD(m_steerMax);
	m_invTurningRadius = 0.f;
	for(int i=0; i<numWheels; i++)
	{
		float alpha = m_wheelStatus[i].steer;
		if ((fabsf(m_wheels[i].offset.y) > 0.01f) && (fabsf(alpha) > 0.01f))
		{
			// NB: this is a signed quantity
			m_invTurningRadius -= tanf(alpha) / m_wheels[i].offset.y;
		}
	}
	m_invTurningRadius *= invNumWheels;

	//=============================================
	// Speed Reduction from steering
	//=============================================
	float topSpeed = m_boost ? m_pSharedParams->handling.boostTopSpeed : m_pSharedParams->handling.topSpeed;
	topSpeed = (float)__fsel(speed, topSpeed, m_pSharedParams->handling.reverseSpeed);
	topSpeed *= m_topSpeedFactor;
	float scale = m_action.bHandBrake ? 1.f : 1.f - (m_pSharedParams->handling.reductionAmount * fabsf(steering));// * approxExp(m_pSharedParams->handling.reductionRate*dt);
	topSpeed = topSpeed * scale;
	float throttle = m_isEnginePowered ? m_movementAction.power * scale : 0.f;
	const float speedNorm = min(absSpeed/topSpeed, 1.f);
	
	const float damageRPMScale = m_damageRPMScale;
	topSpeed = topSpeed * damageRPMScale;
	throttle = throttle * (0.7f + 0.3f * damageRPMScale);
	throttle *= collisionThrottleScale;
	
	//===================================================
	// Stabilisation and Angular Damping
	//===================================================
	const SStabilisation* stabilisation = &m_pSharedParams->stabilisation;
	const float angDamping = approxExp(dt*stabilisation->angDamping);
	const float rollSpeed = yAxis.dot(angVel); 

	// Angular damping of roll and tilt
	const Vec3 angVelZ = zAxis*(angVel*zAxis);
	angVel = (angVel - angVelZ)*angDamping + angVelZ;
	
	int leftContact = contactMap[0][0]|contactMap[0][1];
	int rightContact = contactMap[1][0]|contactMap[1][1];

	if (leftContact&rightContact)
		m_handling.lostContactOneSideTimer=0.f;
	else
		m_handling.lostContactOneSideTimer+=dt;

	// Additional roll damping, if lost contact on one side or both sides
	if ((leftContact&rightContact)==0)
		angVel -= yAxis*(approxOneExp(dt*stabilisation->rollDamping)*rollSpeed);

	// Stop updwards velocity of the vehicle is contact is lost
	if ((leftContact&rightContact)==0 && vel.z>0.f)
		vel.z *= approxExp(dt*stabilisation->upDamping);

	if ((leftContact|rightContact)==0 && zAxis.z>0.f)
	{
		Vec3 right = Vec3(xAxis.x, xAxis.y, 0.f);
		// Calculate an up axis to roll towards
		Vec3 up = right.cross(yAxis).GetNormalized();
		if (up.z < stabilisation->cosMaxTiltAngleAir)
		{
			Vec3 forward = Vec3(yAxis.x, yAxis.y, 0.f).GetNormalized();
			up = Vec3(stabilisation->sinMaxTiltAngleAir*forward.x, stabilisation->sinMaxTiltAngleAir*forward.y, stabilisation->cosMaxTiltAngleAir);
		}
		Quat quatNew;
		Predict(quatNew, m_physStatus[k_physicsThread].q, angVel, dt);
		Quat q = Quat::CreateRotationV0V1(quatNew.GetColumn2(), up);
		Vec3 required = angVel + (approxOneExp(stabilisation->rollfixAir*dt) * 2.f / dt) * q.v;
		angVel += (required - angVel) * approxOneExp(dt*10.f);
	}
	
	pe_simulation_params psim;
	pPhysics->GetParams(&psim);
	float physGravity=-psim.gravity.z;


	// Add grav here for this solver to account for
	vel.z -= physGravity*dt;

	//===============================================
	// Process Lateral and Traction Friction
	//===============================================
	bool invertTankYaw = false;
	if (m_movementAction.isAI)
	{
		invertTankYaw = false;
	}
	else if (g_pGameCVars->v_tankReverseInvertYaw == 1)
	{
		invertTankYaw = speed<-1.0f;
	}
	else if (g_pGameCVars->v_tankReverseInvertYaw == 2)
	{
		invertTankYaw = throttle<-0.1f;
	}
	{
		const float gravityScale = sqr(zAxis.z);
		const float gravity = 9.8f;
		ClampedImpulse maxTractionImpulse[maxWheels];
		ClampedImpulse maxLateralImpulse[maxWheels];
			
		bool lockAllWheels = !isTank && (absSpeed > 2.f) && ((speed * throttle) < 0.f);			// When throttle is opposite to current speed
		bool canDeccelerate = (absSpeed > (m_pSharedParams->handling.decceleration*dt));
		const float accelerationMultiplier = m_pSharedParams->handling.accelMultiplier1 + (m_pSharedParams->handling.accelMultiplier2 - m_pSharedParams->handling.accelMultiplier1)*speedNorm;
		float forcePerWheel = (m_boost ? m_pSharedParams->handling.boostAcceleration : m_pSharedParams->handling.acceleration) * m_chassis.mass * invNumWheels * m_accelFactor;	                // Assume all wheels are powered
		float forcePerWheel2 = m_pSharedParams->handling.decceleration * m_chassis.mass * invNumWheels;
		float handBrakeForce;

		forcePerWheel *= damageRPMScale;
		forcePerWheel2 *= damageRPMScale;
		
		if (m_action.bHandBrake && fabsf(throttle)>0.1f)
		{
			handBrakeForce = m_pSharedParams->handling.handBrakeDeccelerationPowerLock * m_chassis.mass;
			handBrakeForce *= damageRPMScale;
			bPowerLock = 1;
		}
		else
		{
			handBrakeForce = m_pSharedParams->handling.handBrakeDecceleration * m_chassis.mass;
			handBrakeForce *= damageRPMScale;
		}

		int numWheelsThatCanHadnBrake = 0;

		// Prepare
		float contact = 0.f;
		float avWheelSpeed = 0.f;
		for (int i=0; i<numWheels; i++)
		{
			SVehicleWheel* w = &m_wheels[i];
			avWheelSpeed += w->w;
			//w->contact += (1.f - w->contact) * approxExp(dt);
			if (w->bCanLock || lockAllWheels) numWheelsThatCanHadnBrake++;
		}

		contact = (float)numContacts * 0.25f;
		avWheelSpeed *= invNumWheels;

		TickGears(dt, avWheelSpeed, throttle, speed, contact);

		if (numWheelsThatCanHadnBrake) handBrakeForce /= (float)numWheelsThatCanHadnBrake;
		
		const float absSpeedNorm = fabsf(speedNorm);
		float tankDiffSpeed = steering * ( (1.f-absSpeedNorm) * m_pSharedParams->tankHandling.additionalSteeringStationary + m_pSharedParams->tankHandling.additionalSteeringAtMaxSpeed*absSpeedNorm );
		tankDiffSpeed = invertTankYaw ? -tankDiffSpeed : tankDiffSpeed;
		tankDiffSpeed *= damageRPMScale;
		
		float averageSlipSpeedLateral = 0.f;
		float averageSlipSpeedForward = 0.f;
		
		// find and store the friction per wheel
		float surfaceFriction[maxWheels]; // per wheel
		for (int i = 0; i < maxWheels; i++)
		{
			surfaceFriction[i] = 1.0f;
		}

		if (contact > 0.f)
		{
			for (int i=0; i<numWheels; i++)
			{
				// Check the surface per wheel and store the friction for later
				float wheelfriction = -1.0f;
				float dummy;
				unsigned int flags;
				gEnv->pPhysicalWorld->GetSurfaceParameters(m_wheelStatus[i].contactSurfaceIdx, dummy, wheelfriction, flags);

				// if we found a surface under our wheel, multiply it
				if(wheelfriction >= 0.0f)
				{
					surfaceFriction[i] = wheelfriction;
				}
			}
		}

		for (int i=0; i<numWheels; i++)
		{
			SVehicleWheel* w = &m_wheels[i];

			w->contactNormal = m_handling.contactNormal;
			if (m_wheelStatus[i].bContact) w->contactNormal = w->contactNormal + m_wheelStatus[i].normContact;
			//w->contactNormal = w->contactNormal - axis*(axis.dot(w->contactNormal));
			w->contactNormal.normalize();

			if (isTank)
			{
				// When turning, artifically bring wheels closer to the centre so that lateral friction does not stop the the tank from turning
				float s = (0.3f - 0.25f * fabsf(steering));
				w->worldOffset = (w->offset.x * xAxis) + ((s*w->offset.y) * yAxis) + (w->offset.z * zAxis);
				w->w = avWheelSpeed;
			}
			else
			{
				w->worldOffset = bodyRot * w->offset;
			}

			Vec3 axis = (xAxis * cosf(m_wheelStatus[i].steer)) - (yAxis * sinf(m_wheelStatus[i].steer));

			// Calc inline and lateral direction
			w->frictionDir[0] = (w->contactNormal.cross(axis)).normalize();
			w->frictionDir[1] = (w->frictionDir[0].cross(w->contactNormal)).normalize(); //axis;

			Vec3 wheelVel = vel + angVel.cross(w->worldOffset);
			w->slipSpeedLateral = wheelVel.dot(w->frictionDir[1]);
			w->slipSpeed = fabsf(wheelVel.dot(w->frictionDir[0]) - w->w*w->radius);
			averageSlipSpeedForward += w->slipSpeed;
			averageSlipSpeedLateral += w->slipSpeedLateral;

			if (lockAllWheels || (m_action.bHandBrake & w->bCanLock))
			{
				float frictionImpulse = handBrakeForce * dt;
				clampedImpulseInit(&maxTractionImpulse[i], -frictionImpulse, frictionImpulse);
				w->locked = 1;
				w->w = 0.f;
			}
			else 
			{
				float minFrictionImpulse = m_chassis.mass * gravity * dt * invNumWheels;
				float frictionImpulse = forcePerWheel * dt;
				frictionImpulse = max(frictionImpulse, minFrictionImpulse);

				// Grip based on slip speed
				const float grip = m_pSharedParams->handling.grip1 + (m_pSharedParams->handling.grip2 - m_pSharedParams->handling.grip1) * approxOneExp(w->slipSpeed * m_pSharedParams->handling.gripK);
				frictionImpulse *= grip * gravityScale;
				clampedImpulseInit(&maxTractionImpulse[i], -frictionImpulse, frictionImpulse);

				w->locked = 0;
				if (fabsf(throttle) > 0.05f)
				{
					if ((throttle*w->w>0.f) && (throttle*w->w < throttle*w->lastW))
					{
						// Resist the terrain from decreasing the speed of the wheels
						w->w+=(w->w-w->lastW)*approxOneExp(dt);
					}
					float dw = contact*throttle * w->radius * accelerationMultiplier * forcePerWheel * dt * w->invInertia;
					w->w += dw;
				}
				else
				{
					if (canDeccelerate)
					{
						float dw = fsgnf(speed) * w->radius * forcePerWheel2 * dt * w->invInertia;
						w->w = (float)__fsel(fabsf(w->w)-fabsf(dw), w->w-dw, 0.f);
					}
					w->w *= 0.9f;
				}
				w->lastW = w->w;

				if ((w->w * w->radius) > topSpeed)
				{
					float target = topSpeed / w->radius;
					w->w = target;
				}
				//else if ((w->w * w->radius) > topSpeed)
				//{
				//	float target = topSpeed / w->radius;
				//	w->w += (target - w->w) * approxOneExp(m_pSharedParams->handling.reductionRate*dt);
				//}
				else if ((w->w * w->radius) < -topSpeed)
				{
					float target = -topSpeed / w->radius;
					w->w = target;
				}
			}

			if (isTank)
			{
				// Add on differential steering
				w->w -= tankDiffSpeed * fsgnf(w->offset.x);
			}

			maxTractionImpulse[i].min *= m_handling.compressionScale * contact * surfaceFriction[i];
			maxTractionImpulse[i].max *= m_handling.compressionScale * contact * surfaceFriction[i];

			if (w->contactNormal.dot(zAxis)<0.3f)
			{
				maxTractionImpulse[i].min = 0.f;
				maxTractionImpulse[i].max = 0.f;
				maxLateralImpulse[i].min = 0.f;
				maxLateralImpulse[i].max = 0.f;
			}

			// Lateral Friction
			{
				float friction = w->axleIndex==0 ? m_pSharedParams->handling.backFriction : m_pSharedParams->handling.frontFriction;
				float frictionImpulse = friction * m_chassis.mass * gravity * invNumWheels * dt;
				if (m_action.bHandBrake & w->bCanLock)
				{
					if (w->axleIndex==0)
					{
						frictionImpulse *= m_pSharedParams->handling.handBrakeBackFrictionScale;
					}
					else
					{
						frictionImpulse *= m_pSharedParams->handling.handBrakeFrontFrictionScale;
					}
				}

				clampedImpulseInit(&maxLateralImpulse[i], -frictionImpulse, frictionImpulse);
			}

			maxLateralImpulse[i].min *= m_handling.compressionScale * contact * gravityScale * surfaceFriction[i];
			maxLateralImpulse[i].max *= m_handling.compressionScale * contact * gravityScale * surfaceFriction[i];
		}
		
		m_movementInfo.averageSlipSpeedForward = averageSlipSpeedForward * m_invNumWheels * contact;
		m_movementInfo.averageSlipSpeedLateral = averageSlipSpeedLateral * m_invNumWheels * contact;

		if (isTank && (numContacts>=1))
		{
			float impulse = steering * m_pSharedParams->tankHandling.additionalTilt * dt;
			angVel -= yAxis * impulse;
		}

		const int numIterations = 4;
		float solverERP = invNumWheels;
		float erpChange = (1.f - solverERP)/(float)(numIterations-1);

		// Keep track of lateral friction impulses
		Vec3 appliedImpulse[2]		= { Vec3Constants<float>::fVec3_Zero, Vec3Constants<float>::fVec3_Zero };
		Vec3 appliedAngImpulse[2] = { Vec3Constants<float>::fVec3_Zero, Vec3Constants<float>::fVec3_Zero };


		if (contact > 0.f)
		{
			// Iterate
			Vec3 dVel;
			Vec3 dAngVel;
			dVel.zero();
			dAngVel.zero();
			// First pass is explicit, velocity is only added to the chassis after solving all wheels
			for (int i=0; i<numWheels; i++)
			{
				SVehicleWheel* w = &m_wheels[i];
				w->chassisW0 = w->worldOffset.cross(w->frictionDir[0])*m_chassis.invInertia;
				w->chassisW1 = w->worldOffset.cross(w->frictionDir[1])*m_chassis.invInertia;
				w->chassisK0 = computeDenominator(m_chassis.invMass, m_chassis.invInertia, w->worldOffset, w->frictionDir[0]);
				w->chassisK1 = computeDenominator(m_chassis.invMass, m_chassis.invInertia, w->worldOffset, w->frictionDir[1]);
				SolveFriction(dVel, dAngVel, vel, angVel, &m_chassis, w, &maxTractionImpulse[i], &maxLateralImpulse[i], solverERP, dt);
			}
			vel = vel + dVel;
			angVel = angVel + dAngVel;
			solverERP = solverERP + erpChange;

			for (int repeat=1; repeat<numIterations; repeat++)
			{
				for (int i=0; i<numWheels; i++)
				{
					SVehicleWheel* w = &m_wheels[i];
					dVel.zero();
					dAngVel.zero();
					SolveFriction(dVel, dAngVel, vel, angVel, &m_chassis, w, &maxTractionImpulse[i], &maxLateralImpulse[i], solverERP, dt);
					vel = vel + dVel;
					angVel = angVel + dAngVel;
				}
				solverERP = solverERP + erpChange;
			}
		}
	
		//===============================
		// Set the low level wheel speeds
		//===============================
		for (int i=0; i<numWheels; i++)
		{
			if (abs(m_wheels[i].w)<FLT_EPSILON)
				continue; 
			pe_params_wheel wp;
			wp.iWheel = i;
			wp.w = m_wheels[i].w;
			pPhysics->SetParams(&wp, 1);
		}
	}
		
	m_movementInfo.centrifugalAccel = 0.f;
	m_movementInfo.effectiveCentrifugalAccel = 0.f;
	
	float angSpeed = m_handling.contactNormal.dot(angVel); 
	
	//===============================
	// InvR Correction
	//===============================
	if (numContacts>=2)
	{
		// Calculate the effective centrifugal force/acceleration - for sound purposes
		if (fabsf(m_invTurningRadius)>0.01f)
		{
			// This is centrifugal force if the vehicle was obeying the ideal turning the radius
			m_movementInfo.centrifugalAccel = sqr(speed)*m_invTurningRadius;
			// However, the vehicle will be on a *different* effective turning radius due to skidding
			m_movementInfo.effectiveCentrifugalAccel = sqr(angSpeed)/m_invTurningRadius;
		}

		float target = 1.f;
		float angSpring0 = m_pSharedParams->correction.angSpring;
		float lateralSpring0 = m_pSharedParams->correction.lateralSpring;
		if (m_action.bHandBrake)
		{
			angSpring0 *= m_pSharedParams->handling.handBrakeAngCorrectionScale;
			lateralSpring0 *= m_pSharedParams->handling.handBrakeLateralCorrectionScale;
		}
		float angularCorrection = speed*target*m_invTurningRadius - angSpeed;
		float angSpring = approxOneExp(dt*angSpring0);
		angVel = angVel + m_handling.contactNormal*(angularCorrection*angSpring);
		
		vel += - xAxis * (xAxis.dot(vel) * approxOneExp(dt * lateralSpring0));	// Lateral damp

		if (!m_movementAction.isAI)
		{
			// Power sliding, add lateral velocity based on steering and forward speed
			const SPowerSlide* ps = &m_pSharedParams->powerSlide;
			vel += xAxis * ((-absSpeed*ps->lateralSpeedFraction[bPowerLock]*steering - xAxis.dot(vel)) * approxOneExp(dt*ps->spring));
		}
	}

	if (m_movementAction.isAI)
	{
		if (m_movementAction.brake)
		{
			// kill the lateral velocity
			vel = vel - xAxis * (xAxis.dot(vel));
		}
		else
		{
			if (m_aiRequest.HasDirectionOffFromPath())
			{
				Vec3 aiDir = m_aiRequest.GetDirOffFromPath();
				float distSq = aiDir.GetLengthSquared();
				if (distSq>0.01f && distSq<10.f)
				{
					Vec3 error = aiDir - vel;
					error = xAxis * (xAxis.dot(error));
					if (error.GetLengthSquared()>1.f)
					{
						error.normalize();
					}
					vel = vel + error * approxOneExp(10.f*dt);
				}
			}
		}
	}
	if (isTank)
	{
		float requiredAngSpeed = -1.f * (invertTankYaw ? -m_movementAction.rotateYaw : m_movementAction.rotateYaw);
		float angularCorrection = requiredAngSpeed - angSpeed;
		float angSpring = approxOneExp(3.f*dt);
		angVel = angVel + m_handling.contactNormal*(angularCorrection*angSpring);
	}

	if (m_frictionState!=k_frictionUseHiLevel)
		EnableLowLevelPhysics(k_frictionUseHiLevel, THREAD_SAFE);

	// Remove grav since the low level will add it in again
	vel.z += physGravity*dt;

	//==============================================
	// Commit the velocity back to the physics engine
	//==============================================
	pe_action_set_velocity setVelocity;
	setVelocity.v = vel;
	setVelocity.w = angVel;
	pPhysics->Action(&setVelocity, THREAD_SAFE);

	m_collisionNorm *= approxExp(3.f*dt);

	m_chassis.vel = vel;
	m_chassis.angVel = angVel;
	physStatus->v = vel;
	physStatus->w = angVel;
}

#if ENABLE_VEHICLE_DEBUG
void CVehicleMovementArcadeWheeled::DebugCheat(float dt)
{
	if (g_pGameCVars->v_profileMovement == 0) return;

	IPhysicalEntity* pPhysics = GetPhysics();
	if (pPhysics==NULL)
		return;

	SVehiclePhysicsStatus* physStatus = &m_physStatus[k_physicsThread];
	Matrix33 bodyRot( physStatus->q );
	const Vec3 xAxis = bodyRot.GetColumn0();
	const Vec3 yAxis = bodyRot.GetColumn1();
	const Vec3 zAxis = bodyRot.GetColumn2();
	const Vec3 pos = m_physStatus[k_physicsThread].centerOfMass;

	if (g_pGameCVars->v_debugMovementMoveVertically!=0.f || g_pGameCVars->v_debugMovementX!=0.f || g_pGameCVars->v_debugMovementY!=0.f || g_pGameCVars->v_debugMovementZ!=0.f)
	{
		const float angleX = 0.5f * g_pGameCVars->v_debugMovementX;
		const float angleY = 0.5f * g_pGameCVars->v_debugMovementY;
		const float angleZ = 0.5f * g_pGameCVars->v_debugMovementZ;

		Quat qx, qy, qz;
		pe_params_pos physPos;

		qx.SetRotationAA(cosf(DEG2RAD(angleX)), sinf(DEG2RAD(angleX)), xAxis);
		qy.SetRotationAA(cosf(DEG2RAD(angleY)), sinf(DEG2RAD(angleY)), yAxis);
		qz.SetRotationAA(cosf(DEG2RAD(angleZ)), sinf(DEG2RAD(angleZ)), zAxis);

		physStatus->pos.z += g_pGameCVars->v_debugMovementMoveVertically;
		physPos.pos = physStatus->pos;
		physPos.q = qx * qy * qz * physStatus->q;

		pPhysics->SetParams(&physPos, 1);
		g_pGameCVars->v_debugMovementMoveVertically=0.f;
		g_pGameCVars->v_debugMovementX=0.f;
		g_pGameCVars->v_debugMovementY=0.f;
		g_pGameCVars->v_debugMovementZ=0.f;
	}

	if (g_pGameCVars->v_debugMovement)
	{
		const float angleX = 0.5f * dt * m_movementAction.power * g_pGameCVars->v_debugMovementSensitivity;
		const float angleY = 0.5f * dt * m_movementAction.rotateYaw * g_pGameCVars->v_debugMovementSensitivity;

		Quat qx, qy;
		pe_params_pos physPos;
		qx.SetRotationAA(cosf(DEG2RAD(angleX)), sinf(DEG2RAD(angleX)), xAxis);
		qy.SetRotationAA(cosf(DEG2RAD(angleY)), sinf(DEG2RAD(angleY)), yAxis);
		physPos.q = qx * qy * physStatus->q;
		pPhysics->SetParams(&physPos, 1);

		// Kill gravity
    	pe_simulation_params paramsSet;
    	paramsSet.gravityFreefall.Set(0.f, 0.f, 0.f);
    	pPhysics->SetParams(&paramsSet, 1);

		// Kill velocity
		pe_action_set_velocity setVelocity;
		setVelocity.v.zero();
		setVelocity.w.zero();
		pPhysics->Action(&setVelocity, THREAD_SAFE);
		return;
	}
}
#endif

void CVehicleMovementArcadeWheeled::UpdateWaterLevels()
{
	size_t	numberOfWheels = m_wheels.size();

	if(numberOfWheels > 0)
	{
		if(m_iWaterLevelUpdate >= numberOfWheels)
		{
			m_iWaterLevelUpdate = 0;
		}

		pe_status_wheel	wheelStatus;

		wheelStatus.iWheel = m_iWaterLevelUpdate;

		if(GetPhysics()->GetStatus(&wheelStatus))
		{
			I3DEngine	*p3DEngine = gEnv->p3DEngine;

			m_wheels[m_iWaterLevelUpdate].waterLevel = gEnv->p3DEngine->GetWaterLevel(&wheelStatus.ptContact);
		}
		else
		{
			m_wheels[m_iWaterLevelUpdate].waterLevel = WATER_LEVEL_UNKNOWN;
		}

		++ m_iWaterLevelUpdate;
	}
}

void CVehicleMovementArcadeWheeled::ResetWaterLevels()
{
	for(TWheelArray::iterator iWheel = m_wheels.begin(), end = m_wheels.end(); iWheel != end; ++ iWheel)
	{
		iWheel->waterLevel = WATER_LEVEL_UNKNOWN;
	}

	m_iWaterLevelUpdate = 0;
}

#define F2I(x) (int)(x)
#define TMPINT const int

void CVehicleMovementArcadeWheeled::TickGears(float dt, float averageWheelSpeed, float throttle, float forwardSpeed, float contact)
{
	averageWheelSpeed = fabsf(averageWheelSpeed);

	assert(throttle>=-1.f && throttle<=1.f);

	// Break up the conversion to avoid load hit stores

	TMPINT ivThrottle = F2I(throttle*20.f);
	TMPINT ivSpeed = F2I(forwardSpeed*10.f);

	if (m_isEnginePowered)
	{
		const bool isTank = m_pSharedParams->tankHandling.additionalSteeringAtMaxSpeed > 0.f;
		const float topSpeed = (float)__fsel(forwardSpeed, m_pSharedParams->handling.topSpeed, m_pSharedParams->handling.reverseSpeed) * m_topSpeedFactor;
		const float invTopSpeed = 1.f/topSpeed;
		const float wheelRpm = (0.5f*contact*fabsf(forwardSpeed) + (1.f-0.5f*contact)*averageWheelSpeed*m_gears.averageWheelRadius) * invTopSpeed;	// Normalised between 0 and 1

		const float speedNorm = forwardSpeed*invTopSpeed;
		const float ratio = m_pSharedParams->gears.ratios[m_gears.curGear];
		const float invRatio = m_pSharedParams->gears.invRatios[m_gears.curGear];
		const float contactModifier = 1.f/clamp_tpl(contact+0.3f,0.4f,1.3f);

		// RPM as dictated by the wheels (because of the contact modifier rpm goes to a max of 0.75)
		float rpm = (invRatio * wheelRpm)*contactModifier;
		rpm = fabsf(rpm);
		rpm = isTank ? max(rpm, 0.7f*fabsf(m_movementAction.rotateYaw)) : rpm;

		// Target rpm (multiplied by the modulation to fake clutch)
		m_gears.targetRpm = m_gears.modulation*clamp_tpl(rpm, 0.1f, 1.3f);
		
		//////////////////////////////////////
		// Process gear change and RPM change
		//////////////////////////////////////
		const float interpSpeed = (float)__fsel(fabsf(throttle)-0.05f, m_pSharedParams->gears.rpmInterpSpeed, m_pSharedParams->gears.rpmRelaxSpeed);
		const float rpmChange = (m_gears.targetRpm - m_gears.curRpm)*approxOneExp(dt*interpSpeed*3.f);
		m_gears.curRpm += rpmChange;
		m_gears.modulation += (1.f-m_gears.modulation) * approxOneExp(dt*m_pSharedParams->gears.gearChangeSpeed);
		const int32 iThrottle = sgn((int32)ivThrottle);
	
		switch (m_gears.curGear)
		{
			case SVehicleGears::kReverse:
			{
				// Change to first if throttle>eps && speed>eps
				const int32 iSpeedGtz = 1-isneg((int32)ivSpeed);        
				const int32 iThrottleGtz = (iThrottle+1);        
				m_gears.curGear += (iSpeedGtz*iThrottleGtz);
				m_gears.timer = 0.f;
				m_gears.changedUp = 0;
				break;
			}
			case SVehicleGears::kNeutral:
			{
				// Change to up or down based on throttle
				m_gears.curGear += iThrottle;
				m_gears.timer = 0.f;
				m_gears.changedUp = 0;
				break;
			}
			default:
			{
				float steering = m_steering / DEG2RAD(m_steerMax);  // FIXME!
				float filteredRpm = m_gears.curRpm;
				m_gears.timer += dt;
				if (ivSpeed>0 && ivThrottle>0 && m_action.bHandBrake==0) // Accelerating
				{
					if (filteredRpm > (0.7f+0.2f*fabsf(steering))) // Change up
					{
						if (m_gears.timer>m_pSharedParams->gears.minChangeUpTime[m_gears.curGear])
						{
							if (m_gears.curGear<(m_pSharedParams->gears.numGears-1))
							{
								{
									m_gears.curGear++;
									m_gears.modulation = 0.f;
									m_gears.timer = 0.f;
									m_gears.targetRpm = 0.2f;
									m_gears.changedUp = 1;
								}
							}
						}
					}
					else if (m_gears.curGear>(SVehicleGears::kNeutral+1) && filteredRpm<(0.3f+0.3f*fabsf(steering))-0.35f*(float)m_action.bHandBrake)  // Change down
					{
						if (m_gears.timer>m_pSharedParams->gears.minChangeDownTime[m_gears.curGear])
						{
							m_gears.curGear--;
							m_gears.timer = 0.f;
							m_gears.modulation = 0.f;
							m_gears.targetRpm = 0.2f;
							m_gears.changedUp = 0;
						}
					}
				}
				else
				{
					// Possibly Decelerating
					assert(m_gears.curGear > 0 && m_gears.curGear <= SVehicleGears::kMaxGears);
					// Change down if the rpm has fallen below a certain fraction of the lower gear, curGear-1
					// or handbrake is on
					if ((filteredRpm*ratio) < m_pSharedParams->gears.ratios[m_gears.curGear-1]*0.8f || filteredRpm<0.2f || m_action.bHandBrake)
					{
						int noHandBrake = 1-m_action.bHandBrake;
						int throttleAndNoHandBrake = noHandBrake & (((iThrottle+1)>>1)^m_action.bHandBrake);
						if (m_gears.timer>m_pSharedParams->gears.minChangeDownTime[m_gears.curGear] || throttleAndNoHandBrake)
						{
							m_gears.curGear--;
							m_gears.timer = 0.f;
							m_gears.modulation = 0.f;
							m_gears.targetRpm = 0.2f;
							m_gears.changedUp = 0;
						}
					}
				}
				break;
			}
		}
		m_gears.doGearChange = (m_gears.curGear!=m_gears.lastGear) && (m_gears.curGear!=SVehicleGears::kNeutral) ? 1 : 0;
		m_gears.lastGear = m_gears.curGear;

		//////////////////////////////////////
		// RPM - load
		//////////////////////////////////////

		// Calculate the RPM load from the engine changing its rpm value
		// using the differential change, normalised for time and interpSpeed
		float rpmLoad = rpmChange / (dt * interpSpeed) * m_pSharedParams->gears.rpmLoadFactor;
		float rpmLoadDefault = m_pSharedParams->gears.rpmLoadDefaultValue;
		float rpmLoadFromBrake = m_action.bHandBrake ? m_pSharedParams->gears.rpmLoadFromBraking : 0.f;
		rpmLoadFromBrake = ((iThrottle*ivSpeed < -20) ? 1.f : rpmLoadFromBrake)*fabsf(speedNorm);  // Trying to reverse direction ?
		float rpmLoadFromThrottle = fabsf(throttle) * m_pSharedParams->gears.rpmLoadFromThrottle;
		float totalRpmLoad = (rpmLoadDefault + rpmLoad + max(rpmLoadFromBrake, rpmLoadFromThrottle))/sqr(contactModifier);
		
		// Morph the rpmLoad value
		float loadInterpSpeed = (float)__fsel(totalRpmLoad - m_gears.rpmLoad, m_pSharedParams->gears.rpmLoadChangeSpeedUp, m_pSharedParams->gears.rpmLoadChangeSpeedDown);
		m_gears.rpmLoad = clamp_tpl(m_gears.rpmLoad + (totalRpmLoad - m_gears.rpmLoad) * approxOneExp(dt*loadInterpSpeed), -1.f, +1.f);
	}
}

//----------------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::Boost(bool enable)
{  
	if (enable)
	{
		if (m_action.bHandBrake)
			return;
	}

	CVehicleMovementBase::Boost(enable);
}


//------------------------------------------------------------------------
bool CVehicleMovementArcadeWheeled::RequestMovement(CMovementRequest& movementRequest)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	m_movementAction.isAI = true;
	if (!m_isEnginePowered)
		return false;

	CryAutoCriticalSection lk(m_lock);

	if (movementRequest.HasDirectionOffFromPath())
		m_aiRequest.SetDirectionOffFromPath(movementRequest.GetDirOffFromPath());

	if (movementRequest.HasLookTarget())
		m_aiRequest.SetLookTarget(movementRequest.GetLookTarget());
	else
		m_aiRequest.ClearLookTarget();

	if (movementRequest.HasMoveTarget())
	{
		Vec3 entityPos = m_pEntity->GetWorldPos();
		Vec3 start(entityPos);
		Vec3 end( movementRequest.GetMoveTarget() );
		Vec3 pos = ( end - start ) * 100.0f;
		pos +=start;
		m_aiRequest.SetMoveTarget( pos );
	}
	else
		m_aiRequest.ClearMoveTarget();

	float fDesiredSpeed = 0.0f;

	if (movementRequest.HasDesiredSpeed())
		fDesiredSpeed = movementRequest.GetDesiredSpeed();
	else
		m_aiRequest.ClearDesiredSpeed();

	if (movementRequest.HasForcedNavigation())
	{
		const Vec3 forcedNavigation = movementRequest.GetForcedNavigation();
		const Vec3 entityPos = m_pEntity->GetWorldPos();
		m_aiRequest.SetForcedNavigation(forcedNavigation);
		m_aiRequest.SetMoveTarget(entityPos+forcedNavigation.GetNormalizedSafe()*100.0f);
		
		if (fabsf(fDesiredSpeed) <= FLT_EPSILON)
			fDesiredSpeed = forcedNavigation.GetLength();
	}
	else
		m_aiRequest.ClearForcedNavigation();

	m_aiRequest.SetDesiredSpeed(fDesiredSpeed);

	if(fabs(fDesiredSpeed) > FLT_EPSILON)
	{
		m_pVehicle->NeedsUpdate(IVehicle::eVUF_AwakePhysics);
	}

	return true;

}

void CVehicleMovementArcadeWheeled::GetMovementState(SMovementState& movementState)
{
	IPhysicalEntity* pPhysics = GetPhysics();
	if (!pPhysics)
		return;

	movementState.minSpeed = 0.0f;
	movementState.maxSpeed = m_pSharedParams->handling.topSpeed * m_topSpeedFactor;
	movementState.normalSpeed = movementState.maxSpeed;
}
	
void CVehicleMovementArcadeWheeled::EnableMovementProcessing(bool enable)
{
	CVehicleMovementBase::EnableMovementProcessing(enable);
	if (enable==false)
	{
		EnableLowLevelPhysics(k_frictionUseHiLevel, 0);
	}
}


//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::Serialize(TSerialize ser, EEntityAspects aspects) 
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Vehicle movement arcade wheeled serialization");
	
	if(ser.GetSerializationTarget() != eST_Network && ser.IsReading() && m_pVehicle && m_pVehicle->GetEntity())
	{
		SEnvParticleStatus::TEnvEmitters::iterator emitterIt = m_paStats.envStats.emitters.begin();
		SEnvParticleStatus::TEnvEmitters::iterator emitterItEnd = m_paStats.envStats.emitters.end();
		for (; emitterIt != emitterItEnd; ++emitterIt)
		{
			if (emitterIt->slot!=-1)
				m_pVehicle->GetEntity()->FreeSlot(emitterIt->slot);
		}
	}		

	CVehicleMovementBase::Serialize(ser, aspects);

	// removal of the particle emitters is splitted this way to cause minimal change on other vehicles code.	
	if(ser.GetSerializationTarget() != eST_Network && ser.IsReading())
	{
		SEnvParticleStatus::TEnvEmitters::iterator emitterIt = m_paStats.envStats.emitters.begin();
		SEnvParticleStatus::TEnvEmitters::iterator emitterItEnd = m_paStats.envStats.emitters.end();
		for (; emitterIt != emitterItEnd; ++emitterIt)
		{
			emitterIt->matId = -1;
			emitterIt->slot = -1;
		}
	}		
	

	if (ser.GetSerializationTarget() == eST_Network)
	{
		if (aspects&CNetworkMovementArcadeWheeled::CONTROLLED_ASPECT)
		{
			m_netActionSync.Serialize(ser, aspects);
			if (ser.IsReading())
			{
				IPhysicalEntity *pPhysics = GetPhysics();
				if (pPhysics)
				{
					pe_action_awake awake;
					pPhysics->Action(&awake);
				}
			}
		}
	}
	else 
	{	
		ser.Value("brakeTimer", m_brakeTimer);
		ser.Value("brake", m_movementAction.brake);
		ser.Value("tireBlownTimer", m_tireBlownTimer);
		ser.Value("initialHandbreak", m_initialHandbreak);
		ser.Value("topSpeedFactor", m_topSpeedFactor );
		ser.Value("accelFactor", m_accelFactor );

		int blownTires = m_blownTires;
		ser.Value("blownTires", m_blownTires);
		ser.Value("bForceSleep", m_bForceSleep);

		if (ser.IsReading() && blownTires != m_blownTires)
			SetEngineRPMMult(GetWheelCondition());

		ser.Value("m_prevAngle", m_prevAngle);

		m_frictionState = k_frictionNotSet;
	}

	if(ser.IsReading())
	{
		ResetWaterLevels();
	}
};

//------------------------------------------------------------------------
struct SArcadeWheeledNetState
{
	float m_steering;
};

//------------------------------------------------------------------------
SVehicleNetState CVehicleMovementArcadeWheeled::GetVehicleNetState()
{
	SArcadeWheeledNetState arcadeWheeledNetState;
	arcadeWheeledNetState.m_steering = m_steering;
	SVehicleNetState state;
	static_assert(sizeof(SArcadeWheeledNetState) <= sizeof(SVehicleNetState), "Invalid type size!");
	memcpy(&state, &arcadeWheeledNetState, sizeof(SArcadeWheeledNetState));
	return state;
}

//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::SetVehicleNetState(const SVehicleNetState& state)
{
	SArcadeWheeledNetState* arcadeWheeledNetState = (SArcadeWheeledNetState*)&state;
	m_steering = arcadeWheeledNetState->m_steering;
}

//------------------------------------------------------------------------
void CVehicleMovementArcadeWheeled::UpdateSurfaceEffects(const float deltaTime)
{ 
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	if (0 == g_pGameCVars->v_pa_surface)
	{
		ResetParticles();
		return;
	}

	const SVehicleStatus& status = m_pVehicle->GetStatus();
	if (status.speed < 0.01f)
		return;

	float distSq = m_pVehicle->GetEntity()->GetWorldPos().GetSquaredDistance(GetISystem()->GetViewCamera().GetPosition());
	if (distSq > sqr(300.f) || (distSq > sqr(50.f) && !m_isProbablyVisible ))
		return;

	IPhysicalEntity* pPhysics = GetPhysics();

	// don't render particles for drivers in 1st person (E3 request)
	bool hideForFP = false;
	if(!m_pSharedParams->surfaceFXInFpView)
	{
		if (GetMovementType() == eVMT_Land && m_carParams.steerTrackNeutralTurn == 0.f)
		{
			IActor* pActor = m_pVehicle->GetDriver();
			IVehicleSeat* pSeat = pActor ? m_pVehicle->GetSeatForPassenger(pActor->GetEntityId()) : NULL;
			IVehicleView* pView = pSeat ? pSeat->GetCurrentView() : NULL;
			if (pActor && pActor->IsClient() && pView && !pView->IsThirdPerson())
				hideForFP = true;
		}
	}

#if ENABLE_VEHICLE_DEBUG
	if (DebugParticles())
	{
		float color[] = {1,1,1,1};
		IRenderAuxText::Draw2dLabel(100, 280, 1.3f, color, false, "%s:", m_pVehicle->GetEntity()->GetName());
	}
#endif

	SEnvironmentParticles* envParams = m_pPaParams->GetEnvironmentParticles();
	SEnvParticleStatus::TEnvEmitters::iterator emitterIt = m_paStats.envStats.emitters.begin();
	SEnvParticleStatus::TEnvEmitters::iterator emitterItEnd = m_paStats.envStats.emitters.end();


	for (; emitterIt != emitterItEnd; ++emitterIt)
	{ 
		if (emitterIt->layer < 0)
		{
			assert(0);
			continue;
		}

		if (!emitterIt->active)
			continue;

		const SEnvironmentLayer& layer = envParams->GetLayer(emitterIt->layer);

		//if (!layer.active || !layer.IsGroupActive(emitterIt->group))

		// scaling for each wheelgroup is based on vehicle speed + avg. slipspeed
		float slipAvg = 0; 
		bool bContact = false;
		int matId = 0;
		float fWaterLevel = -FLT_MAX;
		float wheelCount = 0.f;
		size_t layerWheelCount = layer.GetWheelCount(emitterIt->group);
		
		if(layerWheelCount && m_numWheels>0)
		{
			fWaterLevel = 0.f;
			for(size_t i=0; i < layerWheelCount; i++)
			{
				unsigned int idx = layer.GetWheelAt(emitterIt->group, i) - 1;
				CRY_ASSERT((unsigned)idx < (unsigned)m_numWheels);
				if ((unsigned)idx >= (unsigned)m_numWheels)
					continue;

				pe_status_wheel* wheelStatus = &m_wheelStatus[idx];
				SVehicleWheel* w = &m_wheels[idx];

				if (w->waterLevel != WATER_LEVEL_UNKNOWN)
					fWaterLevel += max(w->waterLevel, WATER_LEVEL_UNKNOWN);

				if (wheelStatus->bContact)
				{
					bContact = true;

					// take care of water
					if (fWaterLevel > wheelStatus->ptContact.z+0.02f)
					{
						if (fWaterLevel > wheelStatus->ptContact.z+2.0f)
						{
							slipAvg = 0.0f;
							bContact = false;
						}
						matId = gEnv->pPhysicalWorld->GetWaterMat();
					}
					else if (wheelStatus->contactSurfaceIdx > matId)
						matId = wheelStatus->contactSurfaceIdx;
		
					slipAvg += 0.5f*(w->slipSpeed + w->slipSpeedLateral);
				}
				wheelCount += 1.f;
			}
		}
		if (wheelCount==0.f)
			continue;

		if (!bContact && !emitterIt->bContact)
			continue;

		float invWheelCount = 1.f/wheelCount;
			
		fWaterLevel *= invWheelCount;

		emitterIt->bContact = bContact;
		slipAvg *= invWheelCount;

		bool isSlip = !strcmp(layer.GetName(), "slip");
		float vel = isSlip ? 0.f : m_speed;
		vel += slipAvg;

		float countScale = 1;
		float sizeScale = 1;
		float speedScale = 1;

		if (hideForFP || !bContact || matId == 0)    
			countScale = 0;          
		else
			GetParticleScale(layer, vel, 0.f, countScale, sizeScale, speedScale);
		
		IEntity* pEntity = m_pVehicle->GetEntity();
		SEntitySlotInfo info;
		info.pParticleEmitter = 0;
		pEntity->GetSlotInfo(emitterIt->slot, info);

		if (matId != emitterIt->matId)
		{
			// change effect                        
			const char* effect = GetEffectByIndex(matId, layer.GetName());
			IParticleEffect* pEff = 0;   

			if (effect && (pEff = gEnv->pParticleManager->FindEffect(effect)))
			{     
#if ENABLE_VEHICLE_DEBUG
				if (DebugParticles())          
					CryLog("<%s> changes sfx to %s (slot %i)", pEntity->GetName(), effect, emitterIt->slot);
#endif

				if (info.pParticleEmitter)
				{   
					// free old emitter and load new one, for old effect to die gracefully           
					info.pParticleEmitter->Activate(false);            
					pEntity->FreeSlot(emitterIt->slot);
				}         

				emitterIt->slot = pEntity->LoadParticleEmitter(emitterIt->slot, pEff);

				if (emitterIt->slot != -1)
					pEntity->SetSlotLocalTM(emitterIt->slot, Matrix34(emitterIt->quatT));

				info.pParticleEmitter = 0;
				pEntity->GetSlotInfo(emitterIt->slot, info);

				emitterIt->matId = matId;
			}
			else 
			{
#if ENABLE_VEHICLE_DEBUG
				if (DebugParticles())
					CryLog("<%s> found no effect for %i", pEntity->GetName(), matId);
#endif

				// effect not available, disable
				//info.pParticleEmitter->Activate(false);
				countScale = 0.f; 
				emitterIt->matId = 0;
			}    
			
			ISurfaceTypeManager* pSurfaceManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
			ISurfaceType* pSurfaceType = pSurfaceManager->GetSurfaceType(matId);

			if (pSurfaceType)
			{
				string surfaceTypeName = pSurfaceType->GetName();

				if (surfaceTypeName.find("mat_") != std::string::npos)
				{
					CryAudio::SwitchStateId const surfaceStateId = CryAudio::StringToId(surfaceTypeName.substr(4).c_str());
					auto pIEntityAudioComponent = GetAudioProxy();

					if (pIEntityAudioComponent != nullptr)
					{
						pIEntityAudioComponent->SetSwitchState(m_audioControlIDs[eSID_VehicleSurface], surfaceStateId);
					}
				}
			}
		}

		if (emitterIt->matId == 0)      
			countScale = 0.f;

		if (info.pParticleEmitter)
		{
			SpawnParams sp;
			sp.fSizeScale = sizeScale;
			sp.fCountScale = countScale;
			sp.fSpeedScale = speedScale;
			info.pParticleEmitter->SetSpawnParams(sp);
		}

#if ENABLE_VEHICLE_DEBUG
		if (DebugParticles())
		{
			float color[] = {1,1,1,1};
			IRenderAuxText::Draw2dLabel((float)(100+330*emitterIt->layer), (float)(300+25*emitterIt->group), 1.2f, color, false, "group %i, matId %i: sizeScale %.2f, countScale %.2f, speedScale %.2f (emit: %i)", emitterIt->group, emitterIt->matId, sizeScale, countScale, speedScale, info.pParticleEmitter?1:0);
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_pVehicle->GetEntity()->GetSlotWorldTM(emitterIt->slot).GetTranslation(), 0.2f, ColorB(0,0,255,200));
		}
#endif
	}
}

//------------------------------------------------------------------------
bool CVehicleMovementArcadeWheeled::DoGearSound()
{
	return true;
}

void CVehicleMovementArcadeWheeled::GetMemoryUsage(ICrySizer * pSizer) const
{
	pSizer->Add(*this);
	pSizer->AddContainer(m_wheels);
	pSizer->AddContainer(m_wheelStatus);
	CVehicleMovementBase::GetMemoryUsageInternal(pSizer);
}

// There is no need to net serialise this, we dont have a boost button
// and the brake, pedal, and brake are already net-serialised in wheeledvehicleentity.cpp

//------------------------------------------------------------------------
CNetworkMovementArcadeWheeled::CNetworkMovementArcadeWheeled()
: m_steer(0.0f),
	m_pedal(0.0f),
	m_brake(false),
	m_boost(false)
{
}

//------------------------------------------------------------------------
CNetworkMovementArcadeWheeled::CNetworkMovementArcadeWheeled(CVehicleMovementArcadeWheeled *pMovement)
{
	m_steer = pMovement->m_movementAction.rotateYaw;
	m_pedal = pMovement->m_movementAction.power;
	m_brake = pMovement->m_movementAction.brake;
	m_boost = pMovement->m_boost;
}

//------------------------------------------------------------------------
void CNetworkMovementArcadeWheeled::UpdateObject(CVehicleMovementArcadeWheeled *pMovement)
{
	pMovement->m_movementAction.rotateYaw = m_steer;
	pMovement->m_movementAction.power = m_pedal;
	pMovement->m_movementAction.brake = m_brake;
	pMovement->m_boost = m_boost;
}

//------------------------------------------------------------------------
void CNetworkMovementArcadeWheeled::Serialize(TSerialize ser, EEntityAspects aspects)
{
	if (ser.GetSerializationTarget()==eST_Network)
	{
		if (aspects & CONTROLLED_ASPECT)
		{
			NET_PROFILE_SCOPE("NetMovementArcadeWheeled", ser.IsReading());

			ser.Value("pedal", m_pedal, 'vPed');
			ser.Value("steer", m_steer, 'vStr');
			ser.Value("brake", m_brake, 'bool');
			ser.Value("boost", m_boost, 'bool');
		}		
	}	
}
