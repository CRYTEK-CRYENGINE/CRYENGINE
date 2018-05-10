// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "ActionSpeakLine.h"
#include "DialogLineDatabase.h"
#include "ResponseSegment.h"
#include "ResponseSystem.h"
#include "ResponseInstance.h"
#include "SpeakerManager.h"

#include <CryAudio/IAudioSystem.h>
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include <CryDynamicResponseSystem/IDynamicResponseAction.h>

#include <CrySerialization/yasli/BitVectorImpl.h>

using namespace CryDRS;

namespace
{
static const CHashedString s_signalOnLineCancel = "LineCanceled";
static const CHashedString s_signalOnLineSkip = "LineSkipped";
static const CHashedString s_signalOnLineStart = "LineStarted";
static const CHashedString s_signalOnLineFinish = "LineFinished";
}

//--------------------------------------------------------------------------------------------------
DRS::IResponseActionInstanceUniquePtr CActionSpeakLine::Execute(DRS::IResponseInstance* pResponseInstance)
{
	if (!m_lineIDToSpeak.IsValid())
		return nullptr;

	CResponseInstance* pResponseInstanceImpl = static_cast<CResponseInstance*>(pResponseInstance);

	CResponseSystem* pDrs = CResponseSystem::GetInstance();
	CResponseActor* pSpeaker = (!m_speakerOverrideName.IsValid()) ? pResponseInstanceImpl->GetCurrentActor() : pDrs->GetResponseActor(m_speakerOverrideName);

	if (!pSpeaker)
	{
		CryWarning(VALIDATOR_MODULE_DRS, VALIDATOR_WARNING, "Triggered an SpeakLineAction, without a valid Speaker! LineID was: '%s', Speaker override was '%s'", m_lineIDToSpeak.GetText().c_str(), m_speakerOverrideName.GetText().c_str());
		return nullptr;
	}

	DRS::ISpeakerManager::IListener::eLineEvent result = pDrs->GetSpeakerManager()->StartSpeaking(pSpeaker, m_lineIDToSpeak);

	if (result == DRS::ISpeakerManager::IListener::eLineEvent_WasNotStartedForAnyReason)
	{
		//for whatever reason no line was started (maybe because of priority or only-once flag or faulty data)
		if ((m_flags& CActionSpeakLine::ESpeakLineFlags_SendSignalOnSkip) > 0)
		{
			DRS::IVariableCollectionSharedPtr contextVariablesPtr = CResponseSystem::GetInstance()->CreateContextCollection();
			contextVariablesPtr->CreateVariable("Line", m_lineIDToSpeak);
			pSpeaker->QueueSignal(s_signalOnLineSkip, contextVariablesPtr);
		}
		if ((m_flags& CActionSpeakLine::eSpeakLineFlags_CancelResponseOnSkip) > 0)
		{
			pResponseInstanceImpl->Cancel();
		}
		return nullptr;
	}
	else
	{
		if (result == DRS::ISpeakerManager::IListener::eLineEvent_Started
		    && (m_flags& CActionSpeakLine::ESpeakLineFlags_SendSignalOnStart) > 0)
		{
			//remark: if the line is queued for now, then this signal will be send by the actionInstance when the line is actually started
			DRS::IVariableCollectionSharedPtr contextVariablesPtr = CResponseSystem::GetInstance()->CreateContextCollection();
			contextVariablesPtr->CreateVariable("Line", m_lineIDToSpeak);
			pSpeaker->QueueSignal(s_signalOnLineStart, contextVariablesPtr);
		}

		return DRS::IResponseActionInstanceUniquePtr(new CActionSpeakLineInstance(pSpeaker, m_lineIDToSpeak, pResponseInstanceImpl, m_flags));

	}
}

SERIALIZATION_ENUM_BEGIN_NESTED(CActionSpeakLine, ESpeakLineFlags, "Flags")
SERIALIZATION_ENUM(CActionSpeakLine::eSpeakLineFlags_CancelResponseOnSkip, "CancelResponseOnSkip", "CancelResponseOnSkip")
SERIALIZATION_ENUM(CActionSpeakLine::eSpeakLineFlags_CancelResponseOnCanceled, "CancelResponseOnCanceled", "CancelResponseOnCanceled")
SERIALIZATION_ENUM(CActionSpeakLine::ESpeakLineFlags_SendSignalOnStart, "SendSignalOnStart", "SendSignalOnStart")
SERIALIZATION_ENUM(CActionSpeakLine::ESpeakLineFlags_SendSignalOnSkip, "SendSignalOnSkip", "SendSignalOnSkip")
SERIALIZATION_ENUM(CActionSpeakLine::ESpeakLineFlags_SendSignalOnCancel, "SendSignalOnCancel", "SendSignalOnCancel")
SERIALIZATION_ENUM(CActionSpeakLine::ESpeakLineFlags_SendSignalOnFinish, "SendSignalOnFinished", "SendSignalOnFinished")
SERIALIZATION_ENUM(CActionSpeakLine::ESpeakLineFlags_ReevaluteConditionsAfterQueue, "ReEvaluteConditionsAfterQueue", "ReEvaluteConditionsAfterQueue")
SERIALIZATION_ENUM_END()

//--------------------------------------------------------------------------------------------------
void CActionSpeakLine::Serialize(Serialization::IArchive& ar)
{
	ar(m_lineIDToSpeak, "lineToSpeak", "^ Line");
	ar(m_speakerOverrideName, "speakerOverride", "^ SpeakerOverride");
	ar((yasli::BitVector<ESpeakLineFlags> &)m_flags, "flags", "Line Flags");

#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
	if (ar.isEdit())
	{
		if (!m_lineIDToSpeak.IsValid())
		{
			ar.warning(m_lineIDToSpeak.m_textCopy, "No line specified");
		}
		else if (!gEnv->pDynamicResponseSystem->GetDialogLineDatabase()->GetLineSetById(m_lineIDToSpeak))
		{
			ar.warning(m_lineIDToSpeak.m_textCopy, "Specified line not found in the database");
		}
	}
#endif
}

//--------------------------------------------------------------------------------------------------
DRS::IResponseActionInstance::eCurrentState CActionSpeakLineInstance::Update()
{
	return m_currentState;
}

//--------------------------------------------------------------------------------------------------
void CActionSpeakLineInstance::Cancel()
{
	if (m_currentState == CS_RUNNING)
	{
		CSpeakerManager* pSpeakerMgr = CResponseSystem::GetInstance()->GetSpeakerManager();
		pSpeakerMgr->CancelSpeaking(m_pSpeaker, -1, m_lineID, true);
	}
}

//--------------------------------------------------------------------------------------------------
bool CActionSpeakLineInstance::OnLineAboutToStart(const DRS::IResponseActor* pSpeaker, const CHashedString& lineID)
{
	if (m_currentState == CS_RUNNING && m_pSpeaker == pSpeaker && lineID == m_lineID && ((m_flags& CActionSpeakLine::ESpeakLineFlags_ReevaluteConditionsAfterQueue) > 0))
	{
		//if a queued-line is started, we re-check the conditions again
		if (m_pResponseInstance->GetCurrentSegment())
		{
			return m_pResponseInstance->GetCurrentSegment()->AreConditionsMet(m_pResponseInstance);
		}
		return false;
	}
	return true;
}

//--------------------------------------------------------------------------------------------------
void CActionSpeakLineInstance::OnLineEvent(const DRS::IResponseActor* pSpeaker, const CHashedString& lineID, eLineEvent lineEvent, const DRS::IDialogLine* pLine)
{
	//TODO: speaker and lineId is maybe not enough, we should check for a LineInstanceId
	if (pSpeaker == m_pSpeaker && m_lineID == lineID)
	{
		if (lineEvent == eLineEvent_Started)
		{
			if ((m_flags & CActionSpeakLine::ESpeakLineFlags_SendSignalOnStart) > 0)
			{
				DRS::IVariableCollectionSharedPtr contextVariablesPtr = CResponseSystem::GetInstance()->CreateContextCollection();
				contextVariablesPtr->CreateVariable("Line", m_lineID);
				m_pSpeaker->QueueSignal(s_signalOnLineStart, contextVariablesPtr);
			}
		}
		else if (lineEvent == eLineEvent_Finished)
		{
			CRY_ASSERT_MESSAGE(m_currentState == CS_RUNNING, "received a 'line-finished' callback when the speak-line action was not in the running state, should not happen");
			m_currentState = CS_FINISHED;
			if ((m_flags & CActionSpeakLine::ESpeakLineFlags_SendSignalOnFinish) > 0)
			{
				DRS::IVariableCollectionSharedPtr contextVariablesPtr = CResponseSystem::GetInstance()->CreateContextCollection();
				contextVariablesPtr->CreateVariable("Line", m_lineID);
				m_pSpeaker->QueueSignal(s_signalOnLineFinish, contextVariablesPtr);
			}
			CResponseSystem::GetInstance()->GetSpeakerManager()->RemoveListener(this);
		}
		else if (lineEvent == eLineEvent_Canceled)
		{
			CRY_ASSERT_MESSAGE(m_currentState == CS_RUNNING, "received a 'line-canceled' callback when the speak-line action was not in the running state, should not happen");
			m_currentState = CS_CANCELED;
			if ((m_flags & CActionSpeakLine::ESpeakLineFlags_SendSignalOnCancel) > 0)
			{
				DRS::IVariableCollectionSharedPtr contextVariablesPtr = CResponseSystem::GetInstance()->CreateContextCollection();
				contextVariablesPtr->CreateVariable("Line", m_lineID);
				m_pSpeaker->QueueSignal(s_signalOnLineCancel, contextVariablesPtr);
			}
			if (m_flags & CActionSpeakLine::eSpeakLineFlags_CancelResponseOnCanceled)
			{
				m_pResponseInstance->Cancel();  //remark: This will call 'cancel' on our own instance as well, so we need to handle that.
			}
			CResponseSystem::GetInstance()->GetSpeakerManager()->RemoveListener(this);
		}
		else if ((lineEvent & eLineEvent_WasNotStartedForAnyReason) > 0)
		{
			m_currentState = CS_CANCELED;
			if ((m_flags& CActionSpeakLine::ESpeakLineFlags_SendSignalOnSkip) > 0)
			{
				DRS::IVariableCollectionSharedPtr contextVariablesPtr = CResponseSystem::GetInstance()->CreateContextCollection();
				contextVariablesPtr->CreateVariable("Line", m_lineID);
				m_pSpeaker->QueueSignal(s_signalOnLineSkip, contextVariablesPtr);
			}
			if ((m_flags& CActionSpeakLine::eSpeakLineFlags_CancelResponseOnSkip) > 0)
			{
				m_pResponseInstance->Cancel();  //remark: This will call 'cancel' on our own instance as well, so we need to handle that.
			}
			CResponseSystem::GetInstance()->GetSpeakerManager()->RemoveListener(this);
		}
	}
}

//--------------------------------------------------------------------------------------------------
CActionSpeakLineInstance::CActionSpeakLineInstance(CResponseActor* pSpeaker, const CHashedString& lineID, CResponseInstance* pResponseInstance, uint32 flags)
	: m_pSpeaker(pSpeaker)
	, m_lineID(lineID)
	, m_pResponseInstance(pResponseInstance)
	, m_currentState(CS_RUNNING)
	, m_flags(flags)
{
	CResponseSystem::GetInstance()->GetSpeakerManager()->AddListener(this);
}

//--------------------------------------------------------------------------------------------------
CActionSpeakLineInstance::~CActionSpeakLineInstance()
{
	CResponseSystem::GetInstance()->GetSpeakerManager()->RemoveListener(this);
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

DRS::IResponseActionInstanceUniquePtr CActionCancelSpeaking::Execute(DRS::IResponseInstance* pResponseInstance)
{
	CResponseSystem* pDrs = CResponseSystem::GetInstance();
	CResponseActor* pSpeaker = nullptr;
	static const CHashedString s_allKeyWord = "All";

	if (m_speakerOverrideName != s_allKeyWord)
	{
		if (m_speakerOverrideName.IsValid())
		{
			pSpeaker = pDrs->GetResponseActor(m_speakerOverrideName);
		}
		else
		{
			pSpeaker = static_cast<CResponseInstance*>(pResponseInstance)->GetCurrentActor();
		}

		if (!pSpeaker)
		{
			CryWarning(VALIDATOR_MODULE_DRS, VALIDATOR_WARNING, "Triggered an CancelSpeaking Action, without a valid Speaker! Speaker override was '%s'", m_speakerOverrideName.GetText().c_str());
			return nullptr;
		}
	}

	if (pDrs->GetSpeakerManager()->CancelSpeaking(pSpeaker, m_maxPrioToCancel, m_lineId, true) && pSpeaker)
	{
		return DRS::IResponseActionInstanceUniquePtr(new CActionCancelSpeakingInstance(pSpeaker, m_lineId));
	}
	else
	{
		return nullptr;
	}
}

//--------------------------------------------------------------------------------------------------
CryDRS::CActionCancelSpeaking::CActionCancelSpeaking() : m_maxPrioToCancel(-1), m_lineId(CHashedString("All"))
{
}

//--------------------------------------------------------------------------------------------------
CryDRS::CActionCancelSpeaking::CActionCancelSpeaking(const CHashedString& speakerName) : m_maxPrioToCancel(-1), m_lineId(CHashedString("All"))
{
}

//--------------------------------------------------------------------------------------------------
void CActionCancelSpeaking::Serialize(Serialization::IArchive& ar)
{
	ar(m_speakerOverrideName, "speakerOverride", "^ SpeakerOverride");
	ar(m_maxPrioToCancel, "maxPrioToCancel", "^ MaxPrioToCancel");
	ar(m_lineId, "lineToCancel", "^ lineToCancel");

#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
	if (ar.isEdit())
	{
		if (!m_lineId.IsValid())
		{
			ar.warning(m_lineId.m_textCopy, "No line specified");
		}
		else if (!gEnv->pDynamicResponseSystem->GetDialogLineDatabase()->GetLineSetById(m_lineId))
		{
			ar.warning(m_lineId.m_textCopy, "Specified line not found in the database");
		}
	}
#endif
}

//--------------------------------------------------------------------------------------------------
DRS::IResponseActionInstance::eCurrentState CActionCancelSpeakingInstance::Update()
{
	CResponseSystem* pDrs = CResponseSystem::GetInstance();

	if (!pDrs->GetSpeakerManager()->IsSpeaking(m_pSpeaker, m_lineId, true) || !m_pSpeaker)
	{
		return CS_FINISHED;
	}
	return CS_RUNNING;
}

//--------------------------------------------------------------------------------------------------
void CActionCancelSpeakingInstance::Cancel()
{
	m_pSpeaker = nullptr;
}
