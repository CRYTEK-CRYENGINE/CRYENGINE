// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Helper class to setup / update ik torso aim for first person 

-------------------------------------------------------------------------
History:
- 20-8-2009		Benito Gangoso Rodriguez
- 09-9-2009		Tom Berry - reintegrated into non-component system

*************************************************************************/

#pragma once

#ifndef _IKTORSOAIM_HELPER_H_
#define _IKTORSOAIM_HELPER_H_

#include <CryAnimation/ICryAnimation.h>
#include "ITransformationPinning.h"
#include "Animation/PoseModifier/IKTorsoAim.h"
#include "ProceduralWeaponAnimation.h"

struct SParams_WeaponFPAiming;

class CIKTorsoAim_Helper
{
public:

	struct SIKTorsoParams
	{
		explicit SIKTorsoParams(ICharacterInstance *_character, ICharacterInstance *_shadowCharacter, Vec3 _aimDirection, QuatT _viewOffset, Vec3 _targetPosition, int _effectorJoint = -1, int _aimJoint = -1, int _pinJoint = -1, bool _updateTranslationPinning = true, bool _needsSTAPPosition = true, float _blendRate = 4.0f)
			: character(_character)
			, shadowCharacter(_shadowCharacter)
			, viewOffset(_viewOffset)
			, aimDirection(_aimDirection)
			, targetPosition(_targetPosition)
			, blendRate(_blendRate)
			, effectorJoint(_effectorJoint)
			, aimJoint(_aimJoint)
			, pinJoint(_pinJoint)
			, updateTranslationPinning(_updateTranslationPinning)
			, needsSTAPPosition(_needsSTAPPosition)
		{
		}

		ICharacterInstance *character;
		ICharacterInstance *shadowCharacter;
		QuatT viewOffset;
		Vec3 aimDirection;
		Vec3 targetPosition;
		float blendRate;
		int effectorJoint;
		int aimJoint;
		int pinJoint;
		bool updateTranslationPinning;
		bool needsSTAPPosition;
	};

public:
	
	CIKTorsoAim_Helper();

	void Update(SIKTorsoParams& ikTorsoParams);
	void Enable(bool snap = false);
	void Disable(bool snap = false);
	void Reset()
	{
		m_initialized					= false;
		m_enabled							= false;
		m_blendFactor					= 0.0f;
		m_blendFactorPosition	= 0.0f;
	}
	bool IsEnabled() const
	{
		return m_enabled;
	}

	float GetBlendFactor() const
	{
		return m_blendFactor;
	}

	const QuatT &GetLastEffectorTransform() const;

private:
	
	void Init(SIKTorsoParams& ikTorsoParams);

	std::shared_ptr<CIKTorsoAim> m_ikTorsoAim;
	ITransformationPinningPtr  m_transformationPin;
	
	bool m_initialized;
	bool m_enabled;

	float m_blendFactor;
	float m_blendFactorPosition;
};

#endif