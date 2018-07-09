// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Player.h"

#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowLockPlayerNode : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowLockPlayerNode(SActivationInfo* const pActInfo)
	{
	}

	~CFlowLockPlayerNode()
	{
	}

	enum EInputPorts
	{
		EIP_Enable = 0,
		EIP_Disable,
		EIP_LimitYaw,
		EIP_LimitPitch,
		EIP_Filter
	};

	enum EOutputPorts
	{
		EOP_Enabled = 0,
		EOP_Disabled
	};

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Enable",                       _HELP("Enable")),
			InputPortConfig_Void("Disable",                      _HELP("Disable")),
			InputPortConfig<float>("ViewLimitYaw",               _HELP("ViewLimitYaw, in degrees. 180 for no restriction.")),
			InputPortConfig<float>("ViewLimitPitch",             _HELP("ViewLimitPitch, in degrees. 90 for no restriction.")),
			InputPortConfig<string>("actionFilter_ActionFilter", _HELP("Select Action Filter")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("Enabled",  _HELP("Triggered when enabled.")),
			OutputPortConfig_Void("Disabled", _HELP("Triggered when disabled.")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Lock the player's view direction");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_Enable))
			{
				CActor* const pPlayerActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
				if (pPlayerActor)
				{
					const Vec3 dir = pPlayerActor->GetViewRotation().GetColumn1();
					const float rangeH = max(GetPortFloat(pActInfo, EIP_LimitYaw), 0.0001f);
					const float rangeV = max(GetPortFloat(pActInfo, EIP_LimitPitch), 0.0001f);
					pPlayerActor->SetViewLimits(dir, DEG2RAD(rangeH), DEG2RAD(rangeV));

					EnableFilter(GetPortString(pActInfo, EIP_Filter), true);
				}
				ActivateOutput(pActInfo, EOP_Enabled, true);
			}
			else if (IsPortActive(pActInfo, EIP_Disable))
			{
				CActor* const pPlayerActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
				if (pPlayerActor)
				{
					pPlayerActor->SetViewLimits(Vec3(ZERO), 0.0f, 0.0f);
					EnableFilter(GetPortString(pActInfo, EIP_Filter), false);
				}
				ActivateOutput(pActInfo, EOP_Disabled, true);
			}
			break;
		}
	}

	void EnableFilter(const string& filter, const bool bEnable)
	{
		IGameFramework* const pGameFramework = g_pGame->GetIGameFramework();
		if (pGameFramework)
		{
			IActionMapManager* const pActionMapManager = pGameFramework->GetIActionMapManager();
			if (pActionMapManager)
			{
				pActionMapManager->EnableFilter(filter, bEnable);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("Game:LockPlayer", CFlowLockPlayerNode);
