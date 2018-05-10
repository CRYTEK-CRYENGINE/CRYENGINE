// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Script Binding for the Vehicle Seat

   -------------------------------------------------------------------------
   History:
   - 28:04:2004   17:02 : Created by Mathieu Pinard

*************************************************************************/
#ifndef __SCRIPTBIND_VEHICLESEAT_H__
#define __SCRIPTBIND_VEHICLESEAT_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>

struct IVehicleSystem;
struct IGameFramework;
class CVehicleSeat;

class CScriptBind_VehicleSeat :
	public CScriptableBase
{
public:

	CScriptBind_VehicleSeat(ISystem* pSystem, IGameFramework* pGameFW);
	virtual ~CScriptBind_VehicleSeat();

	void         AttachTo(IVehicle* pVehicle, TVehicleSeatId seatId);
	void         Release() { delete this; };

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	//! <code>VehicleSeat.GetVehicleSeat()</code>
	//! <description>Gets the vehicle seat identifier.</description>
	CVehicleSeat* GetVehicleSeat(IFunctionHandler* pH);

	//! <code>VehicleSeat.Reset()</code>
	//! <description>Resets the vehicle seat.</description>
	int Reset(IFunctionHandler* pH);

	//! <code>VehicleSeat.IsFree(actor)</code>
	//!		<param name="actorHandle">Passenger identifier.</param>
	//! <description>Checks if the seat is free.</description>
	int IsFree(IFunctionHandler* pH, ScriptHandle actorHandle);

	//! <code>VehicleSeat.IsDriver()</code>
	//! <description>Checks if the seat is the driver seat.</description>
	int IsDriver(IFunctionHandler* pH);

	//! <code>VehicleSeat.IsGunner()</code>
	//! <description>Checks if the seat is the gunner seat.</description>
	int IsGunner(IFunctionHandler* pH);

	//! <code>VehicleSeat.GetWeaponId(weaponIndex)</code>
	//!		<param name="weaponIndex">Weapon identifier.</param>
	//! <description>Gets the weapon identifier.</description>
	int GetWeaponId(IFunctionHandler* pH, int weaponIndex);

	//! <code>VehicleSeat.GetWeaponCount()</code>
	//! <description>Gets the number of weapons available on this seat.</description>
	int GetWeaponCount(IFunctionHandler* pH);

	//! <code>VehicleSeat.SetAIWeapon(weaponHandle)</code>
	//!		<param name="weaponHandle">Weapon identifier.</param>
	//! <description>Sets the weapon artificial intelligence.</description>
	int SetAIWeapon(IFunctionHandler* pH, ScriptHandle weaponHandle);

	//! <code>VehicleSeat.GetPassengerId()</code>
	//! <description>Gets the passenger identifier.</description>
	int GetPassengerId(IFunctionHandler* pH);

private:

	void RegisterGlobals();
	void RegisterMethods();

	IVehicleSystem* m_pVehicleSystem;
};

#endif //__SCRIPTBIND_VEHICLESEAT_H__
