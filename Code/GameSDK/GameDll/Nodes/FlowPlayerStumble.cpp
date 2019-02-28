// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Actor.h"
#include "PlayerStumble.h"

#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowPlayerStumbleMove : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowPlayerStumbleMove(SActivationInfo* pActInfo){}

	enum EInputPorts
	{
		EIP_Enable = 0,
		EIP_Disable,
		EIP_MinDamping,
		EIP_MaxDamping,
		EIP_PeriodicTime,
		EIP_StrengthX,
		EIP_StrengthY,
		EIP_Randomness,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Enable",         _HELP("")),
			InputPortConfig_Void("Disable",        _HELP("")),
			InputPortConfig<float>("MinDamping",   50.f,      _HELP("Minimum Damping of walk speed")),
			InputPortConfig<float>("MaxDamping",   75.f,      _HELP("Maximum Damping of walk speed")),
			InputPortConfig<float>("PeriodicTime", 4.f,       _HELP("Time in seconds for one stumble")),
			InputPortConfig<float>("StrengthX",    100.f,     _HELP("Stumble strength in X direction")),
			InputPortConfig<float>("StrengthY",    100.f,     _HELP("Stumble strength in Y direction")),
			InputPortConfig<float>("Randomness",   0.f,       _HELP("Value in [0,2]. Higher value means more variation.")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Activates the player beeing like drunken.");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_Enable))
			{
				CActor* pLocalActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
				if (pLocalActor != NULL)
				{
					PlayerActor::Stumble::StumbleParameters stumbleParameters;
					stumbleParameters.minDamping = GetPortFloat(pActInfo, EIP_MinDamping);
					stumbleParameters.maxDamping = GetPortFloat(pActInfo, EIP_MaxDamping);

					stumbleParameters.periodicTime = GetPortFloat(pActInfo, EIP_PeriodicTime);
					stumbleParameters.strengthX = GetPortFloat(pActInfo, EIP_StrengthX);
					stumbleParameters.strengthY = GetPortFloat(pActInfo, EIP_StrengthY);
					stumbleParameters.randomness = GetPortFloat(pActInfo, EIP_Randomness);

					pLocalActor->EnableStumbling(&stumbleParameters);
				}
			}
			else if (IsPortActive(pActInfo, EIP_Disable))
			{
				CActor* pLocalActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
				if (pLocalActor != NULL)
				{
					pLocalActor->DisableStumbling();
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

REGISTER_FLOW_NODE("Actor:PlayerStumble", CFlowPlayerStumbleMove);
