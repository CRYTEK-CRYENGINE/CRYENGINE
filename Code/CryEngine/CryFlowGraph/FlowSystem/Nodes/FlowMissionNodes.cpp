// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowNode_EndLevel : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_EndLevel(SActivationInfo* pActInfo)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Trigger",      _HELP("Finish the current mission (go to next level)")),
			InputPortConfig<string>("NextLevel", _HELP("Which level is the next level?")),
			{ 0 }
		};
		config.sDescription = _HELP("Advance to a new level");
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (!gEnv->pGameFramework->IsInTimeDemo() && event == eFE_Activate && IsPortActive(pActInfo, 0))
		{
			gEnv->pGameFramework->ScheduleEndLevel(GetPortString(pActInfo, 1));
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("Mission:LoadNextLevel", CFlowNode_EndLevel);
