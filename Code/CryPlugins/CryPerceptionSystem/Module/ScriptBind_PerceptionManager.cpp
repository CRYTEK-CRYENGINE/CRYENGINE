// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.


#include "StdAfx.h"
#include "ScriptBind_PerceptionManager.h"
#include "PerceptionManager.h"

//-----------------------------------------------------------------------------------------------------------
CScriptBind_PerceptionManager::CScriptBind_PerceptionManager(ISystem* pSystem) : m_pScriptSystem(nullptr)
{
	m_pSystem = pSystem;
	CScriptableBase::Init(pSystem->GetIScriptSystem(), pSystem);
	SetGlobalName("Perception");

#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_PerceptionManager::

	SCRIPT_REG_TEMPLFUNC(SetPriorityObjectType, "type");

	SCRIPT_REG_FUNC(SoundStimulus);

#undef SCRIPT_REG_CLASSNAME
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_PerceptionManager::SetPriorityObjectType(IFunctionHandler* pH, int type)
{
	if (type < 0)
	{
		return pH->EndFunction();
	}
	CPerceptionManager::GetInstance()->SetPriorityObjectType(uint16(type));
	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_PerceptionManager::SoundStimulus(IFunctionHandler* pH)
{
	int type = 0;
	float radius = 0;
	Vec3 pos(0, 0, 0);
	ScriptHandle hdl(0);
	pH->GetParams(pos, radius, type);
	// can be called from scripts without owner
	if (pH->GetParamCount() > 3)
		pH->GetParam(4, hdl);

	SAIStimulus stim(AISTIM_SOUND, type, (EntityId)hdl.n, 0, pos, ZERO, radius);
	CPerceptionManager::GetInstance()->RegisterStimulus(stim);

	return pH->EndFunction();
}

