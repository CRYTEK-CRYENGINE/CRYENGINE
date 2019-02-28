// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ManualFrameStep.h"

#include <CryGame/IGameFramework.h>
#include <CryRenderer/IRenderAuxGeom.h>

#if MANUAL_FRAME_STEP_ENABLED

#if CRY_PLATFORM_WINDOWS
	#define MANUAL_FRAME_STEP_VIEW_HISTORY 1
#else
	#define MANUAL_FRAME_STEP_VIEW_HISTORY 0
#endif

#if MANUAL_FRAME_STEP_VIEW_HISTORY
	#include <CryCore/Platform/CryWindows.h>
#endif

/*static*/ void CManualFrameStepController::SCVars::Register()
{
	REGISTER_CVAR2("g_manualFrameStepFrequency", &manualFrameStepFrequency, 60.0f, VF_NET_SYNCED, "Manually step through frames with a fixed time step");
}

/*static*/ void CManualFrameStepController::SCVars::Unregister()
{
	if (gEnv->pConsole)
	{
		gEnv->pConsole->UnregisterVariable("g_manualFrameStepFrequency", true);
	}
}

float CManualFrameStepController::SCVars::manualFrameStepFrequency = 0.0f;

//////////////////////////////////////////////////////////////////////////

CManualFrameStepController::CManualFrameStepController()
	: m_framesLeft(-1)
	, m_framesGenerated(0)
	, m_pendingRequest(false)
	, m_previousStepSmoothing(true)
	, m_previousFixedStep(0.0f)
	, m_heldTimer(-1.0f)
{
	if (gEnv->pInput)
	{
		gEnv->pInput->AddEventListener(this);
	}
	if (gEnv->pSystem)
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CManualFrameStepController");
	}

	SCVars::Register();
}

CManualFrameStepController::~CManualFrameStepController()
{
	if (gEnv->pInput)
	{
		gEnv->pInput->RemoveEventListener(this);
	}
	if (gEnv->pSystem)
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	}

	SCVars::Unregister();
}

EManualFrameStepResult CManualFrameStepController::Update()
{
	const EManualFrameStepResult result = (m_framesLeft != 0) ? EManualFrameStepResult::Continue : EManualFrameStepResult::Block;

	if (m_framesLeft > 0)
	{
		--m_framesLeft;
		++m_framesGenerated;

		DisplayDebugInfo();
	}

	if (result == EManualFrameStepResult::Block)
	{
		// Manually update core systems which have to be ticked even when blocking the main loop
		if (gEnv->pInput)
		{
			gEnv->pInput->Update(true);
		}

		if (gEnv->pNetwork)
		{
			gEnv->pNetwork->SyncWithGame(eNGS_FrameStart);
			gEnv->pNetwork->SyncWithGame(eNGS_FrameEnd);
		}

		if (!gEnv->IsEditor() && gEnv->pRenderer)
		{
			gEnv->pSystem->PumpWindowMessage(true, gEnv->pRenderer->GetHWND());
		}
	}
	else if (m_pendingRequest)
	{
		m_pendingRequest = false;
	}

	return result;
}

void CManualFrameStepController::DefineProtocol(IProtocolBuilder* pBuilder)
{
	pBuilder->AddMessageSink(this, GetProtocolDef(), GetProtocolDef());
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CManualFrameStepController, OnNetworkMessage, eNRT_ReliableOrdered, eMPF_NoSendDelay)
{
	ProcessCommand(param.numFrames);

	return true;
}

bool CManualFrameStepController::IsEnabled() const
{
	return m_framesLeft >= 0;
}

void CManualFrameStepController::Enable(const bool enable)
{
	if (IsEnabled() && enable)
	{
		// Already enabled: early out
		return;
	}

	m_framesLeft = enable ? 0 : -1;
	m_framesGenerated = 0;

	// Set fixed step
	if (ICVar* const pFixedStep = gEnv->pConsole->GetCVar("t_FixedStep"))
	{
		if (enable)
		{
			m_previousFixedStep = pFixedStep->GetFVal();
			pFixedStep->Set(1.0f / SCVars::manualFrameStepFrequency);
		}
		else
		{
			pFixedStep->Set(m_previousFixedStep);
		}
	}

	// Disable framerate smoothing
	if (ICVar* const pSmoothing = gEnv->pConsole->GetCVar("t_Smoothing"))
	{
		if (enable)
		{
			m_previousStepSmoothing = (pSmoothing->GetIVal() != 0);
			pSmoothing->Set(false);
		}
		else
		{
			pSmoothing->Set(m_previousStepSmoothing);
		}
	}

	// And capture output folder
	stack_string framesFolder;
	GetFramesFolder(framesFolder);

	if (!gEnv->IsDedicated())
	{
		if (ICVar* const pCaptureFrames = gEnv->pConsole->GetCVar("capture_frames"))
		{
			if (ICVar* const pCaptureFramesFolder = gEnv->pConsole->GetCVar("capture_folder"))
			{
				if (enable)
				{
					pCaptureFramesFolder->Set(framesFolder.c_str());
					pCaptureFrames->Set(1);
				}
				else
				{
					pCaptureFramesFolder->Set("CaptureOutput");
					pCaptureFrames->Set(0);
				}
			}
		}
	}
}

void CManualFrameStepController::GenerateRequest(const uint8 numFrames)
{
	if (gEnv->bServer)
	{
		// Process and sync to clients
		ProcessCommand(numFrames);
	}
	else
	{
		SNetMessage netMessage(numFrames);
		NetRequestStepToServer(netMessage);
	}
}

void CManualFrameStepController::ProcessCommand(const uint8 numFrames)
{
	switch (numFrames)
	{
	case kDisableMask:
		Enable(false);
		break;
	case kEnableMask:
		Enable(true);
		DisplayDebugInfo();
		break;
	default:
		Enable(true);
		m_framesLeft += numFrames;
		break;
	}

	SNetMessage netMessage(numFrames);
	NetSyncClients(netMessage);
}

void CManualFrameStepController::NetSyncClients(const SNetMessage& netMessage)
{
	if (gEnv->bMultiplayer && gEnv->bServer)
	{
		if (IGameServerNub* pGameServerNub = gEnv->pGameFramework->GetIGameServerNub())
		{
			INetSendablePtr pNetMessage = new CSimpleNetMessage<SNetMessage>(netMessage, CManualFrameStepController::OnNetworkMessage);

			pGameServerNub->AddSendableToRemoteClients(pNetMessage, 0, nullptr, nullptr);

			CryLog("[ManualFrameStepController] Client Sync sent. Frames = %u", netMessage.numFrames);
		}
	}
}

void CManualFrameStepController::NetRequestStepToServer(const SNetMessage& netMessage)
{
	if (!gEnv->bServer)
	{
		if(IGameClientNub* pGameClientNub = gEnv->pGameFramework->GetIGameClientNub())
		{
			if (INetChannel* pGameClientChannel = pGameClientNub->GetNetChannel())
			{
				INetSendablePtr pNetMessage = new CSimpleNetMessage<SNetMessage>(netMessage, CManualFrameStepController::OnNetworkMessage);
				pGameClientChannel->AddSendable(pNetMessage, 0, NULL, NULL);
			}
		}
	}
}

bool CManualFrameStepController::OnInputEvent(const SInputEvent& inputEvent)
{
	if ((SCVars::manualFrameStepFrequency < FLT_EPSILON) || (eIDT_Keyboard != inputEvent.deviceType))
		return false;

	const float kKeyHeldThreshold = 0.5f;

	switch (inputEvent.keyId)
	{
	case eKI_Pause:
		if ((eIS_Pressed == inputEvent.state) && (inputEvent.modifiers & eMM_Shift))
		{
			GenerateRequest(IsEnabled() ? kDisableMask : kEnableMask);
		}
		break;
	case eKI_Right:
		if (IsEnabled())
		{
			if (eIS_Released == inputEvent.state)
			{
				m_heldTimer = -1.0f;
			}
			else if (eIS_Down == inputEvent.state)
			{
				const float currTime = gEnv->pSystem->GetITimer()->GetAsyncTime().GetSeconds();
				if (m_heldTimer < 0.0f)
				{
					GenerateRequest(1);
					m_heldTimer = currTime;
				}
				else if (((currTime - m_heldTimer) >= kKeyHeldThreshold) && !m_framesLeft)
				{
					// Defer "held" requests in order to stay in sync with the server.
					// We don't want to send more commands than the server can process
					if (!m_pendingRequest)
					{
						GenerateRequest(1);
					}
					m_pendingRequest = true;
				}
			}
		}
		break;
	#if MANUAL_FRAME_STEP_VIEW_HISTORY
	case eKI_Left:
		{
			// Open last generated frames
			if (IsEnabled() && eIS_Pressed == inputEvent.state)
			{
				const ICVar* const pCaptureFormat = gEnv->pConsole->GetCVar("capture_file_format");
				const char* szFileFormat = pCaptureFormat ? pCaptureFormat->GetString() : "jpg";

				const ICVar* const pUserPrefix = gEnv->pConsole->GetCVar("capture_file_prefix");
				const char* szUserPrefix = pUserPrefix ? pUserPrefix->GetString() : "";
				const char* szRealPrefix = szUserPrefix[0] ? szUserPrefix : "Frame";

				const stack_string& lastFrameName = stack_string().Format("%s%06d.%s", szRealPrefix, m_framesGenerated, szFileFormat);

				stack_string framesFolder;
				GetFramesFolder(framesFolder);

				char buffer[ICryPak::g_nMaxPath];
				const char* szAdjustedFramesFolder = gEnv->pCryPak->AdjustFileName(framesFolder.c_str(), buffer, ICryPak::FLAGS_FOR_WRITING);

				char szBuffer[MAX_PATH + 256];
				cry_sprintf(szBuffer, "explorer.exe %s\\%s", szAdjustedFramesFolder, lastFrameName.c_str());

				STARTUPINFO startupInfo;
				ZeroMemory(&startupInfo, sizeof(startupInfo));
				startupInfo.cb = sizeof(startupInfo);

				PROCESS_INFORMATION processInformation;
				ZeroMemory(&processInformation, sizeof(processInformation));

				CreateProcess(NULL, szBuffer, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInformation);
			}
			break;
		}
	#endif // MANUAL_FRAME_STEP_VIEW_HISTORY
	}

	return false;
}

void CManualFrameStepController::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED:
	{
		if (wparam == 0) //! 0: Game -> Editor, 1: Editor -> Game
		{
			Enable(false);
		}
		break;
	}
	}
}

/*static*/ void CManualFrameStepController::GetFramesFolder(stack_string& outFolder)
{
	outFolder = "%USER%/ManualStepFrames_";
	if (IEntity* const pClientEntity = gEnv->pGameFramework->GetClientEntity())
	{
		outFolder.append(pClientEntity->GetName());
	}
	else
	{
		outFolder.append("Default");
	}
}

/*static*/ void CManualFrameStepController::DisplayDebugInfo()
{
	if (gEnv->pRenderer)
	{
		const int w = IRenderAuxGeom::GetAux()->GetCamera().GetViewSurfaceX();
		const int h = IRenderAuxGeom::GetAux()->GetCamera().GetViewSurfaceZ();

		float x = float(0) + float(w / 2) - 110.0f;
		float y = float(0) + float(h) - 70.0f;

		IRenderAuxText::Draw2dLabel(x, y, 2.0f, Col_Red, false, "ManualFrameStep active");
		x += 25.0f;
		y += 20.0f;

		IRenderAuxText::Draw2dLabel(x, y, 1.3f, Col_White, false, "'Shift+Pause' to toggle");
		y += 12.0f;

		IRenderAuxText::Draw2dLabel(x, y, 1.3f, Col_White, false, "'Right' to generate a new frame");
		y += 12.0f;

		IRenderAuxText::Draw2dLabel(x, y, 1.3f, Col_White, false, "'Left' to view history");
		y += 12.0f;
	}
}

#endif // MANUAL_FRAME_STEP_ENABLED
