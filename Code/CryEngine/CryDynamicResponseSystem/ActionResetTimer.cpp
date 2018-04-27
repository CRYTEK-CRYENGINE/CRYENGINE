// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <CryDynamicResponseSystem/IDynamicResponseAction.h>
#include "ActionResetTimer.h"
#include "VariableCollection.h"
#include "ResponseSystem.h"
#include "ResponseInstance.h"

using namespace CryDRS;

//////////////////////////////////////////////////////////
DRS::IResponseActionInstanceUniquePtr CActionResetTimerVariable::Execute(DRS::IResponseInstance* pResponseInstance)
{
	CVariableCollection* pCollectionToUse = GetCurrentCollection(static_cast<CResponseInstance*>(pResponseInstance));
	if (!pCollectionToUse)
	{
		CVariableCollectionManager* pVcManager = CResponseSystem::GetInstance()->GetVariableCollectionManager();
		pCollectionToUse = pVcManager->CreateVariableCollection(m_collectionName);
	}
	if (pCollectionToUse)
	{
		pCollectionToUse->SetVariableValue(m_variableName, CResponseSystem::GetInstance()->GetCurrentDrsTime());
	}
	return nullptr;
}

//////////////////////////////////////////////////////////
string CActionResetTimerVariable::GetVerboseInfo() const
{
	return "in variable" + GetVariableVerboseName();
}

//////////////////////////////////////////////////////////
void CActionResetTimerVariable::Serialize(Serialization::IArchive& ar)
{
	_Serialize(ar, "^TimerVariable");

#if defined (ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	CVariableCollection* pCollection = CResponseSystem::GetInstance()->GetCollection(m_collectionName);
	if (pCollection)
	{
		if (pCollection->GetVariableValue(m_variableName).GetType() != eDRVT_Float && pCollection->GetVariableValue(m_variableName).GetType() != eDRVT_Undefined)
		{
			ar.warning(m_collectionName.m_textCopy, "Variable to set needs to hold a time value as float");
		}
	}
#endif
}
