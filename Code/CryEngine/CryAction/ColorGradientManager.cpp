// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/IColorGradingCtrl.h>

class CFlowNode_ColorGradient : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum InputPorts
	{
		eIP_Trigger,
	};
	static const SInputPortConfig inputPorts[];

	CFlowNode_ColorGradient(SActivationInfo* pActivationInformation);
	~CFlowNode_ColorGradient();

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActivationInformation);
	virtual void         GetMemoryUsage(ICrySizer* sizer) const;

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_ColorGradient(pActInfo); }

	enum EInputPorts
	{
		eInputPorts_Trigger,
		eInputPorts_TexturePath,
		eInputPorts_TransitionTime,
		eInputPorts_Count,
	};

private:
	//	IGameEnvironment& m_environment;
	ITexture* m_pTexture;
};

const SInputPortConfig CFlowNode_ColorGradient::inputPorts[] =
{
	InputPortConfig_Void("Trigger",            _HELP("")),
	InputPortConfig<string>("tex_TexturePath", _HELP("Path to the Color Chart texture.")),
	InputPortConfig<float>("TransitionTime",   _HELP("Fade in time (Seconds).")),
	{ 0 },
};

CFlowNode_ColorGradient::CFlowNode_ColorGradient(SActivationInfo* pActivationInformation)
	: m_pTexture(NULL)
{
}

CFlowNode_ColorGradient::~CFlowNode_ColorGradient()
{
	SAFE_RELEASE(m_pTexture);
}

void CFlowNode_ColorGradient::GetConfiguration(SFlowNodeConfig& config)
{
	config.pInputPorts = inputPorts;
	config.SetCategory(EFLN_ADVANCED);
}

void CFlowNode_ColorGradient::ProcessEvent(EFlowEvent event, SActivationInfo* pActivationInformation)
{
	if (!gEnv->pRenderer || !gEnv->p3DEngine)
	{
		return;
	}

	//Preload texture
	if (event == IFlowNode::eFE_PrecacheResources && m_pTexture == nullptr)
	{
		const string texturePath = GetPortString(pActivationInformation, eInputPorts_TexturePath);
		const uint32 COLORCHART_TEXFLAGS = FT_NOMIPS | FT_DONT_STREAM | FT_STATE_CLAMP;
		m_pTexture = gEnv->pRenderer->EF_LoadTexture(texturePath.c_str(), COLORCHART_TEXFLAGS);
	}

	if (event == IFlowNode::eFE_Activate && IsPortActive(pActivationInformation, eIP_Trigger))
	{
		const string texturePath = GetPortString(pActivationInformation, eInputPorts_TexturePath);
		const float timeToFade = GetPortFloat(pActivationInformation, eInputPorts_TransitionTime);
		gEnv->p3DEngine->GetColorGradingCtrl()->SetColorGradingLut(texturePath.c_str(), timeToFade);
	}
}

void CFlowNode_ColorGradient::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
}

REGISTER_FLOW_NODE("Image:ColorGradient", CFlowNode_ColorGradient);
