// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryAction.h"
#include "CryActionCVars.h"
#include "IActorSystem.h"
#include "GameObjects/GameObject.h"
#include "IGameRulesSystem.h"
#include "ILevelSystem.h"

#include <CryFlowGraph/IFlowBaseNode.h>

inline IActor* GetAIActor(IFlowNode::SActivationInfo* pActInfo)
{
	if (!pActInfo->pEntity)
		return 0;
	return CCryAction::GetCryAction()->GetIActorSystem()->GetActor(pActInfo->pEntity->GetId());
}

class CFlowPlayer : public CFlowBaseNode<eNCT_Singleton>
{
private:
	enum EInputPort
	{
		eInputPort_Update = 0,
	};

	enum EOuputPort
	{
		eOutputPort_LocalPlayerId = 0,
	};

public:
	CFlowPlayer(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("update", _HELP("Retriggers the entity id. Required for multiplayer")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<EntityId>("entityId", _HELP("Player entity id")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Outputs the local players entity id - NOT USABLE FOR MULTIPLAYER WITHOUT UPDATING BY HAND AFTER GAMESTART");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				bool done = UpdateEntityIdOutput(pActInfo);
				if (!done)
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
				break;
			}

		case eFE_Activate:
			UpdateEntityIdOutput(pActInfo);
			break;

		case eFE_Update:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			UpdateEntityIdOutput(pActInfo);
			break;
		}
	}

	bool UpdateEntityIdOutput(SActivationInfo* pActInfo)
	{
		EntityId playerId = CCryAction::GetCryAction()->GetClientEntityId();

		if (playerId != 0)
		{
			ActivateOutput(pActInfo, eOutputPort_LocalPlayerId, playerId);
			return true;
		}
		else
		{
			return false;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////

void SendFlowHitToEntity(EntityId targetId, EntityId shooterId, int damage, const Vec3& pos)
{
	if (IEntity* pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRulesEntity())
	{
		if (IScriptTable* pGameRulesScript = pGameRules->GetScriptTable())
		{
			IScriptSystem* pSS = pGameRulesScript->GetScriptSystem();
			if (pGameRulesScript->GetValueType("CreateHit") == svtFunction &&
			    pSS->BeginCall(pGameRulesScript, "CreateHit"))
			{
				pSS->PushFuncParam(pGameRulesScript);
				pSS->PushFuncParam(ScriptHandle(targetId));
				pSS->PushFuncParam(ScriptHandle(shooterId));
				pSS->PushFuncParam(ScriptHandle(0)); // weapon
				pSS->PushFuncParam(damage);
				pSS->PushFuncParam(0);        // radius
				pSS->PushFuncParam("");       // material
				pSS->PushFuncParam(0);        // partID
				pSS->PushFuncParam("normal"); // type
				pSS->PushFuncParam(pos);
				pSS->PushFuncParam(FORWARD_DIRECTION); // dir
				pSS->PushFuncParam(FORWARD_DIRECTION); // normal
				pSS->EndCall();
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////

class CFlowDamageActor : public CFlowBaseNode<eNCT_Singleton>
{
	enum
	{
		INP_TRIGGER = 0,
		INP_DAMAGE,
		INP_RELATIVEDAMAGE,
		INP_POSITION
	};

public:
	CFlowDamageActor(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Trigger",        _HELP("Trigger this port to actually damage the actor")),
			InputPortConfig<int>("damage",         0,                                                       _HELP("Amount of damage to exert when [Trigger] is activated"),                                                                           _HELP("Damage")),
			InputPortConfig<int>("damageRelative", 0,                                                       _HELP("Amount of damage to exert when [Trigger] is activated. Is relative to the maximun health of the actor. (100 = full max damage)"),  _HELP("DamageRelative")),
			InputPortConfig<Vec3>("Position",      Vec3(ZERO),                                              _HELP("Position of damage")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Damages attached entity by [Damage] when [Trigger] is activated");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, INP_TRIGGER))
			{
				IActor* pActor = GetAIActor(pActInfo);
				if (pActor)
				{
					int damage = GetPortInt(pActInfo, INP_DAMAGE);
					if (damage == 0)
						damage = (GetPortInt(pActInfo, INP_RELATIVEDAMAGE) * int(pActor->GetMaxHealth())) / 100;

					SendFlowHitToEntity(pActor->GetEntityId(), pActor->GetEntityId(), damage, GetPortVec3(pActInfo, INP_POSITION));
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

class CFlowDamageEntity : public CFlowBaseNode<eNCT_Singleton>
{
	enum
	{
		INP_TRIGGER = 0,
		INP_DAMAGE,
		INP_RELATIVEDAMAGE,
		INP_POSITION
	};

public:
	CFlowDamageEntity(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Trigger",        _HELP("Trigger this port to actually damage the actor")),
			InputPortConfig<int>("damage",         0,                                                       _HELP("Amount of damage to exert when [Trigger] is activated"),                                                                                                                                                             _HELP("Damage")),
			InputPortConfig<int>("damageRelative", 0,                                                       _HELP("Amount of damage to exert when [Trigger] is activated. Is relative to the maximun health of the entity. (100 = full max damage). /nwarning: This will only works for entities with a GetMaxHealth() lua function"),  _HELP("DamageRelative")),
			InputPortConfig<Vec3>("Position",      Vec3(ZERO),                                              _HELP("Position of damage")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Damages attached entity by [Damage] when [Trigger] is activated");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, INP_TRIGGER))
			{
				IEntity* pEntity = pActInfo->pEntity;
				if (pEntity)
				{
					int damage = GetPortInt(pActInfo, INP_DAMAGE);
					if (damage == 0)
					{
						int damageRelative = GetPortInt(pActInfo, INP_RELATIVEDAMAGE);
						if (IScriptTable* pScriptTable = pEntity->GetScriptTable())
						{
							IScriptSystem* pSS = pScriptTable->GetScriptSystem();
							if (pScriptTable->GetValueType("GetMaxHealth") == svtFunction && pSS->BeginCall(pScriptTable, "GetMaxHealth"))
							{
								pSS->PushFuncParam(pScriptTable);
								ScriptAnyValue result;
								if (pSS->EndCallAny(result))
								{
									int maxHealth = 0;
									if (result.CopyTo(maxHealth))
									{
										damage = (damageRelative * maxHealth) / 100;
									}
								}
							}
						}
					}

					SendFlowHitToEntity(pEntity->GetId(), pEntity->GetId(), damage, GetPortVec3(pActInfo, INP_POSITION));
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

class CFlowActorGrabObject : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowActorGrabObject(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<EntityId>("objectId", _HELP("Entity to grab")),
			InputPortConfig_Void("grab",          _HELP("Pulse this to grab object.")),
			InputPortConfig_Void("drop",          _HELP("Pulse this to drop currently grabbed object.")),
			InputPortConfig<bool>("throw",        _HELP("If true, object is thrown forward.")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<bool>("success",          _HELP("true if object was grabbed/dropped successfully")),
			OutputPortConfig<EntityId>("grabbedObjId", _HELP("Currently grabbed object, or 0")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Target entity will try to grab the input object, respectively drop/throw its currently grabbed object.");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				IActor* pActor = GetAIActor(pActInfo);
				if (!pActor)
					break;

				IEntity* pEnt = pActor->GetEntity();
				if (!pEnt)
					break;

				HSCRIPTFUNCTION func = 0;
				int ret = 0;
				IScriptTable* pTable = pEnt->GetScriptTable();
				if (!pTable)
					break;

				if (IsPortActive(pActInfo, 1) && pTable->GetValue("GrabObject", func))
				{
					IEntity* pObj = gEnv->pEntitySystem->GetEntity(GetPortEntityId(pActInfo, 0));
					if (pObj)
					{
						IScriptTable* pObjTable = pObj->GetScriptTable();
						Script::CallReturn(gEnv->pScriptSystem, func, pTable, pObjTable, ret);
					}
					ActivateOutput(pActInfo, 0, ret);
				}
				else if (IsPortActive(pActInfo, 2) && pTable->GetValue("DropObject", func))
				{
					bool bThrow = GetPortBool(pActInfo, 3);
					Script::CallReturn(gEnv->pScriptSystem, func, pTable, bThrow, ret);
					ActivateOutput(pActInfo, 0, ret);
				}

				if (pTable->GetValue("GetGrabbedObject", func))
				{
					ScriptHandle sH(0);
					Script::CallReturn(gEnv->pScriptSystem, func, pTable, sH);
					ActivateOutput(pActInfo, 1, EntityId(sH.n));
				}

				if (func)
					gEnv->pScriptSystem->ReleaseFunc(func);

				break;
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowActorGetHealth : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowActorGetHealth(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Trigger", _HELP("Trigger this port to get the current health")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<int>("Health", _HELP("Current health of entity")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Get health of an entity (actor)");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				IActor* pActor = GetAIActor(pActInfo);
				if (pActor)
				{
					ActivateOutput(pActInfo, 0, pActor->GetHealth());
				}
				else
				{
					GameWarning("CFlowActorGetHealth - No Entity or Entity not an Actor!");
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowActorSetHealth : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowActorSetHealth(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Trigger", _HELP("Trigger this port to set health")),
			InputPortConfig<float>("Value", _HELP("Health value to be set on entity when Trigger is activated")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<float>("Health", _HELP("Current health of entity (activated when Trigger is activated)")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Set health of an entity (actor)");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				IActor* pActor = GetAIActor(pActInfo);
				if (pActor)
				{
					if (!pActor->IsDead())
					{
						pActor->SetHealth(GetPortFloat(pActInfo, 1));
						ActivateOutput(pActInfo, 0, pActor->GetHealth()); // use pActor->GetHealth (might have been clamped to maxhealth]
					}
				}
				else
				{
					GameWarning("CFlowActorSetHealth - No Entity or Entity not an actor!");
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowActorCheckHealth : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowActorCheckHealth(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Trigger",     _HELP("Trigger this port to check health")),
			InputPortConfig<float>("MinHealth", 0.0f,                                       _HELP("Lower limit of range")),
			InputPortConfig<float>("MaxHealth", 100.0f,                                     _HELP("Upper limit of range")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<bool>("InRange", _HELP("True if Health is in range, False otherwise")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Check if health of entity (actor) is in range [MinHealth, MaxHealth]");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				IActor* pActor = GetAIActor(pActInfo);
				if (pActor)
				{
					float health = pActor->GetHealth();
					float minH = GetPortFloat(pActInfo, 1);
					float maxH = GetPortFloat(pActInfo, 2);
					ActivateOutput(pActInfo, 0, minH <= health && health <= maxH);
				}
				else
				{
					CryLogAlways("CFlowActorCheckHealth - No Entity or Entity not an Actor!");
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowGameObjectEvent : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowGameObjectEvent(SActivationInfo* pActInfo)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Trigger",       _HELP("Trigger this port to send the event")),
			InputPortConfig<string>("EventName",  _HELP("Name of event to send")),
			InputPortConfig<string>("EventParam", _HELP("Parameter of the event [event-specific]")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.sDescription = _HELP("Broadcast a game object event or send to a specific entity. EventParam is an event specific string");
		config.SetCategory(EFLN_ADVANCED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (eFE_Activate == event && IsPortActive(pActInfo, 0))
		{
			const string& eventName = GetPortString(pActInfo, 1);
			uint32 eventId = CCryAction::GetCryAction()->GetIGameObjectSystem()->GetEventID(eventName.c_str());
			SGameObjectEvent evt(eventId, eGOEF_ToAll, IGameObjectSystem::InvalidExtensionID, (void*) (GetPortString(pActInfo, 2).c_str()));
			if (pActInfo->pEntity == 0)
			{
				// broadcast to all gameobject events
				CCryAction::GetCryAction()->GetIGameObjectSystem()->BroadcastEvent(evt);
			}
			else
			{
				// send to a specific entity only
				if (CGameObject* pGameObject = (CGameObject*)pActInfo->pEntity->GetProxy(ENTITY_PROXY_USER))
					pGameObject->SendEvent(evt);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowGetSupportedGameRulesForMap : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_GET,
		IN_MAPNAME
	};
	enum EOutputs
	{
		OUT_DONE
	};

	CFlowGetSupportedGameRulesForMap(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Get",        _HELP("Fetch supported gamerules")),
			InputPortConfig<string>("Mapname", _HELP("map name")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<string>("GameRules", _HELP("Supported gamerules pipe-separated string")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Get supported gamerules for a map");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				ILevelInfo* pLevelInfo = NULL;
				string mapname = GetPortString(pActInfo, IN_MAPNAME);

				//If mapname is provided, try and fetch those rules
				if (strcmp(mapname, "") != 0)
					pLevelInfo = CCryAction::GetCryAction()->GetILevelSystem()->GetLevelInfo(mapname);

				if (pLevelInfo)
				{
					size_t numGameRules = pLevelInfo->GetGameRulesCount();
					std::vector<const char*> gameRules(numGameRules);
					numGameRules = pLevelInfo->GetGameRules(gameRules.data(), numGameRules);
					
					string outString = "";
					for (size_t i = 0; i < numGameRules; ++i)
					{
						if (i > 0)
							outString.append("|");
						outString.append(gameRules[i]);
					}
					ActivateOutput(pActInfo, OUT_DONE, outString);
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowGetStateOfEntity : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_GET,
		IN_MAPNAME
	};
	enum EOutputs
	{
		OUT_STATE
	};

	CFlowGetStateOfEntity(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Get", _HELP("Get state")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<string>("State", _HELP("Current state as string")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Get current state of an entity");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				string state = "invalid entity";
				IEntity* ent = pActInfo->pEntity;
				if (ent)
				{
					IEntityScriptComponent* pScriptProxy = ent->GetOrCreateComponent<IEntityScriptComponent>();
					state = "invalid script";
					if (pScriptProxy)
						state = pScriptProxy->GetState();
				}

				ActivateOutput(pActInfo, OUT_STATE, state);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowIsLevelOfType : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_CHECK,
		IN_TYPE
	};
	enum EOutputs
	{
		OUT_RESULT
	};

	CFlowIsLevelOfType(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Check", _HELP("Check if level is of given type")),
			InputPortConfig<string>("Type", "", _HELP("Type you want to check against"),0, _UICONFIG("enum_global:level_types")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<bool>("Result", _HELP("Result")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Check if a level is of given type");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				const string levelType = GetPortString(pActInfo, IN_TYPE);
				bool bResult = CCryAction::GetCryAction()->GetILevelSystem()->GetCurrentLevel()->IsOfType(levelType);

				ActivateOutput(pActInfo, OUT_RESULT, bResult);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowPauseGameUpdate : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_PAUSE,
		IN_UNPAUSE,
		IN_AFFECTS_ACTIONMAP,
		IN_QUERY
	};
	enum EOutputs
	{
		OUT_ISPAUSED,
		OUT_NOTPAUSED,
	};

	CFlowPauseGameUpdate(SActivationInfo * pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Pause", _HELP("Pauses game update")),
			InputPortConfig_Void("Unpause", _HELP("Unpauses game update")),
			InputPortConfig<bool>("AffectsActionMap", _HELP("If true, it will enable/disable action map manager when Upausing/pausing the game update")),
			InputPortConfig_Void("Query", _HELP("Query whether the game update is paused or unpaused")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("IsPaused", _HELP("The game update is paused")),
			OutputPortConfig_Void("NotPaused", _HELP("The game update is NOT paused")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("INPORTANT: USE THIS NODE IN UI FLOWGRAPH (e.g. UI_ACTIONS) NOT IN GAME FLOWGRAPH. This node allows pausing/unpausing the game update and querying its state.");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo *pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
		{
			const bool bAffectActionmap = GetPortBool(pActInfo, IN_AFFECTS_ACTIONMAP);
			if (IsPortActive(pActInfo, IN_PAUSE))
			{
				gEnv->pGameFramework->PauseGame(true, false);
				ActivateOutput(pActInfo, OUT_ISPAUSED, true);
				if (bAffectActionmap) { gEnv->pGameFramework->GetIActionMapManager()->Enable(false, true); }
			}
			else if (IsPortActive(pActInfo, IN_UNPAUSE))
			{
				gEnv->pGameFramework->PauseGame(false, false);
				ActivateOutput(pActInfo, OUT_NOTPAUSED, true);

				if (bAffectActionmap) { gEnv->pGameFramework->GetIActionMapManager()->Enable(true); }
			}
			else if (IsPortActive(pActInfo, IN_QUERY))
			{
				const bool bPaused = gEnv->pGameFramework->IsGamePaused();
				ActivateOutput(pActInfo, bPaused ? OUT_ISPAUSED : OUT_NOTPAUSED, true);
			}
		}
		break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer * s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("Actor:LocalPlayer", CFlowPlayer);
REGISTER_FLOW_NODE("Actor:Damage", CFlowDamageActor);
REGISTER_FLOW_NODE("Entity:Damage", CFlowDamageEntity)
REGISTER_FLOW_NODE("Actor:GrabObject", CFlowActorGrabObject);
REGISTER_FLOW_NODE("Actor:HealthGet", CFlowActorGetHealth);
REGISTER_FLOW_NODE("Actor:HealthSet", CFlowActorSetHealth);
REGISTER_FLOW_NODE("Actor:HealthCheck", CFlowActorCheckHealth);
REGISTER_FLOW_NODE("Game:ObjectEvent", CFlowGameObjectEvent);
REGISTER_FLOW_NODE("Game:GetSupportedGameRulesForMap", CFlowGetSupportedGameRulesForMap);
REGISTER_FLOW_NODE("Game:GetEntityState", CFlowGetStateOfEntity);
REGISTER_FLOW_NODE("Game:IsLevelOfType", CFlowIsLevelOfType);
REGISTER_FLOW_NODE("Game:PauseGameUpdate", CFlowPauseGameUpdate);
