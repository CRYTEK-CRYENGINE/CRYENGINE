// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowSystem/FlowSystemCVars.h"

#include <CryFlowGraph/IFlowBaseNode.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryMovie/IMovieSystem.h>

class CPlaySequence_Node : public CFlowBaseNode<eNCT_Instanced>, public IMovieListener
{
	enum INPUTS
	{
		EIP_Sequence,
		EIP_Start,
		EIP_Pause,
		EIP_Stop,
		EIP_Precache,
		EIP_BreakOnStop,
		EIP_BlendPosSpeed,
		EIP_BlendRotSpeed,
		EIP_PerformBlendOut,
		EIP_StartTime,
		EIP_PlaySpeed,
		EIP_JumpToTime,
		EIP_TriggerJumpToTime,
		EIP_SequenceId, // deprecated
		EIP_TriggerJumpToEnd,
	};

	enum OUTPUTS
	{
		EOP_Started = 0,
		EOP_Done,
		EOP_Finished,
		EOP_Aborted,
		EOP_SequenceTime,
		EOP_CurrentSpeed,
	};

	typedef enum
	{
		PS_Stopped,
		PS_Playing,
		PS_Last
	} EPlayingState;

	_smart_ptr<IAnimSequence> m_pSequence;
	SActivationInfo           m_actInfo;
	EPlayingState             m_playingState;
	float                     m_currentTime;
	float                     m_currentSpeed;

public:
	CPlaySequence_Node(SActivationInfo* pActInfo)
	{
		m_pSequence = 0;
		m_actInfo = *pActInfo;
		m_playingState = PS_Stopped;
		m_currentSpeed = 0.0f;
		m_currentTime = 0.0f;
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	~CPlaySequence_Node()
	{
		if (m_pSequence)
		{
			IMovieSystem* pMovieSystem = gEnv->pMovieSystem;
			if (pMovieSystem)
			{
				pMovieSystem->RemoveMovieListener(m_pSequence, this);
			}
		}
	};

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CPlaySequence_Node(pActInfo);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");
		if (ser.IsWriting())
		{
			ser.EnumValue("m_playingState", m_playingState, PS_Stopped, PS_Last);
			ser.Value("curTime", m_currentTime);
			ser.Value("m_playSpeed", m_currentSpeed);
			bool bPaused = m_pSequence ? m_pSequence->IsPaused() : false;
			ser.Value("m_paused", bPaused);
		}
		else
		{
			EPlayingState playingState;

			{
				// for backward compatibility - TODO: remove
				bool playing = false;
				ser.Value("m_bPlaying", playing);
				playingState = playing ? PS_Playing : PS_Stopped;
				/// end remove
			}

			ser.EnumValue("m_playingState", playingState, PS_Stopped, PS_Last);

			ser.Value("curTime", m_currentTime);
			ser.Value("m_playSpeed", m_currentSpeed);

			bool bPaused;
			ser.Value("m_paused", bPaused);

			if (playingState == PS_Playing)
			{
				// restart sequence, possibly at the last frame
				StartSequence(pActInfo, m_currentTime, m_currentSpeed, false);

				if (m_pSequence && bPaused)
				{
					m_pSequence->Pause();
				}
			}
			else
			{
				// this unregisters only! because all sequences have been stopped already
				// by MovieSystem's Reset in GameSerialize.cpp
				StopSequence(pActInfo, true);
			}
		}
		ser.EndGroup();
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<string>("seq_Sequence_File", _HELP("Name of the Sequence"),                                                                      _HELP("Sequence")),
			InputPortConfig_Void("Trigger",              _HELP("Starts the sequence"),                                                                       _HELP("StartTrigger")),
			InputPortConfig_Void("Pause",                _HELP("Pauses the sequence"),                                                                       _HELP("PauseTrigger")),
			InputPortConfig_Void("Stop",                 _HELP("Stops the sequence"),                                                                        _HELP("StopTrigger")),
			InputPortConfig_Void("Precache",             _HELP("Precache keys that start in the first seconds of the animation. (Solves streaming issues)"), _HELP("PrecacheTrigger")),
			InputPortConfig<bool>("BreakOnStop",         false,                                                                                              _HELP("If set to 'true', stopping the sequence doesn't jump to end.")),
			InputPortConfig<float>("BlendPosSpeed",      0.0f,                                                                                               _HELP("Speed at which position gets blended into animation.")),
			InputPortConfig<float>("BlendRotSpeed",      0.0f,                                                                                               _HELP("Speed at which rotation gets blended into animation.")),
			InputPortConfig<bool>("PerformBlendOut",     false,                                                                                              _HELP("If set to 'true' the cutscene will blend out after it has finished to the new view (please reposition player when 'Started' happens).")),
			InputPortConfig<float>("StartTime",          0.0f,                                                                                               _HELP("Start time from which the sequence'll begin playing.")),
			InputPortConfig<float>("PlaySpeed",          1.0f,                                                                                               _HELP("Speed that this sequence plays at. 1.0 = normal speed, 0.5 = half speed, 2.0 = double speed.")),
			InputPortConfig<float>("JumpToTime",         0.0f,                                                                                               _HELP("Jump to a specific time in the sequence.")),
			InputPortConfig_Void("TriggerJumpToTime",    _HELP("Trigger the animation to jump to 'JumpToTime' seconds.")),
			InputPortConfig<int>("seqid_SequenceId",     0,                                                                                                  _HELP("ID of the Sequence"),                                                                                                                    _HELP("SequenceId (DEPRECATED)")),
			InputPortConfig_Void("TriggerJumpToEnd",     _HELP("Trigger the animation to jump to the end of the sequence.")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Started",      _HELP("Triggered when sequence is started")),
			OutputPortConfig_Void("Done",         _HELP("Triggered when sequence is stopped [either via StopTrigger or aborted via Code]"),_HELP("Done")),
			OutputPortConfig_Void("Finished",     _HELP("Triggered when sequence finished normally")),
			OutputPortConfig_Void("Aborted",      _HELP("Triggered when sequence is aborted (Stopped and BreakOnStop true or via Code)")),
			OutputPortConfig_Void("SequenceTime", _HELP("Current time of the sequence")),
			OutputPortConfig_Void("CurrentSpeed", _HELP("Speed at which the sequence is being played")),
			{ 0 }
		};
		config.sDescription = _HELP("Plays a Trackview Sequence");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Update:
			{
				UpdatePermanentOutputs();
				if (!gEnv->pMovieSystem || !m_pSequence)
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			}
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Stop))
				{
					bool bWasPlaying = m_playingState == PS_Playing;
					const bool bLeaveTime = GetPortBool(pActInfo, EIP_BreakOnStop);

					StopSequence(pActInfo, false, true, bLeaveTime);
					// we trigger manually, as we unregister before the callback happens

					if (bWasPlaying)
					{
						ActivateOutput(pActInfo, EOP_Done, true);    // signal we're done
						ActivateOutput(pActInfo, EOP_Aborted, true); // signal it's been aborted
						NotifyEntities();
					}
				}
				if (IsPortActive(pActInfo, EIP_Start))
				{
					StartSequence(pActInfo, GetPortFloat(pActInfo, EIP_StartTime), GetPortFloat(pActInfo, EIP_PlaySpeed));
				}
				if (IsPortActive(pActInfo, EIP_Pause))
				{
					PauseSequence(pActInfo);
				}
				if (IsPortActive(pActInfo, EIP_Precache))
				{
					PrecacheSequence(pActInfo, GetPortFloat(pActInfo, EIP_StartTime));
				}
				if (IsPortActive(pActInfo, EIP_TriggerJumpToTime))
				{
					if (gEnv->pMovieSystem && m_pSequence)
					{
						SAnimTime time(GetPortFloat(pActInfo, EIP_JumpToTime));
						time = clamp_tpl(time, m_pSequence->GetTimeRange().start, m_pSequence->GetTimeRange().end);
						gEnv->pMovieSystem->SetPlayingTime(m_pSequence, time);
					}
				}
				else if (IsPortActive(pActInfo, EIP_TriggerJumpToEnd))
				{
					if (gEnv->pMovieSystem && m_pSequence)
					{
						SAnimTime endTime = m_pSequence->GetTimeRange().end;
						gEnv->pMovieSystem->SetPlayingTime(m_pSequence, endTime);
					}
				}
				if (IsPortActive(pActInfo, EIP_PlaySpeed))
				{
					const float playSpeed = GetPortFloat(pActInfo, EIP_PlaySpeed);
					if (gEnv->pMovieSystem)
					{
						gEnv->pMovieSystem->SetPlayingSpeed(m_pSequence, playSpeed);
					}
				}
				break;
			}

		case eFE_Initialize:
			{
				StopSequence(pActInfo);
			}
			break;
		}
		;
	};

	virtual void OnMovieEvent(IMovieListener::EMovieEvent event, IAnimSequence* pSequence)
	{
		if (pSequence && pSequence == m_pSequence)
		{
			if (event == IMovieListener::eMovieEvent_Started)
			{
				m_playingState = PS_Playing;
			}
			else if (event == IMovieListener::eMovieEvent_Stopped)
			{
				ActivateOutput(&m_actInfo, EOP_Done, true);
				ActivateOutput(&m_actInfo, EOP_Finished, true);
				NotifyEntities();
				SequenceStopped();
			}
			else if (event == IMovieListener::eMovieEvent_Aborted)
			{
				SequenceAborted();
			}
			else if (event == IMovieListener::eMovieEvent_Updated)
			{
				m_currentTime = gEnv->pMovieSystem->GetPlayingTime(pSequence).ToFloat();
				m_currentSpeed = gEnv->pMovieSystem->GetPlayingSpeed(pSequence);
			}
		}
	}

protected:

	void SequenceAborted()
	{
		ActivateOutput(&m_actInfo, EOP_Done, true);
		ActivateOutput(&m_actInfo, EOP_Aborted, true);
		NotifyEntities();
		SequenceStopped();
	}

	void SequenceStopped()
	{
		UpdatePermanentOutputs();
		if (gEnv->pMovieSystem && m_pSequence)
		{
			gEnv->pMovieSystem->RemoveMovieListener(m_pSequence, this);
		}
		m_pSequence = 0;
		m_actInfo.pGraph->SetRegularlyUpdated(m_actInfo.myID, false);
		m_playingState = PS_Stopped;
	}

	void PrecacheSequence(SActivationInfo* pActInfo, float startTime = 0.0f)
	{
		IMovieSystem* movieSys = gEnv->pMovieSystem;
		if (movieSys)
		{
			IAnimSequence* pSequence = movieSys->FindSequence(GetPortString(pActInfo, EIP_Sequence));
			if (pSequence == NULL)
				pSequence = movieSys->FindSequenceById((uint32)GetPortInt(pActInfo, EIP_SequenceId));
			if (pSequence)
			{
				pSequence->PrecacheData(SAnimTime(startTime));
			}
		}
	}

	void StopSequence(SActivationInfo* pActInfo, bool bUnRegisterOnly = false, bool bAbort = false, bool bLeaveTime = false)
	{
		IMovieSystem* const pMovieSystem = gEnv->pMovieSystem;

		if (pMovieSystem && m_pSequence)
		{
			// we remove first to NOT get notified!
			pMovieSystem->RemoveMovieListener(m_pSequence, this);
			if (!bUnRegisterOnly && pMovieSystem->IsPlaying(m_pSequence))
			{
				if (bAbort) // stops sequence and leaves it at current position
				{
					pMovieSystem->AbortSequence(m_pSequence, bLeaveTime);
				}
				else
				{
					pMovieSystem->StopSequence(m_pSequence);
				}
			}
			SequenceStopped();
		}
	}

	void UpdatePermanentOutputs()
	{
		if (gEnv->pMovieSystem && m_pSequence)
		{
			const float currentTime = gEnv->pMovieSystem->GetPlayingTime(m_pSequence).ToFloat();
			const float currentSpeed = gEnv->pMovieSystem->GetPlayingSpeed(m_pSequence);
			ActivateOutput(&m_actInfo, EOP_SequenceTime, currentTime);
			ActivateOutput(&m_actInfo, EOP_CurrentSpeed, currentSpeed);
		}
	}

	void StartSequence(SActivationInfo* pActInfo, float curTime = 0.0f, float curSpeed = 1.0f, bool bNotifyStarted = true)
	{
		IMovieSystem* pMovieSystem = gEnv->pMovieSystem;
		if (!pMovieSystem)
		{
			return;
		}

		IAnimSequence* pSequence = pMovieSystem->FindSequence(GetPortString(pActInfo, EIP_Sequence));
		if (pSequence == NULL)
		{
			pSequence = pMovieSystem->FindSequenceById((uint32)GetPortInt(pActInfo, EIP_SequenceId));
		}

		// If sequence was changed in the meantime, stop old sequence first
		if (m_pSequence && pSequence != m_pSequence)
		{
			pMovieSystem->RemoveMovieListener(m_pSequence, this);
			pMovieSystem->StopSequence(m_pSequence);
		}

		m_pSequence = pSequence;

		if (m_pSequence)
		{
			if (m_pSequence->IsPaused())
			{
				m_pSequence->Resume();
				return;
			}

			m_pSequence->Resume();
			pMovieSystem->AddMovieListener(m_pSequence, this);
			pMovieSystem->PlaySequence(m_pSequence, NULL, true, false);
			pMovieSystem->SetPlayingTime(m_pSequence, SAnimTime(curTime));
			pMovieSystem->SetPlayingSpeed(m_pSequence, curSpeed);

			// set blend parameters only for tracks with Director Node, having camera track inside
			IViewSystem* pViewSystem = gEnv->pGameFramework->GetIViewSystem();
			if (pViewSystem && m_pSequence->GetActiveDirector())
			{
				bool bDirectorNodeHasCameraTrack = false;
				IAnimNode* pDirectorNode = m_pSequence->GetActiveDirector();

				if (pDirectorNode)
				{
					for (int trackId = 0; trackId < pDirectorNode->GetTrackCount(); ++trackId)
					{
						IAnimTrack* pTrack = pDirectorNode->GetTrackByIndex(trackId);
						if (pTrack && pTrack->GetParameterType() == eAnimParamType_Camera && pTrack->GetNumKeys() > 0)
						{
							bDirectorNodeHasCameraTrack = true;
							break;
						}
					}
				}

				if (bDirectorNodeHasCameraTrack)
				{
					float blendPosSpeed = GetPortFloat(pActInfo, EIP_BlendPosSpeed);
					float blendRotSpeed = GetPortFloat(pActInfo, EIP_BlendRotSpeed);
					bool performBlendOut = GetPortBool(pActInfo, EIP_PerformBlendOut);
					pViewSystem->SetBlendParams(blendPosSpeed, blendRotSpeed, performBlendOut);
				}
			}

			if (bNotifyStarted)
			{
				ActivateOutput(pActInfo, EOP_Started, true);
			}

			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
		}
		else
		{
			// sequence was not found -> hint
			GameWarning("[flow] Animations:PlaySequence: Sequence \"%s\" not found", GetPortString(pActInfo, 0).c_str());
			// maybe we should trigger the output, but if sequence is not found this should be an error
		}

		NotifyEntities();
		// this can happens when a timedemo is being recorded and the sequence is flagged as CUTSCENE
		if (m_pSequence && m_playingState == PS_Playing && !pMovieSystem->IsPlaying(m_pSequence))
		{
			SequenceAborted();
		}

		if (CFlowSystemCVars::Get().g_disableSequencePlayback)
		{
			if (gEnv->pMovieSystem && m_pSequence)
			{
				SAnimTime endTime = m_pSequence->GetTimeRange().end;
				gEnv->pMovieSystem->SetPlayingTime(m_pSequence, endTime);
			}
		}
	}

	void PauseSequence(SActivationInfo* pActInfo)
	{
		if (m_playingState != PS_Playing)
		{
			return;
		}

		IMovieSystem* pMovieSys = gEnv->pMovieSystem;
		if (!pMovieSys)
		{
			return;
		}

		if (m_pSequence)
		{
			m_pSequence->Pause();
		}
	}

	void NotifyEntityScript(IEntity* pEntity)
	{
		IScriptTable* pEntityScript = pEntity->GetScriptTable();
		if (pEntityScript)
		{
			if (m_playingState == PS_Playing && pEntityScript->HaveValue("OnSequenceStart"))
			{
				Script::CallMethod(pEntityScript, "OnSequenceStart");
			}
			else if (m_playingState == PS_Stopped && pEntityScript->HaveValue("OnSequenceStop"))
			{
				Script::CallMethod(pEntityScript, "OnSequenceStop");
			}
		}
	}

	void NotifyEntities()
	{
		IMovieSystem* pMovieSystem = gEnv->pMovieSystem;
		if (!pMovieSystem || !m_pSequence)
		{
			return;
		}

		int nodeCount = m_pSequence->GetNodeCount();
		for (int i = 0; i < nodeCount; ++i)
		{
			IAnimNode* pNode = m_pSequence->GetNode(i);
			if (pNode)
			{
				IAnimEntityNode* pAnimEntityNode = pNode->QueryEntityNodeInterface();
				if (pAnimEntityNode)
				{
					IEntity* pEntity = pAnimEntityNode->GetEntity();
					if (pEntity)
					{
						NotifyEntityScript(pEntity);
					}

					if (EntityGUID* guid = pAnimEntityNode->GetEntityGuid())
					{
						EntityId id = gEnv->pEntitySystem->FindEntityByGuid(*guid);
						if (id != 0)
						{
							IEntity* pEntity2 = gEnv->pEntitySystem->GetEntity(id);
							if (pEntity2)
							{
								NotifyEntityScript(pEntity2);
							}
						}
					}
				}
			}
		}
	}
};

REGISTER_FLOW_NODE("Animations:PlaySequence", CPlaySequence_Node);
