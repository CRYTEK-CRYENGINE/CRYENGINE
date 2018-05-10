// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Script Binding for the Vehicle System

   -------------------------------------------------------------------------
   History:
   - 26:04:2005   : Created by Mathieu Pinard

*************************************************************************/
#ifndef __SCRIPTBIND_VEHICLESYSTEM_H__
#define __SCRIPTBIND_VEHICLESYSTEM_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>

struct IVehicleSystem;
struct IGameFramework;
class CVehicleSystem;

class CScriptBind_VehicleSystem :
	public CScriptableBase
{
public:
	CScriptBind_VehicleSystem(ISystem* pSystem, CVehicleSystem* vehicleSystem);
	virtual ~CScriptBind_VehicleSystem();

	void Release() { delete this; };

	//! <code>VehicleSystem.GetVehicleImplementations()</code>
	//! <description>Get a table of all implemented vehicles.</description>
	int GetVehicleImplementations(IFunctionHandler* pH);

	//! <code>VehicleSystem.GetOptionalScript(vehicleName)</code>
	//! <description>Get an (optional) script for the named vehicle.</description>
	int GetOptionalScript(IFunctionHandler* pH, char* vehicleName);

	//! <code>VehicleSystem.SetTpvDistance(distance)</code>
	//! <description>Distance of camera in third person view.</description>
	int SetTpvDistance(IFunctionHandler* pH, float distance);

	//! <code>VehicleSystem.SetTpvHeight(height)</code>
	//! <description>Height of camera in third person view.</description>
	int SetTpvHeight(IFunctionHandler* pH, float height);

	//! <code>VehicleSystem.ReloadSystem()</code>
	//! <description>Reloads the vehicle system with default values.</description>
	int          ReloadSystem(IFunctionHandler* pH);

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

private:

	void RegisterGlobals();
	void RegisterMethods();

	CVehicleSystem* m_pVehicleSystem;
};

#endif //__SCRIPTBIND_VEHICLESYSTEM_H__
