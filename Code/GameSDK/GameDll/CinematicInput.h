// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Controls script variables coming from track view to add some 
							control/feedback during cutscenes

-------------------------------------------------------------------------
History:
- 28:04:2010   Created by Benito Gangoso Rodriguez

*************************************************************************/

#pragma once

#ifndef _CINEMATIC_INPUT_H_
#define _CINEMATIC_INPUT_H_

#define CINEMATIC_INPUT_PC_MOUSE 1


class CWeapon;

class CCinematicInput
{
	struct SUpdateContext
	{
		SUpdateContext()
			: m_frameTime(0.0333f)
			, m_lookUpLimit(0.0f)
			, m_lookDownLimit(0.0f)
			, m_lookLeftLimit(0.0f)
			, m_lookRightLimit(0.0f)
			, m_recenter(true)
		{

		}

		float m_frameTime;
		float m_lookUpLimit;
		float m_lookDownLimit;
		float m_lookLeftLimit;
		float m_lookRightLimit;
		bool  m_recenter;
	};

	struct SCinematicWeapon
	{
		SCinematicWeapon()
		{
			Reset();
		}

		void Reset()
		{
			m_weaponId = 0;
			m_parentId = 0;
		}

		EntityId	m_weaponId;
		EntityId	m_parentId;
	};

public:

	enum Weapon
	{
		eWeapon_Primary = 0,
		eWeapon_Secondary,
		eWeapon_ClassCount,
	};

	CCinematicInput();
	~CCinematicInput();

	void OnBeginCutScene(int cutSceneFlags);
	void OnEndCutScene(int cutSceneFlags);

	void Update(float frameTime);

	void SetUpWeapon( const CCinematicInput::Weapon& weaponClass, const IEntity* pEntity );
	void OnAction( const EntityId actorId, const ActionId& actionId, int activationMode, float value);

	ILINE bool IsAnyCutSceneRunning() const 
	{ 
		return (m_cutsceneRunningCount > 0); 
	}

	ILINE bool IsPlayerNotActive() const
	{
		return (m_cutscenesNoPlayerRunningCount > 0);
	}

	void OnRayCastDataReceived(const QueuedRayID& rayID, const RayCastResult& result);

private:

	void UpdateForceFeedback(IScriptSystem* pScriptSystem, float frameTime);
	void UpdateAdditiveCameraInput(IScriptSystem* pScriptSystem, float frameTime);

	void UpdateWeapons();
	void UpdateWeaponOrientation( IEntity* pWeaponEntity, const Vec3& targetPosition );

	Ang3 UpdateAdditiveCameraInputWithMouse(const CCinematicInput::SUpdateContext& updateCtx, const Ang3& rawMouseInput);
	Ang3 UpdateAdditiveCameraInputWithController(const CCinematicInput::SUpdateContext& updateCtx, const Ang3& rawMouseInput);

	void RefreshInputMethod(const bool isMouseInput);

	void ClearCutSceneVariables();
	void DisablePlayerForCutscenes();
	void ReEnablePlayerAfterCutscenes();
	void TogglePlayerThirdPerson(bool bEnable);

	CWeapon* GetWeapon(const CCinematicInput::Weapon& weaponClass) const;

	Ang3	m_controllerAccumulatedAngles;
	int		m_cutsceneRunningCount;
	int		m_cutscenesNoPlayerRunningCount;
	bool	m_bPlayerIsThirdPerson;
	bool	m_bPlayerWasInvisible;
	bool	m_bCutsceneDisabledUISystem;
	int		m_iHudState;

	SCinematicWeapon m_weapons[eWeapon_ClassCount];
	QueuedRayID m_aimingRayID;
	float				m_aimingDistance;

#if CINEMATIC_INPUT_PC_MOUSE
	Ang3	m_mouseAccumulatedAngles;
	Ang3	m_mouseAccumulatedInput;
	float m_mouseRecenterTimeOut;
	bool	m_lastUpdateWithMouse;
#endif
};

#endif