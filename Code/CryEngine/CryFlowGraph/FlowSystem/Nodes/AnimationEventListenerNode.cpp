// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

// Listens for a specific animation event and triggers the output.
class AnimationEventListenerNode : public CFlowBaseNode<eNCT_Instanced>, public IEntityEventListener
{
	enum INPUTS
	{
		EIP_Enable = 0,
		EIP_Disable,
		EIP_EventName,
		EIP_Once,
	};

	enum OUTPUTS
	{
		EOP_Enabled = 0,
		EOP_Disabled,
		EOP_EventTriggered,
	};

public:
	AnimationEventListenerNode(SActivationInfo* pActInfo)
	{
		Reset(*pActInfo);
	};

	~AnimationEventListenerNode()
	{
		StopListeningForAnimationEvents();
	};

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new AnimationEventListenerNode(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		if (ser.IsReading())
		{
			bool enabled = false;
			ser.Value("enabled", enabled);
			enabled ? Enable() : Disable();
		}
		else if (ser.IsWriting())
		{
			ser.Value("enabled", m_enabled);
		}
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Enable",       _HELP("Start listening for animation events.")),
			InputPortConfig_Void("Disable",      _HELP("Stop listening for animation events.")),
			InputPortConfig<string>("EventName", _HELP("Name of the animation event to listen for. When received it will trigger the corresponding output.")),
			InputPortConfig<bool>("Once",        true,                                                                                                        _HELP("If this is set to true the node will automatically be disabled after the animation event has been received.")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Enabled",        _HELP("Triggered when animation event received.")),
			OutputPortConfig_Void("Disabled",       _HELP("Triggered when animation event received.")),
			OutputPortConfig_Void("EventTriggered", _HELP("Triggered when animation event received.")),
			{ 0 }
		};

		config.sDescription = _HELP("Listens for a specific animation event and triggers the output.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		m_actInfo = *pActInfo;

		switch (event)
		{
		case eFE_Initialize:
			{
				Reset(*pActInfo);
				break;
			}

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Enable))
					Enable();

				if (IsPortActive(pActInfo, EIP_Disable))
					Disable();

				break;
			}
		}
		;
	};

	// Overriding IEntityEventListener::OnEntityEvent
	virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& entityEvent)
	{
		if (!m_enabled)
			return;

		assert(entityEvent.event == ENTITY_EVENT_ANIM_EVENT);

		if (entityEvent.event == ENTITY_EVENT_ANIM_EVENT)
		{
			const AnimEventInstance* pAnimEvent = reinterpret_cast<const AnimEventInstance*>(entityEvent.nParam[0]);
			ICharacterInstance* pCharacter = reinterpret_cast<ICharacterInstance*>(entityEvent.nParam[1]);

			if (pAnimEvent && pCharacter)
			{
				const bool matchingEvent = stricmp(pAnimEvent->m_EventName, GetEventName().c_str()) == 0;
				if (matchingEvent)
				{
					ActivateOutput(&m_actInfo, EOP_EventTriggered, true);

					const bool autoDisableOnceTriggered = GetPortBool(&m_actInfo, EIP_Once);
					if (autoDisableOnceTriggered)
					{
						Disable();
					}
				}
			}
		}
	}

private:
	void Reset(SActivationInfo& actInfo)
	{
		m_actInfo = actInfo;
		m_entityIdUsedForEventListener = 0;
		m_enabled = false;
	}

	void Enable()
	{
		m_enabled = true;

		ActivateOutput(&m_actInfo, EOP_Enabled, true);

		if (IEntity* pEntity = m_actInfo.pEntity)
			StartListeningForAnimationEvents(pEntity->GetId());
		else
			CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_ERROR, "Trigger input port 'Enable' on AnimationEventListener flow node without an entity.");
	}

	void Disable()
	{
		m_enabled = false;
		ActivateOutput(&m_actInfo, EOP_Disabled, true);
		StopListeningForAnimationEvents();
	}

	void StartListeningForAnimationEvents(const EntityId entityId)
	{
		m_entityIdUsedForEventListener = entityId;
		gEnv->pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_ANIM_EVENT, this);
	}

	void StopListeningForAnimationEvents()
	{
		if (m_entityIdUsedForEventListener != 0)
		{
			gEnv->pEntitySystem->RemoveEntityEventListener(m_entityIdUsedForEventListener, ENTITY_EVENT_ANIM_EVENT, this);
			m_entityIdUsedForEventListener = 0;
		}
	}

	string GetEventName()
	{
		return GetPortString(&m_actInfo, EIP_EventName);
	}

private:
	SActivationInfo m_actInfo;
	EntityId        m_entityIdUsedForEventListener;
	bool            m_enabled;
};

REGISTER_FLOW_NODE("Animations:AnimationEventListener", AnimationEventListenerNode);
