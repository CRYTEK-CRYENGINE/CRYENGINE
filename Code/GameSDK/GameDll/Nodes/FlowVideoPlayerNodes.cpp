// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Game.h"
#include "GameActions.h"

#include <CryFlowGraph/IFlowBaseNode.h>
#include <CrySystem/Scaleform/IFlashUI.h>
#include <CryGame/IGameFramework.h>

//----------------------------------- Video Player --------------------------------------

class CFlowFlashVideoPlayerNode
	: public CFlowBaseNode<eNCT_Instanced>
	  , public IUIElementEventListener
	  , public IActionListener
{
public:
	CFlowFlashVideoPlayerNode(SActivationInfo* pActInfo)
		: m_pElement(NULL)
		, m_bPlaying(false)
	{
		m_pActionMapMan = gEnv->pGameFramework->GetIActionMapManager();
		m_pActionMapMan->AddExtraActionListener(this);

		if (s_actionHandler.GetNumHandlers() == 0)
			s_actionHandler.AddHandler(g_pGame->Actions().ui_skip_video, &CFlowFlashVideoPlayerNode::OnActionSkipVideo);
	}

	virtual ~CFlowFlashVideoPlayerNode()
	{
		if (m_pElement)
			m_pElement->RemoveEventListener(this);
		if (gEnv->pGameFramework && gEnv->pGameFramework->GetIActionMapManager())
			gEnv->pGameFramework->GetIActionMapManager()->RemoveExtraActionListener(this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Play",            _HELP("Display USMPlayer and start playback")),
			InputPortConfig_Void("Stop",            _HELP("Stop playback and hide USMPlayer")),
			InputPortConfig_Void("Pause",           _HELP("Pause playback")),
			InputPortConfig_Void("Resume",          _HELP("Resume playback")),
			InputPortConfig<int>("InstanceID",      -1,                                                                       _HELP("Instance ID of USMPlayer (e.g. to use USMPlayer on dynamic textures)")),
			InputPortConfig<string>("VideoFile",    _HELP("Name of usm file, file should be placed in Libs/UI/ or subfolder"),_HELP("VideoFile"),                                                                       _UICONFIG("dt=file")),
			InputPortConfig<bool>("Transparent",    false,                                                                    _HELP("If player background is transparent or not")),
			InputPortConfig<bool>("Loop",           false,                                                                    _HELP("If true, video playback loops")),
			InputPortConfig<bool>("Skipable",       true,                                                                     _HELP("If true, player can skip video by pressing on of the skip keys (see ActionMap)")),
			InputPortConfig<int>("AudioChannel",    0,                                                                        _HELP("Audio channel")),
			InputPortConfig<int>("SubtitleChannel", 0,                                                                        _HELP("Subtitle channel")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("OnPlay",          _HELP("Triggered once the video started")),
			OutputPortConfig<bool>("OnStop",         _HELP("Triggered once the video stopped. True if the video was finished, false if skipped")),
			OutputPortConfig_Void("OnPause",         _HELP("Triggered once the video is paused")),
			OutputPortConfig_Void("OnResume",        _HELP("Triggered once the video resumes")),
			OutputPortConfig_Void("OnLooped",        _HELP("Triggered once the video looped and start again")),
			OutputPortConfig_Void("OnVideoNotFound", _HELP("Triggered on Video was not found")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Video player node that is using USMPlayer UIElement");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_bPlaying = false;
			m_bSkipable = false;
			m_ActInfo = *pActInfo;

			if (m_pElement)
			{
				m_pElement->Unload();
				m_pElement->SetVisible(false);
				m_pElement->RemoveEventListener(this);
			}

			m_pElement = NULL;
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, eI_Play) || (m_bPlaying && IsPortActive(pActInfo, eI_VideoFile)))
			{
				if (gEnv->pFlashUI && !m_pElement)
				{
					m_pElement = gEnv->pFlashUI->GetUIElement("USMPlayer");
					int instanceId = GetPortInt(pActInfo, eI_InstanceID);
					if (m_pElement && instanceId > -1)
						m_pElement = m_pElement->GetInstance(instanceId);
					if (m_pElement)
					{
						m_pElement->AddEventListener(this, "CFlowVideoPlayerNode");
					}
					else
					{
						gEnv->pLog->LogError("CFlowVideoPlayerNode: UIElement \"USMPlayer\" not found!");
					}
				}
				if (!m_pElement)
					return;

				string video_file = GetPortString(pActInfo, eI_VideoFile);
				if (video_file[0] != '/' && video_file[0] != '\\')
				{
					video_file = '/' + video_file;
				}

				m_pElement->CallFunction("SetMoviePath", SUIArguments::Create(video_file));
				m_pElement->CallFunction("SetLoopMode", SUIArguments::Create(GetPortBool(pActInfo, eI_Loop)));
				m_pElement->CallFunction("SelectAudioChannel", SUIArguments::Create(GetPortInt(pActInfo, eI_AudioChannel)));
				m_pElement->CallFunction("SelectSubtitleChannel", SUIArguments::Create(GetPortInt(pActInfo, eI_SubtitleChannel)));

				const bool bTransparent = GetPortBool(pActInfo, eI_Transparent);
				m_pElement->CallFunction("SetBGAlpha", bTransparent ? SUIArguments::Create(0.f) : SUIArguments::Create(100.f));
				m_pElement->SetAlpha(bTransparent ? 0.f : 100.f);

				m_bSkipable = GetPortBool(pActInfo, eI_Skipable);

				m_pElement->SetVisible(true);
				m_pElement->CallFunction("Play");
				m_bPlaying = true;
			}
			if (m_pElement)
			{
				if (m_bPlaying)
				{
					if (IsPortActive(pActInfo, eI_Stop))
					{
						m_pElement->CallFunction("Stop");
						m_pElement->Unload();
						m_pElement->SetVisible(false);
					}
					if (IsPortActive(pActInfo, eI_Pause))
					{
						m_pElement->CallFunction("Pause");
					}
					if (IsPortActive(pActInfo, eI_Resume))
					{
						m_pElement->CallFunction("Resume");
					}
					if (IsPortActive(pActInfo, eI_Transparent))
					{
						const bool bTransparent = GetPortBool(pActInfo, eI_Transparent);
						m_pElement->CallFunction("SetBGAlpha", bTransparent ? SUIArguments::Create(0.f) : SUIArguments::Create(100.f));
						m_pElement->SetAlpha(bTransparent ? 0.f : 100.f);
					}
					if (IsPortActive(pActInfo, eI_Skipable))
					{
						m_bSkipable = GetPortBool(pActInfo, eI_Skipable);
					}
					if (IsPortActive(pActInfo, eI_Loop))
					{
						m_pElement->CallFunction("SetLoopMode", SUIArguments::Create(GetPortBool(pActInfo, eI_Loop)));
					}
					if (IsPortActive(pActInfo, eI_AudioChannel))
					{
						m_pElement->CallFunction("SelectAudioChannel", SUIArguments::Create(GetPortInt(pActInfo, eI_AudioChannel)));
					}
					if (IsPortActive(pActInfo, eI_SubtitleChannel))
					{
						m_pElement->CallFunction("SelectSubtitleChannel", SUIArguments::Create(GetPortInt(pActInfo, eI_SubtitleChannel)));
					}
				}

			}
			break;
		case eFE_Update:
			if (m_pElement)
			{
				m_pElement->Unload();
				m_pElement->SetVisible(false);
			}
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowFlashVideoPlayerNode(pActInfo);
	}

	// IUIElementEventListener
	virtual void OnUIEvent(IUIElement* pSender, const SUIEventDesc& event, const SUIArguments& args)
	{
		assert(pSender == m_pElement);

		if (strcmp(event.sDisplayName, "OnPlay") == 0)
		{
			ActivateOutput(&m_ActInfo, eO_OnPlay, true);
		}
		else if (strcmp(event.sDisplayName, "OnPause") == 0)
		{
			ActivateOutput(&m_ActInfo, eO_OnPause, true);
		}
		else if (strcmp(event.sDisplayName, "OnResume") == 0)
		{
			ActivateOutput(&m_ActInfo, eO_OnResume, true);
		}
		else if (strcmp(event.sDisplayName, "OnStop") == 0)
		{
			if (m_bPlaying)
			{
				m_bPlaying = false;
				UnloadPlayerNextFrame();

				bool bFinished;
				args.GetArg(0, bFinished);
				ActivateOutput(&m_ActInfo, eO_OnStop, bFinished);
			}
		}
		else if (strcmp(event.sDisplayName, "OnLooped") == 0)
		{
			ActivateOutput(&m_ActInfo, eO_OnLooped, true);
		}
		else if (strcmp(event.sDisplayName, "OnFileNotFound") == 0)
		{
			m_bPlaying = false;
			UnloadPlayerNextFrame();
			ActivateOutput(&m_ActInfo, eO_OnStop, false);
			ActivateOutput(&m_ActInfo, eO_OnVideoNotFound, true);
		}
	}

	virtual void OnUnload(IUIElement* pSender)
	{
		assert(pSender == m_pElement);
		if (m_bPlaying)
			ActivateOutput(&m_ActInfo, eO_OnStop, false);
		m_bPlaying = false;
	}

	virtual void OnInstanceDestroyed(IUIElement* pSender, IUIElement* pDeletedInstance)
	{
		if (pDeletedInstance == m_pElement)
		{
			if (m_bPlaying)
				ActivateOutput(&m_ActInfo, eO_OnStop, false);
			m_pElement = NULL;
			m_bPlaying = false;
		}
	}
	// ~IUIElementEventListener

	void UnloadPlayerNextFrame()
	{
		if (m_pElement) m_pElement->SetVisible(false);
		m_ActInfo.pGraph->SetRegularlyUpdated(m_ActInfo.myID, true);
	}

	// IUIInputExtraListener
	virtual void OnAction(const ActionId& action, int activationMode, float value)
	{
		s_actionHandler.Dispatch(this, 0, action, activationMode, value);
	}
	// ~IUIInputExtraListener

	bool OnActionSkipVideo(EntityId entityId, const ActionId& actionId, int activationMode, float value)
	{
		if (m_pElement && m_bPlaying && m_bSkipable && activationMode == eAAM_OnPress)
		{
			m_pElement->CallFunction("Stop");
			m_pElement->Unload();
			m_pElement->SetVisible(false);
		}
		return false;
	}

private:
	IUIElement*        m_pElement;
	bool               m_bPlaying;
	bool               m_bSkipable;
	SActivationInfo    m_ActInfo;
	IActionMapManager* m_pActionMapMan;
	static TActionHandler<CFlowFlashVideoPlayerNode> s_actionHandler;

	enum EInputPorts
	{
		eI_Play = 0,
		eI_Stop,
		eI_Pause,
		eI_Resume,
		eI_InstanceID,
		eI_VideoFile,
		eI_Transparent,
		eI_Loop,
		eI_Skipable,
		eI_AudioChannel,
		eI_SubtitleChannel
	};

	enum EOutputPorts
	{
		eO_OnPlay = 0,
		eO_OnStop,
		eO_OnPause,
		eO_OnResume,
		eO_OnLooped,
		eO_OnVideoNotFound,
	};
};

TActionHandler<CFlowFlashVideoPlayerNode> CFlowFlashVideoPlayerNode::s_actionHandler;

REGISTER_FLOW_NODE("Video:VideoPlayback", CFlowFlashVideoPlayerNode);
