// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __NETPLAYERINPUT_H__
#define __NETPLAYERINPUT_H__

#pragma once

#include "IPlayerInput.h"
#include "GameCVars.h"

#define SNAP_ERROR_LOGGING 1
#if defined(_RELEASE)
#undef SNAP_ERROR_LOGGING
#define SNAP_ERROR_LOGGING 0
#endif // defined(_RELEASE)

typedef uint8 TSnapType;
enum
{
	eSnap_None,
	eSnap_Minor,
	eSnap_Normal,
	eSnap_Major
};

class CPlayer;

class CNetLerper
{
public:
	enum { k_noError = 0x0, k_desiredError = 0x1, k_lerpError = 0x2 };

	struct SSettings
	{
		float maxInterSpeed;            // Max speed to clamp the interpolation to
		float lookAhead;                // Interpolation is directed towards a predicted position this far in the future
		float maxLookAhead;             // Clamp the look ahead

		float snapDistXY;               // Snap distance between desired and entityPos
		float snapDistZ;
		float snapDistZInAir;

		float minTimeOnGround;

		float allowableLerpErrorXY;     // Allowable error between the predicted lerp position and the entity

		float minLerpDistance;          // Below this speed and distance stop lerping
		float minLerpSpeed;
		
		float snapDistMarkedMajor;

		SSettings();
	};

	struct SLerpPoint
	{
		Vec3 pos;
		Vec3 worldPos;
		Vec3 vel;
	};

	struct SPrediction
	{
		Vec3 predictedPos;     // Where the entity should be this frame
		Vec3 lerpVel;          // What velocity should be set for next frame
		bool shouldSnap;       // Should the entity snap to the predicted position
	};

public:
	CNetLerper();
	~CNetLerper();

	void ApplySettings(SSettings* settings);
	void Reset(CPlayer* player=NULL);

	void AddNewPoint(const Vec3& inPos, const Vec3& inVel, const Vec3& entityPos, EntityId standingOn);
	void Update(float dt, const Vec3& entityPos, SPrediction& predictionOut, const Vec3& groundVel, bool bInAirOrJumping);

	int GetLerpError(const Vec3& errorToDesiredPos, const Vec3& lerpError) const;
	void LogSnapError();

	ILINE void Disable ( ) { m_enabled = false; }
	ILINE TSnapType GetSnappedLastUpdate() const { return m_snapType; }

	void DebugDraw(const SPrediction& prediction, const Vec3& entityPos, bool hadNewData);

protected:
	const SSettings* m_settings;
	SLerpPoint m_desired;
	Vec3 m_lerpedPos;
	Vec3 m_lerpedError;
	float m_clock;
	TSnapType m_snapType;
	EntityId m_standingOn;
	CPlayer* m_pPlayer;
#if SNAP_ERROR_LOGGING
	bool m_snapErrorInProgress;
#endif // SNAP_ERROR_LOGGING
	bool m_enabled;

public:
#if !defined(_RELEASE)
	struct Debug;
	Debug* m_debug;
#endif
};



class CNetPlayerInput : public IPlayerInput
{
public:
	CNetPlayerInput( CPlayer * pPlayer );

	// IPlayerInput
	virtual void PreUpdate();
	virtual void Update();
	virtual void PostUpdate();

	virtual void SetState( const SSerializedPlayerInput& input );
	virtual void GetState( SSerializedPlayerInput& input );

	virtual void OnAction(  const ActionId& action, int activationMode, float value  ) {};

	virtual void Reset();
	virtual void DisableXI(bool disabled);
	virtual void ClearXIMovement() {};

	virtual void GetMemoryUsage(ICrySizer * s) const {s->Add(*this);}

	virtual EInputType GetType() const
	{
		return NETPLAYER_INPUT;
	};

	ILINE TSnapType GetSnappedLastUpdate() const { return m_lerper.GetSnappedLastUpdate(); }

	ILINE virtual uint32 GetMoveButtonsState() const { return 0; }
	ILINE virtual uint32 GetActions() const { return 0; }

	virtual float GetLastRegisteredInputTime() const { return 0.0f; }
	virtual void SerializeSaveGame( TSerialize ser ) {}
	// ~IPlayerInput

	const Vec3& GetDesiredVelocity(bool& inAir) const { inAir=m_curInput.inAir; return m_desiredVelocity; }
	const Vec3 GetRequestedVelocity() const { return m_curInput.deltaMovement * g_pGameCVars->pl_netSerialiseMaxSpeed; }

	ILINE bool HasReceivedUpdate() const
	{
		return (m_lastUpdate.GetValue() != 0);
	}

protected:
	float CalculatePseudoSpeed() const;

	void DoSetState( const SSerializedPlayerInput& input );
	void UpdateInterpolation();
	void UpdateMoveRequest();
	void UpdateDesiredVelocity();

	CPlayer * m_pPlayer;
	SSerializedPlayerInput m_curInput;

	CTimeValue m_lastUpdate;

	Vec3 m_desiredVelocity;

	Vec3 m_lookDir;

	bool m_newInterpolation;

	static CNetLerper::SSettings m_lerperSettings;
	CNetLerper m_lerper;
	Vec3 m_lerpVel;
};


#endif
