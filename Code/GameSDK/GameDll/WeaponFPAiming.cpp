// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryAction/IDebugHistory.h>

#include <CryEntitySystem/IEntitySystem.h>
#include <CryAnimation/ICryAnimation.h>
#include "WeaponFPAiming.h"
#include "GameConstantCVars.h"
#include "Weapon.h"
#include "Player.h"

#include "PlayerAnimation.h"


namespace
{


	bool g_fpAimingStopOnExit = true;
	int g_fpAimingLayer = 0;
	float g_fpAimingEntryTime = 0.25f;
	float g_fpAimingExitTime = 0.2f;
	float g_fpAimingTransitionTime = 0.175f;


}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static float Sign(float v)
{
	return v < 0 ? -1.0f : v > 0 ? 1.0f : 0;
}

static float SignedPow(float linearVal, float curvePower)
{
	return pow_tpl(fabs_tpl(linearVal), curvePower) * Sign(linearVal);
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CFPAction : public TAction<SAnimationContext>
{
	typedef TAction<SAnimationContext> BaseClass;

public:
	void UpdateFragment()
	{
		if( GetStatus() == Installed )
		{
			IScope &rootScope = GetRootScope();
			if (rootScope.IsDifferent(m_fragmentID, m_fragTags))
			{
				SetFragment(m_fragmentID, m_fragTags);
			}
			m_updatedFragment = true;
		}
	}

protected:
	explicit CFPAction(FragmentID fragID)
		: BaseClass( PP_Movement, fragID, TAG_STATE_EMPTY, IAction::Interruptable|IAction::NoAutoBlendOut )
		, m_updatedFragment(false)
	{}

	virtual EStatus Update(float timePassed)
	{
		if( !m_updatedFragment )
			UpdateFragment();

		m_updatedFragment = false;

		return BaseClass::Update(timePassed);
	}

	bool m_updatedFragment;
};

class CFPAimingAction : public CFPAction
{
public:

	DEFINE_ACTION("FPAimingAction");

	typedef CFPAction BaseClass;

	CFPAimingAction()
		:	BaseClass(PlayerMannequin.fragmentIDs.idlePose)
	{
	}

	void SetWeaponParams(const SParams_WeaponFPAiming& params)
	{
		SetParam(CItem::sActionParamCRCs.aimDirection, params.aimDirection);
		SetParam(CItem::sActionParamCRCs.inputMove, params.inputMove);
		SetParam(CItem::sActionParamCRCs.inputRot, params.inputRot);
		SetParam(CItem::sActionParamCRCs.velocity, params.velocity);
		SetParam(CItem::sActionParamCRCs.zoomTransition, params.zoomTransitionFactor);

		UpdateFragment();
	}

	void TriggerExit()
	{
		m_eStatus = IAction::Finished;
		m_flags &= ~IAction::Interruptable;
	}
};

class CFPMotionAction : public CFPAction
{
public:

	DEFINE_ACTION("FPMotionAction");

	typedef CFPAction BaseClass;

	CFPMotionAction()
		:	BaseClass(PlayerMannequin.fragmentIDs.FPIdle)
		,	m_lastAnimationWeigth(1.0f)
	{
	}

	void SetWeaponParams(const SParams_WeaponFPAiming& params, float runFactor)
	{
		enum RunFactorIndices{ RFIndex_Default = 0, RFIndex_Run, RFIndex_Count };
		enum StabilizeIndices{ SIndex_Disabled = 0, SIndex_Enabled, SIndex_Count };

		const float runFactorsTable[RFIndex_Count] = { 1.0f, runFactor };
		const float stabilizeFactorsTable[SIndex_Count] = { 1.0f, 0.0f };

		const float stabilizeWeight = stabilizeFactorsTable[ (uint32)(params.flags.AreAllFlagsActive( eWFPAF_aimStabilized ) == true) ];
		float animationWeight = runFactorsTable[RFIndex_Default];

		if (g_pGameCVars->g_debugWeaponOffset == 2)
		{
			m_fragmentID = PlayerMannequin.fragmentIDs.FPNone;
		}
		else if ( (params.flags.AreAnyFlagsActive( eWFPAF_onGround | eWFPAF_swimming | eWFPAF_ledgeGrabbing ) && params.velocity.GetLengthSquared() > 0.1f) 
					||  params.flags.AreAnyFlagsActive( eWFPAF_jump ) ) 
		{
			m_fragmentID = PlayerMannequin.fragmentIDs.FPMovement;
			if (!params.flags.AreAnyFlagsActive(eWFPAF_swimming))		// If we're swimming we always use full animation weight, otherwise we don't see any arms!
			{
				animationWeight = runFactorsTable[RFIndex_Run];
			}
		}
		else
		{
			const float blendTime = 2.0f;
			m_fragmentID = PlayerMannequin.fragmentIDs.FPIdle;
			animationWeight = min(1.0f, m_lastAnimationWeigth + gEnv->pTimer->GetFrameTime()/blendTime);
		}

		SetAnimWeight( animationWeight * stabilizeWeight );
		m_lastAnimationWeigth = animationWeight;

		const float playbackSpeedTable[2] = { 1.0f, 0.7f };
		const uint32 playbackSpeedIdx = (params.flags.AreAllFlagsActive( eWFPAF_crouched) == true);
		CRY_ASSERT(playbackSpeedIdx < 2);
		SetSpeedBias( playbackSpeedTable[playbackSpeedIdx] );

		UpdateFragment();
	}

private:
	float m_lastAnimationWeigth;
};


class CFPSwayAction : public CFPAction
{
public:

	DEFINE_ACTION("FPMotionAction");

	typedef CFPAction BaseClass;

	CFPSwayAction()
		:	BaseClass(PlayerMannequin.fragmentIDs.FPSway)
	{
	}
};

CWeaponFPAiming::CWeaponFPAiming(bool isReplay)
	:	m_pActionController(NULL)
	, m_smoothedVelocity(ZERO)
	,	m_lastVelocity(ZERO)
	,	m_interpFront(0.0f)
	,	m_interpSide(0.0f)
	,	m_sprintFactor(0.0f)
	, m_replay(isReplay)
	, m_pAction(NULL)
	, m_pMotionAction(NULL)
	, m_pSwayAction(NULL)
{
	m_interpVert  = 0.0f;
	m_interpHoriz = 0.0f;
	m_fallFromHeight = 0.0f;
	m_runFactor = 0.0f;
	m_falling = false;
	m_enabled = false;
}

CWeaponFPAiming::~CWeaponFPAiming()
{
	SAFE_RELEASE(m_pAction);
	SAFE_RELEASE(m_pMotionAction);
	SAFE_RELEASE(m_pSwayAction);
}

void CWeaponFPAiming::SetActive(bool active)
{
	m_enabled = active;
}

void CWeaponFPAiming::ReleaseActions()
{
	SAFE_RELEASE(m_pAction);
	SAFE_RELEASE(m_pMotionAction);
	SAFE_RELEASE(m_pSwayAction);
}


void CWeaponFPAiming::Update(const SParams_WeaponFPAiming &params)
{
	ASSERT_IS_NOT_NULL(params.characterInst);
	ASSERT_IS_NOT_NULL(params.skelAnim);

	const bool hasValidParams = (params.characterInst != NULL) && (params.skelAnim != NULL) && (params.shoulderLookParams != NULL);
	if (!hasValidParams)
	{
		return;
	}

	const float frameTime = gEnv->pTimer->GetFrameTime();

	bool hasAction    = (m_pAction != NULL);
	if (m_enabled != hasAction)
	{
		if (m_enabled && m_pActionController)
		{
			m_pAction = new CFPAimingAction();
			m_pMotionAction = new CFPMotionAction();
			m_pSwayAction = new CFPSwayAction();
			m_pAction->AddRef();
			m_pMotionAction->AddRef();
			m_pSwayAction->AddRef();

			m_pActionController->Queue(*m_pAction);
			m_pActionController->Queue(*m_pMotionAction);
			m_pActionController->Queue(*m_pSwayAction);
		}
	}

	if(m_enabled)
	{
		if(hasAction)
		{
			m_pAction->SetWeaponParams(params);
			m_pMotionAction->SetWeaponParams(params, max(m_runFactor, m_sprintFactor)*params.movementFactor);
			m_pSwayAction->UpdateFragment();
		}
		if(m_replay && m_pActionController)
		{
			// CFPAimingAction
			m_pActionController->SetParam(CItem::sActionParamCRCs.aimDirection, params.aimDirection);
			m_pActionController->SetParam(CItem::sActionParamCRCs.inputMove, params.inputMove);
			m_pActionController->SetParam(CItem::sActionParamCRCs.inputRot, params.inputRot);
			m_pActionController->SetParam(CItem::sActionParamCRCs.velocity, params.velocity);
			m_pActionController->SetParam(CItem::sActionParamCRCs.zoomTransition, params.zoomTransitionFactor);
		}
	}
	

	const float STAP_MF_All					= GetGameConstCVar(STAP_MF_All);
	const float STAP_MF_Scope				= GetGameConstCVar(STAP_MF_Scope);
	const float STAP_MF_ScopeVertical		= GetGameConstCVar(STAP_MF_ScopeVertical);
	const float STAP_MF_HeavyWeapon	= GetGameConstCVar(STAP_MF_HeavyWeapon);
	const float STAP_MF_Up					= GetGameConstCVar(STAP_MF_Up);
	const float STAP_MF_Down				= GetGameConstCVar(STAP_MF_Down);
	const float STAP_MF_Left				= GetGameConstCVar(STAP_MF_Left);
	const float STAP_MF_Right				= GetGameConstCVar(STAP_MF_Right);
	const float STAP_MF_Front				= GetGameConstCVar(STAP_MF_Front);
	const float STAP_MF_Back				= GetGameConstCVar(STAP_MF_Back);
	const float STAP_MF_StrafeLeft	= GetGameConstCVar(STAP_MF_StrafeLeft);
	const float STAP_MF_StrafeRight	= GetGameConstCVar(STAP_MF_StrafeRight);
	const float STAP_MF_VerticalMotion	= GetGameConstCVar(STAP_MF_VerticalMotion);
	const float STAP_MF_VelFactorVertical		= GetGameConstCVar(STAP_MF_VelFactorVertical);
	const float STAP_MF_VelFactorHorizontal	= GetGameConstCVar(STAP_MF_VelFactorHorizontal);

	const float SCOPE_LAND_FACTOR = 0.1f;

	const float MIN_VERT_DIR = 0.2f;
	const float MAX_VERT_DIR = 1.0f;
	const float MIN_HORIZ_DIR  = 0.2f;
	const float MAX_HORIZ_DIR  = 3.0f;
	const float POW_VERT   = 1.0f;
	const float POW_HORIZ  = 2.0f;
	const float HORIZ_VEL_SCALE = 0.2f;
	const float runEaseFactor = 0.2f;

	const float minSpeedsTable[2] = { 0.2f, 0.1f };
	const float maxSpeedsTable[2] = { 4.0f, 2.0f };
	const uint32 speedIdx = (params.flags.AreAnyFlagsActive( eWFPAF_zoomed | eWFPAF_crouched ) == true);
	CRY_ASSERT(speedIdx < 2);
	const float MIN_SPEED = minSpeedsTable[speedIdx];
	const float MAX_SPEED = maxSpeedsTable[speedIdx];

	static const float MAX_FALL_HEIGHT = 5.0f;
	static const float MIN_FALL_HEIGHT = 0.2f;

	const float sprintTransitionTable[2] = { params.sprintToRunBlendTime, params.runToSprintBlendTime };
	const uint32 sprintTransitionIdx = (params.flags.AreAllFlagsActive( eWFPAF_sprinting ) == true);
	CRY_ASSERT(sprintTransitionIdx < 2);
	float sprintTransitionTime = sprintTransitionTable[sprintTransitionIdx];
	float strafeFactor = STAP_MF_All;
	float rotationFactor = STAP_MF_All;
	float vertFactor = 1.0f;
	if (params.flags.AreAllFlagsActive( eWFPAF_zoomed))
	{
		strafeFactor *= STAP_MF_Scope * params.shoulderLookParams->strafeScopeFactor;
		vertFactor *= STAP_MF_ScopeVertical;
		rotationFactor *= STAP_MF_Scope * params.shoulderLookParams->rotateScopeFactor;
	}
	if (params.flags.AreAllFlagsActive( eWFPAF_heavyWeapon ))
	{
		rotationFactor *= STAP_MF_HeavyWeapon;
	}

	strafeFactor *= params.overlayFactor * params.strafeFactor;
	rotationFactor *= params.overlayFactor * params.rotationFactor;

	if (m_enabled && frameTime > 0.0f)
	{
		Ang3 inputRot = params.inputRot;

		float horizInterp = 0.0f;
		float vertInterp  = 0.0f;
		//--- Generate our horizontal & vertical target additive factors
		float absAimDirVert = fabs_tpl(params.aimDirection.z) * rotationFactor;
		if (absAimDirVert > MIN_VERT_DIR)
		{
			float factor = (float)__fsel(params.aimDirection.z, STAP_MF_Up, -STAP_MF_Down);
			vertInterp = vertFactor * factor * pow_tpl(min((absAimDirVert - MIN_VERT_DIR) / (MAX_VERT_DIR - MIN_VERT_DIR), 1.0f), POW_VERT);
		}
		vertInterp  += rotationFactor * STAP_MF_VelFactorVertical * clamp_tpl(params.velocity.z * params.shoulderLookParams->verticalVelocityScale, -1.0f, 1.0f);
		vertInterp  += (inputRot.x * STAP_MF_VerticalMotion * rotationFactor);
		vertInterp = clamp_tpl(vertInterp, -1.0f, 1.0f);

		float absInputRotHoriz = fabs_tpl(inputRot.z) * rotationFactor;
		if (absInputRotHoriz > MIN_HORIZ_DIR)
		{
			float factor = (float)__fsel(inputRot.z, STAP_MF_Left, -STAP_MF_Right);
			horizInterp = rotationFactor * factor * pow_tpl(min((absInputRotHoriz - MIN_HORIZ_DIR) / (MAX_HORIZ_DIR - MIN_HORIZ_DIR), 1.0f), POW_HORIZ);
		}
		Vec3 up(0.0f, 0.0f, 1.0f);
		Vec3 idealRight = up.Cross(params.aimDirection);
		idealRight.NormalizeSafe();
		float rightVel = idealRight.Dot(params.velocity);
		horizInterp  += rotationFactor * STAP_MF_VelFactorHorizontal * clamp_tpl(rightVel * HORIZ_VEL_SCALE, -1.0f, 1.0f); 
		horizInterp = clamp_tpl(horizInterp, -1.0f, 1.0f);

		if (params.flags.AreAllFlagsActive( eWFPAF_sprinting))
		{
			horizInterp = 0.0f;
			vertInterp = 0.0f;
		}

		//--- Interpolate from our previous factors
		if ((m_interpVert < -1.0f) && (m_interpHoriz < -1.0f))
		{
			m_interpVert = vertInterp;
			m_interpHoriz = horizInterp;
		}
		else
		{
			const float decStep = (params.shoulderLookParams->easeFactorDec * frameTime);
			const float incStep = (params.shoulderLookParams->easeFactorInc * frameTime);
			float easeFactor = (float)__fsel(fabs_tpl(m_interpVert) - fabs_tpl(vertInterp), decStep, incStep);
			easeFactor = clamp_tpl(easeFactor, 0.0f, 1.0f);
			vertInterp  = m_interpVert = (vertInterp * easeFactor) + (m_interpVert * (1.0f - easeFactor));

			easeFactor = (float)__fsel(fabs_tpl(m_interpHoriz) - fabs_tpl(horizInterp), decStep, incStep);
			easeFactor = clamp_tpl(easeFactor, 0.0f, 1.0f);
			horizInterp = m_interpHoriz = (horizInterp * easeFactor) + (m_interpHoriz * (1.0f - easeFactor));
		}

		//--- Calculate run anim's additive factor
		float horizSpeed = params.velocity.GetLength2D();
		float runFactor = params.flags.AreAllFlagsActive( eWFPAF_onGround ) ? (horizSpeed - MIN_SPEED) / (MAX_SPEED - MIN_SPEED) : 0.0f;
		runFactor	= params.flags.AreAllFlagsActive( eWFPAF_swimming ) ? 1.0f : runFactor;
		const float horizRotFactor = clamp_tpl(-params.aimDirection.z * (absInputRotHoriz - MIN_HORIZ_DIR) / (MAX_HORIZ_DIR - MIN_HORIZ_DIR), 0.0f, 1.0f);
		runFactor = max(runFactor, horizRotFactor);
		runFactor = clamp_tpl(runFactor, 0.0f, 1.0f);
		m_runFactor = ((runFactor * runEaseFactor) + (m_runFactor * (1.0f - runEaseFactor))) * params.overlayFactor;
		m_sprintFactor -= frameTime * (1.0f/sprintTransitionTime);
		m_sprintFactor = params.flags.AreAllFlagsActive( eWFPAF_sprinting ) ? 1.0f : max(m_sprintFactor, 0.0f);

		//--- Calculate movement acceleration
		const Vec3 inputMoveAugmented = Vec3(
			params.inputMove.x * (1.0f / params.shoulderLookParams->accelerationFrontAugmentation),
			params.inputMove.y * params.shoulderLookParams->accelerationFrontAugmentation,
			0.0f);
		Interpolate(m_smoothedVelocity, inputMoveAugmented, params.shoulderLookParams->velocityLowPassFilter, frameTime);

		const Vec3 velocityDerivative = (m_smoothedVelocity - m_lastVelocity) / frameTime;
		m_lastVelocity = m_smoothedVelocity;

		float interpFront = SignedPow(clamp_tpl(velocityDerivative.y * params.shoulderLookParams->velocityInterpolationMultiplier, -1.0f, 1.0f), params.shoulderLookParams->accelerationSmoothing);
		interpFront *= (float)__fsel(interpFront, STAP_MF_Front, STAP_MF_Back) * strafeFactor;
		Interpolate(m_interpFront, interpFront, params.shoulderLookParams->velocityLowPassFilter, frameTime);
		float interpSide = SignedPow(clamp_tpl(velocityDerivative.x * params.shoulderLookParams->velocityInterpolationMultiplier, -1.0f, 1.0f), params.shoulderLookParams->accelerationSmoothing);
		interpSide *= (float)__fsel(interpSide, STAP_MF_StrafeLeft, STAP_MF_StrafeRight) * strafeFactor;
		Interpolate(m_interpSide, interpSide, params.shoulderLookParams->velocityLowPassFilter, frameTime);
	}

	{
		SWeightData data;
		float vertical = m_interpVert+m_interpFront;
		float horizontal = m_interpHoriz-m_interpSide;

		data.weights[(vertical > 0.f) ? 1 : 0] = fabsf(vertical);
		data.weights[(horizontal > 0.f) ? 3 : 2] = fabsf(horizontal);	
		
		if(m_pSwayAction)
		{
			m_pSwayAction->SetParam("SwayParams", data);
		}
		if(m_replay && m_pActionController)
		{
			m_pActionController->SetParam("SwayParams", data);
		}
	}

	//--- Check for pushing a bump animation
	float currentHeight = params.position.z;
	bool aboutToLand = (params.groundDistance < (-params.velocity.z * frameTime * 2.0f));
	if (!params.flags.AreAllFlagsActive( eWFPAF_onGround ) && !aboutToLand)
	{
		m_fallFromHeight = m_falling ? max(m_fallFromHeight, currentHeight) : currentHeight;
	}
	else if (m_falling)
	{
		float fallHeight = (m_fallFromHeight - currentHeight);
		if (fallHeight > MIN_FALL_HEIGHT)
		{
			float fallFactor = SATURATE((fallHeight - MIN_FALL_HEIGHT) / (MAX_FALL_HEIGHT - MIN_FALL_HEIGHT));

			if (m_pActionController)
			{
				TAction<SAnimationContext>* pBumpAction = new TAction<SAnimationContext>(PP_MovementAction, PlayerMannequin.fragmentIDs.bump);
				pBumpAction->SetParam(CItem::sActionParamCRCs.fallFactor, fallFactor);
				m_pActionController->Queue(*pBumpAction);
			}
		}
	}
	m_falling = !params.flags.AreAllFlagsActive( eWFPAF_onGround ) && !aboutToLand;
}

void CWeaponFPAiming::SetActionController(IActionController *pActionController)
{
	if (m_pActionController != pActionController)
	{
		m_pActionController = pActionController;

		if (m_pAction)
		{
			SAFE_RELEASE(m_pAction);
			SAFE_RELEASE(m_pMotionAction);
			SAFE_RELEASE(m_pSwayAction);
		}	
	}
}

void CWeaponFPAiming::RestartMannequin()
{
	if (m_pActionController && m_pAction)
	{
		m_pActionController->Queue(*m_pAction);
		m_pActionController->Queue(*m_pMotionAction);
		m_pActionController->Queue(*m_pSwayAction);
	}
}
