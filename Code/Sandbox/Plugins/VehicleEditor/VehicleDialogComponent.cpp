// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleDialogComponent.h"

#include "Objects/BaseObject.h"
#include "VehicleData.h"
#include "VehiclePrototype.h"
#include "VehiclePart.h"
#include "VehicleHelperObject.h"
#include "VehicleSeat.h"
#include "VehicleWeapon.h"
#include "VehicleComp.h"

IVeedObject* IVeedObject::GetVeedObject(CBaseObject* pObj)
{
	assert(pObj);
	if (!pObj)
		return NULL;

	if (pObj->IsKindOf(RUNTIME_CLASS(CVehiclePart)))
		return (CVehiclePart*)pObj;

	else if (pObj->IsKindOf(RUNTIME_CLASS(CVehicleHelper)))
		return (CVehicleHelper*)pObj;

	else if (pObj->IsKindOf(RUNTIME_CLASS(CVehicleSeat)))
		return (CVehicleSeat*)pObj;

	else if (pObj->IsKindOf(RUNTIME_CLASS(CVehicleWeapon)))
		return (CVehicleWeapon*)pObj;

	else if (pObj->IsKindOf(RUNTIME_CLASS(CVehicleComponent)))
		return (CVehicleComponent*)pObj;

	else if (pObj->IsKindOf(RUNTIME_CLASS(CVehiclePrototype)))
		return (CVehiclePrototype*)pObj;

	return 0;
}

void VeedLog(const char* s, ...)
{
	static ICVar* pVar = gEnv->pConsole->GetCVar("v_debugdraw");

	if (pVar && pVar->GetIVal() == DEBUGDRAW_VEED)
	{
		char buffer[1024];
		va_list arg;
		va_start(arg, s);
		cry_vsprintf(buffer, s, arg);
		va_end(arg);

		CryLog(buffer);
	}
}

void CVeedObject::UpdateObjectOnVarChangeCallback(IVariable* var)
{
	if (m_ignoreObjectUpdateCallback)
	{
		return;
	}

	m_ignoreOnTransformCallback = true;
	UpdateObjectFromVar();
	m_ignoreOnTransformCallback = false;
}

void CVeedObject::EnableUpdateObjectOnVarChange(const string& childVarName)
{
	IVariable* pVar = GetVariable();
	IVariable* pChildVar = GetChildVar(pVar, childVarName);
	if (pChildVar == NULL)
	{
		return;
	}

	pChildVar->AddOnSetCallback(functor(*this, &CVeedObject::UpdateObjectOnVarChangeCallback));
}

void CVeedObject::DisableUpdateObjectOnVarChange(const string& childVarName)
{
	IVariable* pVar = GetVariable();
	IVariable* pChildVar = GetChildVar(pVar, childVarName);
	if (pChildVar == NULL)
	{
		return;
	}

	pChildVar->RemoveOnSetCallback(functor(*this, &CVeedObject::UpdateObjectOnVarChangeCallback));
}

void CVeedObject::InitOnTransformCallback(CBaseObject* pObject)
{
	assert(pObject != NULL);
	if (pObject == NULL)
	{
		return;
	}

	pObject->AddEventListener(functor(*this, &CVeedObject::OnObjectEventCallback));
}

void CVeedObject::OnObjectEventCallback(CBaseObject* pObject, int eventId)
{
	if (eventId == OBJECT_ON_TRANSFORM)
	{
		if (m_ignoreOnTransformCallback)
		{
			return;
		}
		m_ignoreObjectUpdateCallback = true;
		m_ignoreOnTransformCallback = true;
		OnTransform();
		m_ignoreObjectUpdateCallback = false;
		m_ignoreOnTransformCallback = false;
	}
}

