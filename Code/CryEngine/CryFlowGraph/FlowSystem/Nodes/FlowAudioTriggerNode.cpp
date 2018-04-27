// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowNode_AudioTrigger final : public CFlowBaseNode<eNCT_Instanced>
{
public:

	explicit CFlowNode_AudioTrigger(SActivationInfo* pActInfo)
		: m_flags(eFlowNodeAudioTriggerFlags_None)
		, m_playTriggerId(CryAudio::InvalidControlId)
		, m_stopTriggerId(CryAudio::InvalidControlId)
		, m_requestUserData(CryAudio::ERequestFlags::None, this)
	{}

	//////////////////////////////////////////////////////////////////////////
	virtual ~CFlowNode_AudioTrigger() override
	{
		if ((m_flags & eFlowNodeAudioTriggerFlags_IsListener) > 0)
		{
			RemoveRequestListener();
		}
	}

	CFlowNode_AudioTrigger(CFlowNode_AudioTrigger const&) = delete;
	CFlowNode_AudioTrigger(CFlowNode_AudioTrigger&&) = delete;
	CFlowNode_AudioTrigger& operator=(CFlowNode_AudioTrigger const&) = delete;
	CFlowNode_AudioTrigger& operator=(CFlowNode_AudioTrigger&&) = delete;

	//////////////////////////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) override
	{
		return new CFlowNode_AudioTrigger(pActInfo);
	}

	//////////////////////////////////////////////////////////////////////////
	enum INPUTS
	{
		eIn_PlayTrigger,
		eIn_StopTrigger,
		eIn_Play,
		eIn_Stop,
	};

	enum OUTPUTS
	{
		eOut_Done,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<string>("audioTrigger_PlayTrigger", _HELP("name of the Play Trigger"),                                                                        "PlayTrigger"),
			InputPortConfig<string>("audioTrigger_StopTrigger", _HELP("name of the Stop Trigger"),                                                                        "StopTrigger"),

			InputPortConfig_Void("Play",                        _HELP("Executes the Play Trigger")),
			InputPortConfig_Void("Stop",                        _HELP("Executes the Stop Trigger if it is provided, o/w stops all events started by the Start Trigger")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig_Void("Done", _HELP("Activated when all of the triggered events have finished playing")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("This node executes Audio Triggers.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.SetCategory(EFLN_APPROVED);
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser) override
	{
		bool bPlay = ((m_flags & eFlowNodeAudioTriggerFlags_IsPlaying) > 0);
		ser.BeginGroup("FlowAudioTriggerNode");
		ser.Value("play", bPlay);
		ser.EndGroup();

		if (ser.IsReading())
		{
			Init(pActInfo, bPlay);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				Stop(pActInfo->pEntity);
				Init(pActInfo, false);

				break;
			}
		case eFE_ConnectOutputPort:
			{
				if ((m_flags & eFlowNodeAudioTriggerFlags_IsListener) == 0)
				{
					AddRequestListener();
				}

				break;
			}
		case eFE_DisconnectOutputPort:
			{
				if ((m_flags & eFlowNodeAudioTriggerFlags_IsListener) > 0)
				{
					RemoveRequestListener();
				}

				break;
			}
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIn_Stop))
				{
					Stop(pActInfo->pEntity);
				}

				if (IsPortActive(pActInfo, eIn_Play))
				{
					m_playActivationInfo = *pActInfo;
					Play(pActInfo->pEntity);
				}

				if (IsPortActive(pActInfo, eIn_PlayTrigger))
				{
					Stop(pActInfo->pEntity);
					GetTriggerID(pActInfo, eIn_PlayTrigger, m_playTriggerId);
				}

				if (IsPortActive(pActInfo, eIn_StopTrigger))
				{
					GetTriggerID(pActInfo, eIn_StopTrigger, m_stopTriggerId);
				}

				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}

	//////////////////////////////////////////////////////////////////////////
	void TriggerFinished(CryAudio::ControlId const audioTriggerId)
	{
		if (audioTriggerId == m_playTriggerId)
		{
			ActivateOutput(&m_playActivationInfo, eOut_Done, true);
		}
	}

private:

	enum EPlayMode : CryAudio::EnumFlagsType
	{
		ePlayMode_None = 0,
		ePlayMode_Play = 1,
		ePlayMode_PlayStop = 2,
		ePlayMode_ForceStop = 3,
	};

	enum EFlowNodeAudioTriggerFlags : CryAudio::EnumFlagsType
	{
		eFlowNodeAudioTriggerFlags_None = 0,
		eFlowNodeAudioTriggerFlags_IsPlaying = BIT(0),
		eFlowNodeAudioTriggerFlags_IsListener = BIT(1),
	};

	//////////////////////////////////////////////////////////////////////////
	static void OnAudioTriggerFinished(CryAudio::SRequestInfo const* const pAudioRequestInfo)
	{
		TFlowGraphId const id = static_cast<TFlowGraphId>(reinterpret_cast<UINT_PTR>(pAudioRequestInfo->pUserData));

		if (gEnv->pFlowSystem->GetGraphById(id) != nullptr)
		{
			// Here we are sure that this FlowGraphNode is still valid.
			CFlowNode_AudioTrigger* const pAudioTrigger = static_cast<CFlowNode_AudioTrigger*>(pAudioRequestInfo->pOwner);
			pAudioTrigger->TriggerFinished(pAudioRequestInfo->audioControlId);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void GetTriggerID(SActivationInfo* const pActInfo, uint32 const portIndex, CryAudio::ControlId& outTriggerId)
	{
		outTriggerId = CryAudio::InvalidControlId;
		string const& triggerName = GetPortString(pActInfo, portIndex);

		if (!triggerName.empty())
		{
			outTriggerId = CryAudio::StringToId(triggerName.c_str());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void Init(SActivationInfo* const pActInfo, bool const bPlay)
	{
		m_flags &= ~eFlowNodeAudioTriggerFlags_IsPlaying;

		if (gEnv->pAudioSystem != nullptr)
		{
			GetTriggerID(pActInfo, eIn_PlayTrigger, m_playTriggerId);
			GetTriggerID(pActInfo, eIn_StopTrigger, m_stopTriggerId);
		}

		if (IsOutputConnected(pActInfo, eOut_Done))
		{
			if ((m_flags & eFlowNodeAudioTriggerFlags_IsListener) == 0)
			{
				AddRequestListener();
			}
		}
		else if ((m_flags & eFlowNodeAudioTriggerFlags_IsListener) > 0)
		{
			RemoveRequestListener();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void ExecuteOnProxy(IEntity* const pIEntity, CryAudio::ControlId const audioTriggerId, EPlayMode const playMode)
	{
		IEntityAudioComponent* const pIEntityAudioComponent = pIEntity->GetOrCreateComponent<IEntityAudioComponent>();

		if (pIEntityAudioComponent != nullptr)
		{
			switch (playMode)
			{
			case ePlayMode_Play:
				{
					CryAudio::SRequestUserData const userData(CryAudio::ERequestFlags::DoneCallbackOnExternalThread, this, reinterpret_cast<void*>(static_cast<UINT_PTR>(m_playActivationInfo.pGraph->GetGraphId())), this);
					pIEntityAudioComponent->SetCurrentEnvironments();
					pIEntityAudioComponent->ExecuteTrigger(audioTriggerId, CryAudio::DefaultAuxObjectId, userData);

					break;
				}
			case ePlayMode_PlayStop:
				{
					pIEntityAudioComponent->SetCurrentEnvironments();
					pIEntityAudioComponent->ExecuteTrigger(audioTriggerId);

					break;
				}
			case ePlayMode_ForceStop:
				{
					pIEntityAudioComponent->StopTrigger(audioTriggerId);

					break;
				}
			default:
				{
					assert(false);// Unknown play mode!
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void ExecuteOnGlobalObject(CryAudio::ControlId const audioTriggerId, EPlayMode const playMode)
	{
		switch (playMode)
		{
		case ePlayMode_Play:
			gEnv->pAudioSystem->ExecuteTrigger(audioTriggerId, m_requestUserData);
			break;
		case ePlayMode_PlayStop:
			gEnv->pAudioSystem->ExecuteTrigger(audioTriggerId);
			break;
		case ePlayMode_ForceStop:
			gEnv->pAudioSystem->StopTrigger(audioTriggerId);
			break;
		default:
			// Unknown play mode!
			CRY_ASSERT(false);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void Play(IEntity* const pIEntity)
	{
		if (m_playTriggerId != CryAudio::InvalidControlId)
		{
			if (pIEntity != nullptr)
			{
				ExecuteOnProxy(pIEntity, m_playTriggerId, ePlayMode_Play);
			}
			else
			{
				ExecuteOnGlobalObject(m_playTriggerId, ePlayMode_Play);
			}

			m_flags |= eFlowNodeAudioTriggerFlags_IsPlaying;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void Stop(IEntity* const pIEntity)
	{
		if ((m_flags & eFlowNodeAudioTriggerFlags_IsPlaying) > 0)
		{
			if (m_stopTriggerId != CryAudio::InvalidControlId)
			{
				if (pIEntity != nullptr)
				{
					ExecuteOnProxy(pIEntity, m_stopTriggerId, ePlayMode_PlayStop);
				}
				else
				{
					ExecuteOnGlobalObject(m_stopTriggerId, ePlayMode_PlayStop);
				}
			}
			else
			{
				if (pIEntity != nullptr)
				{
					ExecuteOnProxy(pIEntity, m_playTriggerId, ePlayMode_ForceStop);
				}
				else
				{
					ExecuteOnGlobalObject(m_playTriggerId, ePlayMode_ForceStop);
				}
			}

			m_flags &= ~eFlowNodeAudioTriggerFlags_IsPlaying;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void AddRequestListener()
	{
		gEnv->pAudioSystem->AddRequestListener(&CFlowNode_AudioTrigger::OnAudioTriggerFinished, this, CryAudio::ESystemEvents::TriggerFinished);
		m_flags |= eFlowNodeAudioTriggerFlags_IsListener;
	}

	//////////////////////////////////////////////////////////////////////////
	void RemoveRequestListener()
	{
		gEnv->pAudioSystem->RemoveRequestListener(&CFlowNode_AudioTrigger::OnAudioTriggerFinished, this);
		m_flags &= ~eFlowNodeAudioTriggerFlags_IsListener;
	}

	//////////////////////////////////////////////////////////////////////////
	CryAudio::EnumFlagsType          m_flags;
	CryAudio::ControlId              m_playTriggerId;
	CryAudio::ControlId              m_stopTriggerId;
	SActivationInfo                  m_playActivationInfo;
	CryAudio::SRequestUserData const m_requestUserData;
};

REGISTER_FLOW_NODE("Audio:Trigger", CFlowNode_AudioTrigger);
