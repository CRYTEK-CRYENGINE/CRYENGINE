// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

//////////////////////////////////////////////////////////////////////////
class CMultiplexer_Node : public CFlowBaseNode<eNCT_Singleton>
{
	enum TriggerMode
	{
		eTM_Always = 0,
		eTM_PortsOnly,
		eTM_IndexOnly,
	};

public:
	CMultiplexer_Node(SActivationInfo* pActInfo) {};
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<int>("Index",    _HELP("Index from 0 to 7")),
			InputPortConfig<int>("Mode",     eTM_Always,                 _HELP("IndexOnly: Only [Index] triggers output.\nPortsOnly: Only Port[Index] triggers output.\nAlways: Both [Index] and Port[Index] trigger output."),0, _UICONFIG("enum_int:Always=0,PortsOnly=1,IndexOnly=2")),
			InputPortConfig_AnyType("Port0", _HELP("Input Port 0")),
			InputPortConfig_AnyType("Port1", _HELP("Input Port 1")),
			InputPortConfig_AnyType("Port2", _HELP("Input Port 2")),
			InputPortConfig_AnyType("Port3", _HELP("Input Port 3")),
			InputPortConfig_AnyType("Port4", _HELP("Input Port 4")),
			InputPortConfig_AnyType("Port5", _HELP("Input Port 5")),
			InputPortConfig_AnyType("Port6", _HELP("Input Port 6")),
			InputPortConfig_AnyType("Port7", _HELP("Input Port 7")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("out", _HELP("multiplexer output"), _HELP("Output")),
			{ 0 }
		};
		config.sDescription = _HELP("Selects one of the inputs using the Index and sends it to the output. 3 Different modes!\nIndexOnly: Only [Index] triggers output.\nPortsOnly: Only Port[Index] triggers output.\nAlways: Both [Index] and Port[Index] trigger output.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED); // POLICY CHANGE: Should we output on Initialize?
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			{
				const int mode = GetPortInt(pActInfo, 1);
				if (event == eFE_Initialize && mode == eTM_Always)
					return;

				// initialize & always -> no trigger, why?
				// initialize & ports only -> same as activate
				// initialize & index only -> same as activate

				// activate & always: trigger if Port[Index] or Index is activated
				// activate & port only: trigger only if Port[Index] is activated
				// activate & index only: trigger only if Index is activated

				const int port = (GetPortInt(pActInfo, 0) & 0x07) + 2;
				const bool bPortActive = IsPortActive(pActInfo, port);
				const bool bIndexActive = IsPortActive(pActInfo, 0);

				if (mode == eTM_IndexOnly && !bIndexActive)
					return;
				else if (mode == eTM_PortsOnly && !bPortActive)
					return;
				ActivateOutput(pActInfo, 0, GetPortAny(pActInfo, port));
			}
		}
		;
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CDeMultiplexer_Node : public CFlowBaseNode<eNCT_Singleton>
{
	enum TriggerMode
	{
		eTM_Always = 0,
		eTM_InputOnly,
		eTM_IndexOnly,
	};
public:
	CDeMultiplexer_Node(SActivationInfo* pActInfo) {};
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<int>("Index",    _HELP("Select Output port index from 0 to 7")),
			InputPortConfig<int>("Mode",     eTM_Always,                                    _HELP("InputOnly: Only [Input] triggers output.\nIndexOnly: Only [Index] triggers output.\nBoth: Both [Input] and [Index] trigger output."),0, _UICONFIG("enum_int:Always=0,InputOnly=1,IndexOnly=2")),
			InputPortConfig_AnyType("Input", _HELP("Input port")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Port0", _HELP("Output Port 0")),
			OutputPortConfig_AnyType("Port1", _HELP("Output Port 1")),
			OutputPortConfig_AnyType("Port2", _HELP("Output Port 2")),
			OutputPortConfig_AnyType("Port3", _HELP("Output Port 3")),
			OutputPortConfig_AnyType("Port4", _HELP("Output Port 4")),
			OutputPortConfig_AnyType("Port5", _HELP("Output Port 5")),
			OutputPortConfig_AnyType("Port6", _HELP("Output Port 6")),
			OutputPortConfig_AnyType("Port7", _HELP("Output Port 7")),
			{ 0 }
		};
		config.sDescription = _HELP("Pushes the input to the selected (via Index) output port. 3 different modes:\nInputOnly: Only [Input] triggers output.\nIndexOnly: Only [Index] triggers output.\nBoth: Both [Input] and [Index] trigger output.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED); // POLICY CHANGE: Should we output on Initialize?
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			{
				const int mode = GetPortInt(pActInfo, 1);
				if (event == eFE_Initialize && mode == eTM_Always)
					return;
				const bool bIndexActive = IsPortActive(pActInfo, 0);
				const bool bInputActive = IsPortActive(pActInfo, 2);
				if (mode == eTM_IndexOnly && !bIndexActive)
					return;
				if (mode == eTM_InputOnly && !bInputActive)
					return;
				const int port = (GetPortInt(pActInfo, 0) & 0x07);
				ActivateOutput(pActInfo, port, GetPortAny(pActInfo, 2));
			}
		}
		;
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("Logic:Multiplexer", CMultiplexer_Node);
REGISTER_FLOW_NODE("Logic:DeMultiplexer", CDeMultiplexer_Node);
