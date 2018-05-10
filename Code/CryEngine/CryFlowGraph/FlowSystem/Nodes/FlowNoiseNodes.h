// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Set of Noise flow nodes based on Ken Perlin's "Improving Noise" article

   -------------------------------------------------------------------------
   History:
   - 21:06:2005   16:00 : Created by Marco Koegler

*************************************************************************/

#pragma once

#include <CryMath/PNoise3.h>
#include <CryFlowGraph/IFlowBaseNode.h>

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Noise1D : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Noise1D(SActivationInfo* pActInfo) {};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("x",         _HELP("value to sample noise at")),
			InputPortConfig<float>("frequency", 1.0f,                              _HELP("scale factor for input value")),
			InputPortConfig<float>("amplitude", 1.0f,                              _HELP("range of noise values")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out", _HELP("out = amp * Noise1D(frequency * x)")),
			{ 0 }
		};
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
			float x = GetPortFloat(pActInfo, 0);
			float freq = GetPortFloat(pActInfo, 1);
			float amp = GetPortFloat(pActInfo, 2);

			//float out = amp * m_noise.Noise1D(freq * x);
			float out = amp * gEnv->pSystem->GetNoiseGen()->Noise1D(freq * x);
			ActivateOutput(pActInfo, 0, out);
		}
		;
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
};

class CFlowNode_Noise3D : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Noise3D(SActivationInfo* pActInfo) {};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("v",          _HELP("position to sample noise at")),
			InputPortConfig<float>("frequency", 1.0f,                                 _HELP("scale factor for input vector")),
			InputPortConfig<float>("amplitude", 1.0f,                                 _HELP("range of noise values")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("out", _HELP("out = amp * Noise3D(frequency * v)")),
			{ 0 }
		};
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
			Vec3 v = GetPortVec3(pActInfo, 0);
			float freq = GetPortFloat(pActInfo, 1);
			float amp = GetPortFloat(pActInfo, 2);

			float out = amp * gEnv->pSystem->GetNoiseGen()->Noise3D(freq * v.x, freq * v.y, freq * v.z);
			ActivateOutput(pActInfo, 0, out);
		}
		;
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
};
