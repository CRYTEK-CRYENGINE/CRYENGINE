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
#include "StdAfx.h"
#include <IActionMapManager.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryGame/IGameFramework.h>
#include "IActorSystem.h"
#include <vector>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryMath/Cry_GeoOverlap.h>
#include <IGameRulesSystem.h>

#include "VehicleSystem.h"
#include "Vehicle.h"
#include "VehicleComponent.h"
#include "VehicleSeat.h"
#include "ScriptBind_Vehicle.h"
#include "VehicleDamages.h"
#include "IGameObject.h"

//------------------------------------------------------------------------
// macro for retrieving vehicle and entity
#undef GET_ENTITY
#define GET_ENTITY IVehicle * pVehicle = GetVehicle(pH);      \
  IEntity* pEntity = pVehicle ? pVehicle->GetEntity() : NULL; \
  if (!pEntity) return pH->EndFunction();

//------------------------------------------------------------------------
CScriptBind_Vehicle::CScriptBind_Vehicle(ISystem* pSystem, IGameFramework* pGameFW)
{
	m_pVehicleSystem = pGameFW->GetIVehicleSystem();

	Init(gEnv->pScriptSystem, gEnv->pSystem, 1);

	//CScriptableBase::Init(m_pSS, pSystem);
	//SetGlobalName("vehicle");

	RegisterMethods();
}

//------------------------------------------------------------------------
void CScriptBind_Vehicle::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_Vehicle::

	SCRIPT_REG_TEMPLFUNC(IsInsideRadius, "pos, radius");

	SCRIPT_REG_TEMPLFUNC(MultiplyWithWorldTM, "pos");

	SCRIPT_REG_TEMPLFUNC(AddSeat, "params");

	SCRIPT_REG_TEMPLFUNC(HasHelper, "name");
	SCRIPT_REG_TEMPLFUNC(GetHelperPos, "name, isVehicleSpace");
	SCRIPT_REG_TEMPLFUNC(GetHelperDir, "name, isVehicleSpace");

	SCRIPT_REG_TEMPLFUNC(GetHelperWorldPos, "name");

	SCRIPT_REG_TEMPLFUNC(EnableMovement, "enable");
	SCRIPT_REG_TEMPLFUNC(DisableEngine, "disable");

	SCRIPT_REG_TEMPLFUNC(OnHit, "targetId, shooterId, damage, position, radius, hitTypeId, explosion");
	SCRIPT_REG_TEMPLFUNC(ProcessPassengerDamage, "passengerHandle, actorHealth, damage, pDamageClass, explosion");
	SCRIPT_REG_TEMPLFUNC(Destroy, "");
	SCRIPT_REG_TEMPLFUNC(IsDestroyed, "");

	SCRIPT_REG_TEMPLFUNC(IsUsable, "userId");
	SCRIPT_REG_TEMPLFUNC(OnUsed, "userId, index");

	SCRIPT_REG_TEMPLFUNC(EnterVehicle, "actorId, seatIndex, isAnimationEnabled");
	SCRIPT_REG_TEMPLFUNC(ChangeSeat, "actorId, seatIndex, isAnimationEnabled");
	SCRIPT_REG_TEMPLFUNC(ExitVehicle, "actorId");

	SCRIPT_REG_TEMPLFUNC(GetComponentDamageRatio, "componentName");
	SCRIPT_REG_TEMPLFUNC(OnSpawnComplete, "");

	SCRIPT_REG_TEMPLFUNC(GetSeatForPassenger, "passengerId");
}

//------------------------------------------------------------------------
void CScriptBind_Vehicle::AttachTo(IVehicle* pVehicle)
{
	IScriptTable* pScriptTable = pVehicle->GetEntity()->GetScriptTable();

	if (!pScriptTable)
		return;

	SmartScriptTable thisTable(gEnv->pScriptSystem);
	thisTable->SetValue("vehicleId", ScriptHandle(pVehicle->GetEntityId()));
	thisTable->Delegate(GetMethodsTable());
	pScriptTable->SetValue("vehicle", thisTable);

	SmartScriptTable seatTable(gEnv->pScriptSystem);
	pScriptTable->SetValue("Seats", seatTable);
}

//------------------------------------------------------------------------
CVehicle* CScriptBind_Vehicle::GetVehicle(IFunctionHandler* pH)
{
	ScriptHandle handle;
	SmartScriptTable table;

	if (pH->GetSelf(table))
	{
		if (table->GetValue("vehicleId", handle))
		{
			return (CVehicle*)m_pVehicleSystem->GetVehicle((EntityId)handle.n);
		}
	}

	return 0;
}

//------------------------------------------------------------------------
CScriptBind_Vehicle::~CScriptBind_Vehicle()
{
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::Reset(IFunctionHandler* pH)
{
	IVehicle* pVehicle = GetVehicle(pH);

	if (pVehicle)
		pVehicle->Reset(0);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::IsInsideRadius(IFunctionHandler* pH, Vec3 pos, float radius)
{
	IVehicle* vehicle = GetVehicle(pH);

	if (!vehicle)
		return pH->EndFunction();

	AABB boundingBox;
	IEntity* vehicleEntity = vehicle->GetEntity();
	vehicleEntity->GetWorldBounds(boundingBox);

	Sphere sphere(pos, radius);
	return pH->EndFunction(Overlap::Sphere_AABB(sphere, boundingBox));
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::MultiplyWithWorldTM(IFunctionHandler* pH, Vec3 pos)
{
	GET_ENTITY;

	return pH->EndFunction(pEntity->GetWorldTM() * pos);
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::AddSeat(IFunctionHandler* pH, SmartScriptTable paramsTable)
{
	IVehicle* pVehicle = GetVehicle(pH);
	if (pVehicle)
	{
		pVehicle->AddSeat(paramsTable);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::HasHelper(IFunctionHandler* pH, const char* name)
{
	if (IVehicle* pVehicle = GetVehicle(pH))
	{
		IVehicleHelper* pHelper = pVehicle->GetHelper(name);
		return pH->EndFunction(pHelper != NULL);
	}

	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::GetHelperPos(IFunctionHandler* pH, const char* name, bool isInVehicleSpace)
{
	if (IVehicle* pVehicle = GetVehicle(pH))
	{
		if (IVehicleHelper* pHelper = pVehicle->GetHelper(name))
		{
			if (isInVehicleSpace)
				return pH->EndFunction(pHelper->GetVehicleSpaceTranslation());
			else
				return pH->EndFunction(pHelper->GetLocalSpaceTranslation());
		}
	}

	return pH->EndFunction(Vec3(0.0f, 0.0f, 0.0f));
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::GetHelperDir(IFunctionHandler* pH, const char* name, bool isInVehicleSpace)
{
	if (IVehicle* pVehicle = GetVehicle(pH))
	{
		if (IVehicleHelper* pHelper = pVehicle->GetHelper(name))
		{
			Matrix34 tm;
			if (isInVehicleSpace)
				pHelper->GetVehicleTM(tm);
			else
				tm = pHelper->GetLocalTM();

			return pH->EndFunction(tm.TransformVector(FORWARD_DIRECTION));
		}
	}

	return pH->EndFunction(Vec3(0.0f, 1.0f, 0.0f));
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::GetHelperWorldPos(IFunctionHandler* pH, const char* name)
{
	if (IVehicle* pVehicle = GetVehicle(pH))
	{
		if (IVehicleHelper* pHelper = pVehicle->GetHelper(name))
		{
			return pH->EndFunction(pHelper->GetWorldSpaceTranslation());
		}
	}

	return pH->EndFunction(Vec3(0.0f, 0.0f, 0.0f));
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::EnableMovement(IFunctionHandler* pH, bool enable)
{
	if (CVehicle* pVehicle = GetVehicle(pH))
	{
		if (pVehicle->GetMovement())
			pVehicle->GetMovement()->EnableMovementProcessing(enable);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::DisableEngine(IFunctionHandler* pH, bool disable)
{
	if (CVehicle* pVehicle = GetVehicle(pH))
	{
		if (pVehicle->GetMovement())
			pVehicle->GetMovement()->DisableEngine(disable);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::OnHit(IFunctionHandler* pH, ScriptHandle targetId, ScriptHandle shooterId, float damage, Vec3 position, float radius, int hitTypeId, bool explosion)
{
	if (CVehicle* pVehicle = GetVehicle(pH))
	{
		HitInfo hitInfo;
		hitInfo.shooterId = (EntityId)shooterId.n;
		hitInfo.targetId = (EntityId)targetId.n;
		hitInfo.damage = damage;
		hitInfo.pos = position;
		hitInfo.radius = radius;
		hitInfo.type = hitTypeId;
		hitInfo.explosion = explosion;

		pVehicle->OnHit(hitInfo);
	}
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::ProcessPassengerDamage(IFunctionHandler* pH, ScriptHandle passengerHandle, float actorHealth, float damage, int hitTypeId, bool explosion)
{
	CVehicle* pVehicle = GetVehicle(pH);
	if (!pVehicle || !passengerHandle.n)
		return pH->EndFunction(0.0f);

	if (CVehicleSeat* pSeat = (CVehicleSeat*)pVehicle->GetSeatForPassenger((EntityId)passengerHandle.n))
	{
		float newDamage = pSeat->ProcessPassengerDamage(actorHealth, damage, hitTypeId, explosion);
		return pH->EndFunction(newDamage);
	}

	return pH->EndFunction(0.0f);
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::IsUsable(IFunctionHandler* pH, ScriptHandle userHandle)
{
	CVehicle* pVehicle = GetVehicle(pH);
	if (!pVehicle || !userHandle.n)
		return pH->EndFunction(0);

	int ret = pVehicle->IsUsable((EntityId)userHandle.n);
	return pH->EndFunction(ret);
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::OnUsed(IFunctionHandler* pH, ScriptHandle userHandle, int index)
{
	CVehicle* pVehicle = GetVehicle(pH);
	if (!pVehicle || !userHandle.n)
		return pH->EndFunction(0);

	int ret = pVehicle->OnUsed((EntityId)userHandle.n, index);
	return pH->EndFunction(ret);
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::EnterVehicle(IFunctionHandler* pH, ScriptHandle actorHandle, int seatId, bool isAnimationEnabled)
{
	CVehicle* pVehicle = GetVehicle(pH);
	if (!pVehicle || !actorHandle.n)
		return pH->EndFunction(0);

	IVehicleSeat* pSeat = pVehicle->GetSeatById(seatId);

	bool ret = false;

	if (pSeat)
		ret = pSeat->Enter((EntityId)actorHandle.n, isAnimationEnabled);

	return pH->EndFunction(ret);
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::ChangeSeat(IFunctionHandler* pH, ScriptHandle actorHandle, int seatId, bool isAnimationEnabled)
{
	CVehicle* pVehicle = GetVehicle(pH);
	if (!pVehicle || !actorHandle.n)
		return pH->EndFunction(0);

	bool ret = true;
	pVehicle->ChangeSeat((EntityId)actorHandle.n, seatId, 0);

	return pH->EndFunction(ret);
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::ExitVehicle(IFunctionHandler* pH, ScriptHandle actorHandle)
{
	CVehicle* pVehicle = GetVehicle(pH);
	if (!pVehicle || !actorHandle.n)
		return pH->EndFunction(0);

	if (IVehicleSeat* pSeat = pVehicle->GetSeatForPassenger((EntityId)actorHandle.n))
	{
		bool ret = pSeat->Exit(true);
		return pH->EndFunction(ret);
	}

	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::GetComponentDamageRatio(IFunctionHandler* pH, const char* pComponentName)
{
	CVehicle* pVehicle = GetVehicle(pH);

	if (!pVehicle)
		return pH->EndFunction();

	if (IVehicleComponent* pComponent = pVehicle->GetComponent(pComponentName))
		return pH->EndFunction(pComponent->GetDamageRatio());

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::OnSpawnComplete(IFunctionHandler* pH)
{
	CVehicle* pVehicle = GetVehicle(pH);

	if (!pVehicle)
		return pH->EndFunction();

	pVehicle->OnSpawnComplete();

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::Destroy(IFunctionHandler* pH)
{
	CVehicle* pVehicle = GetVehicle(pH);

	if (!pVehicle)
		return pH->EndFunction();

	pVehicle->Destroy();

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::IsDestroyed(IFunctionHandler* pH)
{
	if (CVehicle* pVehicle = GetVehicle(pH))
	{
		return pH->EndFunction(pVehicle->IsDestroyed());
	}

	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_Vehicle::GetSeatForPassenger(IFunctionHandler* pH, ScriptHandle passengerId)
{
	if (CVehicle* pVehicle = GetVehicle(pH))
	{
		if (IVehicleSeat* pSeat = pVehicle->GetSeatForPassenger((EntityId)passengerId.n))
		{
			return pH->EndFunction(pSeat->GetSeatId());
		}
	}

	return pH->EndFunction();
}
