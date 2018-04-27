// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowNode_Platform : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Platform(SActivationInfo* pActInfo) {}

	enum EInPort
	{
		eInPort_Get = 0
	};
	enum EOutPort
	{
		eOutPort_Pc = 0,
		eOutPort_PS4,
		eOutPort_XboxOne
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig_AnyType("Check", _HELP("Triggers a check of the current platform")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] =
		{
			OutputPortConfig_AnyType("PC",      _HELP("Outputs the signal from Check input if the game runs on PC")),
			OutputPortConfig_AnyType("PS4",     _HELP("Outputs the signal from Check input if the game runs on PS4")),
			OutputPortConfig_AnyType("XboxOne", _HELP("Outputs the signal from Check input if the game runs on XboxOne")),
			{ 0 }
		};
		config.sDescription = _HELP("Provides branching logic for different platforms");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate)
		{
			if (!IsPortActive(pActInfo, eInPort_Get))
				return;
#if CRY_PLATFORM_ORBIS
			ActivateOutput(pActInfo, eOutPort_PS4, GetPortAny(pActInfo, eInPort_Get));
#elif CRY_PLATFORM_DURANGO
			ActivateOutput(pActInfo, eOutPort_XboxOne, GetPortAny(pActInfo, eInPort_Get));
#else
			ActivateOutput(pActInfo, eOutPort_Pc, GetPortAny(pActInfo, eInPort_Get));
#endif
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("Game:CheckPlatform", CFlowNode_Platform);
