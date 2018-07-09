// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************

-------------------------------------------------------------------------
History:
  October 2010 : Jens Schöbel move overlay

*************************************************************************/

// handles turning actions into CMovementRequests and setting player state
// for the local player

#ifndef __PLAYERINPUT_H__
#define __PLAYERINPUT_H__

#pragma once

#include "IActionMapManager.h"
#include "IPlayerInput.h"
#include "Player.h"


struct SPlayerStats;


#define MAX_FREE_CAM_DATA_POINTS 20

#ifndef _RELEASE
#define INCLUDE_DEBUG_ACTIONS
#endif

//#if defined(_RELEASE)
//#define FREE_CAM_SPLINE_ENABLED 0
//#else
#define FREE_CAM_SPLINE_ENABLED 1
//#endif

class CPlayerInput : public IPlayerInput, public IActionListener
{
public:

#if FREE_CAM_SPLINE_ENABLED
	struct SFreeCamPointData
	{
		SFreeCamPointData()
			: valid(false), distanceFromLast(0.f)
		{ }

		bool valid;
		Vec3 position;
		Vec3 lookAtPosition;
		float distanceFromLast;
	};

	SFreeCamPointData m_freeCamData[MAX_FREE_CAM_DATA_POINTS];

	typedef spline::CatmullRomSpline<Vec3> TFreeCamSpline;

	TFreeCamSpline m_freeCamSpline;
	TFreeCamSpline m_freeCamLookAtSpline;

	bool m_freeCamPlaying;
	int m_freeCamCurrentIndex;
	float m_freeCamPlayTimer;
	float m_freeCamTotalPlayTime;
#endif

	enum EMoveButtonMask
	{
		eMBM_Forward	= (1 << 0),
		eMBM_Back			= (1 << 1),
		eMBM_Left			= (1 << 2),
		eMBM_Right		= (1 << 3)
	};

	CPlayerInput( CPlayer * pPlayer );
	~CPlayerInput();

	// IPlayerInput
	virtual void PreUpdate();
	virtual void Update();
	virtual void PostUpdate();
	virtual void SetState( const SSerializedPlayerInput& input );
	virtual void GetState( SSerializedPlayerInput& input );
	virtual void Reset();
	virtual void DisableXI(bool disabled);
	virtual void ClearXIMovement();
	virtual void GetMemoryUsage(ICrySizer * s) const {s->Add(*this);}
	virtual EInputType GetType() const { return PLAYER_INPUT;	};
	ILINE virtual uint32 GetMoveButtonsState() const { return m_moveButtonState; }
	ILINE virtual uint32 GetActions() const { return m_actions; }
	virtual float GetLastRegisteredInputTime() const { return m_lastRegisteredInputTime; }
	virtual void SerializeSaveGame( TSerialize ser );
	ILINE Ang3& GetRawControllerInput() { return m_xi_deltaRotationRaw; }
	ILINE Ang3& GetRawMouseInput() { return m_lastMouseRawInput; }
	ILINE bool IsAimingWithMouse() const { return m_isAimingWithMouse; }
	ILINE bool IsAimingWithHMD() const { return m_isAimingWithHMD; }
	ILINE bool IsCrouchButtonDown() const {return m_crouchButtonDown;}
	// ~IPlayerInput

	// IActionListener
	virtual void OnAction( const ActionId& action, int activationMode, float value );
	// ~IActionListener

	void AddCrouchAction();
	void ClearProneAction() {m_actions &= ~ACTION_PRONE;}
	void ClearCrouchAction() {m_actions &= ~ACTION_CROUCH;}
	void ClearSprintAction() {m_actions &= ~ACTION_SPRINT;}
	void ClearAllExceptAction(uint32 actionFlags);
	void SetProneAction() {m_actions |= ACTION_PRONE;}

	void AddFlyCamPoint();
	void AddFlyCamPoint(Vec3 pos, Vec3 lookAtPos);
	void FlyCamPlay();
	void ForceStopSprinting();

	bool CallTopCancelHandler(TCancelButtonBitfield cancelButtonPressed = kCancelPressFlag_none);
	bool CallAllCancelHandlers();

	ILINE bool IsNearLookAtTarget() const { return m_isNearTheLookAtTarget; }

private:
	void HandleMovingDetachedCamera(const Ang3 &deltaRotation, const Vec3 &m_deltaMovement);
	EStance FigureOutStance();
	void AdjustMoveButtonState( EMoveButtonMask buttonMask, int activationMode );
	bool CheckMoveButtonStateChanged( EMoveButtonMask buttonMask, int activationMode );
	float MapControllerValue(float value, float scale, float curve, bool inverse);
	Ang3 UpdateXIInputs(const Ang3& inputAngles, bool bScaling = true);

	void RemoveInputCancelHandler (IPlayerInputCancelHandler * handler);
	void AddInputCancelHandler (IPlayerInputCancelHandler * handler, ECancelHandlerSlot slot);

	void UpdateWeaponToggle();
	void ToggleVisor();

	bool IsPlayerOkToAction() const;

	void ApplyMovement(Vec3 delta);
	const Vec3 &FilterMovement(const Vec3 &desired);

	bool CanMove() const;
	bool CanCrouch() const;

	bool OnActionMoveForward(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionMoveBack(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionMoveLeft(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionMoveRight(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionRotateYaw(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionRotatePitch(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionHMDRotateYaw(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionHMDRotatePitch(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionHMDRotateRoll(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionVRotatePitch(EntityId entityId, const ActionId& actionId, int activationMode, float value); // needed so player can shake unfreeze while in a vehicle
	bool OnActionVRotateYaw(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionJump(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionCrouch(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionSprint(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionSprintXI(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionUse(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionAttackRightTrigger(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionSpecial(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionChangeFireMode(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionMouseWheelClick(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionToggleVision(EntityId entityId, const ActionId& actionId, int activationMode, float value);

	bool OnActionMindBattle(EntityId entityId, const ActionId& actionId, int activationMode, float value);

	// Cheats
	bool OnActionThirdPerson(EntityId entityId, const ActionId& actionId, int activationMode, float value);
#ifdef INCLUDE_DEBUG_ACTIONS
	bool OnActionFlyMode(EntityId entityId, const ActionId& actionId, int activationMode, float value);
#endif
	bool OnActionGodMode(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionAIDebugDraw(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionPDrawHelpers(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionDMode(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionRecordBookmark(EntityId entityId, const ActionId& actionId, int activationMode, float value);
#ifdef INCLUDE_DEBUG_ACTIONS
	bool OnActionMannequinDebugAI(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionMannequinDebugPlayer(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionAIDebugCenterViewAgent(EntityId entityId, const ActionId& actionId, int activationMode, float value);
#endif

	bool OnActionXIRotateYaw(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionXIRotatePitch(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionXIMoveX(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionXIMoveY(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionXIDisconnect(EntityId entityId, const ActionId& actionId, int activationMode, float value);

	bool OnActionInvertMouse(EntityId entityId, const ActionId& actionId, int activationMode, float value);
		
	bool OnActionFlyCamMoveX(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionFlyCamMoveY(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionFlyCamMoveUp(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionFlyCamMoveDown(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionFlyCamSpeedUp(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionFlyCamSpeedDown(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionFlyCamTurbo(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionFlyCamRotateYaw(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionFlyCamRotatePitch(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionFlyCamSetPoint(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionFlyCamPlay(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionFlyCamClear(EntityId entityId, const ActionId& actionId, int activationMode, float value);

	bool OnActionTagging(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionLookAt(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionPrePickUpItem(EntityId entityId, const ActionId& actionId, int activationMode, float value);

  bool OnActionMoveOverlayTurnOn( EntityId entityId, const ActionId& actionId, int activationMode, float value );
  bool OnActionMoveOverlayTurnOff( EntityId entityId, const ActionId& actionId, int activationMode, float value );
  bool OnActionMoveOverlayWeight( EntityId entityId, const ActionId& actionId, int activationMode, float value );
  bool OnActionMoveOverlayX( EntityId entityId, const ActionId& actionId, int activationMode, float value );
  bool OnActionMoveOverlayY( EntityId entityId, const ActionId& actionId, int activationMode, float value );

	bool OnActionRespawn(EntityId entityId, const ActionId& actionId, int activationMode, float value);

	bool OnActionSelectNextItem(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionQuickGrenadeThrow(EntityId entityId, const ActionId& actionId, int activationMode, float value);

	void NormalizeInput(float& fX, float& fY, float fCoeff, float fCurve);
	void DrawDebugInfo();
	void GetMappingParameters(float& fCoeff, float& fCurve) const;

	void SetSliding(bool set);
	bool PerformJump();

	void SelectNextItem(int direction, bool keepHistory, int category, bool disableVisorFirst);
	void ToggleItem(int primaryCategory, int secondaryCategory, bool disableVisorFirst);

	const bool AllowToggleWeapon(const int activationMode, const float currentTime);
	bool CanLookAt();
	void UpdateAutoLookAtTargetId(float frameTime);

	bool IsItemPickUpScriptAction(const ActionId& actionId) const;

	void ClearDeltaMovement();

	ILINE int DoubleTapGrenadeCategories() 
	{
		if(gEnv->bMultiplayer)
		{
			return g_pGameCVars->g_enableMPDoubleTapGrenadeSwitch ? eICT_Explosive | eICT_Grenade : 0;
		}
		return eICT_Grenade; 
	}
	bool DoubleTapGrenadeAvailable();
	
	static bool UseQuickGrenadeThrow();
	CPlayer* m_pPlayer;
	IPlayerInputCancelHandler * m_inputCancelHandler[kCHS_num];

	uint32 m_actions;
	uint32 m_lastActions;

	Vec3 m_deltaMovement;
	Vec3 m_xi_deltaMovement;
	
	Ang3 m_deltaRotation;
	Ang3 m_lastMouseRawInput;
	Ang3 m_xi_deltaRotation;
	Ang3 m_xi_deltaRotationRaw;
	Ang3 m_HMD_deltaRotation;
	bool m_isNearTheLookAtTarget;
	Ang3 m_lookAtSmoothRate;  // used for interpolation temporal value when doing a lookAt

	Vec3 m_flyCamDeltaMovement;
	Ang3 m_flyCamDeltaRotation;
	bool m_flyCamTurbo;

	float m_suitArmorPressTime;
	float m_suitStealthPressTime;
	float m_lookAtTimeStamp;
	float m_fLastWeaponToggleTimeStamp;
	float m_fLastNoGrenadeTimeStamp;

	EntityId m_standingOn;
	
	uint32 m_moveButtonState;
	Vec3 m_filteredDeltaMovement;

	int m_lastSerializeFrameID;

	float m_lastSensitivityFactor;
	float m_lastRegisteredInputTime;
	CTimeValue m_nextSlideTime;
	
	bool m_bDisabledXIRot;
	bool m_bUseXIInput;
	bool m_lookingAtButtonActive;
	bool m_isAimingWithMouse;
	bool m_isAimingWithHMD;
	bool m_crouchButtonDown;
	bool m_sprintButtonDown;
	bool m_autoPickupMode;
	bool m_openingVisor;
	bool m_playerInVehicleAtFrameStart;

	enum E_AutoPickupModeEnabled
	{
		EAPM_None = 0,
		EAPM_SP		= 1,
		EAPM_MP		= 2,
	};

	TActionHandler<CPlayerInput> m_actionHandler;

	struct SDebugDrawStats
	{
		SDebugDrawStats() : lastRaw(ZERO), lastProcessed(ZERO), vOldAimPos(ZERO) {}

		Ang3 lastRaw;
		Ang3 lastProcessed;
		Vec3 vOldAimPos;
	} m_debugDrawStats;

  struct SMoveOverlay
  {
    SMoveOverlay() 
			: isEnabled(false)
			, weight(0.0f)
			, moveX(0.0f)
			, moveY(0.0f)
		{

		}

    void Reset() {isEnabled = false; weight=0;}
    
		Vec3 GetMixedOverlay(const Vec3 &delta ) const
    {
      Vec3 mixedDelta(delta);
      if ( isEnabled ) 
			{
        mixedDelta.x = delta.x - weight*(delta.x+moveX);
        mixedDelta.y = delta.y - weight*(delta.y+moveY);
      }
      return mixedDelta;
    };

    bool isEnabled;
    float weight;
    float moveX;
    float moveY;

  } m_moveOverlay;

	float m_pendingYawSnapAngle;
	bool m_bYawSnappingActive;
};




class CAIInput : public IPlayerInput
{
public:
	enum EMoveButtonMask
	{
		eMBM_Forward	= (1 << 0),
		eMBM_Back			= (1 << 1),
		eMBM_Left			= (1 << 2),
		eMBM_Right		= (1 << 3)
	};

	CAIInput( CPlayer * pPlayer );
	~CAIInput();

	// IPlayerInput
	virtual void PreUpdate() {};
	virtual void Update() {};
	virtual void PostUpdate() {};

	virtual void OnAction( const ActionId& action, int activationMode, float value ) {};

	virtual void SetState( const SSerializedPlayerInput& input );
	virtual void GetState( SSerializedPlayerInput& input );

	virtual void Reset() {};
	virtual void DisableXI(bool disabled) {};
	virtual void ClearXIMovement() {};

	ILINE virtual uint32 GetMoveButtonsState() const { return 0; }
	ILINE virtual uint32 GetActions() const { return 0; }

	virtual void GetMemoryUsage(ICrySizer * s) const {s->Add(*this);}

	virtual EInputType GetType() const
	{
		return AI_INPUT;
	};

	virtual float GetLastRegisteredInputTime() const { return 0.0f; }
	virtual void SerializeSaveGame( TSerialize ser ) {}
	// ~IPlayerInput

private:
	CPlayer* m_pPlayer;
	SPlayerStats* m_pStats;
	Vec3 m_deltaMovement;
};

#endif
