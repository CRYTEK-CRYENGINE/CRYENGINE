// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ResponseInstance.h"
#include "ResponseManager.h"
#include "VariableCollection.h"
#include "ActionSendSignal.h"
#include "ResponseSystem.h"
#include <CryDynamicResponseSystem/IDynamicResponseAction.h>

using namespace CryDRS;

//--------------------------------------------------------------------------------------------------
DRS::IResponseActionInstanceUniquePtr CActionSendSignal::Execute(DRS::IResponseInstance* pResponseInstance)
{
	pResponseInstance->GetCurrentActor()->QueueSignal(m_signalName, (m_bCopyContextVariables) ? pResponseInstance->GetContextVariables() : nullptr);

	return nullptr;
}

//--------------------------------------------------------------------------------------------------
void CActionSendSignal::Serialize(Serialization::IArchive& ar)
{
	ar(m_signalName, "signal", "^ Signal");
#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
	if (ar.isEdit())
	{
		if (!m_signalName.IsValid())
		{
			ar.warning(m_signalName.m_textCopy, "No signal to send specified");
		}
		else if (CResponseSystem::GetInstance()->GetResponseManager()->GetResponse(m_signalName) == nullptr)
		{
			ar.warning(m_signalName.m_textCopy, "No response exists for the specified signal");
		}
	}
#endif
	ar(m_bCopyContextVariables, "copyContextVar", "^Copy Context Variable");
}

