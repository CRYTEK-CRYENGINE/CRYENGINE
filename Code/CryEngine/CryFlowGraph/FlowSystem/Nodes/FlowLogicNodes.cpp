// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowLogicNodes.h
//  Version:     v1.00
//  Created:     30/6/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CryFlowGraph/IFlowBaseNode.h>

#include <CryMath/Random.h>

class CLogicNode : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CLogicNode(SActivationInfo* pActInfo) : m_bResult(2) {};

	void Serialize(SActivationInfo*, TSerialize ser)
	{
		ser.Value("m_bResult", m_bResult);
	}

	enum EInputs
	{
		INP_A = 0,
		INP_B,
		INP_Always
	};

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) = 0;

	virtual void         GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("A",      _HELP("out = A op B")),
			InputPortConfig<bool>("B",      _HELP("out = A op B")),
			InputPortConfig<bool>("Always", _HELP("if true, the outputs will be activated every time an input is. If false, outputs only will be activated when the value does change")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("out",   _HELP("out = A op B")),
			OutputPortConfig<bool>("true",  _HELP("triggered if out is true")),
			OutputPortConfig<bool>("false", _HELP("triggered if out is false")),
			{ 0 }
		};
		config.sDescription = _HELP("Do logical operation on input ports and outputs result to [out] port. True port is triggered if the result was true, otherwise the false port is triggered.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_bResult = 2;
				break;
			}

		case eFE_Activate:
			{
				bool a = GetPortBool(pActInfo, INP_A);
				bool b = GetPortBool(pActInfo, INP_B);
				int result = Execute(a, b) ? 1 : 0;
				bool activateOutputs = GetPortBool(pActInfo, INP_Always) || m_bResult != result;
				m_bResult = result;
				if (activateOutputs)
				{
					ActivateOutput(pActInfo, 0, result);
					if (result)
						ActivateOutput(pActInfo, 1, true);
					else
						ActivateOutput(pActInfo, 2, true);
				}
			}
		}
		;
	};

private:
	virtual bool Execute(bool a, bool b) = 0;

	int8 m_bResult;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_AND : public CLogicNode
{
public:
	CFlowNode_AND(SActivationInfo* pActInfo) : CLogicNode(pActInfo) {}
	IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_AND(pActInfo); }

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
private:
	bool Execute(bool a, bool b) { return a && b; }
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_OR : public CLogicNode
{
public:
	CFlowNode_OR(SActivationInfo* pActInfo) : CLogicNode(pActInfo) {}
	IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_OR(pActInfo); }

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
private:
	bool Execute(bool a, bool b) { return a || b; }
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_XOR : public CLogicNode
{
public:
	CFlowNode_XOR(SActivationInfo* pActInfo) : CLogicNode(pActInfo) {}
	IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_XOR(pActInfo); }

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
private:
	bool Execute(bool a, bool b) { return a ^ b; }
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_NOT : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_NOT(SActivationInfo* pActInfo) {};
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("in", _HELP("out = not in")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("out", _HELP("out = not in")),
			{ 0 }
		};
		config.sDescription = _HELP("Inverts [in] port, [out] port will output true when [in] is false, and will output false when [in] is true");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			{
				bool a = GetPortBool(pActInfo, 0);
				bool result = !a;
				ActivateOutput(pActInfo, 0, result);
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
class CFlowNode_OnChange : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNode_OnChange(SActivationInfo* pActInfo) { m_bCurrent = false; };
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_OnChange(pActInfo); }
	virtual void         GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("in"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("out"),
			{ 0 }
		};
		config.sDescription = _HELP("Only send [in] value into the [out] when it is different from the previous value");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			{
				bool a = GetPortBool(pActInfo, 0);
				if (a != m_bCurrent)
				{
					ActivateOutput(pActInfo, 0, a);
					m_bCurrent = a;
				}
			}
		}
		;
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
private:
	bool m_bCurrent;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Any : public CFlowBaseNode<eNCT_Singleton>
{
public:
	static const int NUM_INPUTS = 10;

	CFlowNode_Any(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("in1",  _HELP("Input 1")),
			InputPortConfig_AnyType("in2",  _HELP("Input 2")),
			InputPortConfig_AnyType("in3",  _HELP("Input 3")),
			InputPortConfig_AnyType("in4",  _HELP("Input 4")),
			InputPortConfig_AnyType("in5",  _HELP("Input 5")),
			InputPortConfig_AnyType("in6",  _HELP("Input 6")),
			InputPortConfig_AnyType("in7",  _HELP("Input 7")),
			InputPortConfig_AnyType("in8",  _HELP("Input 8")),
			InputPortConfig_AnyType("in9",  _HELP("Input 9")),
			InputPortConfig_AnyType("in10", _HELP("Input 10")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("out", _HELP("Output")),
			{ 0 }
		};
		config.sDescription = _HELP("Port joiner - triggers it's output when any input is triggered");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			for (int i = 0; i < NUM_INPUTS; i++)
			{
				if (IsPortActive(pActInfo, i))
					ActivateOutput(pActInfo, 0, pActInfo->pInputPorts[i]);
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Blocker : public CFlowBaseNode<eNCT_Singleton>
{
public:

	CFlowNode_Blocker(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("Block", false,           _HELP("If true blocks [In] data. Otherwise passes [In] to [Out]")),
			InputPortConfig_AnyType("In",  _HELP("Input")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Out", _HELP("Output")),
			{ 0 }
		};
		config.sDescription = _HELP("Blocker - If enabled blocks [In] signals, otherwise passes them to [Out]");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			if (IsPortActive(pActInfo, 1) && !GetPortBool(pActInfo, 0))
			{
				ActivateOutput(pActInfo, 0, GetPortAny(pActInfo, 1));
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_LogicGate : public CFlowBaseNode<eNCT_Instanced>
{
	enum
	{
		INP_In = 0,
		INP_Closed,
		INP_Open,
		INP_Close
	};

protected:
	bool m_bClosed;

public:
	CFlowNode_LogicGate(SActivationInfo* pActInfo) : m_bClosed(false) {}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_LogicGate(pActInfo);
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		ser.Value("m_bClosed", m_bClosed);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("In",   _HELP("Input")),
			InputPortConfig<bool>("Closed", false,                            _HELP("If true blocks [In] data. Otherwise passes [In] to [Out]")),
			InputPortConfig_Void("Open",    _HELP("Sets [Closed] to false.")),
			InputPortConfig_Void("Close",   _HELP("Sets [Closed] to true.")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Out", _HELP("Output")),
			{ 0 }
		};
		config.sDescription = _HELP("If closed, blocks [In] signals, otherwise passes them to [Out]");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_bClosed = GetPortBool(pActInfo, INP_Closed);
			break;

		case eFE_Activate:
			if (IsPortActive(pActInfo, INP_Closed))
				m_bClosed = GetPortBool(pActInfo, INP_Closed);
			if (IsPortActive(pActInfo, INP_Open))
				m_bClosed = false;
			if (IsPortActive(pActInfo, INP_Close))
				m_bClosed = true;
			if (IsPortActive(pActInfo, INP_In) && !m_bClosed)
				ActivateOutput(pActInfo, INP_In, GetPortAny(pActInfo, INP_In));
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_All : public CFlowBaseNode<eNCT_Instanced>
{
public:
	static const int NUM_INPUTS = 6;

	CFlowNode_All(SActivationInfo* pActInfo) : m_inputCount(0)
	{
		ResetState();
	}

	void ResetState()
	{
		for (unsigned i = 0; i < NUM_INPUTS; i++)
			m_triggered[i] = false;
		m_outputTrig = false;
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		CFlowNode_All* clone = new CFlowNode_All(pActInfo);
		clone->m_inputCount = m_inputCount;
		return clone;
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		char name[32];
		ser.Value("m_inputCount", m_inputCount);
		ser.Value("m_outputTrig", m_outputTrig);
		for (int i = 0; i < NUM_INPUTS; ++i)
		{
			cry_sprintf(name, "m_triggered_%d", i);
			ser.Value(name, m_triggered[i]);
		}
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("in1",   _HELP("Input 1")),
			InputPortConfig_Void("in2",   _HELP("Input 2")),
			InputPortConfig_Void("in3",   _HELP("Input 3")),
			InputPortConfig_Void("in4",   _HELP("Input 4")),
			InputPortConfig_Void("in5",   _HELP("Input 5")),
			InputPortConfig_Void("in6",   _HELP("Input 6")),
			InputPortConfig_Void("Reset", _HELP("Reset")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Out", _HELP("Output")),
			{ 0 }
		};
		config.sDescription = _HELP("All - Triggers output when all connected inputs are triggered.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_ConnectInputPort:
			// Count the number if inputs connected.
			if (pActInfo->connectPort < NUM_INPUTS)
				m_inputCount++;
			break;
		case eFE_DisconnectInputPort:
			// Count the number if inputs connected.
			if (pActInfo->connectPort < NUM_INPUTS)
				m_inputCount--;
			break;
		case eFE_Initialize:
			ResetState();
		// Fall through to check the input.
		case eFE_Activate:
			// Inputs
			int ntrig = 0;
			for (int i = 0; i < NUM_INPUTS; i++)
			{
				if (!m_triggered[i] && IsPortActive(pActInfo, i))
					m_triggered[i] = (event == eFE_Activate);
				if (m_triggered[i])
					ntrig++;
			}
			// Reset
			if (IsPortActive(pActInfo, NUM_INPUTS))
			{
				ResetState();
				ntrig = 0;
			}
			// If all inputs have been triggered, trigger output.
			// make sure we actually have connected inputs!
			if (!m_outputTrig && m_inputCount > 0 && ntrig == m_inputCount)
			{
				ActivateOutput(pActInfo, 0, true);
				m_outputTrig = (event == eFE_Activate);
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	int  m_inputCount;
	bool m_triggered[NUM_INPUTS];
	bool m_outputTrig;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_LogicOnce : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNode_LogicOnce(SActivationInfo* pActInfo) : m_bTriggered(false)
	{
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_LogicOnce(pActInfo);
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		ser.Value("m_bTriggered", m_bTriggered);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("Input1"),
			InputPortConfig_AnyType("Input2"),
			InputPortConfig_AnyType("Input3"),
			InputPortConfig_AnyType("Input4"),
			InputPortConfig_AnyType("Input5"),
			InputPortConfig_AnyType("Input6"),
			InputPortConfig_Void("Reset",     _HELP("Reset (and allow new trigger)")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Out", _HELP("Output")),
			{ 0 }
		};
		config.sDescription = _HELP("Triggers only once and passes from activated Input port to output port [Out].");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_bTriggered = false;
			break;
		case eFE_Activate:
			if (m_bTriggered == false)
			{
				for (int i = 0; i < 6; i++)
				{
					if (IsPortActive(pActInfo, i))
					{
						ActivateOutput(pActInfo, 0, GetPortAny(pActInfo, i));
						m_bTriggered = true;
						break;
					}
				}
			}
			if (IsPortActive(pActInfo, 6))
				m_bTriggered = false;
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	bool m_bTriggered;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_LogicCountBlocker : public CFlowBaseNode<eNCT_Instanced>
{
	enum
	{
		INP_In = 0,
		INP_Reset,
		INP_Limit,

		OUT_Out = 0,
	};

public:
	CFlowNode_LogicCountBlocker(SActivationInfo* pActInfo) : m_timesTriggered(0)
	{
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_LogicCountBlocker(pActInfo);
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		ser.Value("timesTriggered", m_timesTriggered);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig_AnyType("In"),
			InputPortConfig_Void("Reset", _HELP("Reset (counter starts from 0 again)")),
			InputPortConfig<int>("Limit", 1,                                            _HELP("How many times 'in' triggered values are sent to the output. After this number is reached, the output will not be triggered again (unless a reset is called)"),0, _UICONFIG("v_min=1,v_max=1000000000")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Out", _HELP("Output the value sent to 'in'")),
			{ 0 }
		};
		config.sDescription = _HELP("The value triggered into 'in' is sent to 'out' a maximum number of times");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_timesTriggered = 0;
			break;
		case eFE_Activate:
			int limit = GetPortInt(pActInfo, INP_Limit);

			if (m_timesTriggered < limit && IsPortActive(pActInfo, INP_In))
			{
				ActivateOutput(pActInfo, OUT_Out, GetPortAny(pActInfo, INP_In));
				m_timesTriggered++;
				break;
			}
			if (IsPortActive(pActInfo, INP_Reset))
				m_timesTriggered = 0;
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	int m_timesTriggered;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_LogicNoSerializeOnce : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNode_LogicNoSerializeOnce(SActivationInfo* pActInfo) : m_bTriggered(false)
	{
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_LogicNoSerializeOnce(pActInfo);
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		// this node IN PURPOSE does not serialize m_bTriggered. The whole idea of this node is to be used for things that have to be triggered only once in a level,
		// even if a previous savegame is loaded. Its first use is for Tutorial PopUps: after the player see one, that popup should not popup again even if the player loads a previous savegame.
		// It is not perfect: leaving the game, re-launching, and using the "Resume Game" option, will make the node to be triggered again if an input is activated.

		//		ser.Value("m_bTriggered", m_bTriggered);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("Input1"),
			InputPortConfig_Void("Reset",     _HELP("Reset (and allow new trigger)")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Out", _HELP("Output")),
			{ 0 }
		};
		config.sDescription = _HELP("Triggers only once and passes from activated Input port to output port [Out]. \nWARNING!! The triggered flag is not serialized on savegame!. \nThis means that even if a previous savegame is loaded after the node has been triggered, the node wont be triggered again");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			if (gEnv->IsEditor())
				m_bTriggered = false;
			break;
		case eFE_Activate:
			if (m_bTriggered == false)
			{
				for (int i = 0; i < 6; i++)
				{
					if (IsPortActive(pActInfo, i))
					{
						ActivateOutput(pActInfo, 0, GetPortAny(pActInfo, i));
						m_bTriggered = true;
						break;
					}
				}
			}
			if (IsPortActive(pActInfo, 6))
				m_bTriggered = false;
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	bool m_bTriggered;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_RandomSelect : public CFlowBaseNode<eNCT_Singleton>
{
public:

	static const int NUM_OUTPUTS = 10;

	CFlowNode_RandomSelect(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("In",  _HELP("Input")),
			InputPortConfig<int>("outMin", _HELP("Min number of outputs to activate.")),
			InputPortConfig<int>("outMax", _HELP("Max number of outputs to activate.")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Out1",  _HELP("Output1")),
			OutputPortConfig_AnyType("Out2",  _HELP("Output2")),
			OutputPortConfig_AnyType("Out3",  _HELP("Output3")),
			OutputPortConfig_AnyType("Out4",  _HELP("Output4")),
			OutputPortConfig_AnyType("Out5",  _HELP("Output5")),
			OutputPortConfig_AnyType("Out6",  _HELP("Output6")),
			OutputPortConfig_AnyType("Out7",  _HELP("Output7")),
			OutputPortConfig_AnyType("Out8",  _HELP("Output8")),
			OutputPortConfig_AnyType("Out9",  _HELP("Output9")),
			OutputPortConfig_AnyType("Out10", _HELP("Output10")),
			{ 0 }
		};
		config.sDescription = _HELP("Passes the activated input value to a random amount [outMin <= random <= outMax] outputs.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				int minOut = GetPortInt(pActInfo, 1);
				int maxOut = GetPortInt(pActInfo, 2);

				minOut = CLAMP(minOut, 0, NUM_OUTPUTS);
				maxOut = CLAMP(maxOut, 0, NUM_OUTPUTS);
				if (maxOut < minOut)
					std::swap(minOut, maxOut);

				int n = cry_random(minOut, maxOut);

				// Collect the outputs to use
				static int out[NUM_OUTPUTS];
				for (unsigned i = 0; i < NUM_OUTPUTS; i++)
					out[i] = -1;
				int nout = 0;
				for (int i = 0; i < NUM_OUTPUTS; i++)
				{
					if (IsOutputConnected(pActInfo, i))
					{
						out[nout] = i;
						nout++;
					}
				}
				if (n > nout)
					n = nout;

				// Shuffle
				for (int i = 0; i < n; i++)
					std::swap(out[i], out[cry_random(0, nout - 1)]);

				// Set outputs.
				for (int i = 0; i < n; i++)
				{
					if (out[i] == -1)
						continue;
					ActivateOutput(pActInfo, out[i], GetPortAny(pActInfo, 0));
				}

			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_RandomTrigger : public CFlowBaseNode<eNCT_Instanced>
{
public:

	static const int NUM_OUTPUTS = 10;

	CFlowNode_RandomTrigger(SActivationInfo* pActInfo) : m_nOutputCount(0) { Init(); Reset(); }

	enum INPUTS
	{
		EIP_Input = 0,
		EIP_Reset,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("In", _HELP("Input")),
			InputPortConfig_Void("Reset", _HELP("Reset randomness")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Out1",  _HELP("Output1")),
			OutputPortConfig_AnyType("Out2",  _HELP("Output2")),
			OutputPortConfig_AnyType("Out3",  _HELP("Output3")),
			OutputPortConfig_AnyType("Out4",  _HELP("Output4")),
			OutputPortConfig_AnyType("Out5",  _HELP("Output5")),
			OutputPortConfig_AnyType("Out6",  _HELP("Output6")),
			OutputPortConfig_AnyType("Out7",  _HELP("Output7")),
			OutputPortConfig_AnyType("Out8",  _HELP("Output8")),
			OutputPortConfig_AnyType("Out9",  _HELP("Output9")),
			OutputPortConfig_AnyType("Out10", _HELP("Output10")),
			OutputPortConfig_Void("Done",     _HELP("Triggered after all [connected] outputs have triggered")),
			{ 0 }
		};
		config.sDescription = _HELP("On each [In] trigger, triggers one of the connected outputs in random order.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_ConnectOutputPort:
			if (pActInfo->connectPort < NUM_OUTPUTS)
			{
				++m_nConnectionCounts[pActInfo->connectPort];
				// check if already connected
				for (int i = 0; i < m_nOutputCount; ++i)
				{
					if (m_nConnectedPorts[i] == pActInfo->connectPort)
						return;
				}
				m_nConnectedPorts[m_nOutputCount++] = pActInfo->connectPort;
				Reset();
			}
			break;
		case eFE_DisconnectOutputPort:
			if (pActInfo->connectPort < NUM_OUTPUTS)
			{
				for (int i = 0; i < m_nOutputCount; ++i)
				{
					// check if really connected
					if (m_nConnectedPorts[i] == pActInfo->connectPort)
					{
						if (m_nConnectionCounts[pActInfo->connectPort] == 1)
						{
							m_nConnectedPorts[i] = m_nPorts[m_nOutputCount - 1]; // copy last value to here
							m_nConnectedPorts[m_nOutputCount - 1] = -1;
							--m_nOutputCount;
							Reset();
						}
						--m_nConnectionCounts[pActInfo->connectPort];
						return;
					}
				}
			}
			break;
		case eFE_Initialize:
			Reset();
			break;

		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_Reset))
			{
				Reset();
			}
			if (IsPortActive(pActInfo, EIP_Input))
			{
				int numCandidates = m_nOutputCount - m_nTriggered;
				if (numCandidates <= 0)
					return;
				const int cand = cry_random(0, numCandidates - 1);
				const int whichPort = m_nPorts[cand];
				m_nPorts[cand] = m_nPorts[numCandidates - 1];
				m_nPorts[numCandidates - 1] = -1;
				++m_nTriggered;
				assert(whichPort >= 0 && whichPort < NUM_OUTPUTS);
				// CryLogAlways("CFlowNode_RandomTrigger: Activating %d", whichPort);
				ActivateOutput(pActInfo, whichPort, GetPortAny(pActInfo, EIP_Input));
				assert(m_nTriggered > 0 && m_nTriggered <= m_nOutputCount);
				if (m_nTriggered == m_nOutputCount)
				{
					// CryLogAlways("CFlowNode_RandomTrigger: Done.");
					// Done
					ActivateOutput(pActInfo, NUM_OUTPUTS, true);
					Reset();
				}
			}
			break;
		}
	}

	void Init()
	{
		for (int i = 0; i < NUM_OUTPUTS; ++i)
		{
			m_nConnectedPorts[i] = -1;
			m_nConnectionCounts[i] = 0;
		}
	}

	void Reset()
	{
		memcpy(m_nPorts, m_nConnectedPorts, sizeof(m_nConnectedPorts));
		m_nTriggered = 0;
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		CFlowNode_RandomTrigger* pClone = new CFlowNode_RandomTrigger(pActInfo);
		// copy connected ports to cloned node
		// because atm. we don't send the  eFE_ConnectOutputPort or eFE_DisconnectOutputPort
		// to cloned graphs (see CFlowGraphBase::Clone)
		memcpy(pClone->m_nConnectedPorts, m_nConnectedPorts, sizeof(m_nConnectedPorts));
		memcpy(pClone->m_nConnectionCounts, m_nConnectionCounts, sizeof(m_nConnectionCounts));
		pClone->Reset();
		return pClone;
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		char name[32];
		ser.Value("m_nOutputCount", m_nOutputCount);
		ser.Value("m_nTriggered", m_nTriggered);
		for (int i = 0; i < NUM_OUTPUTS; ++i)
		{
			cry_sprintf(name, "m_nPorts_%d", i);
			ser.Value(name, m_nPorts[i]);
		}
		// the m_nConnectedPorts must not be serialized. it's generated automatically
		// sanity check
		if (ser.IsReading())
		{
			bool bNeedReset = false;
			for (int i = 0; i < NUM_OUTPUTS && !bNeedReset; ++i)
			{
				bool bFound = false;
				for (int j = 0; j < NUM_OUTPUTS && !bFound; ++j)
				{
					bFound = (m_nPorts[i] == m_nConnectedPorts[j]);
				}
				bNeedReset = !bFound;
			}
			// if some of the serialized port can not be found, reset
			if (bNeedReset)
				Reset();
		}
	}

	int m_nConnectedPorts[NUM_OUTPUTS];   // array with port-numbers which are connected.
	int m_nPorts[NUM_OUTPUTS];            // permutation of m_nConnectedPorts array with m_nOutputCount valid entries
	int m_nConnectionCounts[NUM_OUTPUTS]; // number of connections on each port
	int m_nTriggered;
	int m_nOutputCount;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Sequentializer : public CFlowBaseNode<eNCT_Instanced>
{
public:

	enum
	{
		NUM_OUTPUTS = 10,
		PORT_NONE   = 0xffffffff,
	};

	CFlowNode_Sequentializer(SActivationInfo* pActInfo)
		: m_needToCheckConnectedPorts(true)
		, m_closed(false)
		, m_lastTriggeredPort(PORT_NONE)
		, m_numConnectedPorts(0)
	{
		ZeroArray(m_connectedPorts);
	}

	enum INPUTS
	{
		IN_Input = 0,
		IN_Closed,
		IN_Open,
		IN_Close,
		IN_Reset,
		IN_Reverse
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static_assert(PORT_NONE + 1 == 0, "Unexpected enum value!"); // or else the automatic boundary checks when incrememting the port number would not work

		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("In",    _HELP("Input")),
			InputPortConfig<bool>("Closed",  false,                                         _HELP("If true blocks all signals.")),
			InputPortConfig_Void("Open",     _HELP("Sets [Closed] to false.")),
			InputPortConfig_Void("Close",    _HELP("Sets [Closed] to true.")),
			InputPortConfig_Void("Reset",    _HELP("Forces next output to be Port1 again")),
			InputPortConfig<bool>("Reverse", false,                                         _HELP("If true, the order of output activation is reversed.")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Out1",  _HELP("Output1")),
			OutputPortConfig_AnyType("Out2",  _HELP("Output2")),
			OutputPortConfig_AnyType("Out3",  _HELP("Output3")),
			OutputPortConfig_AnyType("Out4",  _HELP("Output4")),
			OutputPortConfig_AnyType("Out5",  _HELP("Output5")),
			OutputPortConfig_AnyType("Out6",  _HELP("Output6")),
			OutputPortConfig_AnyType("Out7",  _HELP("Output7")),
			OutputPortConfig_AnyType("Out8",  _HELP("Output8")),
			OutputPortConfig_AnyType("Out9",  _HELP("Output9")),
			OutputPortConfig_AnyType("Out10", _HELP("Output10")),
			{ 0 }
		};
		config.sDescription = _HELP("On each [In] trigger, triggers one of the connected outputs in sequential order.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_closed = GetPortBool(pActInfo, IN_Closed);

		case eFE_ConnectOutputPort:
		case eFE_DisconnectOutputPort:
			{
				m_lastTriggeredPort = PORT_NONE;
				m_needToCheckConnectedPorts = true;
				m_numConnectedPorts = 0;
				break;
			}

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, IN_Closed))
					m_closed = GetPortBool(pActInfo, IN_Closed);
				if (IsPortActive(pActInfo, IN_Open))
					m_closed = false;
				if (IsPortActive(pActInfo, IN_Close))
					m_closed = true;

				if (IsPortActive(pActInfo, IN_Reset))
				{
					m_lastTriggeredPort = PORT_NONE;
				}

				if (m_needToCheckConnectedPorts)
				{
					m_needToCheckConnectedPorts = false;
					m_numConnectedPorts = 0;
					for (int port = 0; port < NUM_OUTPUTS; ++port)
					{
						if (IsOutputConnected(pActInfo, port))
						{
							m_connectedPorts[m_numConnectedPorts] = port;
							m_numConnectedPorts++;
						}
					}
				}

				if (IsPortActive(pActInfo, IN_Input) && m_numConnectedPorts > 0 && !m_closed)
				{
					bool reversed = GetPortBool(pActInfo, IN_Reverse);
					unsigned int port = m_lastTriggeredPort;

					if (reversed)
					{
						port = min(m_numConnectedPorts - 1, port - 1); // this takes care of both the initial state when it has the PORT_NONE value, and the overflow in the normal decrement situation
					}
					else
					{
						port = (port + 1) % m_numConnectedPorts;
					}

					ActivateOutput(pActInfo, m_connectedPorts[port], GetPortAny(pActInfo, IN_Input));
					m_lastTriggeredPort = port;
				}
				break;
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		CFlowNode_Sequentializer* pClone = new CFlowNode_Sequentializer(pActInfo);
		return pClone;
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		ser.Value("m_lastTriggeredPort", m_lastTriggeredPort);
		ser.Value("m_closed", m_closed);
	}

	bool IsOutputConnected(SActivationInfo* pActInfo, int nPort) const
	{
		SFlowAddress addr(pActInfo->myID, nPort, true);
		return pActInfo->pGraph->IsOutputConnected(addr);
	}

	bool         m_needToCheckConnectedPorts;
	bool         m_closed;
	unsigned int m_lastTriggeredPort;
	unsigned int m_numConnectedPorts;
	int          m_connectedPorts[NUM_OUTPUTS];
};

REGISTER_FLOW_NODE("Logic:AND", CFlowNode_AND);
REGISTER_FLOW_NODE("Logic:OR", CFlowNode_OR);
REGISTER_FLOW_NODE("Logic:XOR", CFlowNode_XOR);
REGISTER_FLOW_NODE("Logic:NOT", CFlowNode_NOT);
REGISTER_FLOW_NODE("Logic:OnChange", CFlowNode_OnChange);
REGISTER_FLOW_NODE("Logic:Any", CFlowNode_Any);
REGISTER_FLOW_NODE("Logic:Blocker", CFlowNode_Blocker);
REGISTER_FLOW_NODE("Logic:All", CFlowNode_All);
REGISTER_FLOW_NODE("Logic:RandomSelect", CFlowNode_RandomSelect);
REGISTER_FLOW_NODE("Logic:RandomTrigger", CFlowNode_RandomTrigger);
REGISTER_FLOW_NODE("Logic:Once", CFlowNode_LogicOnce);
REGISTER_FLOW_NODE("Logic:CountBlocker", CFlowNode_LogicCountBlocker);
REGISTER_FLOW_NODE("Logic:NoSerializeOnce", CFlowNode_LogicNoSerializeOnce);
REGISTER_FLOW_NODE("Logic:Gate", CFlowNode_LogicGate);
REGISTER_FLOW_NODE("Logic:Sequentializer", CFlowNode_Sequentializer);
