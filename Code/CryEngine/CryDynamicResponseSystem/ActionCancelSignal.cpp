// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ActionCancelSignal.h"

#include "ResponseInstance.h"
#include "ResponseSystem.h"

using namespace CryDRS;

namespace
{
const CHashedString g_allKeyWord = "All";
const CHashedString g_allButNotCurrentKeyWord = "AllNotCurrent";
}

//--------------------------------------------------------------------------------------------------
DRS::IResponseActionInstanceUniquePtr CActionCancelSignal::Execute(DRS::IResponseInstance* pResponseInstance)
{
	if (m_signalName == g_allKeyWord && m_bOnAllActors)  //special case: stop all signals on all actors -> reset
	{
		CResponseSystem::GetInstance()->Reset(DRS::IDynamicResponseSystem::eResetHint_StopRunningResponses);
		return nullptr;
	}
	else
	{
		CResponseInstance* pInstance = static_cast<CResponseInstance*>(pResponseInstance);
		CHashedString usedSignal;
		DRS::SignalInstanceId instanceIdToNotCancel = DRS::s_InvalidSignalId;

		if (m_signalName == g_allButNotCurrentKeyWord)
		{
			instanceIdToNotCancel = pInstance->GetSignalInstanceId();
		}
		else if (m_signalName != g_allKeyWord)
		{
			usedSignal = (m_signalName.IsValid()) ? m_signalName : pInstance->GetSignalName();
		}
		const bool bSomethingCanceled = CResponseSystem::GetInstance()->CancelSignalProcessing(usedSignal, (m_bOnAllActors) ? nullptr : pInstance->GetCurrentActor(), instanceIdToNotCancel);
		if (!bSomethingCanceled || pInstance->GetCurrentSegment() == nullptr)
		{
			return nullptr;  //seems like we canceled our own response-instance, in this case we don`t start a new instance here.
		}
		return DRS::IResponseActionInstanceUniquePtr(new CActionCancelSignalInstance(usedSignal, (m_bOnAllActors) ? nullptr : pInstance->GetCurrentActor(), pInstance->GetSignalInstanceId()));
	}
}

//--------------------------------------------------------------------------------------------------
void CActionCancelSignal::Serialize(Serialization::IArchive& ar)
{
	ar(m_signalName, "signal", "^ Signal");
	ar(m_bOnAllActors, "onAllActors", "^OnAllActors");

#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
	if (ar.isEdit())
	{
		if (!m_signalName.IsValid())
		{
			ar.warning(m_signalName.m_textCopy, "No signal to cancel specified");
		}
		else if (m_signalName != g_allButNotCurrentKeyWord && m_signalName != g_allKeyWord
		         && CResponseSystem::GetInstance()->GetResponseManager()->GetResponse(m_signalName) == nullptr)
		{
			ar.warning(m_signalName.m_textCopy, "No response exists for the specified signal");
		}
	}
#endif
}

//--------------------------------------------------------------------------------------------------
DRS::IResponseActionInstance::eCurrentState CryDRS::CActionCancelSignalInstance::Update()
{
	if (m_signal.m_signalName.IsValid() || m_signal.m_pSender)
	{
		if (CResponseSystem::GetInstance()->GetResponseManager()->IsSignalProcessed(m_signal))
		{
			return IResponseActionInstance::CS_RUNNING;
		}
	}
	return IResponseActionInstance::CS_FINISHED;
}

//--------------------------------------------------------------------------------------------------
void CryDRS::CActionCancelSignalInstance::Cancel()
{
	m_signal.m_signalName.Clear();
	m_signal.m_pSender = nullptr;
}

//--------------------------------------------------------------------------------------------------
CryDRS::CActionCancelSignalInstance::CActionCancelSignalInstance(const CHashedString& signalName, CResponseActor* pSender /*= nullptr*/, DRS::SignalInstanceId instanceToSkip /*= DRS::s_InvalidSignalId*/)
	: m_signal(signalName, pSender, nullptr)
{
	m_signal.m_id = instanceToSkip;
}
