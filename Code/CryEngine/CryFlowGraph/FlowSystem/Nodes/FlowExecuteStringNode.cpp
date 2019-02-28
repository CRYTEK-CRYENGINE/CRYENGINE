// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowNode_ExecuteString : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_ExecuteString(SActivationInfo* pActInfo) {}

	enum EInPorts
	{
		SET = 0,
		STRING,
		NEXT_FRAME,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<SFlowSystemVoid>("Set", _HELP("Trigger to Set CVar's value")),
			InputPortConfig<string>("String",       _HELP("String you want to execute")),
			InputPortConfig<bool>("NextFrame",      false,                                _HELP("If true it will execute the command next frame")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			{ 0 }
		};
		config.sDescription = _HELP("Executes a string like when using the console");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_DEBUG);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate)
		{
			const bool isSet = (IsPortActive(pActInfo, SET));
			if (isSet)
			{
				const string& stringToExecute = GetPortString(pActInfo, STRING);
				const bool nextFrame = GetPortBool(pActInfo, NEXT_FRAME);
				gEnv->pConsole->ExecuteString(stringToExecute, true, nextFrame);
				;
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("Debug:ExecuteString", CFlowNode_ExecuteString);
