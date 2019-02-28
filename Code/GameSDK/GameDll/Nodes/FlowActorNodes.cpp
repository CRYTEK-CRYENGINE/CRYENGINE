// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"
#include "GameRulesTypes.h"
#include "GameRulesModules/IGameRulesKillListener.h"
#include "GameRules.h"
#include "Player.h"
#include "AI/GameAISystem.h"
#include "AI/GameAIEnv.h"

#include <CryFlowGraph/IFlowBaseNode.h>
#include <CryAnimation/IFacialAnimation.h>

//////////////////////////////////////////////////////////////////////////
// Counts how many AIs died
//////////////////////////////////////////////////////////////////////////
class CFlowNode_AIBodyCount : public CFlowBaseNode<eNCT_Instanced>, SGameRulesListener
{
	enum EInputPorts
	{
		eINP_Enable = 0,
		eINP_Disable,
		eINP_Reset
	};

	enum EOutputPorts
	{
		eOUT_TotalDeaths = 0,
		eOUT_EnemyDeaths
	};
public:
	CFlowNode_AIBodyCount(SActivationInfo* pActInfo)
		: m_totalDeaths(0)
		, m_enemyDeaths(0)
	{
	}
	~CFlowNode_AIBodyCount()
	{
		CGameRules* pGameRules = g_pGame->GetGameRules();
		if (pGameRules)
			pGameRules->RemoveGameRulesListener(this);
	}
	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("TotalDeaths", m_totalDeaths);
		ser.Value("EnemyDeaths", m_enemyDeaths);
	}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inp_config[] = {
			InputPortConfig_Void("Enable",  _HELP("Enables the body counter (disabled by default from start)")),
			InputPortConfig_Void("Disable", _HELP("Disables the body counter")),
			InputPortConfig_Void("Reset",   _HELP("Resets the body counter to 0 for both outputs")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("Total"),
			OutputPortConfig<int>("Enemy"),
			{ 0 }
		};

		config.sDescription = _HELP("Counts how many AIs have been killed");
		config.pInputPorts = inp_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_AIBodyCount(pActInfo); }
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;
				m_totalDeaths = 0;
				m_enemyDeaths = 0;
				break;
			}
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eINP_Enable))
				{
					CGameRules* pGameRules = g_pGame->GetGameRules();
					if (pGameRules)
						pGameRules->AddGameRulesListener(this);
				}

				if (IsPortActive(pActInfo, eINP_Disable))
				{
					CGameRules* pGameRules = g_pGame->GetGameRules();
					if (pGameRules)
						pGameRules->RemoveGameRulesListener(this);
				}

				if (IsPortActive(pActInfo, eINP_Reset))
				{
					m_totalDeaths = 0;
					m_enemyDeaths = 0;
					ActivateOutput(&m_actInfo, eOUT_TotalDeaths, m_totalDeaths);
					ActivateOutput(&m_actInfo, eOUT_EnemyDeaths, m_enemyDeaths);
				}
				break;
			}
		}
	}

	// inherited from SGameRulesListener
	virtual void OnActorDeath(CActor* pActor)
	{
		IAIObject* pAI = pActor->GetEntity()->GetAI();

		if (!pAI)
			return;

		++m_totalDeaths;
		ActivateOutput(&m_actInfo, eOUT_TotalDeaths, m_totalDeaths);

		IAIObject* pClientAI = gEnv->pGameFramework->GetClientActor()->GetEntity()->GetAI();

		if (pAI->IsHostile(pClientAI, false))
		{
			++m_enemyDeaths;
			ActivateOutput(&m_actInfo, eOUT_EnemyDeaths, m_enemyDeaths);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	SActivationInfo m_actInfo;
	int             m_totalDeaths;
	int             m_enemyDeaths;
};

class CFlowActorAliveCheck : public CFlowBaseNode<eNCT_Instanced>
{
	bool m_errorLogSent;
public:
	CFlowActorAliveCheck(SActivationInfo* pActInfo)
		: m_errorLogSent(false)
	{
	}

	enum EInputs
	{
		eIP_Trigger = 0
	};

	enum EOutputs
	{
		eOP_Status = 0,
		eOP_Alive,
		eOP_Dead
	};

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowActorAliveCheck(pActInfo);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Trigger", _HELP("Trigger this port to get the current actor status")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<bool>("Status", _HELP("true if is alive, false if dead")),
			OutputPortConfig_Void("Alive",   _HELP("triggered if is alive")),
			OutputPortConfig_Void("Dead",    _HELP("triggered if is dead")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_SUPPORTED;
		config.sDescription = _HELP("Check the death/alive status of an entity (actor)");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_errorLogSent = false;
			break;

		case eFE_Activate:
			if (IsPortActive(pActInfo, eIP_Trigger))
			{
				IActor* pActor = pActInfo->pEntity ? gEnv->pGameFramework->GetIActorSystem()->GetActor(pActInfo->pEntity->GetId()) : NULL;
				if (pActor)
				{
					bool isAlive = !pActor->IsDead();
					ActivateOutput(pActInfo, eOP_Status, isAlive);
					if (isAlive)
						ActivateOutput(pActInfo, eOP_Alive, true);
					else
						ActivateOutput(pActInfo, eOP_Dead, true);
				}
				else if (!m_errorLogSent)
				{
					IEntitySystem* pESys = gEnv->pEntitySystem;
					IEntity* pGraphEntity = pESys->GetEntity(pActInfo->pGraph->GetGraphEntity(0));
					GameWarning("[flow] Actor:AliveCheck - flowgraph entity: %d:'%s' - no entity or entity is not an actor. Entity: %d:'%s'",
					            pActInfo->pGraph->GetGraphEntity(0), pGraphEntity ? pGraphEntity->GetName() : "<NULL>",
					            pActInfo->pEntity ? pActInfo->pEntity->GetId() : 0, pActInfo->pEntity ? pActInfo->pEntity->GetName() : "<NULL>");
					m_errorLogSent = true;
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowPlayerIsInAir : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowPlayerIsInAir(SActivationInfo* pActInfo)
	{
	}

	enum EInputs
	{
		eIP_Trigger = 0
	};

	enum EOutputs
	{
		eOP_Status = 0,
		eOP_InAir,
		eOP_NotInAir
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Trigger", _HELP("Trigger this port to get the player InAir status")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig<bool>("Status",  _HELP("true if is InAir, false if not")),
			OutputPortConfig_Void("InAir",    _HELP("triggered if is InAir")),
			OutputPortConfig_Void("NotInAir", _HELP("triggered if is NOT InAir")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Check the InAir/NotInAir status of the player. InAir = jumping or falling");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;

		case eFE_Activate:
			if (IsPortActive(pActInfo, eIP_Trigger))
			{
				IActor* pClientActor = gEnv->pGameFramework->GetClientActor();
				if (pClientActor && pClientActor->IsPlayer())
				{
					CPlayer* pPlayer = static_cast<CPlayer*>(pClientActor);
					bool isInAir = pPlayer->IsInAir() || pPlayer->IsInFreeFallDeath();
					ActivateOutput(pActInfo, eOP_Status, isInAir);
					if (isInAir)
						ActivateOutput(pActInfo, eOP_InAir, true);
					else
						ActivateOutput(pActInfo, eOP_NotInAir, true);
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

class CFlowActorFacialAnim : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowActorFacialAnim(SActivationInfo* pActInfo)
	{
	}

	enum EInputs
	{
		eIP_Start = 0,
		eIP_Stop,
		eIP_Sequence,
		eIP_Layer,
		eIP_Exclusive,
		eIP_Loop,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Start",       _HELP("Trigger the facial sequence")),
			InputPortConfig_Void("Stop",        _HELP("Stop the facial sequence")),
			InputPortConfig<string>("Sequence", _HELP("Sequence (fsq file) to play")),
			InputPortConfig<int>("Layer",       _HELP("Layer to play that sequence")),
			InputPortConfig<bool>("Exclusive",  _HELP("Exclusive animation ?")),
			InputPortConfig<bool>("Loop",       _HELP("Play in Loop ?")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Play facial sequences on Actors");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, eIP_Start) && pActInfo->pEntity)
			{
				if (ICharacterInstance* pCharacter = pActInfo->pEntity->GetCharacter(0))
				{
					if (IFacialInstance* pInstance = pCharacter->GetFacialInstance())
					{
						const string sSequence = GetPortString(pActInfo, eIP_Sequence);
						if (IFacialAnimSequence* pSequence = pInstance->LoadSequence(sSequence))
						{
							int iLayer = GetPortInt(pActInfo, eIP_Layer);
							bool bLoop = GetPortBool(pActInfo, eIP_Loop);
							bool bExclusive = GetPortBool(pActInfo, eIP_Exclusive);
							pInstance->PlaySequence(pSequence, (EFacialSequenceLayer) iLayer, bExclusive, bLoop);
						}
					}
				}
			}
			else if (IsPortActive(pActInfo, eIP_Stop) && pActInfo->pEntity)
			{
				if (ICharacterInstance* pCharacter = pActInfo->pEntity->GetCharacter(0))
				{
					if (IFacialInstance* pInstance = pCharacter->GetFacialInstance())
					{
						pInstance->StopSequence((EFacialSequenceLayer) GetPortInt(pActInfo, eIP_Layer));
					}
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

static const uint32 INVALID_FACIAL_CHANNEL_ID = ~0;

class CFlowActorFacialExpression : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowActorFacialExpression(SActivationInfo* pActInfo)
	{
		m_channel = INVALID_FACIAL_CHANNEL_ID;
	}

	enum EInputs
	{
		eIP_Start = 0,
		eIP_Stop,
		eIP_Expression,
		eIP_Weight,
		eIP_FadeTime,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Start",         _HELP("Trigger the facial expression")),
			InputPortConfig_Void("Stop",          _HELP("Stop the facial expression")),
			InputPortConfig<string>("Expression", _HELP("Expression to play")),
			InputPortConfig<float>("Weight",      _HELP("Weight")),
			InputPortConfig<float>("FadeTime",    _HELP("Fade time")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Play facial expressions on Actors");
		config.SetCategory(EFLN_DEBUG);
	}
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowActorFacialExpression(pActInfo); }

	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, eIP_Start) && pActInfo->pEntity)
			{
				if (ICharacterInstance* pCharacter = pActInfo->pEntity->GetCharacter(0))
				{
					if (IFacialInstance* pInstance = pCharacter->GetFacialInstance())
					{
						if (IFacialModel* pFacialModel = pInstance->GetFacialModel())
						{
							if (IFacialEffectorsLibrary* pLibrary = pFacialModel->GetLibrary())
							{
								IFacialEffector* pEffector = NULL;
								if (pEffector = pLibrary->Find(GetPortString(pActInfo, eIP_Expression).c_str()))
								{
									float fWeight = GetPortFloat(pActInfo, eIP_Weight);
									float fFadeTime = GetPortFloat(pActInfo, eIP_FadeTime);

									if (m_channel != INVALID_FACIAL_CHANNEL_ID)
									{
										// we fade out with the same fadeTime as fade in
										pInstance->StopEffectorChannel(m_channel, fFadeTime);
										m_channel = INVALID_FACIAL_CHANNEL_ID;
									}

									m_channel = pInstance->StartEffectorChannel(pEffector, fWeight, fFadeTime);
								}
							}
						}
					}
				}
			}
			else if (IsPortActive(pActInfo, eIP_Stop) && pActInfo->pEntity && m_channel != INVALID_FACIAL_CHANNEL_ID)
			{
				if (ICharacterInstance* pCharacter = pActInfo->pEntity->GetCharacter(0))
				{
					if (IFacialInstance* pInstance = pCharacter->GetFacialInstance())
					{
						float fFadeTime = GetPortFloat(pActInfo, eIP_FadeTime);
						pInstance->StopEffectorChannel(m_channel, fFadeTime);
						m_channel = INVALID_FACIAL_CHANNEL_ID;
					}
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	uint32 m_channel;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_PlayerKnockDown : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_PlayerKnockDown(SActivationInfo* pActInfo)
	{
	}

	enum EInputs
	{
		eIN_KnockDown = 0,
		eIN_BackwardsImpulse
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("KnockDown", _HELP("Triggers player knock down action")),
			InputPortConfig<float>("Impulse", _HELP("Backwards impulse applied when knocking down")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Knock down local player");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIN_KnockDown))
				{
					CActor* pLocalActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
					if (pLocalActor)
					{
						const float backwardsImpulse = GetPortFloat(pActInfo, eIN_BackwardsImpulse);
						pLocalActor->KnockDown(backwardsImpulse);
					}
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
class CFlowNode_PlayerInteractiveAnimation : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_PlayerInteractiveAnimation(SActivationInfo* pActInfo)
	{
	}

	enum EInputs
	{
		eIN_Play = 0,
		eIN_Interaction
	};

	enum EOutputs
	{
		eOUT_Done = 0
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Play",               _HELP("Triggers interactive action")),
			InputPortConfig<string>("InteractionName", _HELP("Interactive action to play")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("Done", _HELP("Triggers when the animation is done")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Plays interactive animation for Player");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIN_Play))
				{
					CActor* pLocalActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
					if (pLocalActor)
					{
						const string& interactionName = GetPortString(pActInfo, eIN_Interaction);
						pLocalActor->StartInteractiveActionByName(interactionName.c_str(), true);

						pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
					}
				}
			}
			break;
		case eFE_Update:
			{
				CPlayer* pLocalPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor());
				if (pLocalPlayer)
				{
					if (pLocalPlayer->IsInteractiveActionDone())
					{
						ActivateOutput(pActInfo, eOUT_Done, true);
						pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					}
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
class CFlowNode_PlayerCinematicControl : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_PlayerCinematicControl(SActivationInfo* pActInfo)
	{
	}

	enum EInputs
	{
		eIN_HolsterWeapon = 0,
		eIN_LowerWeapon,
		eIN_RestrictMovement,
		eIN_RestrictToWalk,
		eIN_TutorialMode,
		eIN_ResetAll
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("HolsterWeapon",    _HELP("Holster current player weapon")),
			InputPortConfig_Void("LowerWeapon",      _HELP("Lowers current player weapon")),
			InputPortConfig_Void("RestrictMovement", _HELP("Disables super jump, sprint and others")),
			InputPortConfig_Void("RestrictToWalk",   _HELP("Scales movement speed by 50%")),
			InputPortConfig_Void("TutorialMode",     _HELP("Places the player in turorial mode by restricting weapon fire, lowering his weapon, etc.")),
			InputPortConfig_Void("ResetAll",         _HELP("Go back to normal control when cinematic is over")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Restrict player controls during cinematics");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIN_HolsterWeapon))
				{
					CPlayer* pLocalPlayer = GetLocalPlayer();
					if (pLocalPlayer)
					{
						pLocalPlayer->SetCinematicFlag(SPlayerStats::eCinematicFlag_HolsterWeapon);
					}
				}
				if (IsPortActive(pActInfo, eIN_LowerWeapon))
				{
					CPlayer* pLocalPlayer = GetLocalPlayer();
					if (pLocalPlayer)
					{
						pLocalPlayer->SetCinematicFlag(SPlayerStats::eCinematicFlag_LowerWeapon);
						pLocalPlayer->SetCinematicFlag(SPlayerStats::eCinematicFlag_RestrictMovement);
					}
				}
				if (IsPortActive(pActInfo, eIN_RestrictMovement))
				{
					CPlayer* pLocalPlayer = GetLocalPlayer();
					if (pLocalPlayer)
					{
						pLocalPlayer->SetCinematicFlag(SPlayerStats::eCinematicFlag_RestrictMovement);
					}
				}
				if (IsPortActive(pActInfo, eIN_RestrictToWalk))
				{
					CPlayer* pLocalPlayer = GetLocalPlayer();
					if (pLocalPlayer)
					{
						pLocalPlayer->SetCinematicFlag(SPlayerStats::eCinematicFlag_WalkOnly);
					}
				}
				if (IsPortActive(pActInfo, eIN_TutorialMode))
				{
					CPlayer* pLocalPlayer = GetLocalPlayer();
					if (pLocalPlayer)
					{
						pLocalPlayer->SetCinematicFlag(SPlayerStats::eCinematicFlag_LowerWeapon);
					}
				}
				if (IsPortActive(pActInfo, eIN_ResetAll))
				{
					CPlayer* pLocalPlayer = GetLocalPlayer();
					if (pLocalPlayer)
					{
						pLocalPlayer->ResetCinematicFlags();
					}
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:

	CPlayer* GetLocalPlayer() const
	{
		IActor* pActor = g_pGame->GetIGameFramework()->GetClientActor();
		;

		if (pActor)
		{
			CRY_ASSERT(pActor->GetActorClass() == CPlayer::GetActorClassType());

			return static_cast<CPlayer*>(pActor);
		}

		return NULL;
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_PlayerLookAt : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_PlayerLookAt(SActivationInfo* pActInfo)
	{
	}

	enum EInputs
	{
		eIN_EnableLookAt = 0,
		eIN_DisableLookAt,
		eIP_Force,
		eIP_Time,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Enable",              _HELP("Enables look at target option for the local player")),
			InputPortConfig_Void("Disable",             _HELP("Disables look at target option for the local player")),
			InputPortConfig<bool>("Force",              _HELP("Forces the player to look at the target, even if no button is pressed.")),
			InputPortConfig<float>("InterpolationTime", 0.2f,                                                                            _HELP("Smoothing time (seconds)")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Enable/Disable look at target option for the player");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIN_EnableLookAt))
				{
					float interpolationTime = GetPortFloat(pActInfo, eIP_Time);
					CActor* pLocalActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
					if (pLocalActor)
					{
						EntityId targetId = pActInfo->pEntity ? pActInfo->pEntity->GetId() : 0;
						if (GetPortBool(pActInfo, eIP_Force))
						{
							pLocalActor->SetForceLookAtTargetId(targetId, interpolationTime);
						}
						else
						{
							pLocalActor->SetLookAtTargetId(targetId, interpolationTime);
						}
					}
				}
				else if (IsPortActive(pActInfo, eIN_DisableLookAt))
				{
					CActor* pLocalActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
					if (pLocalActor)
					{
						pLocalActor->SetLookAtTargetId(0);
					}
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
class CFlowNode_ActorKillPlayer : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_ActorKillPlayer(SActivationInfo* pActInfo) {}

	enum EInputs
	{
		eIN_Trigger = 0,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Kill", _HELP("Triggers to kill the local player")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Instantly kills the local player.");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIN_Trigger))
				{
					CGameRules* pGameRules = g_pGame->GetGameRules();
					if (pGameRules)
					{
						const EntityId clientId = g_pGame->GetIGameFramework()->GetClientActorId();

						HitInfo suicideInfo(clientId, clientId, clientId,
						                    10000, 0, 0, -1, CGameRules::EHitType::Punish, ZERO, ZERO, ZERO);

						pGameRules->ClientHit(suicideInfo);

						//Execute a second time, to skip 'mercy time' protection
						if (!gEnv->bMultiplayer)
						{
							pGameRules->ClientHit(suicideInfo);
						}
					}
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
class CFlowNode_ActorSetPlayerModel : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNode_ActorSetPlayerModel(SActivationInfo* pActInfo)
	{

	}

	~CFlowNode_ActorSetPlayerModel()
	{
		m_pPlayerModel.reset();
	}

	enum EInputs
	{
		eIN_Set = 0,
		eIN_Model,
	};

	enum EOutputs
	{
		eOUT_Set = 0,
		eOUT_Fail,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Set",      _HELP("Triggers model changing ")),
			InputPortConfig<string>("Model", _HELP("Model file name")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<bool>("Set",  _HELP("true if the model has been set successfully ")),
			OutputPortConfig<bool>("Fail", _HELP("true if the model hasn't been set")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Set local player's model");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_ActorSetPlayerModel(pActInfo); }

	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{

		switch (event)
		{
		case eFE_Initialize:
			{
				// pre-cache the model
				const string& modelName = GetPortString(pActInfo, eIN_Model);
				m_pPlayerModel = gEnv->pCharacterManager->CreateInstance(modelName.c_str());
			}
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIN_Set))
				{
					CActor* pLocalActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
					if (pLocalActor != NULL && pLocalActor->GetEntity())
					{
						const string& modelName = GetPortString(pActInfo, eIN_Model);
						if (m_pPlayerModel)
						{
							const bool loaded = pLocalActor->SetActorModel(modelName.c_str());
							if (loaded)
							{
								pLocalActor->Physicalize(pLocalActor->GetStance());
							}
							ActivateOutput(pActInfo, loaded ? eOUT_Set : eOUT_Fail, true);
						}
						else
						{
							CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Actor:SetPlayerModel:Character %s not valid", modelName.c_str());
							ActivateOutput(pActInfo, eOUT_Fail, true);
						}
					}
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	_smart_ptr<ICharacterInstance> m_pPlayerModel;

};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_PlayerDropObject : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_PlayerDropObject(SActivationInfo* pActInfo)
	{
	}

	enum EInputs
	{
		eIN_DropIt = 0,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("DropIt", _HELP("When triggered, the player will drop any currently held object or enemy")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Force the player to drop any currently held object/enemy");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIN_DropIt))
				{
					CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
					if (pActor != NULL && pActor->IsPlayer())
					{
						CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
						if (pPlayer->IsInPickAndThrowMode())
							pPlayer->ExitPickAndThrow();
					}
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
class CFlowNode_GetNearestActor : public CFlowBaseNode<eNCT_Singleton>
{
public:

	CFlowNode_GetNearestActor(SActivationInfo* pActInfo)
	{
	}
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	enum EInput
	{
		eINP_Trigger = 0,
		eINP_CenterPos,
		eINP_MaxDist,
		eINP_Faction,
		eINP_FactionFilterType,
		eINP_DeadAliveFilter,
	};

	enum EOutput
	{
		eOUT_NearestActorId = 0,
		eOUT_Faction,
		eOUT_Position,
		eOUT_Distance,
		eOUT_Alive
	};

	enum EFactionFilterType
	{
		eFCT_NoFilter = 0,
		eFCT_SameFactionOnly,
		eFCT_AllButFaction,
		eFCT_AnyFriendly,
		eFCT_AnyHostile,
	};

	enum EDeadAliveFilter
	{
		eDA_Any = 0,
		eDA_Alive,
		eDA_Dead
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<EntityId>("ActorId", _HELP("entityId of the nearest actor")),
			OutputPortConfig<int>("Faction",      _HELP("Faction of the nearest actor")),
			OutputPortConfig<Vec3>("Pos",         _HELP("Position of the nearest actor")),
			OutputPortConfig<float>("Distance",   _HELP("Distance of the nearest actor to the centerPos check")),
			OutputPortConfig<bool>("Alive",       _HELP("True if the nearest actor is alive")),
			{ 0 }
		};
		static const SInputPortConfig inp_config[] = {
			InputPortConfig_Void("Trigger",           _HELP("The node only checks for nearest actor when this is triggered")),
			InputPortConfig<Vec3>("CenterPos",        Vec3(0,                                                                 0,                                                                                              0), _HELP("Position to compare actors with.")),
			InputPortConfig<float>("MaxDist",         0,                                                                      _HELP("Only consider actors that are nearer than this. 0 = no distance limit.")),
			InputPortConfig<string>("Faction",        "",                                                                     _HELP("Used in combination with 'FactionFilterType' input to filter actors based on faction"),  0,  "enum_global:Faction"),
			InputPortConfig<int>("FactionFilterType", 0,                                                                      _HELP("Defines how the 'Faction' input is used for filtering."),                                0,  _UICONFIG("enum_int:(0)No_faction_check=0,(1)Only_that_faction=1,(2)All_but_that_faction=2,(3)Any_friendly=3,(4)Any_Hostile=4")),
			InputPortConfig<int>("DeadAliveFilter",   0,                                                                      _HELP("Filters actors by death/alive status."),                                                 0,  _UICONFIG("enum_int:(0)Any,(1)Alive=1,(2)Dead=2")),
			{ 0 }
		};

		config.sDescription = _HELP("Outputs the nearest actor to a given position. WARNING: potentially performance heavy, dont trigger it every frame.");
		config.pInputPorts = inp_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eINP_Trigger))
				{
					CheckNearestActor(pActInfo);
				}
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CheckNearestActor(SActivationInfo* pActInfo)
	{
		float maxDist = GetPortFloat(pActInfo, eINP_MaxDist);
		float maxDist2 = maxDist == 0 ? FLT_MAX : maxDist * maxDist;
		Vec3 centerPos = GetPortVec3(pActInfo, eINP_CenterPos);

		IActor* pNearestActor = NULL;
		float nearestDist2 = maxDist2;

		EFactionFilterType factionFilterType = (EFactionFilterType)GetPortInt(pActInfo, eINP_FactionFilterType);
		IFactionMap& factionMap = gEnv->pAISystem->GetFactionMap();
		uint8 factionID = factionMap.GetFactionID(GetPortString(pActInfo, eINP_Faction));
		EDeadAliveFilter deadAliveFilter = (EDeadAliveFilter)GetPortInt(pActInfo, eINP_DeadAliveFilter);

		if (factionID == IFactionMap::InvalidFactionID)
			factionFilterType = eFCT_NoFilter;

		IActorIteratorPtr actorIt = gEnv->pGameFramework->GetIActorSystem()->CreateActorIterator();
		while (IActor* pActor = actorIt->Next())
		{
			if (pActor)
			{
				const IAIObject* pAIObject = pActor->GetEntity()->GetAI();
				if (pAIObject)
				{
					uint8 actorFactionID = pAIObject->GetFactionID();
					bool bValid = true;

					switch (factionFilterType)
					{
					case eFCT_SameFactionOnly:
						{
							bValid = actorFactionID == factionID;
							break;
						}

					case eFCT_AllButFaction:
						{
							bValid = actorFactionID != factionID;
							break;
						}

					case eFCT_AnyFriendly:
						{
							bValid = factionMap.GetReaction(factionID, actorFactionID) == IFactionMap::Friendly;
							break;
						}

					case eFCT_AnyHostile:
						{
							bValid = factionMap.GetReaction(factionID, actorFactionID) == IFactionMap::Hostile;
							break;
						}
					}

					if (deadAliveFilter != eDA_Any)
					{
						EDeadAliveFilter actorDeadAliveStatus = pActor->IsDead() ? eDA_Dead : eDA_Alive;
						bValid = bValid && (actorDeadAliveStatus == deadAliveFilter);
					}

					if (bValid)
					{
						float dist2 = centerPos.GetSquaredDistance(pActor->GetEntity()->GetPos());
						if (dist2 <= nearestDist2)
						{
							nearestDist2 = dist2;
							pNearestActor = pActor;
						}
					}

				}
			}
		}

		if (pNearestActor)
		{
			ActivateOutput(pActInfo, eOUT_NearestActorId, pNearestActor->GetEntityId());
			ActivateOutput(pActInfo, eOUT_Faction, int(pNearestActor->GetEntity()->GetAI()->GetFactionID()));
			ActivateOutput(pActInfo, eOUT_Position, pNearestActor->GetEntity()->GetPos());
			ActivateOutput(pActInfo, eOUT_Distance, sqrt(nearestDist2));
			ActivateOutput(pActInfo, eOUT_Alive, !pNearestActor->IsDead());
		}
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_ActorKill : public CFlowBaseNode<eNCT_Instanced>, public IGameRulesKillListener
{
public:
	CFlowNode_ActorKill(SActivationInfo* pActInfo)
	{
	}

	virtual ~CFlowNode_ActorKill()
	{
		// safety unregister
		if (CGameRules* pGameRules = g_pGame->GetGameRules())
		{
			pGameRules->UnRegisterKillListener(this);
		}
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_ActorKill(pActInfo);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		if (ser.IsReading())
		{
			RegisterOrUnregisterListener(pActInfo);
		}
	}

	enum EInputPorts
	{
		EIP_Enable = 0,
		EIP_KillerId,
		EIP_VictimId,
	};

	enum EOutputPorts
	{
		EOP_Kill = 0,
		EOP_CollisionKill,
		EOP_KillerId,
		EOP_VictimtId,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<bool>("Enable",       false,                                                      _HELP("Enable/Disable KillInfo")),
			InputPortConfig<EntityId>("KillerId", _HELP("When connected, limit Kill report to this entity")),
			InputPortConfig<EntityId>("VictimId", _HELP("When connected, limit Kill report to this entity")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("Kill",           _HELP("Triggers if the kill conditions are fulfilled")),
			OutputPortConfig<bool>("CollisionKill", _HELP("True if the kill was caused by a collision")),
			OutputPortConfig<EntityId>("KillerId",  _HELP("EntityID of the Killer")),
			OutputPortConfig<EntityId>("VictimId",  _HELP("EntityID of the Victim")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Tracks Kills on Actors.\nAll input conditions (KillerId, VictimId) must be fulfilled to output.\nIf a condition is left empty/not connected, it is regarded as fulfilled.");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_actInfo = *pActInfo;  // fall through and enable/disable listener
		case eFE_Activate:
			RegisterOrUnregisterListener(pActInfo);
			break;
		}
	}

	void RegisterOrUnregisterListener(SActivationInfo* pActInfo)
	{
		CGameRules* pGameRules = g_pGame->GetGameRules();
		if (!pGameRules)
		{
			GameWarning("[flow] CFlowHitInfoNode::RegisterListener: No GameRules!");
			return;
		}

		if (GetPortBool(pActInfo, EIP_Enable))
		{
			pGameRules->RegisterKillListener(this);
		}
		else
		{
			pGameRules->UnRegisterKillListener(this);
		}
	}

	// IGameRulesKillListener
	virtual void OnEntityKilledEarly(const HitInfo& hitInfo) {};

	virtual void OnEntityKilled(const HitInfo& hitInfo)
	{
		IF_UNLIKELY (GetPortBool(&m_actInfo, EIP_Enable) == false)
			return;

		const EntityId killerId = GetPortEntityId(&m_actInfo, EIP_KillerId);
		const EntityId victimId = GetPortEntityId(&m_actInfo, EIP_VictimId);

		if ((killerId != 0) && (killerId != hitInfo.shooterId))
			return;

		if ((victimId != 0) && (victimId != hitInfo.targetId))
			return;

		if (IsVictimActor(hitInfo.targetId) == false)
			return;

		ActivateOutput(&m_actInfo, EOP_Kill, true);
		ActivateOutput(&m_actInfo, EOP_CollisionKill, (hitInfo.type == CGameRules::EHitType::Collision));
		ActivateOutput(&m_actInfo, EOP_KillerId, killerId);
		ActivateOutput(&m_actInfo, EOP_VictimtId, victimId);
	}
	// ~IGameRulesKillListener

	bool IsVictimActor(const EntityId victimId) const
	{
		return (g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(victimId) != NULL);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	SActivationInfo m_actInfo;
};

//////////////////////////////////////////////////////////////////////////
class CFlowRagdollizeCharacter : public CFlowBaseNode<eNCT_Instanced>
{
private:
	enum EInputPorts
	{
		EIP_Ragdollize = 0,
		EIP_ImpulseMin,
		EIP_ImpulseMax,
		EIP_AngImpulseMin,
		EIP_AngImpulseMax,
		EIP_ImpulsePoint,
		EIP_Unragdollize
	};

	enum EOutputPorts
	{
		EOP_OnRagdollize = 0,
		EOP_OnUnRagdollize,
		EOP_Impulse,
		EOP_AngImpulse
	};

public:
	CFlowRagdollizeCharacter(SActivationInfo* pActInfo)
	{
	}

	virtual ~CFlowRagdollizeCharacter()
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowRagdollizeCharacter(pActInfo);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Ragdollize",     _HELP("Ragdollize assigned actor")),
			InputPortConfig<Vec3>("ImpulseMin",    Vec3(0,                                                             0,  0), _HELP("")),
			InputPortConfig<Vec3>("ImpulseMax",    Vec3(0,                                                             0,  0), _HELP("")),
			InputPortConfig<Vec3>("AngImpulseMin", Vec3(0,                                                             0,  0), _HELP("")),
			InputPortConfig<Vec3>("AngImpulseMax", Vec3(0,                                                             0,  0), _HELP("")),
			InputPortConfig<Vec3>("ImpulseOrigin", Vec3(0,                                                             0,  0), _HELP("")),
			InputPortConfig_Void("Unragdollize",   _HELP("Unragdollize assigned actor (dead actors will be skipped)")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("OnRagdollize", _HELP("")),
			OutputPortConfig_Void("UnRagdollize", _HELP("")),
			OutputPortConfig<Vec3>("Impulse",     _HELP("")),
			OutputPortConfig<Vec3>("AngImpulse",  _HELP("")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("(un)ragdollize a character with a defined impulse");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (!pActInfo->pEntity)
					return;

				CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pActInfo->pEntity->GetId()));

				if (!pActor)
					return;

				if (IsPortActive(pActInfo, EIP_Ragdollize))
				{
					if (pActor->CanRagDollize())
					{
						pActor->RagDollize(true);

						Vec3 impulseValue = cry_random_componentwise(
						  GetPortVec3(pActInfo, EIP_ImpulseMin),
						  GetPortVec3(pActInfo, EIP_ImpulseMax));
						Vec3 angImpulseValue = cry_random_componentwise(
						  GetPortVec3(pActInfo, EIP_AngImpulseMin),
						  GetPortVec3(pActInfo, EIP_AngImpulseMax));
						Vec3 impulseOrigin = pActor->GetEntity()->GetWorldPos();
						Matrix34 transMat = pActor->GetEntity()->GetWorldTM();

						pe_action_impulse action;
						action.point = transMat.TransformPoint(impulseOrigin);
						action.impulse = transMat.TransformVector(impulseValue);

						if (!angImpulseValue.IsZero())
							action.angImpulse = transMat.TransformVector(angImpulseValue);

						IEntity* pEntityImpulse = pActor->GetEntity();
						while (pEntityImpulse->GetParent())
						{
							pEntityImpulse = pEntityImpulse->GetParent();
						}

						IPhysicalEntity* pPhysEntity = pEntityImpulse->GetPhysics();
						if (pPhysEntity)
							pPhysEntity->Action(&action);

						ActivateOutput(pActInfo, EOP_OnRagdollize, 1);

						if (!impulseValue.IsZero())
							ActivateOutput(pActInfo, EOP_Impulse, impulseValue);

						if (!angImpulseValue.IsZero())
							ActivateOutput(pActInfo, EOP_AngImpulse, angImpulseValue);
					}
				}

				if (IsPortActive(pActInfo, EIP_Unragdollize))
				{
					if (pActor->IsDead())
						return;

					if (pActor->IsPlayer())
					{
						CPlayer* pPlayer = static_cast<CPlayer*>(pActor);

						if (pPlayer)
							pPlayer->UnRagdollize();
					}
					else if (pActor->CanRagDollize())
						pActor->Revive(CActor::kRFR_Spawn);

					ActivateOutput(pActInfo, EOP_OnUnRagdollize, 1);
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowActorMovementParameter : public CFlowBaseNode<eNCT_Instanced>
{
private:
	enum EInputPorts
	{
		EIP_Set = 0,
		EIP_Get,
		EIP_JumpHeight,
		EIP_WalkSpeed,
		EIP_CrouchSpeed,
		EIP_SwimSpeed
	};

	enum EOutputPorts
	{
		EOP_JumpHeight = 0,
		EOP_WalkSpeed,
		EOP_CrouchSpeed,
		EOP_SwimSpeed
	};

public:
	CFlowActorMovementParameter(SActivationInfo* pActInfo)
	{
	}

	virtual ~CFlowActorMovementParameter()
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowActorMovementParameter(pActInfo);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_Void("Set",           _HELP("Set actor values")),
			InputPortConfig_Void("Get",           _HELP("Get current actor values")),
			InputPortConfig<float>("JumpHeight",  0.f,                               _HELP("actor jump height")),
			InputPortConfig<float>("WalkSpeed",   0.f,                               _HELP("actor base walking speed (without multipliers)")),
			InputPortConfig<float>("CrouchSpeed", 0.f,                               _HELP("actor base crouch speed (without multipliers)")),
			InputPortConfig<float>("SwimSpeed",   0.f,                               _HELP("actor base swim speed (without multipliers)")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig<float>("JumpHeight",  _HELP("current actor jump height")),
			OutputPortConfig<float>("WalkSpeed",   _HELP("current walking speed (including multipliers)")),
			OutputPortConfig<float>("CrouchSpeed", _HELP("current crouching speed (including multipliers)")),
			OutputPortConfig<float>("SwimSpeed",   _HELP("current swimming speed (including multipliers)")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Set/get actor parameters during runtime. Output always includes multipliers!");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (!pActInfo->pEntity)
					return;

				CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pActInfo->pEntity->GetId()));

				if (!pActor)
					return;

				SActorParams& actorParams = pActor->GetActorParams();

				if (IsPortActive(pActInfo, EIP_Set))
				{
					if (float jumpHeight = GetPortFloat(pActInfo, EIP_JumpHeight))
					{
						actorParams.jumpHeight = jumpHeight;
					}

					if (float walkSpeed = GetPortFloat(pActInfo, EIP_WalkSpeed))
					{
						pActor->SetStanceMaxSpeed(STANCE_STAND, walkSpeed);
					}

					if (float crouchSpeed = GetPortFloat(pActInfo, EIP_CrouchSpeed))
					{
						pActor->SetStanceMaxSpeed(STANCE_CROUCH, crouchSpeed);
					}

					if (float swimSpeed = GetPortFloat(pActInfo, EIP_SwimSpeed))
					{
						pActor->SetStanceMaxSpeed(STANCE_SWIM, swimSpeed);
					}
				}
				else if (IsPortActive(pActInfo, EIP_Get))
				{
					if (pActor->IsPlayer())
					{
						CPlayer* pPlayer = static_cast<CPlayer*>(pActor);

						ActivateOutput(pActInfo, EOP_JumpHeight, actorParams.jumpHeight);
						ActivateOutput(pActInfo, EOP_WalkSpeed, pPlayer->GetStanceMaxSpeed(STANCE_STAND));
						ActivateOutput(pActInfo, EOP_CrouchSpeed, pPlayer->GetStanceMaxSpeed(STANCE_CROUCH));
						ActivateOutput(pActInfo, EOP_SwimSpeed, pPlayer->GetStanceMaxSpeed(STANCE_SWIM));
					}
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
REGISTER_FLOW_NODE( "AI:BodyCount",		CFlowNode_AIBodyCount );
REGISTER_FLOW_NODE( "Actor:FacialAnim",		CFlowActorFacialAnim );
REGISTER_FLOW_NODE( "Actor:FacialExpression",		CFlowActorFacialExpression );
REGISTER_FLOW_NODE( "Actor:PlayerKnockDown",		CFlowNode_PlayerKnockDown );
REGISTER_FLOW_NODE( "Actor:PlayerInteractiveAnimation", CFlowNode_PlayerInteractiveAnimation );
REGISTER_FLOW_NODE( "Actor:PlayerLookAt",		CFlowNode_PlayerLookAt );
REGISTER_FLOW_NODE( "Actor:AliveCheck", CFlowActorAliveCheck);
REGISTER_FLOW_NODE( "Actor:PlayerCinematicControl", CFlowNode_PlayerCinematicControl);
REGISTER_FLOW_NODE( "Actor:KillPlayer",	CFlowNode_ActorKillPlayer );
REGISTER_FLOW_NODE( "Actor:SetPlayerModel",	CFlowNode_ActorSetPlayerModel );
REGISTER_FLOW_NODE( "Actor:PlayerDropObject", CFlowNode_PlayerDropObject );
REGISTER_FLOW_NODE( "Actor:GetNearestActor", CFlowNode_GetNearestActor );
REGISTER_FLOW_NODE("Actor:KillInfo", CFlowNode_ActorKill);
REGISTER_FLOW_NODE("Actor:PlayerIsInAir", CFlowPlayerIsInAir);
REGISTER_FLOW_NODE("Actor:RagdollizeCharacter", CFlowRagdollizeCharacter);
REGISTER_FLOW_NODE("Actor:LocalPlayerMovementParameter", CFlowActorMovementParameter);
