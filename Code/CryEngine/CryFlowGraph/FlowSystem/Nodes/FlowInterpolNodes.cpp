// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowInterpolNodes.cpp
//  Version:     v1.00
//  Created:     27/4/2006 by AlexL.
//  Compilers:   Visual Studio.NET 2005
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

template<typename T> struct Limits
{
	static const char* min_name;
	static const char* max_name;
	static const T     min_val;
	static const T     max_val;
};

template<typename T> struct LimitsColor
{
	static const char* min_name;
	static const char* max_name;
	static const T     min_val;
	static const T     max_val;
};

template<typename T> const char* Limits<T>::min_name = "StartValue";
template<typename T> const char* Limits<T>::max_name = "EndValue";

template<> const float Limits<float>::min_val = 0.0f;
template<> const float Limits<float>::max_val = 1.0f;
template<> const int Limits<int>::min_val = 0;
template<> const int Limits<int>::max_val = 255;
template<> const Vec3 Limits<Vec3>::min_val = Vec3(0.0f, 0.0f, 0.0f);
template<> const Vec3 Limits<Vec3>::max_val = Vec3(1.0f, 1.0f, 1.0f);
template<> const Vec3 LimitsColor<Vec3>::min_val = Vec3(0.0f, 0.0f, 0.0f);
template<> const Vec3 LimitsColor<Vec3>::max_val = Vec3(1.0f, 1.0f, 1.0f);
template<> const char* LimitsColor<Vec3>::min_name = "color_StartValue";
template<> const char* LimitsColor<Vec3>::max_name = "color_EndValue";

namespace Easing
{
float Linear(float ratio)
{
	return ratio;
}

float QuadIn(float ratio)
{
	return pow(ratio, 2);
}

float QuadOut(float ratio)
{
	return 1 - QuadIn(1 - ratio);
}

float QuadInOut(float ratio)
{
	if (ratio < 0.5f)
	{
		return 2.0f * QuadIn(ratio);
	}
	else
	{
		return -1.0f + (4.0f - 2.0f * ratio) * ratio;
	}
}

float CubicIn(float ratio)
{
	return pow(ratio, 3);
}

float CubicOut(float ratio)
{
	return 1.0f - CubicIn(1.0f - ratio);
}

float CubicInOut(float ratio)
{
	if (ratio < .5f)
	{
		return 4.0f * CubicIn(ratio);
	}
	else
	{
		float factor = (2.0f * ratio - 2.0f);
		return (ratio - 1.0f) * pow(factor, 2) + 1.0f;
	}
}

float QuartIn(float ratio)
{
	return pow(ratio, 4);
}

float QuartOut(float ratio)
{
	return 1.0f - QuartIn(1.0f - ratio);
}

float QuartInOut(float ratio)
{
	if (ratio < .5f)
	{
		return 8.0f * QuartIn(ratio);
	}
	else
	{
		return 1.0f - 8.0f * QuartIn(--ratio);
	}
}

float QuintIn(float ratio)
{
	return pow(ratio, 5);
}

float QuintOut(float ratio)
{
	return 1.0f - QuintIn(1.0f - ratio);
}

float QuintInOut(float ratio)
{
	if (ratio < .5f)
	{
		return 16.0f * QuintIn(ratio);
	}
	else
	{
		return 1.0f + 16.0f * QuintIn(--ratio);
	}
}

float CircIn(float ratio)
{
	return 1.0f - (sqrt(1.0f - QuadIn(ratio)));
}

float CircOut(float ratio)
{
	return 1.0f - CircIn(1.0f - ratio);
}

float CircInOut(float ratio)
{
	ratio *= 2.0f;
	if (ratio < 1.0f)
	{
		return -.5f * (sqrt(1.0f - QuadIn(ratio)) - 1.0f);
	}
	else
	{
		return .5f * (sqrt(1.0f - QuadIn(ratio - 2.0f)) + 1.0f);
	}
}

float BackIn(float ratio)
{
	const float overshoot = 1.70158f;
	return QuadIn(ratio) * ((overshoot + 1.0f) * ratio - overshoot);
}

float BackOut(float ratio)
{
	return 1.0f - BackIn(1.0f - ratio);
}

float BackInOut(float ratio)
{
	if (ratio <= 0.5f)
	{
		return BackIn(ratio * 2.0f) / 2;
	}
	else
	{
		return BackOut(ratio * 2.0f - 1.0f) / 2.0f + 0.5f;
	}
}

float ElasticIn(float ratio)
{
	--ratio;
	const float period = 0.071999f;
	const float amplitude = -pow(2.0f, 10.0f * ratio);
	const float frequency = 20.943951f;
	return amplitude * sin((ratio - period) * frequency);
}

float ElasticOut(float ratio)
{
	const float period = 0.030396f;
	const float amplitude = pow(2.0f, -10.0f * ratio);
	const float frequency = 20.943951f;
	return amplitude * sin((ratio - period) * frequency) + 1.0f;
}

float ElasticInOut(float ratio)
{
	ratio *= 2.0f;
	--ratio;
	const float period = .1125f;
	const float frequency = 13.962634f;
	const float sinVal = sin((ratio - period) * frequency);
	if (ratio < 0.0f)
	{
		const float amplitude = pow(2.0f, 10.0f * ratio);
		return -.5f * amplitude * sinVal;
	}
	else
	{
		const float amplitude = pow(2.0f, -10.0f * ratio);
		return .5f * amplitude * sinVal + 1.0f;
	}
}

float BounceOut(float ratio)
{
	const float length = 2.75f;
	const float touchdown[3] = { 1.0f / length, 2.0f / length, 2.5f / length };
	const float amplitude = pow(length, 2);
	const float offsets[3] = { 1.5f / length, 2.25f / length, 2.625f / length };

	if (ratio < touchdown[0])
	{
		return amplitude * QuadIn(ratio);
	}
	else if (ratio < touchdown[1])
	{
		return amplitude * QuadIn(ratio - offsets[0]) + .75f;
	}
	else if (ratio < touchdown[2])
	{
		return amplitude * QuadIn(ratio - offsets[1]) + .9375f;
	}
	else
	{
		return amplitude * QuadIn(ratio - offsets[2]) + .984375f;
	}
}

float BounceIn(float ratio)
{
	return 1.0f - BounceOut(1.0f - ratio);
}

float BounceInOut(float ratio)
{
	if (ratio < .5f)
	{
		return BounceIn(ratio * 2.0f) * .5f;
	}
	else
	{
		return BounceOut(ratio * 2.0f - 1.0f) * .5f + 0.5f;
	}
}

float SlowMotion(float ratio)
{
	float result = ratio + (.5f - ratio) * .7f;
	if (ratio < .15f)
	{
		ratio = 1.0f - (ratio * 6.66f);
		return result - ratio * CubicIn(ratio) * result;
	}
	else if (ratio > .85f)
	{
		float factor = (ratio - .85f) * 6.66f;
		return result + (ratio - result) * factor * CubicIn(factor);
	}
	return result;
}

#define EasingFunctionsList(func)                                  \
  func(EEasingLinear, "Linear", &Easing::Linear)                   \
  func(EEasingQuadIn, "QuadIn", &Easing::QuadIn)                   \
  func(EEasingQuadOut, "QuadOut", &Easing::QuadOut)                \
  func(EEasingQuadInOut, "QuadInOut", &Easing::QuadInOut)          \
  func(EEasingCubicIn, "CubicIn", &Easing::CubicIn)                \
  func(EEasingCubicOut, "CubicOut", &Easing::CubicOut)             \
  func(EEasingCubicInOut, "CubicInOut", &Easing::CubicInOut)       \
  func(EEasinQuartIn, "QuartIn", &Easing::QuartIn)                 \
  func(EEasinQuartOut, "QuartOut", &Easing::QuartOut)              \
  func(EEasinQuartInOut, "QuartInOut", &Easing::QuartInOut)        \
  func(EEasinQuintIn, "QuintIn", &Easing::QuintIn)                 \
  func(EEasinQuintOut, "QuintOut", &Easing::QuintOut)              \
  func(EEasinQuintInOut, "QuintInOut", &Easing::QuintInOut)        \
  func(EEasinCircIn, "CircIn", &Easing::CircIn)                    \
  func(EEasinCircOut, "CircOut", &Easing::CircOut)                 \
  func(EEasinCircInOut, "CircInOut", &Easing::CircInOut)           \
  func(EEasingBackIn, "BackIn", &Easing::BackIn)                   \
  func(EEasingBackOut, "BackOut", &Easing::BackOut)                \
  func(EEasingBackInOut, "BackInOut", &Easing::BackInOut)          \
  func(EEasingElasticIn, "ElasticIn", &Easing::ElasticIn)          \
  func(EEasingElasticOut, "ElasticOut", &Easing::ElasticOut)       \
  func(EEasingElasticInOut, "ElasticInOut", &Easing::ElasticInOut) \
  func(EEasingBounceIn, "BounceIn", &Easing::BounceIn)             \
  func(EEasingBounceOut, "BounceOut", &Easing::BounceOut)          \
  func(EEasingBounceInOut, "BounceInOut", &Easing::BounceInOut)    \
  func(EEasingSlowMotion, "SlowMotion", &Easing::SlowMotion)       \

enum EEasingFunction
{
#define BUILD_EASING_ENUM(enumValue, ...) enumValue,
	EasingFunctionsList(BUILD_EASING_ENUM)
#undef BUILD_EASING_ENUM
	COUNT
};

typedef float (* TEasingFunctionPtr)(float);
TEasingFunctionPtr g_easingFunctions[] =
{
#define BUILD_EASING_ENUM_FUNCTION_ARRAY(enumValue, enumName, enumFunctionPtr) enumFunctionPtr,
	EasingFunctionsList(BUILD_EASING_ENUM_FUNCTION_ARRAY)
#undef BUILD_EASING_ENUM_FUNCTION_ARRAY
};

TEasingFunctionPtr GetFunctionByIndex(uint index)
{
	return g_easingFunctions[(index < EEasingFunction::COUNT) ? index : 0];
}

string GetDropdownString()
{
	static string easingFunctionEnumString;
	if (easingFunctionEnumString.empty())
	{
		string temp;
		easingFunctionEnumString = "enum_int:";
#define BUILD_EASING_UI_ENUMS(enumValue, enumName, ...) \
  temp.Format(",%02d:%s=%d", static_cast<int>(enumValue), enumName, static_cast<int>(enumValue)); \
		easingFunctionEnumString.append(temp);
		EasingFunctionsList(BUILD_EASING_UI_ENUMS);
#undef BUILD_EASING_UI_ENUMS
	}
	return easingFunctionEnumString;
}
}

//////////////////////////////////////////////////////////////////////////
template<class T, class L = Limits<T>>
class CFlowInterpolNode : public CFlowBaseNode<eNCT_Instanced>
{
	typedef CFlowInterpolNode<T, L> Self;
public:
	enum EInputs
	{
		EIP_Start,
		EIP_Stop,
		EIP_StartValue,
		EIP_EndValue,
		EIP_Time,
		EIP_UpdateFrequency,
		EIP_EasingFunction
	};
	enum EOutputs
	{
		EOP_Value,
		EOP_Done
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	CFlowInterpolNode(SActivationInfo* pActInfo)
		: m_startTime(0.0f),
		m_endTime(0.0f),
		m_startValue(L::min_val),
		m_endValue(L::min_val),
		m_updateFrequency(0.0f),
		m_lastUpdateTime(0.0f),
		m_easingFunction(nullptr)
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new Self(pActInfo);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");
		ser.Value("m_startTime", m_startTime);
		ser.Value("m_endTime", m_endTime);
		ser.Value("m_startValue", m_startValue);
		ser.Value("m_endValue", m_endValue);
		ser.Value("m_updateFrequency", m_updateFrequency);
		ser.Value("m_lastUpdateTime", m_lastUpdateTime);
		ser.EndGroup();
		// regular update is handled by FlowGraph serialization
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Start",                         _HELP("StartInterpol")),
			InputPortConfig_Void("Stop",                          _HELP("StopInterpol")),
			InputPortConfig<T>(L::min_name,           L::min_val, _HELP("Start value")),
			InputPortConfig<T>(L::max_name,           L::max_val, _HELP("End value")),
			InputPortConfig<float>("Time",            1.0f,       _HELP("Time in seconds")),
			InputPortConfig<float>("UpdateFrequency", 0.0f,       _HELP("Update frequency in seconds (0 = every frame)")),
			InputPortConfig<int>("Easing",                        _HELP("Easing-Function that is applied to the interpolation"),0,                                                       _UICONFIG(Easing::GetDropdownString().c_str())),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<T>("Value",  _HELP("Current Value")),
			OutputPortConfig_Void("Done", _HELP("Triggered when finished")),
			{ 0 }
		};

		config.sDescription = _HELP("Interpol nodes can be used to interpolate from an initial value to an end value based on a easing-function within a given time frame.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	bool GetValue(SActivationInfo* pActInfo, int nPort, T& value)
	{
		T* pVal = (pActInfo->pInputPorts[nPort].GetPtr<T>());
		if (pVal)
		{
			value = *pVal;
			return true;
		}
		return false;
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Stop))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					ActivateOutput(pActInfo, EOP_Done, true);
				}
				if (IsPortActive(pActInfo, EIP_Start))
				{
					m_startTime = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();
					m_endTime = m_startTime + GetPortFloat(pActInfo, EIP_Time) * 1000.0f;
					m_easingFunction = Easing::GetFunctionByIndex(GetPortInt(pActInfo, EIP_EasingFunction));
					GetValue(pActInfo, EIP_StartValue, m_startValue);
					GetValue(pActInfo, EIP_EndValue, m_endValue);
					m_updateFrequency = GetPortFloat(pActInfo, EIP_UpdateFrequency) * 1000.0f;
					m_lastUpdateTime = m_startTime;
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
					Interpol(pActInfo, 0.0f);
				}
			}
			break;
		case eFE_Update:
			{
				const float fTime = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();
				if (fTime >= m_endTime)
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					ActivateOutput(pActInfo, EOP_Done, true);
					Interpol(pActInfo, 1.0f);
					m_lastUpdateTime = fTime;
				}
				else if ((fTime - m_lastUpdateTime) >= m_updateFrequency)
				{
					const float fDuration = m_endTime - m_startTime;
					float fPosition;
					if (fDuration <= 0.0f)
						fPosition = 1.0f;
					else
					{
						fPosition = (fTime - m_startTime) / fDuration;
						fPosition = clamp_tpl(fPosition, 0.0f, 1.0f);
					}
					Interpol(pActInfo, fPosition);
					m_lastUpdateTime = fTime;
				}
			}
			break;
		}
	}

protected:
	void Interpol(SActivationInfo* pActInfo, const float fPosition)
	{
		T value = ::Lerp(m_startValue, m_endValue, m_easingFunction(fPosition));
		ActivateOutput(pActInfo, EOP_Value, value);
	}

	float m_startTime;
	float m_endTime;
	T m_startValue;
	T m_endValue;
	float m_updateFrequency, m_lastUpdateTime;
	Easing::TEasingFunctionPtr m_easingFunction;
};

namespace FlowInterpolNodes
{
template<class T>
bool valueReached(const T& currVal, const T& toVal, const T& minDistToVal)
{
	return currVal == toVal;
}

template<>
bool valueReached(const float& currVal, const float& toVal, const float& minDistToVal)
{
	return fabsf(currVal - toVal) <= minDistToVal;
}

template<>
bool valueReached(const Vec3& currVal, const Vec3& toVal, const Vec3& minDistToVal)
{
	bool bdone = fabsf(currVal.x - toVal.x) <= minDistToVal.x &&
	             fabsf(currVal.y - toVal.y) <= minDistToVal.y &&
	             fabsf(currVal.z - toVal.z) <= minDistToVal.z;
	return bdone;
}

template<class T>
T CalcMinDistToVal(const T& startVal, const T& toVal)
{
	return 0;
}

template<>
float CalcMinDistToVal(const float& startVal, const float& toVal)
{
	const float FACTOR = (1 / 100.f);
	const float MIN_DIST = 0.01f;
	return max(fabsf(toVal - startVal) * FACTOR, MIN_DIST);
}

template<>
Vec3 CalcMinDistToVal(const Vec3& startVal, const Vec3& toVal)
{
	const float FACTOR = (1 / 100.f);
	const float MIN_DIST = 0.01f;
	Vec3 calc(max(fabsf(toVal.x - startVal.x) * FACTOR, MIN_DIST),
	          max(fabsf(toVal.y - startVal.y) * FACTOR, MIN_DIST),
	          max(fabsf(toVal.z - startVal.z) * FACTOR, MIN_DIST)
	          );
	return calc;
}
}

//////////////////////////////////////////////////////////////////////////
template<class T, class L = Limits<T>>
class CFlowSmoothNode : public CFlowBaseNode<eNCT_Instanced>
{
	typedef CFlowSmoothNode<T, L> Self;
public:
	enum EInputs
	{
		EIP_InitialValue,
		EIP_Value,
		EIP_Time
	};
	enum EOutputs
	{
		EOP_Value,
		EOP_Done
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	CFlowSmoothNode(SActivationInfo* pActInfo)
		: m_val(L::min_val),
		m_valRate(L::min_val),
		m_minDistToVal(L::min_val)
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new Self(pActInfo);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");
		ser.Value("m_val", m_val);
		ser.Value("m_valRate", m_valRate);
		ser.Value("m_minDistToVal", m_minDistToVal);
		ser.EndGroup();
		// regular update is handled by FlowGraph serialization
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<T>("InitialValue", L::min_val, _HELP("Initial value")),
			InputPortConfig<T>("Value",        L::min_val, _HELP("Target value")),
			InputPortConfig<float>("Time",     1.0f,       _HELP("Smoothing time (seconds)")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<T>("Value",  _HELP("Current Value")),
			OutputPortConfig_Void("Done", _HELP("Triggered when finished")),
			{ 0 }
		};

		config.sDescription = _HELP("Smooth nodes can be used to interpolate in a non-linear way (damped spring system) from an initial value to an end value within a given time frame. Calculation will slow as it reaches the end value.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	bool GetValue(SActivationInfo* pActInfo, int nPort, T& value)
	{
		T* pVal = (pActInfo->pInputPorts[nPort].GetPtr<T>());
		if (pVal)
		{
			value = *pVal;
			return true;
		}
		return false;
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				GetValue(pActInfo, EIP_InitialValue, m_val);
				m_valRate = L::min_val;
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				break;
			}

		case eFE_Activate:
			{
				T toValue;
				GetValue(pActInfo, EIP_Value, toValue);
				if (IsPortActive(pActInfo, EIP_Value))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
					m_minDistToVal = FlowInterpolNodes::CalcMinDistToVal(m_val, toValue);
				}
				if (IsPortActive(pActInfo, EIP_InitialValue))
				{
					GetValue(pActInfo, EIP_InitialValue, m_val);
					m_minDistToVal = FlowInterpolNodes::CalcMinDistToVal(m_val, toValue);
				}
				break;
			}

		case eFE_Update:
			{
				T toValue;
				GetValue(pActInfo, EIP_Value, toValue);
				SmoothCD(m_val, m_valRate, gEnv->pTimer->GetFrameTime(), toValue, GetPortFloat(pActInfo, EIP_Time));
				if (FlowInterpolNodes::valueReached(m_val, toValue, m_minDistToVal))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					ActivateOutput(pActInfo, EOP_Done, true);
					m_val = toValue;
					m_valRate = L::min_val;
				}
				ActivateOutput(pActInfo, EOP_Value, m_val);
				break;
			}
		}
	}

	T m_val;
	T m_valRate;
	T m_minDistToVal;  // used to check when the value is so near that it must be considered reached
};

class CFlowNode_Easing : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		EIP_Ratio,
		EIP_EasingFunction
	};
	enum EOutputs
	{
		EOP_Result
	};

	CFlowNode_Easing(SActivationInfo* pActInfo)
	{
	}

	virtual ~CFlowNode_Easing()
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<float>("Ratio", _HELP("Current ratio (0-1)")),
			InputPortConfig<int>("Easing",  _HELP("Easing-Function that is applied to the interpolation."),0, _UICONFIG(Easing::GetDropdownString().c_str())),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<float>("Result", _HELP("Ratio with applied easing.")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("This node applies a transition-function to its input and returns its result.");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Ratio))
				{
					const Easing::TEasingFunctionPtr easing = Easing::GetFunctionByIndex(GetPortInt(pActInfo, EIP_EasingFunction));
					const float currentRatio = GetPortFloat(pActInfo, EIP_Ratio);
					ActivateOutput(pActInfo, EOP_Result, easing(currentRatio));
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

REGISTER_FLOW_NODE("Interpol:Easing", CFlowNode_Easing);

typedef CFlowInterpolNode<int>                     IntInterpol;
typedef CFlowInterpolNode<float>                   FloatInterpol;
typedef CFlowInterpolNode<Vec3>                    Vec3Interpol;
typedef CFlowInterpolNode<Vec3, LimitsColor<Vec3>> ColorInterpol;

REGISTER_FLOW_NODE("Interpol:Int", IntInterpol);
REGISTER_FLOW_NODE("Interpol:Float", FloatInterpol);
REGISTER_FLOW_NODE("Interpol:Vec3", Vec3Interpol);
REGISTER_FLOW_NODE("Interpol:Color", ColorInterpol);

typedef CFlowSmoothNode<int>   SmoothInt;
typedef CFlowSmoothNode<float> SmoothFloat;
typedef CFlowSmoothNode<Vec3>  SmoothVec3;
typedef CFlowSmoothNode<Vec3>  SmoothColor;

REGISTER_FLOW_NODE("Interpol:SmoothInt", SmoothInt);
REGISTER_FLOW_NODE("Interpol:SmoothFloat", SmoothFloat);
REGISTER_FLOW_NODE("Interpol:SmoothVec3", SmoothVec3);
REGISTER_FLOW_NODE("Interpol:SmoothColor", SmoothColor);
