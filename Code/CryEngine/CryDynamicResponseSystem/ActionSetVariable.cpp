// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "ActionSetVariable.h"
#include "Response.h"
#include "ResponseInstance.h"
#include "ResponseSystem.h"
#include "Variable.h"
#include "VariableCollection.h"
#include <CryString/StringUtils.h>
#include <CrySerialization/Enum.h>
#include <CrySerialization/Decorators/Range.h>
#include <CryDynamicResponseSystem/IDynamicResponseAction.h>
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>

using namespace CryDRS;
using namespace CryStringUtils;

CActionSetVariable::CActionSetVariable
  (const CHashedString& pCollection
  , const CHashedString& variableName
  , CVariableValue targetValue
  , EChangeOperation operation
  , float cooldown)
	: m_valueToSet(targetValue)
	, m_changeOperation(operation)
	, m_cooldown(cooldown)
{
	IVariableUsingBase::m_collectionName = pCollection;
	IVariableUsingBase::m_variableName = variableName;
}

//--------------------------------------------------------------------------------------------------
string CActionSetVariable::GetVerboseInfo() const
{
#if defined(DRS_COLLECT_DEBUG_DATA)
	string cooldown = (m_cooldown > 0.0f) ? " , cooldown: " + CryStringUtils::toString(m_cooldown) : "";
	if (m_changeOperation == eChangeOperation_Increment)
	{
		return string("Increment '") + GetVariableVerboseName() + "' by '" + m_valueToSet.GetValueAsString() + "' (type:" + m_valueToSet.GetTypeAsString() + ")" + cooldown;
	}
	else if (m_changeOperation == eChangeOperation_Decrement)
	{
		return string("Decrement '") + GetVariableVerboseName() + "' by '" + m_valueToSet.GetValueAsString() + "' (type:" + m_valueToSet.GetTypeAsString() + ")" + cooldown;
	}
	else if (m_changeOperation == eChangeOperation_Set)
	{
		return string("Set '") + GetVariableVerboseName() + "' to '" + m_valueToSet.GetValueAsString() + "' (type:" + m_valueToSet.GetTypeAsString() + ")" + cooldown;
	}
#endif
	return string("SetVariableAction: '") + GetVariableVerboseName();
}

//--------------------------------------------------------------------------------------------------
DRS::IResponseActionInstanceUniquePtr CActionSetVariable::Execute(DRS::IResponseInstance* pResponseInstance)
{
	CVariableCollection* pCollectionToUse = GetCurrentCollection(static_cast<CResponseInstance*>(pResponseInstance));
	if (!pCollectionToUse)
	{
		CVariableCollectionManager* pVcManager = CResponseSystem::GetInstance()->GetVariableCollectionManager();
		pCollectionToUse = pVcManager->CreateVariableCollection(m_collectionName);
	}

	if (pCollectionToUse)
	{
		if (m_changeOperation == eChangeOperation_Set)
		{
			DrsLogInfo((string("Set variable '") + GetVariableVerboseName() + "' from value '" + GetCurrentVariableValue().GetValueAsString() + "' to value '" + m_valueToSet.GetValueAsString() + "' with cooldown " + toString(m_cooldown).c_str()).c_str());
			pCollectionToUse->SetVariableValue(m_variableName, m_valueToSet, true, m_cooldown); //remark: if the variable is not existing, we simply create it
		}
		else if (m_changeOperation == eChangeOperation_Increment)
		{
			CVariable* pVariable = pCollectionToUse->GetVariable(m_variableName);
			if (pVariable)
			{
				DrsLogInfo((string("Incrementing variable '") + GetVariableVerboseName() + "' from value '" + GetCurrentVariableValue().GetValueAsString() + "' with '" + m_valueToSet.GetValueAsString() + "' with cooldown " + toString(m_cooldown).c_str()).c_str());
				pCollectionToUse->SetVariableValue(pVariable, pVariable->m_value + m_valueToSet, m_cooldown);
			}
			else
			{
				DrsLogInfo((string("Variable to increment did not exist. Will do a SetOperation instead: Set variable '") + GetVariableVerboseName() + "' from value 'UNDEFINED' to value '" + m_valueToSet.GetValueAsString() + "' with cooldown " + toString(m_cooldown).c_str()).c_str());
				pCollectionToUse->SetVariableValue(m_variableName, m_valueToSet, true, m_cooldown); //remark: if the variable does not exist, we create it and do a SET instead of an ADD
			}
		}
		else if (m_changeOperation == eChangeOperation_Decrement)
		{
			CVariable* pVariable = pCollectionToUse->GetVariable(m_variableName);
			if (pVariable)
			{
				DrsLogInfo((string("Decrementing variable '") + GetVariableVerboseName() + "' from value '" + pCollectionToUse->GetVariableValue(m_variableName).GetValueAsString() + "' with '" + m_valueToSet.GetValueAsString() + "' with cooldown " + toString(m_cooldown).c_str()).c_str());
				pCollectionToUse->SetVariableValue(pVariable, pVariable->m_value - m_valueToSet, m_cooldown);
			}
			else
			{
				DrsLogInfo((string("Variable to decrement did not exist! Will do a SetOperation instead: Set variable '") + GetVariableVerboseName() + "' from value '" + pCollectionToUse->GetVariableValue(m_variableName).GetValueAsString() + "' to value '-" + m_valueToSet.GetValueAsString() + "' with cooldown " + toString(m_cooldown).c_str()).c_str());
				pCollectionToUse->SetVariableValue(m_variableName, -m_valueToSet, true, m_cooldown); //remark: if the variable does not exist, we create it and do a SET instead of an SUBTRACT
			}
		}
	}
	else
	{
		DrsLogWarning((string("Collection with name '") + m_collectionName.GetText() + "' not existing.").c_str());
	}

	return nullptr;
}

//--------------------------------------------------------------------------------------------------
void CActionSetVariable::Serialize(Serialization::IArchive& ar)
{
	if (ar.openBlock("SetVariableValues1", " "))
	{
		_Serialize(ar, "^^ Variable", "^^>140>Collection");
		ar.closeBlock();
	}

	if (ar.openBlock("SetVariableValues2", " "))
	{
		ar(m_changeOperation, "Operation", "^^>Operation");
		ar(m_valueToSet, "Value", "^^ Value ");
		ar(Serialization::Decorators::Range<float>(m_cooldown, 0.0f, 120.0f), "Cooldown", "^^> Cooldown");
		ar.closeBlock();
	}

#if !defined(_RELEASE)
	if (ar.isEdit() && ar.isOutput())
	{
		ar(GetVerboseInfo(), "ConditionDesc", "!^<");

		if (m_cooldown < 0.0f)
		{
			ar.warning(m_cooldown, "Cooldown cannot be negative");
		}
		if (m_valueToSet.GetType() == eDRVT_Undefined)
		{
			ar.warning(m_cooldown, "No value to set specified");
		}
		if (m_changeOperation != CActionSetVariable::eChangeOperation_Set && m_valueToSet.GetType() != eDRVT_Float && m_valueToSet.GetType() != eDRVT_Int)
		{
			ar.warning(m_cooldown, "Increment/Decrement is only supported for FLOAT and INTEGER variables");
		}
		CVariableCollection* pCollection = CResponseSystem::GetInstance()->GetCollection(m_collectionName);
		if (pCollection)
		{
			if (!pCollection->GetVariableValue(m_variableName).DoTypesMatch(m_valueToSet))
			{
				ar.warning(m_cooldown, "The type of the value to set and the type of the variable don't match!");
			}
		}
	}
#endif
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

namespace DRS
{
SERIALIZATION_ENUM_BEGIN_NESTED(CActionSetVariable, EChangeOperation, "OperationType")
SERIALIZATION_ENUM(CActionSetVariable::eChangeOperation_Set, "Set", "Set")
SERIALIZATION_ENUM(CActionSetVariable::eChangeOperation_Increment, "Increment", "Increment")
SERIALIZATION_ENUM(CActionSetVariable::eChangeOperation_Decrement, "Decrement", "Decrement")
SERIALIZATION_ENUM_END()
}
