// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

//////////////////////////////////////////////////////////////////////////
class CIndexer_Node : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CIndexer_Node(SActivationInfo* pActInfo) {};
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("Port1", _HELP("Input Port 0")),
			InputPortConfig_AnyType("Port2", _HELP("Input Port 1")),
			InputPortConfig_AnyType("Port3", _HELP("Input Port 2")),
			InputPortConfig_AnyType("Port4", _HELP("Input Port 3")),
			InputPortConfig_AnyType("Port5", _HELP("Input Port 4")),
			InputPortConfig_AnyType("Port6", _HELP("Input Port 5")),
			InputPortConfig_AnyType("Port7", _HELP("Input Port 6")),
			InputPortConfig_AnyType("Port8", _HELP("Input Port 7")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("out", _HELP("PortIndex which was triggered [1-8]"), _HELP("PortIndex")),
			{ 0 }
		};
		config.sDescription = _HELP("Whenever an input port is activated, returns the index of that port [1-8]. WARNING: Does not account for multiple activations on different ports!");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				int o = 0;
				for (int i = 0; i < 8; i++)
				{
					if (IsPortActive(pActInfo, i))
					{
						o = i + 1;
						break;
					}
				}
				ActivateOutput(pActInfo, 0, o);
				break;
			}
		case eFE_Initialize:
			ActivateOutput(pActInfo, 0, 0);
			break;
		}
		;
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("Logic:Indexer", CIndexer_Node);
