// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowGameSDKBaseNode.h"
#include "Player.h"

//////////////////////////////////////////////////////////////////////////

class CFlowControlPlayerHealthEffect : public CFlowBaseNode<eNCT_Singleton>
{
	enum
	{
		INP_ENABLE = 0,
		INP_DISABLE,
		INP_INTENSITY
	};

public:
	CFlowControlPlayerHealthEffect(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Enable",      _HELP("Enable manual control of the player health effects. Warning! while enabled, the normal ingame player health effects are disabled!")),
			InputPortConfig_Void("Disable",     _HELP("Disable manual control of the player health effects")),
			InputPortConfig<float>("Intensity", 0,                                                                                                                                          _HELP("amount of the damaged health effect (0..1)")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.sDescription = _HELP("When enabled, the intensity of the player damaged health effects will be controlled manually using the 'intensity' input.\nWarning! until this node is disabled again, the normal ingame player health effects will not work!");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				CPlayer* pClientPlayer = (CPlayer*)g_pGame->GetIGameFramework()->GetClientActor();
				if (pClientPlayer)
				{
					if (IsPortActive(pActInfo, INP_ENABLE))
					{
						if (gEnv->IsCutscenePlaying())
							pClientPlayer->GetPlayerHealthGameEffect().SetActive(true);
						pClientPlayer->GetPlayerHealthGameEffect().ExternalSetEffectIntensity(GetPortFloat(pActInfo, INP_INTENSITY));
						pClientPlayer->GetPlayerHealthGameEffect().EnableExternalControl(true);
					}
					if (IsPortActive(pActInfo, INP_INTENSITY))
						pClientPlayer->GetPlayerHealthGameEffect().ExternalSetEffectIntensity(GetPortFloat(pActInfo, INP_INTENSITY));
					if (IsPortActive(pActInfo, INP_DISABLE))
					{
						pClientPlayer->GetPlayerHealthGameEffect().EnableExternalControl(false);
						if (gEnv->IsCutscenePlaying())
							pClientPlayer->GetPlayerHealthGameEffect().SetActive(false);
					}
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

REGISTER_FLOW_NODE("Image:ControlPlayerHealthEffect", CFlowControlPlayerHealthEffect);
