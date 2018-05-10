// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowNode_LogicCondition : public CFlowBaseNode<eNCT_Instanced>
{
	enum
	{
		INP_Condition = 0,
		INP_In,
		INP_CondFalse,
		INP_CondTrue
	};

	enum
	{
		OUT_OnFalse = 0,
		OUT_OnTrue
	};

protected:
	bool m_condition;

public:
	CFlowNode_LogicCondition(SActivationInfo*) : m_condition(false) {}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_LogicCondition(pActInfo);
	}

	void Serialize(SActivationInfo*, TSerialize ser)
	{
		ser.Value("condition", m_condition);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("Condition",   _HELP("If condition is FALSE input [In] will be routed to [OnFalse], otherwise to [OnTrue]")),
			InputPortConfig_AnyType("In",        _HELP("Value to route to [OnFalse] or [OnTrue], based on [Condition]")),
			InputPortConfig_AnyType("CondFalse", _HELP("Sets [Condition] to false.")),
			InputPortConfig_AnyType("CondTrue",  _HELP("Sets [Condition] to true.")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("OnFalse", _HELP("Set to input [In] if [Condition] is FALSE)")),
			OutputPortConfig_AnyType("OnTrue",  _HELP("Set to input [In] if [Condition] is TRUE")),
			{ 0 }
		};
		config.sDescription = _HELP("Node to route data flow based on a boolean condition.\nSetting input [Value] will route it either to [OnFalse] or [OnTrue].");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);

	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_condition = GetPortBool(pActInfo, INP_Condition);
			break;

		case eFE_Activate:
			if (IsPortActive(pActInfo, INP_Condition))
				m_condition = GetPortBool(pActInfo, INP_Condition);
			if (IsPortActive(pActInfo, INP_CondFalse))
				m_condition = false;
			if (IsPortActive(pActInfo, INP_CondTrue))
				m_condition = true;

			// only port [In] triggers an output
			if (IsPortActive(pActInfo, INP_In))
			{
				if (m_condition == false)
					ActivateOutput(pActInfo, OUT_OnFalse, GetPortAny(pActInfo, INP_In));
				else
					ActivateOutput(pActInfo, OUT_OnTrue, GetPortAny(pActInfo, INP_In));
			}
			break;
		}
	}
};

class CFlowNode_LogicConditionInverse : public CFlowBaseNode<eNCT_Instanced>
{
	enum EInputs
	{
		IN_CONDITION = 0,
		IN_CONDFALSE,
		IN_CONDTRUE,
		IN_TRUE,
		IN_FALSE,
	};

protected:
	bool m_condition;

public:
	CFlowNode_LogicConditionInverse(SActivationInfo*) : m_condition(false) {}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_LogicConditionInverse(pActInfo);
	}

	void Serialize(SActivationInfo*, TSerialize ser)
	{
		ser.Value("condition", m_condition);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("Condition",   _HELP("If condition is FALSE input [False] will be routed to output, otherwise input [True] will be")),
			InputPortConfig_AnyType("CondFalse", _HELP("Sets [Condition] to false.")),
			InputPortConfig_AnyType("CondTrue",  _HELP("Sets [Condition] to true.")),
			InputPortConfig_AnyType("True",      _HELP("value to redirect to output when [Condition] is TRUE)")),
			InputPortConfig_AnyType("False",     _HELP("value to redirect to output when [Condition] is FALSE)")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Out", _HELP("Value to output from [False] or [True], based on [Condition]")),
			{ 0 }
		};
		config.sDescription = _HELP("Node to route data flow based on a boolean condition.\nDepending on [Condition], either input [True] or [False] will be redirected to [out].");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);

	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_condition = GetPortBool(pActInfo, IN_CONDITION);
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, IN_CONDITION))
					m_condition = GetPortBool(pActInfo, IN_CONDITION);
				if (IsPortActive(pActInfo, IN_CONDFALSE))
					m_condition = false;
				if (IsPortActive(pActInfo, IN_CONDTRUE))
					m_condition = true;

				if (m_condition)
				{
					if (IsPortActive(pActInfo, IN_TRUE))
						ActivateOutput(pActInfo, 0, GetPortAny(pActInfo, IN_TRUE));
				}
				else
				{
					if (IsPortActive(pActInfo, IN_FALSE))
						ActivateOutput(pActInfo, 0, GetPortAny(pActInfo, IN_FALSE));
				}
				break;
			}
		}
	}
};

REGISTER_FLOW_NODE("Logic:Condition", CFlowNode_LogicCondition)
REGISTER_FLOW_NODE("Logic:ConditionInverse", CFlowNode_LogicConditionInverse)
