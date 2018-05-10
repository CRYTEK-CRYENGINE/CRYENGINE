// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowMathNodes.h
//  Version:     v1.00
//  Created:     7/6/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

#include <CryMath/Random.h>

//////////////////////////////////////////////////////////////////////////
// Math nodes.
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Calculate : public CFlowBaseNode<eNCT_Singleton>
{
	enum
	{
		INP_DoCalc,
		INP_Operation,
		INP_A,
		INP_B
	};
	enum EOperation
	{
		EOP_ADD = 0,
		EOP_SUB,
		EOP_MUL,
		EOP_DIV
	};

public:
	CFlowNode_Calculate(SActivationInfo* pActInfo) {};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("DoCalc", _HELP("Do the calc and send the result to the output when receiving event on this port")),
			InputPortConfig<int>("Op",     EOP_ADD,                                                                                  _HELP("Operation"),_HELP("Operation"), _UICONFIG("enum_int:Add=0,Sub=1,Mul=2,Div=3")),
			InputPortConfig<float>("A",    _HELP(" ")),
			InputPortConfig<float>("B",    _HELP(" ")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out"),
			{ 0 }
		};
		config.sDescription = _HELP("[out] = result of the specified operation with [A] and [B]");
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
				if (IsPortActive(pActInfo, INP_DoCalc))
				{
					float a = GetPortFloat(pActInfo, INP_A);
					float b = GetPortFloat(pActInfo, INP_B);
					int op = GetPortInt(pActInfo, INP_Operation);
					float out = 0;
					switch (op)
					{
					case EOP_ADD:
						out = a + b;
						break;

					case EOP_SUB:
						out = a - b;
						break;

					case EOP_MUL:
						out = a * b;
						break;

					case EOP_DIV:
						if (!iszero(b))
							out = a / b;
						break;
					}

					ActivateOutput(pActInfo, 0, out);
				}
				break;
			}
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Round : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Round(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("In"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("outRounded"),
			{ 0 }
		};
		config.sDescription = _HELP("Rounds an input float value to an integer value");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			float inpVal = GetPortFloat(pActInfo, 0);
			int out = int_round(inpVal);
			ActivateOutput(pActInfo, 0, out);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_FloatToString : public CFlowBaseNode<eNCT_Singleton>
{
	enum INP
	{
		IN_NUMBER = 0,
		IN_AMOUNT_OF_DECIMALS,
	};

public:
	CFlowNode_FloatToString(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("Number"),
			InputPortConfig<int>("AmountOfDecimals"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<string>("out"),
			{ 0 }
		};
		config.sDescription = _HELP("Outputs a float in string format, with limited amount of decimals. Used for debug visualization");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_DEBUG);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			float inpVal = GetPortFloat(pActInfo, IN_NUMBER);
			int decimals = GetPortInt(pActInfo, IN_AMOUNT_OF_DECIMALS);
			stack_string formatStr;
			formatStr.Format("%%.%df", decimals);
			string tempstr;
			tempstr.Format(formatStr.c_str(), inpVal);
			ActivateOutput(pActInfo, 0, tempstr);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Add : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Add(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("A"),
			InputPortConfig<float>("B"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out"),
			{ 0 }
		};
		config.sDescription = _HELP("[out] = [A] + [B]");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			float a = GetPortFloat(pActInfo, 0);
			float b = GetPortFloat(pActInfo, 1);
			float out = a + b;
			ActivateOutput(pActInfo, 0, out);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};
//////////////////////////////////////////////////////////////////////////
class CFlowNode_Sub : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Sub(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("A"),
			InputPortConfig<float>("B"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out"),
			{ 0 }
		};
		config.sDescription = _HELP("[out] = [A] - [B]");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			float a = GetPortFloat(pActInfo, 0);
			float b = GetPortFloat(pActInfo, 1);
			float out = a - b;
			ActivateOutput(pActInfo, 0, out);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};
//////////////////////////////////////////////////////////////////////////
class CFlowNode_Mul : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Mul(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("A"),
			InputPortConfig<float>("B"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out"),
			{ 0 }
		};
		config.sDescription = _HELP("[out] = [A] * [B]");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			float a = GetPortFloat(pActInfo, 0);
			float b = GetPortFloat(pActInfo, 1);
			float out = a * b;
			ActivateOutput(pActInfo, 0, out);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_CalculateVec3 : public CFlowBaseNode<eNCT_Singleton>
{
	enum
	{
		INP_DoCalc,
		INP_Operation,
		INP_A,
		INP_B
	};
	enum EOperation
	{
		EOP_ADD = 0,
		EOP_SUB,
		EOP_MUL,
		EOP_CROSS,
		EOP_DOT
	};

public:
	CFlowNode_CalculateVec3(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("DoCalc", _HELP("Do the calc and send the result to the output when receiving event on this port")),
			InputPortConfig<int>("Op",     EOP_ADD,                                                                                  _HELP("Operation"),_HELP("Operation"), _UICONFIG("enum_int:Add=0,Sub=1,Mul=2,Cross=3,Dot=4")),
			InputPortConfig<Vec3>("A",     _HELP("Vector A")),
			InputPortConfig<Vec3>("B",     _HELP("Vector B")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("out", _HELP("A Vec3 with the result of the operation or a float for a Dot product")),
			{ 0 }
		};
		config.sDescription = _HELP("[out] = result of the specified operation with [A] and [B]");
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
				if (IsPortActive(pActInfo, INP_DoCalc))
				{
					Vec3 a = GetPortVec3(pActInfo, INP_A);
					Vec3 b = GetPortVec3(pActInfo, INP_B);
					int op = GetPortInt(pActInfo, INP_Operation);

					if (op == EOP_DOT)
					{
						float out = a.Dot(b);
						ActivateOutput(pActInfo, 0, out);
					}
					else
					{
						Vec3 out(0, 0, 0);
						switch (op)
						{
						case EOP_ADD:
							out = a + b;
							break;

						case EOP_SUB:
							out = a - b;
							break;

						case EOP_MUL:
							out.x = a.x * b.x;
							out.y = a.y * b.y;
							out.z = a.z * b.z;
							break;

						case EOP_CROSS:
							out = a.Cross(b);
							break;
						}
						ActivateOutput(pActInfo, 0, out);
					}

				}
				break;
			}
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_AddVec3 : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_AddVec3(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("A", _HELP("out=A+B")),
			InputPortConfig<Vec3>("B", _HELP("out=A+B")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("out", _HELP("out=A+B")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			Vec3 a = GetPortVec3(pActInfo, 0);
			Vec3 b = GetPortVec3(pActInfo, 1);
			Vec3 out = a + b;
			ActivateOutput(pActInfo, 0, out);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};
//////////////////////////////////////////////////////////////////////////
class CFlowNode_SubVec3 : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_SubVec3(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("A", _HELP("out=A-B")),
			InputPortConfig<Vec3>("B", _HELP("out=A-B")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("out", _HELP("out=A-B")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			Vec3 a = GetPortVec3(pActInfo, 0);
			Vec3 b = GetPortVec3(pActInfo, 1);
			Vec3 out = a - b;
			ActivateOutput(pActInfo, 0, out);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};
//////////////////////////////////////////////////////////////////////////
class CFlowNode_MulVec3 : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_MulVec3(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("A", _HELP("out=A*B")),
			InputPortConfig<Vec3>("B", _HELP("out=A*B")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("out", _HELP("out=A*B")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			Vec3 a = GetPortVec3(pActInfo, 0);
			Vec3 b = GetPortVec3(pActInfo, 1);
			Vec3 out(a.x * b.x, a.y * b.y, a.z * b.z);
			ActivateOutput(pActInfo, 0, out);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_CrossVec3 : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_CrossVec3(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("A", _HELP("out=A^B")),
			InputPortConfig<Vec3>("B", _HELP("out=A^B")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("out", _HELP("out=A^B")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			Vec3 a = GetPortVec3(pActInfo, 0);
			Vec3 b = GetPortVec3(pActInfo, 1);
			Vec3 out = a.Cross(b);
			ActivateOutput(pActInfo, 0, out);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_DotVec3 : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_DotVec3(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("A", _HELP("out=A.B")),
			InputPortConfig<Vec3>("B", _HELP("out=A.B")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out", "out=A.B"),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			Vec3 a = GetPortVec3(pActInfo, 0);
			Vec3 b = GetPortVec3(pActInfo, 1);
			float out = a.Dot(b);
			ActivateOutput(pActInfo, 0, out);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_MagnitudeVec3 : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_MagnitudeVec3(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("vector"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("length"),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			ActivateOutput(pActInfo, 0, GetPortVec3(pActInfo, 0).GetLength());
		}
		;
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_ReciprocalVec3 : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_ReciprocalVec3(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("vector"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("length", _HELP("1.0 / length[vector]")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			Vec3 in(GetPortVec3(pActInfo, 0));
			Vec3 out(abs(in.x) > 0.0001f ? 1.0f / in.x : 0.0f,
			         abs(in.y) > 0.0001f ? 1.0f / in.y : 0.0f,
			         abs(in.z) > 0.0001f ? 1.0f / in.z : 0.0f);
			ActivateOutput(pActInfo, 0, out);
		}
		;
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_ScaleVec3 : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_ScaleVec3(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("vector"),
			InputPortConfig<float>("scale"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("out"),
			{ 0 }
		};
		config.sDescription = _HELP("[out] = [vector] * [scale]");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			Vec3 a = GetPortVec3(pActInfo, 0);
			float b = GetPortFloat(pActInfo, 1);
			Vec3 out = a * b;
			ActivateOutput(pActInfo, 0, out);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_NormalizeVec3 : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_NormalizeVec3(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("vector", _HELP("||out||=1")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("out",     _HELP("||out||=1")),
			OutputPortConfig<float>("lenght", _HELP("||out||=1"), _HELP("length")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			Vec3 a = GetPortVec3(pActInfo, 0);
			float l = a.len();

			if (l > 0)
				ActivateOutput(pActInfo, 0, a * (1.0f / l));
			ActivateOutput(pActInfo, 1, l);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_EqualVec3 : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_EqualVec3(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("A", _HELP("out triggered when A==B")),
			InputPortConfig<Vec3>("B", _HELP("out triggered when A==B")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("out"),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			Vec3 a = GetPortVec3(pActInfo, 0);
			Vec3 b = GetPortVec3(pActInfo, 1);
			if (a.IsEquivalent(b))
				ActivateOutput(pActInfo, 0, true);
			else
				ActivateOutput(pActInfo, 0, false);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Reciprocal : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Reciprocal(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("A", _HELP("out=1/A")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out", _HELP("out=1/A")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			float a = GetPortFloat(pActInfo, 0);
			float out = abs(a) > 0.0001f ? 1.0f / a : 0.0f;
			ActivateOutput(pActInfo, 0, out);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Power : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Power(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("base",  _HELP("out=base^power")),
			InputPortConfig<float>("power", _HELP("out=base^power")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out", "out=base^power"),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			float a = GetPortFloat(pActInfo, 0);
			float b = GetPortFloat(pActInfo, 1);
			float out = pow(a, b);
			ActivateOutput(pActInfo, 0, out);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Sqrt : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Sqrt(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("A", _HELP("out=sqrt(A)")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out", _HELP("out=sqrt(A)")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			float a = GetPortFloat(pActInfo, 0);
			float out = sqrtf(a);
			ActivateOutput(pActInfo, 0, out);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Abs : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Abs(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("A", _HELP("out=|A|")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out", _HELP("out=|A|")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			float a = GetPortFloat(pActInfo, 0);
			float out = abs(a);
			ActivateOutput(pActInfo, 0, out);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Div : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Div(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("A"),
			InputPortConfig<float>("B"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out"),
			{ 0 }
		};
		config.sDescription = _HELP("[out] = [A] / [B]");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			float a = GetPortFloat(pActInfo, 0);
			float b = GetPortFloat(pActInfo, 1);
			float out = 0.0f;
			if (!iszero(b))
				out = a / b;
			ActivateOutput(pActInfo, 0, out);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};
//////////////////////////////////////////////////////////////////////////
class CFlowNode_Remainder : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Remainder(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("A"),
			InputPortConfig<float>("B"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("out"),
			{ 0 }
		};
		config.sDescription = _HELP("[out] = remainder of [A]/[B]");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			float a = GetPortFloat(pActInfo, 0);
			float b = GetPortFloat(pActInfo, 1);
			int ia = FtoI(a);
			int ib = FtoI(b);
			int out;
			if (ib != 0)
				out = ia % ib;
			else
				out = 0;
			ActivateOutput(pActInfo, 0, out);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_InRange : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_InRange(SActivationInfo* pActInfo) {}

	enum
	{
		INP_In = 0,
		INP_Min,
		INP_Max,
	};
	enum EOperation
	{
		EOP_Out = 0,
		EOP_True,
		EOP_False,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("In"),
			InputPortConfig<float>("Min"),
			InputPortConfig<float>("Max"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("out",  _HELP("true when [in]>=[Min] and [in]<=[Max], false otherwise")),
			OutputPortConfig_Void("true",  _HELP("triggered if In is in range (>=min and <=max)")),
			OutputPortConfig_Void("false", _HELP("triggered if In is in range (>=min and <=max)")),
			{ 0 }
		};
		config.sDescription = _HELP("[out] is true when [in]>=[Min] and [in]<=[Max], false otherwise");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		bool bOut = false;
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			float in = GetPortFloat(pActInfo, INP_In);
			float v_min = GetPortFloat(pActInfo, INP_Min);
			float v_max = GetPortFloat(pActInfo, INP_Max);
			bOut = (in >= v_min && in <= v_max) ? true : false;

			if (bOut)
				ActivateOutput(pActInfo, EOP_True, true);
			else
				ActivateOutput(pActInfo, EOP_False, true);

			ActivateOutput(pActInfo, EOP_Out, bOut);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Equal : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Equal(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("A"),
			InputPortConfig<float>("B"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("out"),
			OutputPortConfig_Void("true", _HELP("triggered if out is true")),
			OutputPortConfig_Void("false",_HELP("triggered if out is false")),
			{ 0 }
		};
		config.sDescription = _HELP("[out] is true when [A]==[B], false otherwise");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		bool bOut = false;
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			float a = GetPortFloat(pActInfo, 0);
			float b = GetPortFloat(pActInfo, 1);
			bOut = (fabs(a - b) < 0.00001f) ? true : false;

			if (bOut)
				ActivateOutput(pActInfo, 1, true);
			else
				ActivateOutput(pActInfo, 2, true);

			ActivateOutput(pActInfo, 0, bOut);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_EqualCheck : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInPorts
	{
		IN_A = 0,
		IN_B,
		IN_CHECK
	};

public:
	CFlowNode_EqualCheck(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("A"),
			InputPortConfig<float>("B"),
			InputPortConfig_Void("Check"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("out"),
			OutputPortConfig_Void("true", _HELP("triggered if out is true")),
			OutputPortConfig_Void("false",_HELP("triggered if out is false")),
			{ 0 }
		};
		config.sDescription = _HELP("[out] is true when [A]==[B], false otherwise. The check is only performed when the 'Check' input is triggered");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		bool bOut = false;
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, IN_CHECK))
				{
					float a = GetPortFloat(pActInfo, 0);
					float b = GetPortFloat(pActInfo, 1);
					bOut = (fabs(a - b) < 0.00001f) ? true : false;

					if (bOut)
						ActivateOutput(pActInfo, 1, true);
					else
						ActivateOutput(pActInfo, 2, true);

					ActivateOutput(pActInfo, 0, bOut);
				}
				break;
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Less : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Less(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("A"),
			InputPortConfig<float>("B"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("out"),
			OutputPortConfig_Void("true", _HELP("triggered if out is true")),
			OutputPortConfig_Void("false",_HELP("triggered if out is false")),
			{ 0 }
		};
		config.sDescription = _HELP("[out] is true when [A] < [B], false otherwise");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		bool bOut = false;
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			float a = GetPortFloat(pActInfo, 0);
			float b = GetPortFloat(pActInfo, 1);
			bOut = (a < b) ? true : false;

			if (bOut)
				ActivateOutput(pActInfo, 1, true);
			else
				ActivateOutput(pActInfo, 2, true);

			ActivateOutput(pActInfo, 0, bOut);

			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_LessCheck : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInPorts
	{
		IN_A = 0,
		IN_B,
		IN_CHECK
	};

public:
	CFlowNode_LessCheck(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("A"),
			InputPortConfig<float>("B"),
			InputPortConfig_Void("Check"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("out"),
			OutputPortConfig_Void("true", _HELP("triggered if out is true")),
			OutputPortConfig_Void("false",_HELP("triggered if out is false")),
			{ 0 }
		};
		config.sDescription = _HELP("[out] is true when [A] < [B], false otherwise. The check is only performed when the 'Check' input is triggered");
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
				if (IsPortActive(pActInfo, IN_CHECK))
				{
					float a = GetPortFloat(pActInfo, IN_A);
					float b = GetPortFloat(pActInfo, IN_B);
					bool bOut = (a < b) ? true : false;

					if (bOut)
						ActivateOutput(pActInfo, 1, true);
					else
						ActivateOutput(pActInfo, 2, true);

					ActivateOutput(pActInfo, 0, bOut);
				}
				break;
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};
//////////////////////////////////////////////////////////////////////////
class CFlowNode_Counter : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNode_Counter(SActivationInfo* pActInfo) { m_nCounter = 0; }
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_Counter(pActInfo); }

	virtual void         Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("m_nCounter", m_nCounter);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("in"),
			InputPortConfig_Void("reset"),
			InputPortConfig<int>("max"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("count"),
			{ 0 }
		};
		config.sDescription = _HELP("When [in] port is triggered, internal counter is increased and sent to [out], when count reaches [max] counter is set to 0");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_nCounter = 0;
			ActivateOutput(pActInfo, 0, m_nCounter);
			break;

		case eFE_Activate:
			int nMax = GetPortInt(pActInfo, 2);
			int nCounter = m_nCounter;
			if (IsPortActive(pActInfo, 0))
				nCounter++;
			if (IsPortActive(pActInfo, 1))
				nCounter = 0;

			if (nMax > 0 && nCounter >= nMax)
				nCounter = 0;

			if (nCounter != m_nCounter || event == eFE_Initialize)
			{
				m_nCounter = nCounter;
				ActivateOutput(pActInfo, 0, m_nCounter);
			}
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
private:
	int m_nCounter;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_PortCounter : public CFlowBaseNode<eNCT_Instanced>
{
	static const int PORT_COUNT = 16;

public:
	CFlowNode_PortCounter(SActivationInfo* pActInfo) { Reset(); }
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_PortCounter(pActInfo); }

	virtual void         GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("reset",       _HELP("Trigger this to reset the total and port counters")),
			InputPortConfig<int>("portThreshold",  0,                                                          _HELP("When portCount reaches portThreshold, portTrigger is activated")),
			InputPortConfig<int>("totalThreshold", 0,                                                          _HELP("When totalCount reaches totalThreshold, totalTrigger is activated")),
			InputPortConfig_AnyType("in01"),
			InputPortConfig_AnyType("in02"),
			InputPortConfig_AnyType("in03"),
			InputPortConfig_AnyType("in04"),
			InputPortConfig_AnyType("in05"),
			InputPortConfig_AnyType("in06"),
			InputPortConfig_AnyType("in07"),
			InputPortConfig_AnyType("in08"),
			InputPortConfig_AnyType("in09"),
			InputPortConfig_AnyType("in10"),
			InputPortConfig_AnyType("in11"),
			InputPortConfig_AnyType("in12"),
			InputPortConfig_AnyType("in13"),
			InputPortConfig_AnyType("in14"),
			InputPortConfig_AnyType("in15"),
			InputPortConfig_AnyType("in16"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("portCount",     _HELP("Total number of ports that have been set since last reset. Only counts each port once. (0 to 16)")),
			OutputPortConfig<int>("totalCount",    _HELP("Sum of all the times any of the ports have been set, since last reset.")),
			OutputPortConfig<bool>("portTrigger",  _HELP("triggered when port count reaches port threshold.")),
			OutputPortConfig<bool>("totalTrigger", _HELP("triggered when total count reaches total threshold.")),
			{ 0 }
		};
		config.sDescription = _HELP("Counts the number of activated 'in' ports. Allows designers to, for example, trigger an action when 7 out of 12 entities have died");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_ADVANCED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
		case eFE_Activate:

			//--- Reset?
			if (IsPortActive(pActInfo, 0) || (event == eFE_Initialize))
			{
				Reset();
			}

			//--- Any in ports activated?
			int iPort;
			int nPorts = m_nPorts;
			int nTotal = m_nTotal;
			for (iPort = 0; iPort < PORT_COUNT; ++iPort)
			{
				if (IsPortActive(pActInfo, iPort + 3))
				{
					if (!m_abPorts[iPort])
					{
						m_abPorts[iPort] = true;
						++nPorts;
					}
					++nTotal;
				}
			}

			//--- Check triggers
			if (!m_bPortTriggered && (nPorts >= GetPortInt(pActInfo, 1)))
			{
				m_bPortTriggered = true;
				ActivateOutput(pActInfo, 2, m_bPortTriggered);
			}
			if (!m_bTotalTriggered && (nTotal >= GetPortInt(pActInfo, 2)))
			{
				m_bTotalTriggered = true;
				ActivateOutput(pActInfo, 3, m_bTotalTriggered);
			}
			//--- Output any changed totals
			if (nPorts != m_nPorts)
			{
				m_nPorts = nPorts;
				ActivateOutput(pActInfo, 0, m_nPorts);
			}
			if (nTotal != m_nTotal)
			{
				m_nTotal = nTotal;
				ActivateOutput(pActInfo, 1, m_nTotal);
			}
			break;
		}
	};

	void Serialize(SActivationInfo*, TSerialize ser)
	{
		ser.BeginGroup("Local");
		ser.Value("m_nPorts", m_nPorts);
		ser.Value("m_nTotal", m_nTotal);
		ser.Value("m_bPortTriggered", m_bPortTriggered);
		ser.Value("m_bTotalTriggered", m_bTotalTriggered);
		if (ser.IsWriting())
		{
			std::vector<bool> ab(m_abPorts, m_abPorts + PORT_COUNT);
			ser.Value("m_abPorts", ab);
		}
		else
		{
			memset((void*)m_abPorts, 0, sizeof(m_abPorts)); // clear them in case we can't read all values
			std::vector<bool> ab;
			ser.Value("m_abPorts", ab);
			std::copy(ab.begin(), ab.end(), m_abPorts);
		}
		ser.EndGroup();
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	int  m_nPorts;
	int  m_nTotal;
	bool m_bPortTriggered;
	bool m_bTotalTriggered;
	bool m_abPorts[PORT_COUNT];

	void Reset()
	{
		m_nPorts = 0;
		m_nTotal = 0;
		m_bPortTriggered = false;
		m_bTotalTriggered = false;
		memset((void*)m_abPorts, 0, sizeof(m_abPorts));
	}
};
//////////////////////////////////////////////////////////////////////////
class CFlowNode_Random : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Random(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("generate", _HELP("generate a random number between min and max")),
			InputPortConfig<float>("min",    _HELP("minimum value of the random number")),
			InputPortConfig<float>("max",    _HELP("maximum value of the random number")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out",      _HELP("random number between min and max")),
			OutputPortConfig<int>("outRounded", _HELP("[out] but rounded to next integer value")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
			case eFE_Initialize:
				float min = GetPortFloat(pActInfo, 1);
				float max = GetPortFloat(pActInfo, 2);
				float f = cry_random(min, max);
				ActivateOutput(pActInfo, 0, f);
				ActivateOutput(pActInfo, 1, int_round(f));
			}
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Clamp : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Clamp(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("in"),
			InputPortConfig<float>("min"),
			InputPortConfig<float>("max"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out"),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
		case eFE_Activate:
			{
				float in = GetPortFloat(pActInfo, 0);
				float min = GetPortFloat(pActInfo, 1);
				float max = GetPortFloat(pActInfo, 2);
				ActivateOutput(pActInfo, 0, clamp_tpl(in, min, max));
			}
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_ClampVec3 : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_ClampVec3(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("in"),
			InputPortConfig<Vec3>("min"),
			InputPortConfig<Vec3>("max"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("out"),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
		case eFE_Activate:
			{
				Vec3 in = GetPortVec3(pActInfo, 0);
				Vec3 min = GetPortVec3(pActInfo, 1);
				Vec3 max = GetPortVec3(pActInfo, 2);
				ActivateOutput(pActInfo, 0, Vec3(clamp_tpl(in.x, min.x, max.x), clamp_tpl(in.y, min.y, max.y), clamp_tpl(in.z, min.z, max.z)));
			}
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_SetVec3 : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_SetVec3(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Set", _HELP("Trigger to output")),
			InputPortConfig<Vec3>("In", _HELP("Value to be set on output")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("Out"),
			{ 0 }
		};
		config.sDescription = _HELP("Send input value to the output when event on set port is received.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED); // POLICY-CHANGE: don't send to output on initialize
	}
	virtual void ProcessEvent(EFlowEvent evt, SActivationInfo* pActInfo)
	{
		switch (evt)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				ActivateOutput(pActInfo, 0, GetPortVec3(pActInfo, 1));
			}
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_SetColor : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_SetColor(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Set",       _HELP("Trigger to output")),
			InputPortConfig<Vec3>("color_In", _HELP("Color to be set on output")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("Out"),
			{ 0 }
		};
		config.sDescription = _HELP("Send input value to the output when event on set port is received.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent evt, SActivationInfo* pActInfo)
	{
		switch (evt)
		{
		case eFE_Activate:
		case eFE_Initialize:
			if (IsPortActive(pActInfo, 0))
			{
				ActivateOutput(pActInfo, 0, GetPortVec3(pActInfo, 1));
			}
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_ToVec3 : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_ToVec3(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("x"),
			InputPortConfig<float>("y"),
			InputPortConfig<float>("z"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("vec3"),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			float x = GetPortFloat(pActInfo, 0);
			float y = GetPortFloat(pActInfo, 1);
			float z = GetPortFloat(pActInfo, 2);
			ActivateOutput(pActInfo, 0, Vec3(x, y, z));
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_ToBoolean : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_ToBoolean(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("true",  _HELP("Will output true when event on this port received")),
			InputPortConfig_Void("false", _HELP("Will output false when event on this port received")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("out", _HELP("Output true or false depending on the input ports")),
			{ 0 }
		};
		config.sDescription = _HELP("Output true or false depending on the input ports");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED); // No Longer output false on Initialize
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
				ActivateOutput(pActInfo, 0, true);
			else if (IsPortActive(pActInfo, 1))
				ActivateOutput(pActInfo, 0, false);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_FromBoolean : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_FromBoolean(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("Value", _HELP("Boolean value")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("False", _HELP("Triggered if Boolean was False")),
			OutputPortConfig_Void("True",  _HELP("Triggered if Boolean was True")),
			{ 0 }
		};
		config.sDescription = _HELP("Boolean switch");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED); // No Longer output false on Initialize
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			const bool bValue = GetPortBool(pActInfo, 0);
			ActivateOutput(pActInfo, bValue ? 1 : 0, true);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_FromVec3 : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_FromVec3(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("vec3"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("x"),
			OutputPortConfig<float>("y"),
			OutputPortConfig<float>("z"),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			Vec3 v = GetPortVec3(pActInfo, 0);
			ActivateOutput(pActInfo, 0, v.x);
			ActivateOutput(pActInfo, 1, v.y);
			ActivateOutput(pActInfo, 2, v.z);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};
//////////////////////////////////////////////////////////////////////////
class CFlowNode_SinCos : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_SinCos(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("in"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("sin"),
			OutputPortConfig<float>("cos"),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			float x = GetPortFloat(pActInfo, 0);
			ActivateOutput(pActInfo, 0, sinf(x));
			ActivateOutput(pActInfo, 1, cosf(x));
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_AnglesToDir : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_AnglesToDir(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("angles", _HELP("Input angles.")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("dir",   _HELP("Output direction vector.")),
			OutputPortConfig<float>("roll", _HELP("Output roll.")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			{
				Ang3 angles(DEG2RAD(GetPortVec3(pActInfo, 0)));
				Vec3 dir(Matrix33::CreateRotationXYZ(angles).GetColumn(1));

				ActivateOutput(pActInfo, 0, dir);
				ActivateOutput(pActInfo, 1, angles.y);
			}
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};
//////////////////////////////////////////////////////////////////////////
class CFlowNode_DirToAngles : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_DirToAngles(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("dir",   _HELP("Input direction.")),
			InputPortConfig<float>("roll", _HELP("Input roll.")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("angles", "Output angles."),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			{
				Vec3 dir(GetPortVec3(pActInfo, 0));
				float roll(DEG2RAD(GetPortFloat(pActInfo, 1)));
				Ang3 angles(Ang3::GetAnglesXYZ(Matrix33::CreateOrientation(dir, Vec3(0, 0, 1), roll)));
				ActivateOutput(pActInfo, 0, RAD2DEG(Vec3(angles)));
			}
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_SetNumber : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_SetNumber(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("set",  _HELP("Send value to output when receiving event on this port")),
			InputPortConfig<float>("in", _HELP("Value to be set on output")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out"),
			{ 0 }
		};
		config.sDescription = _HELP("Send a float input value to the output when event on set port is received");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED); // POLICY-CHANGE: don't send to output on initialize
	}
	virtual void ProcessEvent(EFlowEvent evt, SActivationInfo* pActInfo)
	{
		switch (evt)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				ActivateOutput(pActInfo, 0, GetPortFloat(pActInfo, 1));
			}
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_SetInteger : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_SetInteger(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("set", _HELP("Send value to output when receiving event on this port")),
			InputPortConfig<int>("in",  _HELP("Value to be set on output")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("out"),
			{ 0 }
		};
		config.sDescription = _HELP("Send an integer input value to the output when event on set port is received");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED); // POLICY-CHANGE: don't send to output on initialize
	}
	virtual void ProcessEvent(EFlowEvent evt, SActivationInfo* pActInfo)
	{
		switch (evt)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				ActivateOutput(pActInfo, 0, GetPortInt(pActInfo, 1));
			}
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Sin : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Sin(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("in", _HELP("out=sinus(in) in degrees")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out", _HELP("out=sinus(in) in degrees")),
			{ 0 }
		};
		config.sDescription = _HELP("Outputs sinus of the input in degrees");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			{
				float a = GetPortFloat(pActInfo, 0);
				float out = sinf(DEG2RAD(a));
				ActivateOutput(pActInfo, 0, out);
			}
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};
//////////////////////////////////////////////////////////////////////////
class CFlowNode_Cos : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Cos(SActivationInfo* pActInfo) {}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("in", _HELP("out=cosinus(in) in degrees")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out", _HELP("out=cosinus(in) in degrees")),
			{ 0 }
		};
		config.sDescription = _HELP("Outputs cosinus of the input in degrees");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			{
				float a = GetPortFloat(pActInfo, 0);
				float out = cos(DEG2RAD(a));
				ActivateOutput(pActInfo, 0, out);
			}
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};
//////////////////////////////////////////////////////////////////////////
class CFlowNode_Tan : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Tan(SActivationInfo* pActInfo) {}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("in", _HELP("out=tangent(in) in degrees")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out", _HELP("out=tangent(in) in degrees")),
			{ 0 }
		};
		config.sDescription = _HELP("Outputs tangent of the input in degrees");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			{
				float a = GetPortFloat(pActInfo, 0);
				float out = tanf(DEG2RAD(a));
				ActivateOutput(pActInfo, 0, out);
			}
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_SinInverse : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_SinInverse(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("in", _HELP("out=sinus_inverse(in) in degrees")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out", _HELP("out=sinus_inverse(in) in degrees")),
			{ 0 }
		};
		config.sDescription = _HELP("Outputs sinus inverse (arcsin) of the input in degrees");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			{
				float a = clamp_tpl(GetPortFloat(pActInfo, 0), -1.0f, 1.0f);
				float out = RAD2DEG(asinf(a));
				ActivateOutput(pActInfo, 0, out);
			}
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};
//////////////////////////////////////////////////////////////////////////
class CFlowNode_CosInverse : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_CosInverse(SActivationInfo* pActInfo) {}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("in", _HELP("out=cosinus(in) in degrees")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out", _HELP("out=cosinus(in) in degrees")),
			{ 0 }
		};
		config.sDescription = _HELP("Outputs cosinus inverse (arccos) of the input in degrees");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			{
				float a = clamp_tpl(GetPortFloat(pActInfo, 0), -1.0f, 1.0f);
				float out = RAD2DEG(acosf(a));
				ActivateOutput(pActInfo, 0, out);
			}
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};
//////////////////////////////////////////////////////////////////////////
class CFlowNode_TanInverse : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_TanInverse(SActivationInfo* pActInfo) {}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("in", _HELP("out=tangent_inverse(in) in degrees")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out", _HELP("out=tangent_inverse(in) in degrees")),
			{ 0 }
		};
		config.sDescription = _HELP("Outputs tangent inverse (arctan) of the input in degrees");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			{
				float a = GetPortFloat(pActInfo, 0);
				float out = RAD2DEG(atanf(a));
				ActivateOutput(pActInfo, 0, out);
			}
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_UpDownCounter : public CFlowBaseNode<eNCT_Instanced>
{
	int m_counter;
	int m_highLimit;
	int m_lowLimit;

public:
	CFlowNode_UpDownCounter(SActivationInfo* pActInfo)
	{
		m_counter = 0;
		m_highLimit = 10;
		m_lowLimit = 0;
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_UpDownCounter(pActInfo); }

	virtual void         Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("m_counter", m_counter);
		ser.Value("m_lowLimit", m_lowLimit);
		ser.Value("m_highLimit", m_highLimit);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<int>("Preset",    _HELP("Preset value")),
			InputPortConfig<int>("HighLimit", _HELP("Upper limit")),
			InputPortConfig<int>("LowLimit",  _HELP("Low limit")),
			InputPortConfig<bool>("Dec",      _HELP("Decrement")),
			InputPortConfig<bool>("Inc",      _HELP("Increment")),
			InputPortConfig<bool>("Wrap",     _HELP("The counter will wrap if TRUE")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out", _HELP("counter output")),
			{ 0 }
		};
		config.sDescription = _HELP("Implements an up/down counter");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_ADVANCED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			{
				if (IsPortActive(pActInfo, 0) || event == eFE_Initialize)
				{
					m_counter = GetPortInt(pActInfo, 0);
				}
				else
				{
					if (IsPortActive(pActInfo, 3))
						m_counter--;
					if (IsPortActive(pActInfo, 4))
						m_counter++;
				}
				if (IsPortActive(pActInfo, 1))
					m_highLimit = GetPortInt(pActInfo, 1);
				if (IsPortActive(pActInfo, 2))
					m_lowLimit = GetPortInt(pActInfo, 2);
				if (m_counter > m_highLimit)
				{
					if (GetPortBool(pActInfo, 5) == true)
						m_counter = m_lowLimit;
					else
						m_counter = m_highLimit;
				}
				if (m_counter < m_lowLimit)
				{
					if (GetPortBool(pActInfo, 5) == true)
						m_counter = m_highLimit;
					else
						m_counter = m_lowLimit;
				}
				ActivateOutput(pActInfo, 0, m_counter);
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_TrigArcSin : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInputPorts
	{
		EIP_In,
	};

	enum EOutputPorts
	{
		EOP_Out,
	};

public:
	CFlowNode_TrigArcSin(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<float>("in", 0.0f, _HELP("out = arcsinus(in) in degrees")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<float>("out", _HELP("out = arcsinus(in) in degrees")),
			{ 0 }
		};

		// Fill in configuration
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Outputs arcsinus of the input in degrees");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				// Check which port was activated
				if (IsPortActive(pActInfo, EIP_In))
				{
					float answer = asin_tpl(DEG2RAD(GetPortFloat(pActInfo, EIP_In)));
					if (_isnan(answer)) answer = 0.0f;
					ActivateOutput(pActInfo, EOP_Out, RAD2DEG(answer));
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

////////////////////////////////////////////////////
class CFlowNode_TrigArcCos : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInputPorts
	{
		EIP_In,
	};

	enum EOutputPorts
	{
		EOP_Out,
	};

public:
	CFlowNode_TrigArcCos(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<float>("in", 0.0f, _HELP("out = arccosinus(in) in degrees")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<float>("out", _HELP("out = arccosinus(in) in degrees")),
			{ 0 }
		};

		// Fill in configuration
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Outputs arccosinus of the input in degrees");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				// Check which port was activated
				if (IsPortActive(pActInfo, EIP_In))
				{
					float answer = acos_tpl(DEG2RAD(GetPortFloat(pActInfo, EIP_In)));
					if (_isnan(answer)) answer = 0.0f;
					ActivateOutput(pActInfo, EOP_Out, RAD2DEG(answer));
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

////////////////////////////////////////////////////
class CFlowNode_TrigArcTan : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInputPorts
	{
		EIP_In,
	};

	enum EOutputPorts
	{
		EOP_Out,
	};

public:
	CFlowNode_TrigArcTan(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<float>("in", 0.0f, _HELP("out = arctangens(in) in degrees")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<float>("out", _HELP("out = arctangens(in) in degrees")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Outputs arctangens of the input in degrees");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				// Check which port was activated
				if (IsPortActive(pActInfo, EIP_In))
				{
					float answer = atan_tpl(DEG2RAD(GetPortFloat(pActInfo, EIP_In)));
					if (_isnan(answer)) answer = 0.0f;
					ActivateOutput(pActInfo, EOP_Out, RAD2DEG(answer));
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

////////////////////////////////////////////////////
class CFlowNode_Ceil : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInputPorts
	{
		EIP_In,
	};

	enum EOutputPorts
	{
		EOP_Out,
	};

public:
	CFlowNode_Ceil(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<float>("A", 0.0f, _HELP("out = ceil(A)")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<float>("out", _HELP("out = ceil(A)")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Outputs ceiling of A");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				// Check which port was activated
				if (IsPortActive(pActInfo, EIP_In))
				{
					float answer = ceil_tpl(GetPortFloat(pActInfo, EIP_In));
					ActivateOutput(pActInfo, EOP_Out, answer);
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

////////////////////////////////////////////////////
class CFlowNode_Floor : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInputPorts
	{
		EIP_In,
	};

	enum EOutputPorts
	{
		EOP_Out,
	};

public:
	CFlowNode_Floor(SActivationInfo* pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<float>("A", 0.0f, _HELP("out = floor(A)")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<float>("out", _HELP("out = floor(A)")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Outputs floor of A");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				// Check which port was activated
				if (IsPortActive(pActInfo, EIP_In))
				{
					float answer = floor_tpl(GetPortFloat(pActInfo, EIP_In));
					ActivateOutput(pActInfo, EOP_Out, answer);
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

class CFlowNode_Modulo : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Modulo(SActivationInfo* pActInfo) {};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("A"),
			InputPortConfig<float>("B"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out"),
			{ 0 }
		};
		config.sDescription = _HELP("[out] = [A] % [B]");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			float a = GetPortFloat(pActInfo, 0);
			float b = GetPortFloat(pActInfo, 1);
			float out = fmod_tpl(a, b);
			ActivateOutput(pActInfo, 0, out);
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_Wrap : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Wrap(SActivationInfo* pActInfo) {};

	enum EInputPorts
	{
		eIP_Set = 0,
		eIP_Min,
		eIP_Max,
		eIP_Value,
	};

	enum EOutputPorts
	{
		eOP_Out = 0,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Set"),
			InputPortConfig<float>("Min"),
			InputPortConfig<float>("Max"),
			InputPortConfig<float>("Value"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out"),
			{ 0 }
		};
		config.sDescription = _HELP("Wraps Value around the interval defined by Min and Max");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, eIP_Set))
			{
				const float minVal = GetPortFloat(pActInfo, eIP_Min);
				const float maxVal = GetPortFloat(pActInfo, eIP_Max);

				if (minVal <= maxVal)
				{
					const float inputVal = GetPortFloat(pActInfo, eIP_Value);

					// Move to a [0 ; X] interval
					const float dividend = (inputVal - minVal);
					const float divisor = (maxVal - minVal);

					// Get the remainder
					float remainder = std::remainder(dividend, divisor);
					if (remainder < 0.0f)
					{
						// the remainder function is a bit awkward as it will give a result in the interval [-max/2; max/2] :
						// remainder( 5.1, 3 ) => -0.9
						// Though, it behaves better than fmod regarding negative numbers
						remainder += divisor;
					}

					// Move the value back to the original interval
					const float wrappedVal = remainder + minVal;

					ActivateOutput(pActInfo, eOP_Out, wrappedVal);
				}
				else
				{
					GameWarning("Math:Wrap FG Node has an invalid range : [min=%.2f, max=%.2f]", minVal, maxVal);
				}
			}
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("Math:Round", CFlowNode_Round);
REGISTER_FLOW_NODE("Math:Add", CFlowNode_Add);
REGISTER_FLOW_NODE("Math:Sub", CFlowNode_Sub);
REGISTER_FLOW_NODE("Math:Mul", CFlowNode_Mul);
REGISTER_FLOW_NODE("Math:Div", CFlowNode_Div)
REGISTER_FLOW_NODE("Math:Equal", CFlowNode_Equal)
REGISTER_FLOW_NODE("Math:EqualCheck", CFlowNode_EqualCheck)
REGISTER_FLOW_NODE("Math:InRange", CFlowNode_InRange)
REGISTER_FLOW_NODE("Math:Less", CFlowNode_Less)
REGISTER_FLOW_NODE("Math:LessCheck", CFlowNode_LessCheck)
REGISTER_FLOW_NODE("Math:Counter", CFlowNode_Counter)
REGISTER_FLOW_NODE("Math:PortCounter", CFlowNode_PortCounter);
REGISTER_FLOW_NODE("Math:Remainder", CFlowNode_Remainder)
REGISTER_FLOW_NODE("Math:Reciprocal", CFlowNode_Reciprocal);
REGISTER_FLOW_NODE("Math:Random", CFlowNode_Random);
REGISTER_FLOW_NODE("Math:Power", CFlowNode_Power);
REGISTER_FLOW_NODE("Math:Sqrt", CFlowNode_Sqrt);
REGISTER_FLOW_NODE("Math:Abs", CFlowNode_Abs);
REGISTER_FLOW_NODE("Math:Clamp", CFlowNode_Clamp);
REGISTER_FLOW_NODE("Math:SinCos", CFlowNode_SinCos);
REGISTER_FLOW_NODE("Math:AnglesToDir", CFlowNode_AnglesToDir);
REGISTER_FLOW_NODE("Math:DirToAngles", CFlowNode_DirToAngles);
REGISTER_FLOW_NODE("Math:SetNumber", CFlowNode_SetNumber);
REGISTER_FLOW_NODE("Math:SetInteger", CFlowNode_SetInteger);
REGISTER_FLOW_NODE("Math:BooleanTo", CFlowNode_ToBoolean);
REGISTER_FLOW_NODE("Math:BooleanFrom", CFlowNode_FromBoolean);
REGISTER_FLOW_NODE("Math:UpDownCounter", CFlowNode_UpDownCounter);
REGISTER_FLOW_NODE("Math:SetColor", CFlowNode_SetColor);
REGISTER_FLOW_NODE("Math:Sinus", CFlowNode_Sin);
REGISTER_FLOW_NODE("Math:Cosinus", CFlowNode_Cos);
REGISTER_FLOW_NODE("Math:Tangent", CFlowNode_Tan);
REGISTER_FLOW_NODE("Math:SinusInverse", CFlowNode_SinInverse);
REGISTER_FLOW_NODE("Math:CosinusInverse", CFlowNode_CosInverse);
REGISTER_FLOW_NODE("Math:TangentInverse", CFlowNode_TanInverse);
REGISTER_FLOW_NODE("Math:Calculate", CFlowNode_Calculate);
REGISTER_FLOW_NODE("Debug:FloatToString", CFlowNode_FloatToString);
REGISTER_FLOW_NODE("Math:ArcSinus", CFlowNode_TrigArcSin);
REGISTER_FLOW_NODE("Math:ArcCosinus", CFlowNode_TrigArcCos);
REGISTER_FLOW_NODE("Math:ArcTangens", CFlowNode_TrigArcTan);
REGISTER_FLOW_NODE("Math:Ceil", CFlowNode_Ceil);
REGISTER_FLOW_NODE("Math:Floor", CFlowNode_Floor);
REGISTER_FLOW_NODE("Math:Mod", CFlowNode_Modulo);
REGISTER_FLOW_NODE("Math:Wrap", CFlowNode_Wrap);

REGISTER_FLOW_NODE("Vec3:AddVec3", CFlowNode_AddVec3);
REGISTER_FLOW_NODE("Vec3:SubVec3", CFlowNode_SubVec3);
REGISTER_FLOW_NODE("Vec3:MulVec3", CFlowNode_MulVec3);
REGISTER_FLOW_NODE("Vec3:CrossVec3", CFlowNode_CrossVec3);
REGISTER_FLOW_NODE("Vec3:DotVec3", CFlowNode_DotVec3);
REGISTER_FLOW_NODE("Vec3:NormalizeVec3", CFlowNode_NormalizeVec3);
REGISTER_FLOW_NODE("Vec3:ScaleVec3", CFlowNode_ScaleVec3);
REGISTER_FLOW_NODE("Vec3:EqualVec3", CFlowNode_EqualVec3);
REGISTER_FLOW_NODE("Vec3:MagnitudeVec3", CFlowNode_MagnitudeVec3);
REGISTER_FLOW_NODE("Vec3:ToVec3", CFlowNode_ToVec3)
REGISTER_FLOW_NODE("Vec3:FromVec3", CFlowNode_FromVec3)
REGISTER_FLOW_NODE("Vec3:ReciprocalVec3", CFlowNode_ReciprocalVec3);
REGISTER_FLOW_NODE("Vec3:ClampVec3", CFlowNode_ClampVec3);
REGISTER_FLOW_NODE("Vec3:SetVec3", CFlowNode_SetVec3);
REGISTER_FLOW_NODE("Vec3:Calculate", CFlowNode_CalculateVec3);
