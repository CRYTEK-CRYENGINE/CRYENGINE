// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a class which handle client actions on vehicles.

-------------------------------------------------------------------------
History:
- 17:10:2006: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLECLIENT_H__
#define __VEHICLECLIENT_H__

#include "IVehicleSystem.h"

class CPlayer;

enum EVehicleActionExtIds
{
	eVAI_ActionsExt = eVAI_Others,
	eVAI_RipoffWeapon,
};

enum EVehicleEventExtIds
{
	eVE_EventsExt = eVE_Last,
};

class CVehicleClient
	: public IVehicleClient
{
public:

	virtual bool Init();
	virtual void Reset();
	virtual void Release() { delete this; }

	virtual void OnAction(IVehicle* pVehicle, EntityId actorId, const ActionId& actionId, int activationMode, float value);
	virtual void PreUpdate(IVehicle* pVehicle, EntityId actorId, float frameTime);
	virtual void OnEnterVehicleSeat(IVehicleSeat* pSeat);
	virtual void OnExitVehicleSeat(IVehicleSeat* pSeat);

protected:

	void ProcessXIAction(IVehicle* pVehicle, const EntityId actorId, const float value, const TVehicleActionId leftAction, const TVehicleActionId rightAction, bool& leftFlag, bool& rightFlag);
	void EnableActionMaps(IVehicleSeat* pSeat, bool enable);
	void ResetFlags();
	bool ShouldInvertPitch(IVehicle* pVehicle) const;

	typedef std::map<ActionId, int> TActionNameIdMap;
	TActionNameIdMap m_actionNameIds;

private:
	void InsertActionMapValue(const char * pString, int eVehicleAction);

	struct SPairedAction
	{
		float m_postive;
		float m_negative;

		void Reset()
		{
			m_postive = 0.f;
			m_negative = 0.f;
		}

		float GetAccumlated()
		{
			return m_postive + m_negative;
		}
	};

	Ang3 m_xiRotation;
  SPairedAction m_fLeftRight;
  SPairedAction m_fForwardBackward;
  SPairedAction m_fAccelDecel;
	bool m_bMovementFlagForward;
	bool m_bMovementFlagBack;
	bool m_bMovementFlagRight;
	bool m_bMovementFlagLeft;
	bool m_bMovementFlagUp;
	bool m_bMovementFlagDown;
	bool m_bMovementFlagStrafeLeft;
	bool m_bMovementFlagStrafeRight;

  bool m_tp;

};

#endif
