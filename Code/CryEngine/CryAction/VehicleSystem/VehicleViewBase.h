// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a base class for the vehicle views

   -------------------------------------------------------------------------
   History:
   - 06:07:2006: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEVIEWBASE_H__
#define __VEHICLEVIEWBASE_H__

class CVehicleSeat;

class CVehicleViewBase
	: public IVehicleView
{
public:

	CVehicleViewBase();

	// IVehicleView
	virtual bool        Init(IVehicleSeat* pSeat, const CVehicleParams& table);
	virtual void        Reset();
	virtual void        ResetPosition() {};
	virtual void        Release()       { delete this; }

	virtual const char* GetName()       { return NULL; }

	virtual bool        IsThirdPerson() = 0;
	virtual bool        IsPassengerHidden()      { return m_hidePlayer; }

	virtual bool        IsEnabled() const        { return m_isEnabled; }
	virtual void        SetEnabled(bool enabled) { m_isEnabled = enabled; }

	virtual void        OnStartUsing(EntityId passengerId);
	virtual void        OnStopUsing();

	virtual void        OnAction(const TVehicleActionId actionId, int activationMode, float value);
	virtual void        UpdateView(SViewParams& viewParams, EntityId playerId) {}

	virtual void        Update(const float frameTime);
	virtual void Serialize(TSerialize serialize, EEntityAspects);

	virtual void SetDebugView(bool debug)    { m_isDebugView = debug; }
	virtual bool IsDebugView()               { return m_isDebugView; }

	virtual bool ShootToCrosshair()          { return true; }
	virtual bool IsAvailableRemotely() const { return m_isAvailableRemotely; }
	// ~IVehicleView

	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) {}

	bool         Init(CVehicleSeat* pSeat);

protected:

	IVehicle*     m_pVehicle;
	CVehicleSeat* m_pSeat;

	// view settings (changed only inside Init)

	bool  m_isRotating;

	Vec3  m_rotationMin;
	Vec3  m_rotationMax;
	Vec3  m_rotationInit;
	float m_relaxDelayMax;
	float m_relaxTimeMax;
	float m_velLenMin;
	float m_velLenMax;

	bool  m_isSendingActionOnRotation;
	float m_rotationBoundsActionMult;

	// status variables (changed during run-time)

	Ang3  m_rotation;
	Vec3  m_rotatingAction;
	Ang3  m_viewAngleOffset;

	Vec3  m_rotationCurrentSpeed;
	float m_rotationValRateX;  // used for SmoothCD call on the rotation
	float m_rotationValRateZ;
	float m_rotationTimeAcc;
	float m_rotationTimeDec;

	bool  m_isRelaxEnabled;
	float m_relaxDelay;
	float m_relaxTime;

	int   m_yawLeftActionOnBorderAAM;
	int   m_yawRightActionOnBorderAAM;
	int   m_pitchUpActionOnBorderAAM;
	int   m_pitchDownActionOnBorderAAM;

	float m_pitchVal;
	float m_yawVal;

	struct SViewGeneratedAction
	{
		TVehicleActionId actionId;
		int              activationMode;
	};

	bool m_hidePlayer;
	bool m_isEnabled;
	bool m_isDebugView;
	bool m_isAvailableRemotely;
	bool m_playerViewThirdOnExit;

	typedef std::vector<string> TVehiclePartNameVector;
	TVehiclePartNameVector m_hideParts;
};

#endif
