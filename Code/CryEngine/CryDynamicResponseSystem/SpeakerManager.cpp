// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SpeakerManager.h"
#include "ResponseSystem.h"

#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include <CryEntitySystem/IEntityProxy.h>
#include <CryAnimation/IAttachment.h>
#include <CrySystem/IConsole.h>
#include <CryRenderer/IRenderer.h>
#include <CryGame/IGameFramework.h>
#include <CryGame/IGame.h>
#include "DialogLineDatabase.h"

namespace
{
static const uint32 s_attachmentPosName = CCrc32::ComputeLowercase("voice");
static const CHashedString s_isTalkingVariableName = "IsTalking";
static const CHashedString s_lastLineId = "LastSpokenLine";
static const int s_characterSlot = 0;  //todo: make this a property of the drs actor
}

using namespace CryDRS;

//--------------------------------------------------------------------------------------------------
CSpeakerManager::CSpeakerManager() : m_pActiveSpeakerVariable(nullptr)
{
	//the audio callbacks we are interested in, all related to audio-asset (trigger or standaloneFile) finished or failed to start
	gEnv->pAudioSystem->AddRequestListener(&CSpeakerManager::OnAudioCallback, this, eAudioRequestType_AudioCallbackManagerRequest, eAudioCallbackManagerRequestType_ReportFinishedTriggerInstance | eAudioCallbackManagerRequestType_ReportStoppedFile | eAudioCallbackManagerRequestType_ReportStartedFile);
	gEnv->pAudioSystem->AddRequestListener(&CSpeakerManager::OnAudioCallback, this, eAudioRequestType_AudioObjectRequest, eAudioObjectRequestType_ExecuteTrigger | eAudioObjectRequestType_PlayFile);

	m_audioRtpcIdGlobal = INVALID_AUDIO_CONTROL_ID;
	m_audioRtpcIdLocal = INVALID_AUDIO_CONTROL_ID;
	m_numActiveSpeaker = 0;

	REGISTER_CVAR2("drs_dialogSubtitles", &m_displaySubtitlesCVar, 0, VF_NULL, "Toggles use of subtitles for dialog lines on and off.\n");
	REGISTER_CVAR2("drs_dialogAudio", &m_playAudioCVar, 1, VF_NULL, "Toggles playback of audio files for dialog lines on and off.\n");
	REGISTER_CVAR2("drs_dialogsSamePriorityCancels", &m_samePrioCancelsLinesCVar, 1, VF_NULL, "If a new line is started with the same priority as a already running line, that line will be canceled.");

	m_pDrsDialogDialogRunningEntityRtpcName = REGISTER_STRING("drs_dialogEntityRtpcName", "", 0, "name of the rtpc on the entity to set to 1 when someone is speaking");
	m_pDrsDialogDialogRunningGlobalRtpcName = REGISTER_STRING("drs_dialogGlobalRtpcName", "", 0, "name of the global rtpc to set to 1 when someone is speaking");

	m_pLipsyncProvider = nullptr;
	m_pDefaultLipsyncProvider = nullptr;
}

//--------------------------------------------------------------------------------------------------
CSpeakerManager::~CSpeakerManager()
{
	delete(m_pDefaultLipsyncProvider);

	gEnv->pAudioSystem->RemoveRequestListener(nullptr, this);  //remove all listener-callback-functions from this object

	gEnv->pConsole->UnregisterVariable("drs_dialogSubtitles", true);
	gEnv->pConsole->UnregisterVariable("drs_dialogAudio", true);
	gEnv->pConsole->UnregisterVariable("drs_dialogsLinesWithSamePriorityCancel", true);

	if (m_pDrsDialogDialogRunningEntityRtpcName)
	{
		m_pDrsDialogDialogRunningEntityRtpcName->Release();
		m_pDrsDialogDialogRunningEntityRtpcName = nullptr;
	}
	if (m_pDrsDialogDialogRunningGlobalRtpcName)
	{
		m_pDrsDialogDialogRunningGlobalRtpcName->Release();
		m_pDrsDialogDialogRunningGlobalRtpcName = nullptr;
	}
}

//--------------------------------------------------------------------------------------------------
void CSpeakerManager::Init()
{
	CVariableCollection* pGlobalVariableCollection = CResponseSystem::GetInstance()->GetCollection("Global");
	CRY_ASSERT(pGlobalVariableCollection);
	m_pActiveSpeakerVariable = pGlobalVariableCollection->CreateVariable("ActiveSpeakers", 0);

	if (!m_pLipsyncProvider)
	{
		m_pDefaultLipsyncProvider = new CDefaultLipsyncProvider;
		m_pLipsyncProvider = m_pDefaultLipsyncProvider;
	}

	if (m_pDrsDialogDialogRunningEntityRtpcName)
	{
		gEnv->pAudioSystem->GetAudioRtpcId(m_pDrsDialogDialogRunningEntityRtpcName->GetString(), m_audioRtpcIdLocal);
	}
	if (m_pDrsDialogDialogRunningGlobalRtpcName)
	{
		gEnv->pAudioSystem->GetAudioRtpcId(m_pDrsDialogDialogRunningGlobalRtpcName->GetString(), m_audioRtpcIdGlobal);
	}
}

//--------------------------------------------------------------------------------------------------
void CSpeakerManager::Update()
{
	SET_DRS_USER_SCOPED("SpeakerManager Update");
	if (m_activeSpeakers.empty())
	{
		SetNumActiveSpeaker(0);
		return;
	}

	const float currentTime = GetISystem()->GetITimer()->GetCurrTime();
	const float fColorBlue[4] = { 0.4f, 0.4f, 1.0f, 1.0f };
	float currentPos2Y = 10.0f;

	//draw text and flag finished speakers
	for (SpeakerList::iterator it = m_activeSpeakers.begin(), itEnd = m_activeSpeakers.end(); it != itEnd; ++it)
	{
		if (m_pLipsyncProvider && it->endingConditions & eEC_WaitingForLipsync)
		{
			if (!m_pLipsyncProvider->Update(it->lipsyncId, it->pActor, it->pPickedLine))
			{
				it->endingConditions &= ~eEC_WaitingForLipsync;
			}
		}

		if (currentTime - it->finishTime >= 0)
		{
			it->endingConditions &= ~eEC_WaitingForTimer;
		}
		else if (gEnv->pEntitySystem && gEnv->pRenderer)
		{
			CResponseActor* pActor = it->pActor;
			if (pActor)
			{
				IEntity* pEntity = pActor->GetLinkedEntity();
				if (pEntity)
				{
					if (it->speechAuxProxy != DEFAULT_AUDIO_PROXY_ID)
					{
						UpdateAudioProxyPosition(pEntity, *it);
					}

					if (m_displaySubtitlesCVar != 0)
					{
						if (gEnv->pGame && gEnv->pGame->GetIGameFramework() && gEnv->pGame->GetIGameFramework()->GetClientActorId() == pEntity->GetId())
						{
							gEnv->pRenderer->Draw2dLabel(8.0f, currentPos2Y, 2.0f, fColorBlue, false, "%s", it->text.c_str());
							currentPos2Y += 10.0f;
						}
						else
						{
							gEnv->pRenderer->DrawLabelEx(pEntity->GetWorldPos() + Vec3(0.0f, 0.0f, 2.0f), 2.0f, fColorBlue, true, true, "%s", it->text.c_str());
						}
					}
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_DRS, VALIDATOR_WARNING, "An DRS-actor without an Entity detected: LineText: '%s', Actor: '%s'\n", (!it->text.empty()) ? it->text.c_str() : "no text", pActor->GetName().GetText().c_str());
				}
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_DRS, VALIDATOR_WARNING, "An active speaker without a DRS-Actor detected: LineText: '%s' \n", (!it->text.empty()) ? it->text.c_str() : "no text");
			}
		}
	}

	//remove finished speakers (and start queued lines, if any)
	for (SpeakerList::iterator it = m_activeSpeakers.begin(); it != m_activeSpeakers.end(); )
	{
		if (it->endingConditions == eEC_Done)
		{
			//one line has finished, lets see if someone was waiting for this
			for (QueuedSpeakerList::iterator itQueued = m_queuedSpeakers.begin(); itQueued != m_queuedSpeakers.end(); ++itQueued)
			{
				if (itQueued->pSpeechThatWeWaitingFor == &*it)
				{
					itQueued->pSpeechThatWeWaitingFor = nullptr;  //our code, to start them, after we are done with this loop
				}
			}
			ReleaseSpeakerAudioProxy(*it, true);
			it->pActor->GetLocalVariables()->SetVariableValue(s_isTalkingVariableName, false);

			if (m_pLipsyncProvider)
			{
				m_pLipsyncProvider->OnLineEnded(it->lipsyncId, it->pActor, it->pPickedLine);
			}

			InformListener(it->pActor, it->lineID, DRS::ISpeakerManager::IListener::eLineEvent_Finished, it->pPickedLine);
			it = m_activeSpeakers.erase(it);
		}
		else
		{
			++it;
		}
	}

	//now we start all queued lines, that are ready to be started
	for (QueuedSpeakerList::iterator itQueued = m_queuedSpeakers.begin(), itQueuedEnd = m_queuedSpeakers.end(); itQueued != itQueuedEnd; ++itQueued)
	{
		const SWaitingInfo& currentQueuedInfo = *itQueued;
		if (currentQueuedInfo.pSpeechThatWeWaitingFor == nullptr)
		{
			StartSpeaking(currentQueuedInfo.pActor, currentQueuedInfo.lineID);
			m_queuedSpeakers.erase(itQueued);
			return; //we only start one queued line per update
		}
	}
}

//--------------------------------------------------------------------------------------------------
bool CSpeakerManager::IsSpeaking(const DRS::IResponseActor* pActor, const CHashedString& lineID /*= CHashedString::GetEmpty()*/) const
{
	for (SpeakerList::const_iterator it = m_activeSpeakers.begin(), itEnd = m_activeSpeakers.end(); it != itEnd; ++it)
	{
		if (it->pActor == pActor && (!lineID.IsValid() || it->lineID == lineID))
		{
			return true;
		}
	}

	//not really speaking this right now, but going to say it.
	for (QueuedSpeakerList::const_iterator it = m_queuedSpeakers.begin(), itEnd = m_queuedSpeakers.end(); it != itEnd; ++it)
	{
		if (it->pActor == pActor && (!lineID.IsValid() || it->lineID == lineID))
		{
			return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------
bool CSpeakerManager::StartSpeaking(DRS::IResponseActor* pIActor, const CHashedString& lineID)
{
	if (!pIActor)
	{
		CryWarning(VALIDATOR_MODULE_DRS, VALIDATOR_WARNING, "StartSpeaking was called without a valid DRS Actor.");
		return false;
	}

	CResponseActor* pActor = static_cast<CResponseActor*>(pIActor);
	IEntity* pEntity = pActor->GetLinkedEntity();
	if (!pEntity)
	{
		InformListener(pActor, lineID, DRS::ISpeakerManager::IListener::eLineEvent_CouldNotBeStarted, nullptr);
		CryWarning(VALIDATOR_MODULE_DRS, VALIDATOR_WARNING, "StartSpeaking was called with a DRS Actor that has no Entity assigned to it.");
		return false;
	}

	int priority = 50; //default priority
	CDialogLineDatabase* pLineDatabase = static_cast<CDialogLineDatabase*>(CResponseSystem::GetInstance()->GetDialogLineDatabase());
	const CDialogLine* pDialogLine = pLineDatabase->GetLineByID(lineID, &priority);

	string textToShow;
	AudioControlId audioStartTriggerID = INVALID_AUDIO_CONTROL_ID;
	AudioControlId audioStopTriggerID = INVALID_AUDIO_CONTROL_ID;
	if (pDialogLine)
	{
		textToShow = pDialogLine->GetText();
		string audioTrigger = pDialogLine->GetStartAudioTrigger();
		if (!audioTrigger.empty())
		{
			gEnv->pAudioSystem->GetAudioTriggerId(audioTrigger.c_str(), audioStartTriggerID);
		}
		audioTrigger = pDialogLine->GetEndAudioTrigger();
		if (!audioTrigger.empty())
		{
			gEnv->pAudioSystem->GetAudioTriggerId(audioTrigger.c_str(), audioStopTriggerID);
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_DRS, VALIDATOR_WARNING, "Triggered an SpeakLineAction, with a line that did not exist in the database! LineID was: '%s'", lineID.GetText().c_str());
		textToShow.Format("Missing: %s", lineID.GetText().c_str());
	}

	const IEntityAudioProxyPtr pEntityAudioProxy = crycomponent_cast<IEntityAudioProxyPtr>(pEntity->CreateProxy(ENTITY_PROXY_AUDIO));

	SSpeakInfo* pSpeakerInfoToUse = nullptr;
	bool bReusedSpeaker = false;

	//check if the actor is already talking
	for (SSpeakInfo& activateSpeaker : m_activeSpeakers)
	{
		if (activateSpeaker.pActor == pActor)
		{
			if (priority > activateSpeaker.priority || (m_samePrioCancelsLinesCVar != 0 && priority >= activateSpeaker.priority && lineID != activateSpeaker.lineID))  //if the new line is not less important, stop the old one
			{
				//if someone is waiting for this line to finish, and now we are replacing that line, we have to remove the queued line as well
				for (QueuedSpeakerList::iterator itQueued = m_queuedSpeakers.begin(); itQueued != m_queuedSpeakers.end(); )
				{
					if (itQueued->pSpeechThatWeWaitingFor == &activateSpeaker)
					{
						InformListener(itQueued->pActor, itQueued->lineID, DRS::ISpeakerManager::IListener::eLineEvent_CanceledWhileQueued, nullptr);
						itQueued = m_queuedSpeakers.erase(itQueued);
					}
					else
					{
						++itQueued;
					}
				}

				if (activateSpeaker.stopTriggerID != INVALID_AUDIO_CONTROL_ID && m_playAudioCVar != 0)
				{
					//soft interruption: we execute the stop trigger on the old line. That trigger should cause the old line to end after a while. And only then, do we start the playback of the next line
					SAudioCallBackInfo callbackInfo(this, (void* const)(pDialogLine), (void* const)(activateSpeaker.pActor), eAudioRequestFlags_PriorityNormal | eAudioRequestFlags_SyncFinishedCallback);
					if (!pEntityAudioProxy->ExecuteTrigger(activateSpeaker.stopTriggerID, activateSpeaker.speechAuxProxy, callbackInfo))
					{
						//failed to start the stop trigger, therefore we fallback to hard-interruption
						pEntityAudioProxy->StopTrigger(activateSpeaker.startTriggerID, activateSpeaker.speechAuxProxy);
					}
					else
					{
						//so we have started the stop-trigger, now we have to wait for it to finish before we can start the new line.
						activateSpeaker.endingConditions |= eEC_WaitingForStopTrigger;
						activateSpeaker.priority = priority; //we set the priority of the ending-line to the new priority, so that the stopping wont be canceled by someone else.

						SWaitingInfo newWaitInfo;
						newWaitInfo.pSpeechThatWeWaitingFor = &activateSpeaker;
						newWaitInfo.pActor = pActor;
						newWaitInfo.lineID = lineID;
						m_queuedSpeakers.push_back(newWaitInfo);

						InformListener(pActor, lineID, DRS::ISpeakerManager::IListener::eLineEvent_Queued, nullptr);
						InformListener(activateSpeaker.pActor, activateSpeaker.lineID, DRS::ISpeakerManager::IListener::eLineEvent_Canceling, activateSpeaker.pPickedLine);
						return true;
					}
				}
				else
				{
					pEntityAudioProxy->StopTrigger(activateSpeaker.startTriggerID, activateSpeaker.speechAuxProxy);
					InformListener(pActor, activateSpeaker.lineID, DRS::ISpeakerManager::IListener::eLineEvent_Canceled, activateSpeaker.pPickedLine);
				}
				pSpeakerInfoToUse = &activateSpeaker;
				bReusedSpeaker = true;
				break;
			}
			else
			{
				InformListener(pActor, lineID, DRS::ISpeakerManager::IListener::eLineEvent_SkippedBecauseOfPriority, activateSpeaker.pPickedLine);
				return false;
			}
		}
	}

#if !defined(_RELEASE)
	if (m_displaySubtitlesCVar == 0)
	{
		CryLog("SPEAKING: %s: %s \n", (pActor) ? pActor->GetName().GetText().c_str() : "Unnamed", textToShow.c_str());
	}
#endif

	if (!pSpeakerInfoToUse)
	{
		m_activeSpeakers.push_back(SSpeakInfo());
		pSpeakerInfoToUse = &m_activeSpeakers.back();
		pSpeakerInfoToUse->speechAuxProxy = DEFAULT_AUDIO_PROXY_ID;
		pSpeakerInfoToUse->voiceAttachmentIndex = -1;  // -1 means invalid ID;
	}

	pSpeakerInfoToUse->pPickedLine = pDialogLine;
	pSpeakerInfoToUse->pActor = pActor;
	pSpeakerInfoToUse->text = textToShow;
	pSpeakerInfoToUse->lineID = lineID;
	pSpeakerInfoToUse->priority = priority;

	pSpeakerInfoToUse->startTriggerID = audioStartTriggerID;
	pSpeakerInfoToUse->stopTriggerID = audioStopTriggerID;
	pSpeakerInfoToUse->endingConditions = eEC_Done;
	pSpeakerInfoToUse->lipsyncId = DRS::s_InvalidLipSyncId;
	pSpeakerInfoToUse->standaloneFile = (pDialogLine) ? pDialogLine->GetStandaloneFile() : string();

	bool bHasAudioAsset = (audioStartTriggerID != INVALID_AUDIO_CONTROL_ID || !pSpeakerInfoToUse->standaloneFile.empty()) && m_playAudioCVar != 0;

	if (bHasAudioAsset)
	{
		if (m_audioRtpcIdLocal != INVALID_AUDIO_CONTROL_ID)
		{
			pEntityAudioProxy->SetRtpcValue(m_audioRtpcIdLocal, 1.0f, INVALID_AUDIO_PROXY_ID);
		}

		if (pSpeakerInfoToUse->speechAuxProxy == DEFAULT_AUDIO_PROXY_ID)
		{
			const ICharacterInstance* pCharacter = pEntity->GetCharacter(s_characterSlot);
			if (pCharacter)
			{
				const IAttachmentManager* pAttachmentManager = pCharacter->GetIAttachmentManager();
				if (pAttachmentManager)
				{
					pSpeakerInfoToUse->voiceAttachmentIndex = pAttachmentManager->GetIndexByName("voice");
					if (pSpeakerInfoToUse->voiceAttachmentIndex == -1)
						pSpeakerInfoToUse->voiceAttachmentIndex = pAttachmentManager->GetIndexByName("eye_left");
					if (pSpeakerInfoToUse->voiceAttachmentIndex != -1)
					{
						pSpeakerInfoToUse->speechAuxProxy = pEntityAudioProxy->CreateAuxAudioProxy();
						UpdateAudioProxyPosition(pEntity, *pSpeakerInfoToUse);
					}
				}
			}
		}

		SAudioCallBackInfo callbackInfo(this, (void* const)(pDialogLine), (void* const)(pActor), eAudioRequestFlags_PriorityNormal | eAudioRequestFlags_SyncFinishedCallback);

		bool bAudioPlaybackStarted = true;

		if (pSpeakerInfoToUse->startTriggerID)
		{
			bAudioPlaybackStarted = pEntityAudioProxy->ExecuteTrigger(pSpeakerInfoToUse->startTriggerID, pSpeakerInfoToUse->speechAuxProxy, callbackInfo);
		}
		else
		{
			bAudioPlaybackStarted = pEntityAudioProxy->PlayFile(pSpeakerInfoToUse->standaloneFile.c_str(), pSpeakerInfoToUse->speechAuxProxy, callbackInfo);
		}

		if (bAudioPlaybackStarted)
		{
			pSpeakerInfoToUse->finishTime = GetISystem()->GetITimer()->GetCurrTime() + 100;
			pSpeakerInfoToUse->endingConditions = eEC_WaitingForStartTrigger;
		}
		else
		{
			ReleaseSpeakerAudioProxy(*pSpeakerInfoToUse, false);
			pSpeakerInfoToUse->startTriggerID = INVALID_AUDIO_CONTROL_ID;
			pSpeakerInfoToUse->stopTriggerID = INVALID_AUDIO_CONTROL_ID;
			pSpeakerInfoToUse->speechAuxProxy = DEFAULT_AUDIO_PROXY_ID;
			pSpeakerInfoToUse->standaloneFile.clear();
		}
	}

	if (m_pLipsyncProvider)
	{
		pSpeakerInfoToUse->lipsyncId = m_pLipsyncProvider->OnLineStarted(pActor, pSpeakerInfoToUse->pPickedLine);
		if (pSpeakerInfoToUse->lipsyncId != DRS::s_InvalidLipSyncId)
		{
			pSpeakerInfoToUse->endingConditions |= eEC_WaitingForLipsync;
			pSpeakerInfoToUse->finishTime = GetISystem()->GetITimer()->GetCurrTime() + 100;
		}
	}

	if (pSpeakerInfoToUse->endingConditions == eEC_Done)  //fallback, if no other ending conditions were applied, display subtitles for some time
	{
		pSpeakerInfoToUse->finishTime = GetISystem()->GetITimer()->GetCurrTime();
		pSpeakerInfoToUse->finishTime += (2 + textToShow.length() / 16);
		pSpeakerInfoToUse->endingConditions = eEC_WaitingForTimer;
	}

	InformListener(pActor, lineID, DRS::ISpeakerManager::IListener::eLineEvent_Started, pDialogLine);

	SET_DRS_USER_SCOPED("SpeakerManager Start Speaking");
	if (!bReusedSpeaker)
	{
		pActor->GetLocalVariables()->SetVariableValue(s_isTalkingVariableName, true);
		pActor->GetLocalVariables()->SetVariableValue(s_lastLineId, lineID);
	}
	SetNumActiveSpeaker((int)m_activeSpeakers.size());

	return true;
}

//--------------------------------------------------------------------------------------------------
void CSpeakerManager::UpdateAudioProxyPosition(IEntity* pEntity, const SSpeakInfo& speakerInfo)
{
	//todo: check if we can use a 'head' proxy created and defined in schematyc
	IEntityAudioProxy* const pEntityAudioProxy = static_cast<IEntityAudioProxy*>(pEntity->GetProxy(ENTITY_PROXY_AUDIO));
	if (pEntityAudioProxy)
	{
		const ICharacterInstance* pCharacter = pEntity->GetCharacter(s_characterSlot);
		if (pCharacter)
		{
			const IAttachmentManager* pAttachmentManager = pCharacter->GetIAttachmentManager();
			if (pAttachmentManager)
			{
				const IAttachment* pAttachment = pAttachmentManager->GetInterfaceByIndex(speakerInfo.voiceAttachmentIndex);
				if (pAttachment)
				{
					pEntityAudioProxy->SetAuxAudioProxyOffset(Matrix34(pAttachment->GetAttModelRelative()), speakerInfo.speechAuxProxy);
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------
bool CSpeakerManager::CancelSpeaking(const DRS::IResponseActor* pActor, int maxPrioToCancel)
{
	bool bSomethingWasCanceled = false;
	for (SpeakerList::iterator it = m_activeSpeakers.begin(); it != m_activeSpeakers.end(); ++it)
	{
		if ((it->pActor == pActor || pActor == nullptr) && (it->priority <= maxPrioToCancel || maxPrioToCancel < 0))
		{
			//to-check: currently cancel speaking will not execute the stop trigger
			InformListener(static_cast<const CResponseActor*>(pActor), it->lineID, DRS::ISpeakerManager::IListener::eLineEvent_Canceled, it->pPickedLine);
			it->endingConditions = eEC_Done;
			it->finishTime = 0.0f;  //trigger finished, will be removed in the next update then
			bSomethingWasCanceled = true;
		}
	}
	return bSomethingWasCanceled;
}

//--------------------------------------------------------------------------------------------------
void CSpeakerManager::Reset()
{
	m_queuedSpeakers.clear();

	for (SpeakerList::iterator it = m_activeSpeakers.begin(), itEnd = m_activeSpeakers.end(); it != itEnd; ++it)
	{
		SET_DRS_USER_SCOPED("SpeakerManager Reset");
		it->pActor->GetLocalVariables()->SetVariableValue(s_isTalkingVariableName, false);
		ReleaseSpeakerAudioProxy(*it, true);
	}
	m_activeSpeakers.clear();
}

//--------------------------------------------------------------------------------------------------
void CSpeakerManager::ReleaseSpeakerAudioProxy(SSpeakInfo& speakerInfo, bool stopTrigger)
{
	if (speakerInfo.speechAuxProxy != DEFAULT_AUDIO_PROXY_ID || stopTrigger)
	{
		IEntity* pEntity = speakerInfo.pActor->GetLinkedEntity();
		if (pEntity)
		{
			IEntityAudioProxy* const pEntityAudioProxy = static_cast<IEntityAudioProxy*>(pEntity->GetProxy(ENTITY_PROXY_AUDIO));
			if (pEntityAudioProxy)
			{
				if (stopTrigger)
				{
					if (speakerInfo.startTriggerID != INVALID_AUDIO_CONTROL_ID)
					{
						pEntityAudioProxy->StopTrigger(speakerInfo.startTriggerID, speakerInfo.speechAuxProxy);
					}
					if (!speakerInfo.standaloneFile.empty())
					{
						pEntityAudioProxy->StopFile(speakerInfo.standaloneFile, speakerInfo.speechAuxProxy);
					}
				}
				if (speakerInfo.speechAuxProxy != DEFAULT_AUDIO_PROXY_ID)
				{
					pEntityAudioProxy->RemoveAuxAudioProxy(speakerInfo.speechAuxProxy);
					speakerInfo.speechAuxProxy = DEFAULT_AUDIO_PROXY_ID;
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------
void CSpeakerManager::OnAudioCallback(const SAudioRequestInfo* const pAudioRequestInfo)
{
	CSpeakerManager* pSpeakerManager = CResponseSystem::GetInstance()->GetSpeakerManager();
	const CDialogLine* pDialogLine = reinterpret_cast<const CDialogLine*>(pAudioRequestInfo->pUserData);
	const CResponseActor* pActor = reinterpret_cast<const CResponseActor*>(pAudioRequestInfo->pUserDataOwner);

	if (pAudioRequestInfo->requestResult == eAudioRequestResult_Failure &&
		(pAudioRequestInfo->audioRequestType == eAudioRequestType_AudioObjectRequest && (pAudioRequestInfo->specificAudioRequest == eAudioObjectRequestType_ExecuteTrigger || pAudioRequestInfo->specificAudioRequest == eAudioObjectRequestType_PlayFile)
		|| pAudioRequestInfo->audioRequestType == eAudioRequestType_AudioCallbackManagerRequest && pAudioRequestInfo->specificAudioRequest == eAudioCallbackManagerRequestType_ReportStartedFile))
	{
		//handling of failure executing the start / stop trigger or the standalone file
		for (SpeakerList::iterator it = pSpeakerManager->m_activeSpeakers.begin(), itEnd = pSpeakerManager->m_activeSpeakers.end(); it != itEnd; ++it)
		{
			if (it->pActor == pActor && pDialogLine == it->pPickedLine)
			{
				if (pAudioRequestInfo->audioControlId == it->startTriggerID)  //if the start trigger fails, we still want to display the subtitle for some time, //will also be met for StandaloneFiles, because startTriggerID == 0
				{
					int textLength = (pDialogLine) ? strlen(pDialogLine->GetText()) : 16;
					it->finishTime = GetISystem()->GetITimer()->GetCurrTime() + (2 + (textLength / 16));
					it->endingConditions |= eEC_WaitingForTimer;
					it->endingConditions &= ~eEC_WaitingForStartTrigger;
				}
				else if (pAudioRequestInfo->audioControlId == it->stopTriggerID) //if the stop trigger fails, we simply stop the start-trigger directly (in the next update)
				{
					it->finishTime = 0.0f;
					it->endingConditions = eEC_Done;
					it->endingConditions &= ~eEC_WaitingForStopTrigger;
				}
				return;
			}
		}
	}
	else if (pAudioRequestInfo->audioRequestType == eAudioRequestType_AudioCallbackManagerRequest
	         && (pAudioRequestInfo->specificAudioRequest == eAudioCallbackManagerRequestType_ReportFinishedTriggerInstance
	             || pAudioRequestInfo->specificAudioRequest == eAudioCallbackManagerRequestType_ReportStoppedFile))
	{
		for (SpeakerList::iterator it = pSpeakerManager->m_activeSpeakers.begin(), itEnd = pSpeakerManager->m_activeSpeakers.end(); it != itEnd; ++it)
		{
			if (it->pActor == pActor && pDialogLine == it->pPickedLine)
			{
				if (pAudioRequestInfo->audioControlId == it->startTriggerID)  //will also be met for StandaloneFiles, because startTriggerID == 0
				{
					it->endingConditions &= ~eEC_WaitingForStartTrigger;
				}
				if (pAudioRequestInfo->audioControlId == it->stopTriggerID)
				{
					it->endingConditions &= ~eEC_WaitingForStopTrigger;
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------
void CSpeakerManager::OnActorRemoved(const CResponseActor* pActor)
{
	for (SpeakerList::iterator it = m_activeSpeakers.begin(); it != m_activeSpeakers.end(); )
	{
		if (it->pActor == pActor)
		{
			InformListener(pActor, it->lineID, DRS::ISpeakerManager::IListener::eLineEvent_Canceled, it->pPickedLine);
			it = m_activeSpeakers.erase(it);
		}
		else
		{
			++it;
		}
	}

	for (QueuedSpeakerList::iterator itQueued = m_queuedSpeakers.begin(); itQueued != m_queuedSpeakers.end(); )
	{
		if (itQueued->pActor == pActor)
		{
			InformListener(itQueued->pActor, itQueued->lineID, DRS::ISpeakerManager::IListener::eLineEvent_CanceledWhileQueued, nullptr);
			itQueued = m_queuedSpeakers.erase(itQueued);
		}
		else
		{
			++itQueued;
		}
	}
}

//--------------------------------------------------------------------------------------------------
bool CSpeakerManager::AddListener(DRS::ISpeakerManager::IListener* pListener)
{
	for (const DRS::ISpeakerManager::IListener* pExistingListener : m_listeners)
	{
		if (pExistingListener == pListener)
		{
			DrsLogWarning("The same LineListener was added twice.");
			return false;
		}

	}
	m_listeners.push_back(pListener);
	return true;
}

//--------------------------------------------------------------------------------------------------
bool CSpeakerManager::RemoveListener(DRS::ISpeakerManager::IListener* pListener)
{
	for (ListenerList::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
	{
		if (*it == pListener)
		{
			m_listeners.erase(it);
			return true;
		}
	}
	return false;
}

//--------------------------------------------------------------------------------------------------
void CSpeakerManager::InformListener(const DRS::IResponseActor* pSpeaker, const CHashedString& lineID, DRS::ISpeakerManager::IListener::eLineEvent event, const CDialogLine* pLine)
{
	for (DRS::ISpeakerManager::IListener* pExistingListener : m_listeners)
	{
		pExistingListener->OnLineEvent(pSpeaker, lineID, event, static_cast<const DRS::IDialogLine*>(pLine));
	}
}

//--------------------------------------------------------------------------------------------------
void CSpeakerManager::SetCustomLipsyncProvider(DRS::ISpeakerManager::ILipsyncProvider* pProvider)
{
	delete(m_pDefaultLipsyncProvider);  //we dont need the default one anymore, once a custom one is set.
	m_pDefaultLipsyncProvider = nullptr;

	m_pLipsyncProvider = pProvider;
}

//--------------------------------------------------------------------------------------------------
void CSpeakerManager::SetNumActiveSpeaker(int newAmountOfSpeaker)
{
	if (m_numActiveSpeaker != newAmountOfSpeaker)
	{
		m_numActiveSpeaker = newAmountOfSpeaker;
		m_pActiveSpeakerVariable->SetValue(newAmountOfSpeaker);

		if (m_audioRtpcIdGlobal != INVALID_AUDIO_CONTROL_ID)
		{
			SAudioRequest request;
			SAudioObjectRequestData<eAudioObjectRequestType_SetRtpcValue> requestData(m_audioRtpcIdGlobal, (float)newAmountOfSpeaker);
			request.flags = eAudioRequestFlags_PriorityNormal;
			request.pData = &requestData;
			gEnv->pAudioSystem->PushRequest(request);
		}
	}
}

//--------------------------------------------------------------------------------------------------
DRS::LipSyncID CDefaultLipsyncProvider::OnLineStarted(DRS::IResponseActor* pActor, const DRS::IDialogLine* pDialogLine)
{
	//Start lipsync animation
	int animationID = -1;
	if (pDialogLine)
	{
		if (!pDialogLine->GetLipsyncAnimation().empty())
		{
			IEntity* pEntity = pActor->GetLinkedEntity();
			if (pEntity)
			{
				ICharacterInstance* pCharacter = pEntity->GetCharacter(s_characterSlot);
				if (pCharacter)
				{
					const IAnimationSet* pAnimSet = pCharacter->GetIAnimationSet();
					if (pAnimSet)
					{
						CryCharAnimationParams params;
						params.m_fTransTime = m_lipsyncTransitionTime;
						params.m_nLayerID = m_lipsyncAnimationLayer;
						params.m_nUserToken = pActor->GetName().GetHash();
						params.m_nFlags = CA_ALLOW_ANIM_RESTART;

						animationID = pAnimSet->GetAnimIDByName(pDialogLine->GetLipsyncAnimation().c_str());

						if (animationID < 0)                                                                    //fallback 1: use audio trigger name as animation name
						{
							animationID = pAnimSet->GetAnimIDByName(pDialogLine->GetStartAudioTrigger().c_str()); //default: if no special line is provided, we look for a animation with the same name as the audio trigger
						}
						if (animationID < 0)                                                                    //fallback 2: use default animation
						{
							animationID = pAnimSet->GetAnimIDByName(m_defaultLipsyncAnimationName);
							if (animationID >= 0)
							{
								params.m_nFlags |= CA_LOOP_ANIMATION;
								params.m_fPlaybackSpeed = 1.25f;
							}
						}

						if (animationID >= 0)
						{
							ISkeletonAnim* pSkeletonAnimation = pCharacter->GetISkeletonAnim();
							if (!pSkeletonAnimation->StartAnimationById(animationID, params))
							{
								animationID = -1;
							}
						}
					}
				}
			}
		}
	}
	return (animationID < 0) ? DRS::s_InvalidLipSyncId : static_cast<DRS::LipSyncID>(animationID);
}

//--------------------------------------------------------------------------------------------------
void CDefaultLipsyncProvider::OnLineEnded(DRS::LipSyncID lipsyncId, DRS::IResponseActor* pSpeaker, const DRS::IDialogLine* pLine)
{
	//stop lipsync animation
	if (lipsyncId != DRS::s_InvalidLipSyncId)
	{
		IEntity* pEntity = pSpeaker->GetLinkedEntity();
		if (pEntity)
		{
			ICharacterInstance* pCharacter = pEntity->GetCharacter(s_characterSlot);
			if (pCharacter)
			{
				ISkeletonAnim* pSkeletonAnimation = pCharacter->GetISkeletonAnim();
				if (pSkeletonAnimation)
				{
					// NOTE: there is no simple way to just stop the exact animation we started, but this should do too:
					pSkeletonAnimation->StopAnimationInLayer(m_lipsyncAnimationLayer, m_lipsyncTransitionTime);
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------
CDefaultLipsyncProvider::CDefaultLipsyncProvider()
{
	ICVar* pVariable = REGISTER_STRING("drs_dialogsDefaultTalkAnimation", "relaxed_tac_leaningOverRailEnd_nw_3p_01", 0, "The fallback talk animation, if no specialized talk animation is set for a line");
	if (pVariable)
		m_defaultLipsyncAnimationName = pVariable->GetString();

	REGISTER_CVAR2("drs_dialogsLipsyncAnimationLayer", &m_lipsyncAnimationLayer, 11, VF_NULL, "The animation layer used for dialog lipsync animations");
	REGISTER_CVAR2("drs_dialogsLipsyncTransitionTime", &m_lipsyncTransitionTime, 0.25, VF_NULL, "The blend in/out times for lipsync animations");
}

//--------------------------------------------------------------------------------------------------
CDefaultLipsyncProvider::~CDefaultLipsyncProvider()
{
	if (pDefaultLipsyncAnimationNameVariable)
	{
		pDefaultLipsyncAnimationNameVariable->Release();
		pDefaultLipsyncAnimationNameVariable = nullptr;
	}
	gEnv->pConsole->UnregisterVariable("drs_dialogsLipsyncAnimationLayer", true);
	gEnv->pConsole->UnregisterVariable("drs_dialogsLipsyncTransitionTime", true);
}

//--------------------------------------------------------------------------------------------------
bool CDefaultLipsyncProvider::Update(DRS::LipSyncID lipsyncId, DRS::IResponseActor* pSpeaker, const DRS::IDialogLine* pLine)
{
	//check if animation has finished, or when its a looping animation always returns false (we dont want the speakermanager wait for it to finish)
	if (lipsyncId != DRS::s_InvalidLipSyncId)
	{
		IEntity* pEntity = pSpeaker->GetLinkedEntity();
		if (pEntity)
		{
			ICharacterInstance* pCharacter = pEntity->GetCharacter(s_characterSlot);
			if (pCharacter)
			{
				ISkeletonAnim* pSkeletonAnimation = pCharacter->GetISkeletonAnim();
				if (pSkeletonAnimation)
				{
					const CAnimation* pLipSyncAni = pSkeletonAnimation->FindAnimInFIFO(pSpeaker->GetName().GetHash(), m_lipsyncAnimationLayer);
					return pLipSyncAni && !pLipSyncAni->HasStaticFlag(CA_LOOP_ANIMATION);
				}
			}
		}
	}
	return false;
}
