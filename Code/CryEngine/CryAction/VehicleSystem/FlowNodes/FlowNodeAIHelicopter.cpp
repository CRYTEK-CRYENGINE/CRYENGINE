// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryFlowGraph/IFlowBaseNode.h>

#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIActor.h>

namespace
{
void AISendSignal(IEntity* const pEntity, const char* const signalName)
{
	CRY_ASSERT(pEntity);
	CRY_ASSERT(signalName);

	IAIObject* const pAiObject = pEntity->GetAI();
	if (pAiObject)
	{
		const uint32 signalNameCrc = CCrc32::Compute(signalName);
		gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER, 1, signalName, pAiObject, NULL, signalNameCrc);
	}
}
}

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Helicopter_EnableCombatMode
	: public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		eInputPort_Enable,
		eInputPort_Disable,
	};

public:
	CFlowNode_Helicopter_EnableCombatMode(SActivationInfo* pActInfo)
	{
	}

	~CFlowNode_Helicopter_EnableCombatMode()
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inPorts[] =
		{
			InputPortConfig_Void("Enable"),
			InputPortConfig_Void("Disable"),
			{ 0 }
		};

		static const SOutputPortConfig outPorts[] =
		{
			{ 0 }
		};

		config.pInputPorts = inPorts;
		config.pOutputPorts = outPorts;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("When enabled, combat mode changes the behaviour from trying to strictly follow paths to trying to find the best position in the path from where to engage the target. The default state is disabled.");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		IEntity* pEntity = pActInfo->pEntity;
		if (pEntity == NULL)
		{
			return;
		}

		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, eInputPort_Enable))
			{
				AISendSignal(pEntity, "CombatTargetEnabled");
			}
			else if (IsPortActive(pActInfo, eInputPort_Disable))
			{
				AISendSignal(pEntity, "CombatTargetDisabled");
			}
			break;
		}
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Helicopter_EnableFiring
	: public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		eInputPort_Enable,
		eInputPort_Disable,
	};

public:
	CFlowNode_Helicopter_EnableFiring(SActivationInfo* pActInfo)
	{
	}

	~CFlowNode_Helicopter_EnableFiring()
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inPorts[] =
		{
			InputPortConfig_Void("Enable"),
			InputPortConfig_Void("Disable"),
			{ 0 }
		};

		static const SOutputPortConfig outPorts[] =
		{
			{ 0 }
		};

		config.pInputPorts = inPorts;
		config.pOutputPorts = outPorts;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("When enabled in combination with combat mode the AI is allowed to shoot at the target. If disabled it just tactically positions itself to places from where it can see/hit the target. The default state is enabled.");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		IEntity* pEntity = pActInfo->pEntity;
		if (pEntity == NULL)
		{
			return;
		}

		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, eInputPort_Enable))
			{
				AISendSignal(pEntity, "FiringAllowed");
			}
			else if (IsPortActive(pActInfo, eInputPort_Disable))
			{
				AISendSignal(pEntity, "FiringNotAllowed");
			}
			break;
		}
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Helicopter_FollowPath
	: public CFlowBaseNode<eNCT_Instanced>
	, public IEntityEventListener
{
	enum INPUTS
	{
		eInputPort_Start,
		eInputPort_Stop,
		eInputPort_PathName,
		eInputPort_Loop,
		eInputPort_Speed,
	};

	enum OUTPUTS
	{
		eOutputPort_ArrivedAtEnd,
		eOutputPort_ArrivedNearToEnd,
		eOutputPort_Stopped,
	};

public:
	CFlowNode_Helicopter_FollowPath(SActivationInfo* pActInfo)
		: m_scriptEventEntityId(0)
		, m_activateArrivedAtPathEnd(false)
		, m_activateArrivedCloseToPathEnd(false)
		, m_activateStopped(false)
		, m_restartOnPostSerialize(false)
	{
	}

	~CFlowNode_Helicopter_FollowPath()
	{
		UnregisterAsPathNotificationListener();
	}

	virtual IFlowNodePtr Clone(SActivationInfo *pActInfo)
	{
		return new CFlowNode_Helicopter_FollowPath(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inPorts[] =
		{
			InputPortConfig_Void("Start"),
			InputPortConfig_Void("Stop"),
			InputPortConfig<string>("PathName"),
			InputPortConfig<bool>("LoopPath",   false),
			InputPortConfig<float>("Speed",     30),
			{ 0 }
		};

		static const SOutputPortConfig outPorts[] =
		{
			OutputPortConfig_Void("ArrivedAtEnd"),
			OutputPortConfig_Void("ArrivedNearToEnd"),
			OutputPortConfig_Void("Stopped"),
			{ 0 }
		};

		config.pInputPorts = inPorts;
		config.pOutputPorts = outPorts;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Set the path the AI should follow. When not in combat mode it will follow as accurately as possible from start to end. When in combat mode it represents the path along where the AI is allowed to position itself/patrol.");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		IEntity* pEntity = pActInfo->pEntity;
		if (pEntity == NULL)
		{
			return;
		}

		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, eInputPort_Start))
			{
				NotifyCurrentListenerFollowPathStopped(pEntity);

				const bool loop = GetPortBool(pActInfo, eInputPort_Loop);
				StartFollowPath(pActInfo, loop ? eSFPLM_StartFromClosestPathLocation : eSFPLM_StartFromBeginningOfPath);
			}
			else if (IsPortActive(pActInfo, eInputPort_Stop))
			{
				ActivateOutput(pActInfo, eOutputPort_Stopped, true);
				AISendSignal(pEntity, "StopFollowPath");
				Reset(pActInfo);
			}
			else if (IsPortActive(pActInfo, eInputPort_Speed))
			{
				ProcessEventSpeedPortActivated(pActInfo);
			}
			break;

		case eFE_Update:
			{
				bool reset = false;
				if (m_activateArrivedCloseToPathEnd)
				{
					m_activateArrivedCloseToPathEnd = false;
					ActivateOutput(pActInfo, eOutputPort_ArrivedNearToEnd, true);
					// Don't reset, let's still keep listening for end or stop.
				}

				if (m_activateArrivedAtPathEnd)
				{
					m_activateArrivedAtPathEnd = false;
					ActivateOutput(pActInfo, eOutputPort_ArrivedAtEnd, true);
					reset = true;
				}

				if (m_activateStopped)
				{
					m_activateStopped = false;
					ActivateOutput(pActInfo, eOutputPort_Stopped, true);
					reset = true;
				}

				if (reset)
				{
					Reset(pActInfo);
				}
			}
			break;
		}
	}

	void Reset(SActivationInfo* pActInfo)
	{
		UnregisterAsPathNotificationListener();

		m_activateArrivedAtPathEnd = false;
		m_activateArrivedCloseToPathEnd = false;
		m_activateStopped = false;

		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);

		m_restartOnPostSerialize = false;
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("m_restartOnPostSerialize", m_restartOnPostSerialize);
	}

	virtual void PostSerialize(SActivationInfo* pActInfo)
	{
		if (m_restartOnPostSerialize)
		{
			StartFollowPath(pActInfo, eSFPLM_StartFromClosestPathLocation);
		}
	}

	virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
	{
		if (event.event != ENTITY_EVENT_SCRIPT_EVENT)
		{
			return;
		}

		const char* eventName = reinterpret_cast<const char*>(event.nParam[0]);
		const uint32 eventNameCrc = CCrc32::Compute(eventName);

		static const uint32 s_arrivedAtPathEndCrc = CCrc32::Compute("ArrivedAtPathEnd");
		static const uint32 s_arrivedCloseToPathEndCrc = CCrc32::Compute("ArrivedCloseToPathEnd");
		static const uint32 s_pathFollowingStoppedCrc = CCrc32::Compute("PathFollowingStopped");

		if (eventNameCrc == s_arrivedAtPathEndCrc)
		{
			m_activateArrivedAtPathEnd = true;
		}
		else if (eventNameCrc == s_arrivedCloseToPathEndCrc)
		{
			m_activateArrivedCloseToPathEnd = true;
		}
		else if (eventNameCrc == s_pathFollowingStoppedCrc)
		{
			// Ignore event because the decision to stop the path following doesn't come from a different flownode, so it
			// means that the path hasn't changed so we will continue following it later (likely when exiting combat mode)
		}
	}

private:
	void RegisterAsPathNotificationListener(const EntityId entityId)
	{
		UnregisterAsPathNotificationListener();

		if (entityId)
		{
			CFlowNode_Helicopter_FollowPath* pNode = GetCurrentFollowPathNode(entityId);
			if (pNode)
			{
				pNode->UnregisterAsPathNotificationListener();
			}
			SetCurrentFollowPathNode(entityId, this);

			m_scriptEventEntityId = entityId;
			gEnv->pEntitySystem->AddEntityEventListener(m_scriptEventEntityId, ENTITY_EVENT_SCRIPT_EVENT, this);
		}
	}

	void UnregisterAsPathNotificationListener()
	{
		if (m_scriptEventEntityId)
		{
			CFlowNode_Helicopter_FollowPath* pNode = GetCurrentFollowPathNode(m_scriptEventEntityId);
			if (pNode == this)
			{
				SetCurrentFollowPathNode(m_scriptEventEntityId, NULL);
			}

			gEnv->pEntitySystem->RemoveEntityEventListener(m_scriptEventEntityId, ENTITY_EVENT_SCRIPT_EVENT, this);
			m_scriptEventEntityId = 0;
		}
	}

	static CFlowNode_Helicopter_FollowPath* GetCurrentFollowPathNode(const EntityId entityId)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
		if (pEntity)
		{
			IScriptTable* pScriptTable = pEntity->GetScriptTable();
			if (pScriptTable)
			{
				ScriptAnyValue value;
				const bool getValueSuccess = pScriptTable->GetValueAny("Helicopter_FlowNodeFollowPath", value);
				if (getValueSuccess)
				{
					return reinterpret_cast<CFlowNode_Helicopter_FollowPath*>(value.GetScriptHandle().ptr);
				}
			}
		}
		return NULL;
	}

	static void SetCurrentFollowPathNode(const EntityId entityId, CFlowNode_Helicopter_FollowPath* pNode)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
		if (pEntity)
		{
			IScriptTable* pScriptTable = pEntity->GetScriptTable();
			if (pScriptTable)
			{
				ScriptAnyValue value;
				value.SetScriptHandle(pNode);
				pScriptTable->SetValueAny("Helicopter_FlowNodeFollowPath", value);
			}
		}
	}

	void NotifyCurrentListenerFollowPathStopped(IEntity* pEntity)
	{
		assert(pEntity);
		const EntityId entityId = pEntity->GetId();
		CFlowNode_Helicopter_FollowPath* pNode = GetCurrentFollowPathNode(entityId);
		if (pNode && pNode != this)
		{
			pNode->m_activateStopped = true;
		}
	}

	void ProcessEventSpeedPortActivated(SActivationInfo* pActInfo)
	{
		assert(pActInfo != NULL);

		IEntity* pEntity = pActInfo->pEntity;
		if (pEntity == NULL)
		{
			return;
		}

		IAIObject* pAiObject = pEntity->GetAI();
		IF_UNLIKELY (pAiObject == NULL)
		{
			return;
		}

		IAIActor* pAiActor = pAiObject->CastToIAIActor();
		IF_UNLIKELY (pAiActor == NULL)
		{
			return;
		}

		IScriptTable* pEntityTable = pEntity->GetScriptTable();
		IF_UNLIKELY (pEntityTable == NULL)
		{
			return;
		}

		const float speed = GetPortFloat(pActInfo, eInputPort_Speed);

		pEntityTable->SetValue("Helicopter_Speed", speed);
	}

	enum StartFollowPathLocationMode
	{
		eSFPLM_StartFromBeginningOfPath,
		eSFPLM_StartFromClosestPathLocation
	};

	void StartFollowPath(SActivationInfo* pActInfo, const StartFollowPathLocationMode startPathLocationMode)
	{
		IEntity* pEntity = pActInfo->pEntity;
		IF_UNLIKELY (pEntity == NULL)
		{
			return;
		}

		Reset(pActInfo);

		IScriptTable* pEntityTable = pEntity->GetScriptTable();
		IF_UNLIKELY (pEntityTable == NULL)
		{
			return;
		}

		IAIObject* pAiObject = pEntity->GetAI();
		IF_UNLIKELY (pAiObject == NULL)
		{
			return;
		}

		IAIActor* pAiActor = pAiObject->CastToIAIActor();
		IF_UNLIKELY (pAiActor == NULL)
		{
			return;
		}

		const EntityId entityId = pEntity->GetId();
		RegisterAsPathNotificationListener(entityId);

		const string& pathName = GetPortString(pActInfo, eInputPort_PathName);
		const bool loop = GetPortBool(pActInfo, eInputPort_Loop);
		const float speed = GetPortFloat(pActInfo, eInputPort_Speed);

		pAiActor->SetPathToFollow(pathName.c_str());

		pEntityTable->SetValue("Helicopter_Loop", loop);
		pEntityTable->SetValue("Helicopter_Speed", speed);

		const bool startFromClosestLocation = (startPathLocationMode == eSFPLM_StartFromClosestPathLocation);
		pEntityTable->SetValue("Helicopter_StartFromClosestLocation", startFromClosestLocation);

		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);

		AISendSignal(pEntity, "StartFollowPath");

		m_restartOnPostSerialize = true;
	}

private:
	EntityId m_scriptEventEntityId;
	bool     m_activateArrivedAtPathEnd;
	bool     m_activateArrivedCloseToPathEnd;
	bool     m_activateStopped;

	bool     m_restartOnPostSerialize;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Helicopter_ForceFire
	: public CFlowBaseNode<eNCT_Singleton>
	  , public IEntityEventListener
{
	enum INPUTS
	{
		eInputPort_Enable,
		eInputPort_Disable,
		eInputPort_Target,
	};

	enum OUTPUTS
	{
		eOutputPort_Finished,
	};

public:
	CFlowNode_Helicopter_ForceFire(SActivationInfo* pActInfo)
		: m_scriptEventEntityId(0)
		, m_activateFinished(false)
	{
	}

	~CFlowNode_Helicopter_ForceFire()
	{
		UnregisterScriptEventListener();
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inPorts[] =
		{
			InputPortConfig_Void("Enable"),
			InputPortConfig_Void("Disable"),
			InputPortConfig<EntityId>("Target"),
			{ 0 }
		};

		static const SOutputPortConfig outPorts[] =
		{
			OutputPortConfig_Void("Finished"),
			{ 0 }
		};

		config.pInputPorts = inPorts;
		config.pOutputPorts = outPorts;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Forces the attention target of the fligh ai to a specific entity.");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		IEntity* pEntity = pActInfo->pEntity;
		if (pEntity == NULL)
		{
			return;
		}

		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, eInputPort_Enable))
			{
				IScriptTable* pEntityTable = pEntity->GetScriptTable();
				IF_UNLIKELY (pEntityTable == NULL)
				{
					return;
				}

				const EntityId targetEntityId = GetPortEntityId(pActInfo, eInputPort_Target);
				pEntityTable->SetValue("Helicopter_ForcedTargetId", targetEntityId);

				RegisterScriptEventlistener(pEntity->GetId());
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);

				AISendSignal(pEntity, "StartForceFire");
			}
			else if (IsPortActive(pActInfo, eInputPort_Disable))
			{
				AISendSignal(pEntity, "StopForceFire");
			}
			break;

		case eFE_Update:
			{
				if (m_activateFinished)
				{
					ActivateOutput(pActInfo, eOutputPort_Finished, true);
					Reset(pActInfo);
				}
			}
			break;
		}
	}

	virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
	{
		if (event.event != ENTITY_EVENT_SCRIPT_EVENT)
		{
			return;
		}

		const char* eventName = reinterpret_cast<const char*>(event.nParam[0]);
		const uint32 eventNameCrc = CCrc32::Compute(eventName);

		static const uint32 s_forceAttentionTargetFinishedCrc = CCrc32::Compute("ForceAttentionTargetFinished");

		if (eventNameCrc == s_forceAttentionTargetFinishedCrc)
		{
			m_activateFinished = true;
		}
	}

private:
	void RegisterScriptEventlistener(const EntityId entityId)
	{
		UnregisterScriptEventListener();

		if (entityId)
		{
			m_scriptEventEntityId = entityId;
			gEnv->pEntitySystem->AddEntityEventListener(m_scriptEventEntityId, ENTITY_EVENT_SCRIPT_EVENT, this);
		}
	}

	void UnregisterScriptEventListener()
	{
		if (m_scriptEventEntityId)
		{
			gEnv->pEntitySystem->RemoveEntityEventListener(m_scriptEventEntityId, ENTITY_EVENT_SCRIPT_EVENT, this);
			m_scriptEventEntityId = 0;
		}
	}

	void Reset(SActivationInfo* pActInfo)
	{
		m_activateFinished = false;
		UnregisterScriptEventListener();
		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
	}

private:
	EntityId m_scriptEventEntityId;
	bool     m_activateFinished;
};

REGISTER_FLOW_NODE("Vehicle:Helicopter:FollowPath", CFlowNode_Helicopter_FollowPath);
REGISTER_FLOW_NODE("Vehicle:Helicopter:EnableCombatMode", CFlowNode_Helicopter_EnableCombatMode);
REGISTER_FLOW_NODE("Vehicle:Helicopter:EnableFiring", CFlowNode_Helicopter_EnableFiring);
REGISTER_FLOW_NODE("Vehicle:Helicopter:ForceFire", CFlowNode_Helicopter_ForceFire);
