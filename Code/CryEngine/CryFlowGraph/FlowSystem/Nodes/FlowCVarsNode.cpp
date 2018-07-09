// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

#define ALLOW_CVAR_FLOWNODE // uncomment this to recover the functionality

class CFlowNode_CVar : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_CVar(SActivationInfo* pActInfo) {}

	enum EInPorts
	{
		SET = 0,
		GET,
		NAME,
		VALUE,
	};
	enum EOutPorts
	{
		CURVALUE = 0,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<SFlowSystemVoid>("Set", _HELP("Trigger to Set CVar's value")),
			InputPortConfig<SFlowSystemVoid>("Get", _HELP("Trigger to Get CVar's value")),
			InputPortConfig<string>("CVar",         _HELP("Name of the CVar to set/get [case-INsensitive]")),
			InputPortConfig<string>("Value",        _HELP("Value of the CVar to set")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<string>("CurValue", _HELP("Current Value of the CVar [Triggered on Set/Get]")),
			{ 0 }
		};
#ifdef ALLOW_CVAR_FLOWNODE
		config.sDescription = _HELP("Sets/Gets the value of a console variable (CVar).");
#else
		config.sDescription = _HELP("---- this node is currently disabled in code ------");
#endif
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_DEBUG);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
#if defined(ALLOW_CVAR_FLOWNODE)
		if (event == eFE_Activate)
		{
			const bool isSet = (IsPortActive(pActInfo, SET));
			const bool isGet = (IsPortActive(pActInfo, GET));
			if (!isGet && !isSet)
				return;

			const string& cvar = GetPortString(pActInfo, NAME);
			ICVar* pICVar = gEnv->pConsole->GetCVar(cvar.c_str());
			if (pICVar != 0)
			{
				if (isSet)
				{
					const string& val = GetPortString(pActInfo, VALUE);
					pICVar->Set(val.c_str());
				}
				const string curVal = pICVar->GetString();
				ActivateOutput(pActInfo, CURVALUE, curVal);
			}
			else
			{
				CryLogAlways("[flow] Cannot find console variable '%s'", cvar.c_str());
			}
		}
#endif
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("Debug:ConsoleVariable", CFlowNode_CVar);
