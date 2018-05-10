// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __WEAPONFP_AIMING_H__
#define __WEAPONFP_AIMING_H__

#include "ItemDefinitions.h"
#include "ItemSharedParams.h"
#include <CryCore/CryFlags.h>

class CWeapon;
struct SZoomParams;
struct SProceduralRecoilParams;

enum EWeaponFPAimingFlags
{
	eWFPAF_onGround        = BIT(0),
	eWFPAF_swimming        = BIT(1),
	eWFPAF_zoomed          = BIT(2),
	eWFPAF_heavyWeapon     = BIT(3),
	eWFPAF_sprinting       = BIT(4),
	eWFPAF_crouched        = BIT(5),
	eWFPAF_thirdPerson     = BIT(6),
	eWFPAF_ledgeGrabbing   = BIT(7),
	eWFPAF_aimStabilized   = BIT(8),
	eWFPAF_jump					   = BIT(9)
};

struct SParams_WeaponFPAiming
{
	SParams_WeaponFPAiming() 
		: 
		shoulderLookParams(0),
		skelAnim(NULL),
		characterInst(NULL),
		aimDirection(ZERO),
		velocity(ZERO),
		position(ZERO),
		inputRot(ZERO),
		inputMove(ZERO),
		groundDistance(0.0f),
		rotationFactor(1.0f),
		strafeFactor(1.0f),
		movementFactor(1.0f),
		overlayFactor(1.0f),
		runToSprintBlendTime(0.2f),
		sprintToRunBlendTime(0.2f),
		poseActionTime(0.0f),
		zoomTransitionFactor(0.f)
	{
		flags.AddFlags(eWFPAF_onGround);
	}

	const SAimLookParameters* shoulderLookParams;
	struct ISkeletonAnim  *skelAnim;
	struct ICharacterInstance *characterInst;

	Ang3			inputRot;
	Vec3			inputMove;
	Vec3			aimDirection;
	Vec3			velocity;
	Vec3			position;
	float			groundDistance;
	float			runToSprintBlendTime;
	float			sprintToRunBlendTime;
	float			rotationFactor;
	float			strafeFactor;
	float			movementFactor;
	float			overlayFactor;
	float			poseActionTime;
	float			zoomTransitionFactor;
	
	CCryFlags<uint32> flags;
};

class IActionController;
class CWeaponFPAiming
{
	friend class CFPAimingAction;

public:

	CWeaponFPAiming(bool isReplay);
	~CWeaponFPAiming();

	void Update(const SParams_WeaponFPAiming &params);
	void SetActive(bool active);
	float GetRunFactor() const { return m_runFactor; }

	float GetHorizontalSweay() const {return m_interpHoriz;}
	float GetVerticalSweay() const {return m_interpVert;}
	float GetSprintFactor() const {return m_sprintFactor;}

	void SetActionController(IActionController *pActionController);

	void ReleaseActions();
	void RestartMannequin();

private:
	IActionController *m_pActionController;

	float m_interpVert;
	float m_interpHoriz;
	float m_fallFromHeight;
	float m_runFactor;
	float m_sprintFactor;
	bool  m_falling;
	bool  m_enabled;
	bool  m_replay;
	Vec3  m_smoothedVelocity;
	Vec3  m_lastVelocity;
	float m_interpFront;
	float m_interpSide;

	class CFPAimingAction *m_pAction;
	class CFPMotionAction *m_pMotionAction;
	class CFPSwayAction		*m_pSwayAction;

};

#endif //__WEAPONFP_AIMING_H__
