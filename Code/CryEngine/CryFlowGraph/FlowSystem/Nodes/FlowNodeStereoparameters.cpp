// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

//////////////////////////////////////////////////////////////////////////
class CFlowNode_ReadStereoParameters : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		EIP_Read,
		EIP_Stop,
	};

	enum EOutputs
	{
		EOP_CurrentEyeDistance,
		EOP_CurrentScreenDistance,
		EOP_CurrentHUDDistance,
		EOP_Flipped,
	};

	CFlowNode_ReadStereoParameters(SActivationInfo* activationInfo)
	{
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Read", _HELP("Starts reading stereo values")),
			InputPortConfig_Void("Stop", _HELP("Stops reading stereo values")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("EyeDistance",    _HELP("Current value of eye distance")),
			OutputPortConfig<float>("ScreenDistance", _HELP("Current value of screen distance")),
			OutputPortConfig<float>("HUDDistance",    _HELP("Current value of HUD distance")),
			OutputPortConfig<bool>("Flipped",         _HELP("Is stereo flipped?")),
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* activationInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			activationInfo->pGraph->SetRegularlyUpdated(activationInfo->myID, false);
			break;

		case eFE_Activate:
			if (IsPortActive(activationInfo, EIP_Read))
				activationInfo->pGraph->SetRegularlyUpdated(activationInfo->myID, true);
			if (IsPortActive(activationInfo, EIP_Stop))
				activationInfo->pGraph->SetRegularlyUpdated(activationInfo->myID, false);
			break;

		case eFE_Update:
			ActivateOutput(activationInfo, EOP_CurrentEyeDistance, gEnv->pConsole->GetCVar("r_StereoEyeDist")->GetFVal());
			ActivateOutput(activationInfo, EOP_CurrentScreenDistance, gEnv->pConsole->GetCVar("r_StereoScreenDist")->GetFVal());
			ActivateOutput(activationInfo, EOP_CurrentHUDDistance, gEnv->pConsole->GetCVar("r_StereoHudScreenDist")->GetFVal());
			ActivateOutput(activationInfo, EOP_Flipped, gEnv->pConsole->GetCVar("r_StereoFlipEyes")->GetIVal() != 0);

			bool test = gEnv->pConsole->GetCVar("r_StereoFlipEyes")->GetIVal() != 0;
			float v = gEnv->pConsole->GetCVar("r_StereoEyeDist")->GetFVal();
			int x = 0;
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_StereoParameters : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputs
	{
		EIP_EyeDistance,
		EIP_ScreenDistance,
		EIP_HUDDistance,
		EIP_Duration,
		EIP_Start,
	};
	enum EOutputs
	{
		EOP_CurrentEyeDistance,
		EOP_CurrentScreenDistance,
		EOP_CurrentHUDDistance,
		EOP_TimeLeft,
		EOP_Done,
	};

	CFlowNode_StereoParameters(SActivationInfo* activationInfo)
		: m_duration(0.0f)
		, m_startTime(0.0f)
	{
		m_startEyeDistance = m_targetEyeDistance = m_currentEyeDistance = gEnv->pConsole->GetCVar("r_StereoEyeDist")->GetFVal();
		m_startScreenDistance = m_targetScreenDistance = m_currentScreenDistance = gEnv->pConsole->GetCVar("r_StereoScreenDist")->GetFVal();
		m_startHUDDistance = m_targetHUDDistance = m_currentHUDDistance = gEnv->pConsole->GetCVar("r_StereoHudScreenDist")->GetFVal();
	};

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_StereoParameters(pActInfo); }

	virtual void         GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("EyeDistance",    _HELP("Set stereo eye distance")),
			InputPortConfig<float>("ScreenDistance", 1.0f,                             _HELP("Set stereo screen distance")),
			InputPortConfig<float>("HUDDistance",    1.0f,                             _HELP("Set stereo HUD distance")),
			InputPortConfig<float>("Duration",       1.0f,                             _HELP("Duration of the interpolation in seconds (0 = set immediately)")),
			InputPortConfig_Void("Start",            _HELP("Starts interpolation")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("CurrentEyeDistance",    _HELP("Current value of eye distance")),
			OutputPortConfig<float>("CurrentScreenDistance", _HELP("Current value of screen distance")),
			OutputPortConfig<float>("CurrentHUDDistance",    _HELP("Current value of HUD distance")),
			OutputPortConfig<float>("TimeLeft",              _HELP("Time left to the end of the interpolation")),
			OutputPortConfig_Void("Done",                    _HELP("Interpolation is done.")),
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* activationInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			activationInfo->pGraph->SetRegularlyUpdated(activationInfo->myID, false);

			m_targetEyeDistance = GetPortFloat(activationInfo, EIP_EyeDistance);
			m_targetScreenDistance = GetPortFloat(activationInfo, EIP_ScreenDistance);
			m_targetHUDDistance = GetPortFloat(activationInfo, EIP_HUDDistance);
			m_duration = GetPortFloat(activationInfo, EIP_Duration);

			break;

		case eFE_Activate:
			if (IsPortActive(activationInfo, EIP_Start))
			{
				activationInfo->pGraph->SetRegularlyUpdated(activationInfo->myID, true);

				m_startTime = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();

				m_startEyeDistance = gEnv->pConsole->GetCVar("r_StereoEyeDist")->GetFVal();
				m_startScreenDistance = gEnv->pConsole->GetCVar("r_StereoScreenDist")->GetFVal();
				m_startHUDDistance = gEnv->pConsole->GetCVar("r_StereoHudScreenDist")->GetFVal();
			}

			if (IsPortActive(activationInfo, EIP_EyeDistance))
				m_targetEyeDistance = GetPortFloat(activationInfo, EIP_EyeDistance);
			if (IsPortActive(activationInfo, EIP_ScreenDistance))
				m_targetScreenDistance = GetPortFloat(activationInfo, EIP_ScreenDistance);
			if (IsPortActive(activationInfo, EIP_HUDDistance))
				m_targetHUDDistance = GetPortFloat(activationInfo, EIP_HUDDistance);
			if (IsPortActive(activationInfo, EIP_Duration))
				m_duration = GetPortFloat(activationInfo, EIP_Duration);

			break;

		case eFE_Update:
			float currentTime = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();
			float endTime = m_startTime + m_duration * 1000.0f;

			if (currentTime > endTime)
			{
				ActivateOutput(activationInfo, EOP_Done, true);
				activationInfo->pGraph->SetRegularlyUpdated(activationInfo->myID, false);
				currentTime = endTime;
			}

			float s = 1.0f;
			if (endTime - m_startTime > 0.0f)
			{
				s = CLAMP((currentTime - m_startTime) / (endTime - m_startTime), 0.0f, 1.0f);
			}

			m_currentEyeDistance = LERP(m_startEyeDistance, m_targetEyeDistance, s);
			m_currentScreenDistance = LERP(m_startScreenDistance, m_targetScreenDistance, s);
			m_currentHUDDistance = LERP(m_startHUDDistance, m_targetHUDDistance, s);

			ActivateOutput(activationInfo, EOP_CurrentEyeDistance, m_currentEyeDistance);
			ActivateOutput(activationInfo, EOP_CurrentScreenDistance, m_currentScreenDistance);
			ActivateOutput(activationInfo, EOP_CurrentHUDDistance, m_currentHUDDistance);
			ActivateOutput(activationInfo, EOP_TimeLeft, (currentTime - m_startTime) / 1000.0f);

			gEnv->pConsole->GetCVar("r_StereoEyeDist")->Set(m_currentEyeDistance);
			gEnv->pConsole->GetCVar("r_StereoScreenDist")->Set(m_currentScreenDistance);
			gEnv->pConsole->GetCVar("r_StereoHudScreenDist")->Set(m_currentHUDDistance);

			break;

		}
		;
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	float m_targetEyeDistance;
	float m_targetScreenDistance;
	float m_targetHUDDistance;

	float m_startEyeDistance;
	float m_startScreenDistance;
	float m_startHUDDistance;

	float m_currentEyeDistance;
	float m_currentScreenDistance;
	float m_currentHUDDistance;

	float m_duration;
	float m_startTime;
};

REGISTER_FLOW_NODE("Stereo:ReadStereoParameters", CFlowNode_ReadStereoParameters);
REGISTER_FLOW_NODE("Stereo:StereoParameters", CFlowNode_StereoParameters);
