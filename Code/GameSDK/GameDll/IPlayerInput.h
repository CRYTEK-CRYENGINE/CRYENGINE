// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IPLAYERINPUT_H__
#define __IPLAYERINPUT_H__

#pragma once

#include <CryAISystem/IAgent.h> // for EStance
#include <CryNetwork/ISerialize.h>
#include "IGameObject.h"
#include "Network/SerializeDirHelper.h"

class CPlayer;

struct SSerializedPlayerInput
{
	uint8 stance;
	uint8 bodystate;
	Vec3 deltaMovement;
	Vec3 lookDirection;
	Vec3 bodyDirection;
	bool sprint;
	bool aiming;
	bool usinglookik;
	bool allowStrafing;
	bool isDirty;
	bool inAir;
	float pseudoSpeed;
	Vec3 position;
	uint8 physCounter;

	EntityId standingOn;

	SSerializedPlayerInput() :
		stance(STANCE_STAND),
		bodystate(0),
		deltaMovement(ZERO),
		position(ZERO),
		lookDirection(FORWARD_DIRECTION),
		bodyDirection(FORWARD_DIRECTION),
		sprint(false),
		aiming(false),
		usinglookik(false),
		allowStrafing(true),
		isDirty(false),
		inAir(false),
		pseudoSpeed(0.0f),
		standingOn(0),
		physCounter(0)
	{
	}

	void Serialize( TSerialize ser, EEntityAspects aspect);
};

#define CancelPressFlagList(func)        \
	func(kCancelPressFlag_switchItem)      \
	func(kCancelPressFlag_crouchOrProne)   \
	func(kCancelPressFlag_forceAll)        \
	func(kCancelPressFlag_jump)            \

AUTOENUM_BUILDFLAGS_WITHZERO(CancelPressFlagList, kCancelPressFlag_none);

typedef TBitfield TCancelButtonBitfield;

#ifndef _RELEASE
#define DEFINE_CANCEL_HANDLER_NAME(...) string DbgGetCancelHandlerName() const { return string().Format(__VA_ARGS__); }
#else
#define DEFINE_CANCEL_HANDLER_NAME(...)
#endif

struct IPlayerInputCancelHandler
{
public:
	virtual ~IPlayerInputCancelHandler() {}
	virtual bool HandleCancelInput(CPlayer & player, TCancelButtonBitfield cancelButtonPressed) = 0;

#ifndef _RELEASE
	virtual string DbgGetCancelHandlerName() const = 0;
#endif
};

enum ECancelHandlerSlot
{
	kCHS_default,
	kCHS_num
};

struct IPlayerInput
{
	enum EInputType
	{
		NULL_INPUT = 0,
		PLAYER_INPUT,
		NETPLAYER_INPUT,
		AI_INPUT,
		DEDICATED_INPUT,
	
		LAST_INPUT_TYPE,
	};

	virtual ~IPlayerInput() {};

	virtual void PreUpdate() = 0;
	virtual void Update() = 0;
	virtual void PostUpdate() = 0;

	virtual void OnAction( const ActionId& action, int activationMode, float value ) = 0;

	virtual void SetState( const SSerializedPlayerInput& input ) = 0;
	virtual void GetState( SSerializedPlayerInput& input ) = 0;

	virtual void RemoveInputCancelHandler (IPlayerInputCancelHandler * handler) {}
	virtual void AddInputCancelHandler (IPlayerInputCancelHandler * handler, ECancelHandlerSlot slot = kCHS_default) {}
	virtual bool CallTopCancelHandler(TCancelButtonBitfield cancelButtonPressed = kCancelPressFlag_none) { return false; }
	virtual bool CallAllCancelHandlers() { return false; }

	virtual void Reset() = 0;
	virtual void DisableXI(bool disabled) = 0;
	virtual void ClearXIMovement() = 0;

	virtual EInputType GetType() const = 0;
	
	virtual uint32 GetMoveButtonsState() const = 0;
	virtual uint32 GetActions() const = 0;

	virtual float GetLastRegisteredInputTime() const = 0;
	virtual void SerializeSaveGame( TSerialize ser ) = 0;

	virtual void GetMemoryUsage(ICrySizer *pSizer ) const =0;
};

#endif
