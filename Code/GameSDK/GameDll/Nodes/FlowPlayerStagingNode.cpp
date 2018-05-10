// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Game.h"
#include "Player.h"
#include "PlayerInput.h"
#include "PlayerMovementController.h"

#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowPlayerLinkNode : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowPlayerLinkNode(SActivationInfo* pActInfo)
	{
	}

	~CFlowPlayerLinkNode()
	{
	}

	enum EInputPorts
	{
		EIP_Link = 0,
		EIP_Unlink,
		EIP_Target,
		EIP_DrawPlayer,
		EIP_KeepTransform,
	};

	enum EOutputPorts
	{
		EOP_Linked = 0,
		EOP_Unlinked,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Link",                 _HELP("Link the Player to Target Entity")),
			InputPortConfig_Void("Unlink",               _HELP("Unlink the Player (from any Entity)")),
			InputPortConfig<EntityId>("Target",          _HELP("Target Entity Id")),
			InputPortConfig<int>("DrawPlayer",           0,                                            _HELP("Draw the Player"),                 0, _UICONFIG("enum_int:NoChange=0,Hide=-1,Show=1")),
			InputPortConfig<bool>("KeepTransfromDetach", true,                                         _HELP("Keep Transformation on Detach")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("Linked",   _HELP("Trigger if Linked")),
			OutputPortConfig_Void("Unlinked", _HELP("Trigger if Unlinked")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Linking the Player to an Entity (with FreeLook)");
		config.SetCategory(EFLN_OBSOLETE);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			const int nDrawPlayer = GetPortInt(pActInfo, EIP_DrawPlayer);
			if (IsPortActive(pActInfo, EIP_Link))
			{
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(GetPortEntityId(pActInfo, EIP_Target));
				if (pEntity)
				{
					CActor* pPlayerActor = static_cast<CActor*>(gEnv->pGameFramework->GetClientActor());
					if (pPlayerActor)
					{
						SActorStats* pActorStats = pPlayerActor->GetActorStats();
						if (pActorStats)
						{
							if (nDrawPlayer == -1)
								pActorStats->isHidden = true;
							else if (nDrawPlayer == 1)
								pActorStats->isHidden = false;
						}
						pPlayerActor->LinkToEntity(pEntity->GetId());
						ActivateOutput(pActInfo, EOP_Linked, true);
					}
				}
			}
			if (IsPortActive(pActInfo, EIP_Unlink))
			{
				CActor* pPlayerActor = static_cast<CActor*>(gEnv->pGameFramework->GetClientActor());
				if (pPlayerActor)
				{
					SActorStats* pActorStats = pPlayerActor->GetActorStats();
					if (pActorStats)
					{
						if (nDrawPlayer == -1)
							pActorStats->isHidden = true;
						else if (nDrawPlayer == 1)
							pActorStats->isHidden = false;
					}
					pPlayerActor->LinkToEntity(0, GetPortBool(pActInfo, EIP_KeepTransform));
					ActivateOutput(pActInfo, EOP_Unlinked, true);
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

REGISTER_FLOW_NODE("Actor:PlayerLink", CFlowPlayerLinkNode);

