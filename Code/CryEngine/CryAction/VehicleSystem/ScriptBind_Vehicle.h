// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Script Binding for the Vehicle System

   -------------------------------------------------------------------------
   History:
   - 05:10:2004   12:05 : Created by Mathieu Pinard

*************************************************************************/
#ifndef __SCRIPTBIND_VEHICLE_H__
#define __SCRIPTBIND_VEHICLE_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>

struct IVehicleSystem;
struct IGameFramework;
class CVehicle;

class CScriptBind_Vehicle :
	public CScriptableBase
{
public:
	CScriptBind_Vehicle(ISystem* pSystem, IGameFramework* pGameFW);
	virtual ~CScriptBind_Vehicle();

	void         Release() { delete this; };

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	void AttachTo(IVehicle* pVehicle);

	//! <code>Vehicle.GetVehicle()</code>
	//! <description>Gets the vehicle identifier.</description>
	CVehicle* GetVehicle(IFunctionHandler* pH);

	//! <code>Vehicle.Reset()</code>
	//! <description>Resets the vehicle.</description>
	int Reset(IFunctionHandler* pH);

	//! <code>Vehicle.IsInsideRadius( pos, radius )</code>
	//! <description>Checks if the vehicle is inside the specified radius.</description>
	int IsInsideRadius(IFunctionHandler* pH, Vec3 pos, float radius);

	//! <code>Vehicle.MultiplyWithWorldTM( pos )</code>
	//!		<param name="pos">Position vector.</param>
	//! <description>Multiplies with the world transformation matrix.</description>
	int MultiplyWithWorldTM(IFunctionHandler* pH, Vec3 pos);

	//! <code>Vehicle.ResetSlotGeometry( slot, filename, geometry )</code>
	int ResetSlotGeometry(IFunctionHandler* pH, int slot, const char* filename, const char* geometry);

	//! <code>Vehicle.AddSeat( paramsTable )</code>
	//!		<param name="paramsTable">Seat parameters.</param>
	//! <description>Adds a seat to the vehicle.</description>
	int AddSeat(IFunctionHandler* pH, SmartScriptTable paramsTable);

	//! <code>Vehicle.HasHelper(name)</code>
	//!		<param name="name">Helper name.</param>
	//! <description>Checks if the vehicle has the specified helper.</description>
	int HasHelper(IFunctionHandler* pH, const char* name);

	//! <code>Vehicle.GetHelperPos(name, isInVehicleSpace)</code>
	//!		<param name="name">Helper name.</param>
	//!		<param name="isInVehicleSpace">.</param>
	//! <description>Gets the helper position.</description>
	int GetHelperPos(IFunctionHandler* pH, const char* name, bool isInVehicleSpace);

	//! <code>Vehicle.GetHelperDir( name, isInVehicleSpace )</code>
	//!		<param name="name">Helper name.</param>
	//!		<param name="isInVehicleSpace">.</param>
	//! <description>Gets the helper direction.</description>
	int GetHelperDir(IFunctionHandler* pH, const char* name, bool isInVehicleSpace);

	//! <code>Vehicle.GetHelperWorldPos( name )</code>
	//!		<param name="name">Helper name.</param>
	//! <description>Gets the helper position in the world coordinates.</description>
	int GetHelperWorldPos(IFunctionHandler* pH, const char* name);

	//! <code>Vehicle.EnableMovement( enable )</code>
	//!		<param name="enable">True to enable movement, false to disable.</param>
	//! <description>Enables/disables the movement of the vehicle.</description>
	int EnableMovement(IFunctionHandler* pH, bool enable);

	//! <code>Vehicle.DisableEngine( disable )</code>
	//!		<param name="disable">True to disable the engine, false to enable.</param>
	//! <description>Disables/enables the engine of the vehicle.</description>
	int DisableEngine(IFunctionHandler* pH, bool disable);

	//! <code>Vehicle.OnHit( targetId, shooterId, damage, position, radius, pHitClass, explosion )</code>
	//!		<param name="targetId">Target identifier.</param>
	//!		<param name="shooterId">Shooter identifier.</param>
	//!		<param name="damage">Damage amount.</param>
	//!		<param name="radius">Radius of the hit.</param>
	//!		<param name="hitTypeId">Hit type.</param>
	//!		<param name="explosion">True if the hit cause an explosion, false otherwise.</param>
	//! <description>Event that occurs after the vehicle is hit.</description>
	int OnHit(IFunctionHandler* pH, ScriptHandle targetId, ScriptHandle shooterId, float damage, Vec3 position, float radius, int hitTypeId, bool explosion);

	//! <code>Vehicle.ProcessPassengerDamage( passengerId, actorHealth, damage, pDamageClass, explosion )</code>
	//!		<param name="passengerId">Passenger identifier.</param>
	//!		<param name="actorHealth">Actor health amount.</param>
	//!		<param name="damage">Damage amount.</param>
	//!		<param name="hitTypeId">Damage type.</param>
	//!		<param name="explosion">True if there is an explosion, false otherwise.</param>
	//! <description>Processes passenger damages.</description>
	int ProcessPassengerDamage(IFunctionHandler* pH, ScriptHandle passengerId, float actorHealth, float damage, int hitTypeId, bool explosion);

	//! <code>Vehicle.Destroy()</code>
	//! <description>Destroys the vehicle.</description>
	int Destroy(IFunctionHandler* pH);

	//! <code>Vehicle.IsDestroyed()</code>
	//! <description>Checks if the vehicle is destroyed.</description>
	int IsDestroyed(IFunctionHandler* pH);

	//! <code>Vehicle.IsUsable( userHandle )</code>
	//!		<param name="userHandle">User identifier.</param>
	//! <description>Checks if the vehicle is usable by the user.</description>
	int IsUsable(IFunctionHandler* pH, ScriptHandle userHandle);

	//! <code>Vehicle.OnUsed( userHandle, index )</code>
	//!		<param name="userHandle">User identifier.</param>
	//!		<param name="index">Seat identifier.</param>
	//! <description>Events that occurs when the user uses the vehicle.</description>
	int OnUsed(IFunctionHandler* pH, ScriptHandle userHandle, int index);

	//! <code>Vehicle.EnterVehicle( actorHandle, seatId, isAnimationEnabled )</code>
	//!		<param name="actorHandle">Actor identifier.</param>
	//!		<param name="seatId">Seat identifier.</param>
	//!		<param name="isAnimationEnabled - True to enable the animation, false otherwise.</param>
	//! <description>Makes the actor entering the vehicle.</description>
	int EnterVehicle(IFunctionHandler* pH, ScriptHandle actorHandle, int seatId, bool isAnimationEnabled);

	//! <code>Vehicle.ChangeSeat(actorHandle, seatId, isAnimationEnabled)</code>
	//!		<param name="actorHandle">Actor identifier.</param>
	//!		<param name="seatId">Seat identifier.</param>
	//!		<param name="isAnimationEnabled - True to enable the animation, false otherwise.</param>
	//! <description>Makes the actor changing the seat inside the vehicle.</description>
	int ChangeSeat(IFunctionHandler* pH, ScriptHandle actorHandle, int seatId, bool isAnimationEnabled);

	//! <code>Vehicle.ExitVehicle( actorHandle )</code>
	//!		<param name="actorHandle">Actor identifier.</param>
	//! <description>Makes the actor going out from the vehicle.
	int ExitVehicle(IFunctionHandler* pH, ScriptHandle actorHandle);

	//! <code>Vehicle.GetComponentDamageRatio( componentName )</code>
	//! <description>Gets the damage ratio of the specified component.</description>
	int GetComponentDamageRatio(IFunctionHandler* pH, const char* pComponentName);

	//! <code>Vehicle.GetSeatForPassenger(id)</code>
	//! <returns>Vehicle seat id for the specified passenger.</returns>
	//!		<param name="id">passenger id.</param>
	int GetSeatForPassenger(IFunctionHandler* pH, ScriptHandle passengerId);

	//! <code>Vehicle.OnSpawnComplete()</code>
	//! <description>Callback into game code for OnSpawnComplete.</description>
	int OnSpawnComplete(IFunctionHandler* pH);

private:

	void RegisterMethods();

	IGameFramework* m_pGameFW;
	IVehicleSystem* m_pVehicleSystem;
};

#endif //__SCRIPTBIND_VEHICLE_H__
