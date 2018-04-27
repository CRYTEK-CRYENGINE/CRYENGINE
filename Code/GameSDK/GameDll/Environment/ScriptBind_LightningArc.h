// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _SCRIPTBIND_LIGHTNING_ARC_H_
#define _SCRIPTBIND_LIGHTNING_ARC_H_

#pragma once

#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>


class CLightningArc;



class CScriptBind_LightningArc: public CScriptableBase
{
public:
	CScriptBind_LightningArc(ISystem* pSystem);

	void AttachTo(CLightningArc* pLightingArc);

	int TriggerSpark(IFunctionHandler* pFunction);
	int Enable(IFunctionHandler* pFunction, bool enable);
	int ReadLuaParameters(IFunctionHandler* pFunction);
};



#endif
