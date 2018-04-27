// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ActionCopyVariable.h"
#include "VariableCollection.h"
#include "ResponseInstance.h"

#include <CryEntitySystem/IEntityComponent.h>
#include <CryDynamicResponseSystem/IDynamicResponseAction.h>

using namespace CryDRS;

DRS::IResponseActionInstanceUniquePtr CActionCopyVariable::Execute(DRS::IResponseInstance* pResponseInstance)
{	
	CResponseInstance* pInstanceImpl = static_cast<CResponseInstance*>(pResponseInstance);
	if (CVariable* pSourceVar = m_sourceVariable.GetCurrentVariable(pInstanceImpl))
	{
		CVariableCollection* pTargetCollection = m_targetVariable.GetCurrentCollection(pInstanceImpl);
		if (!pTargetCollection && !m_targetVariable.m_collectionName.IsValid())
		{
			pTargetCollection = CResponseSystem::GetInstance()->GetVariableCollectionManager()->CreateVariableCollection(m_targetVariable.m_collectionName);
		}
		if (pTargetCollection)
		{
			if (CVariable* pTargetVar = pTargetCollection->CreateOrGetVariable(m_targetVariable.m_variableName))
			{
				if (m_changeOperation == eChangeOperation_Set)
				{
					pTargetCollection->SetVariableValue(pTargetVar, pSourceVar->m_value, m_cooldown);
				}
				else if (m_changeOperation == eChangeOperation_Increment)
				{
					pTargetCollection->SetVariableValue(pTargetVar, pTargetVar->m_value + pSourceVar->m_value, m_cooldown);
				}
				else if (m_changeOperation == eChangeOperation_Decrement)
				{
					pTargetCollection->SetVariableValue(pTargetVar, pTargetVar->m_value - pSourceVar->m_value, m_cooldown);
				}
				else if (m_changeOperation == eChangeOperation_Swap)
				{
					CVariableValue temp = pTargetVar->m_value;
					pTargetCollection->SetVariableValue(pTargetVar, pSourceVar->m_value, m_cooldown);
					pTargetCollection->SetVariableValue(pSourceVar, temp, m_cooldown);
				}
			}
		}
	}
	else
	{
		DrsLogWarning(string().Format("Could not fetch source variable '%s::%s'", m_sourceVariable.m_collectionName.GetText().c_str(), m_sourceVariable.m_variableName.GetText().c_str()).c_str());
	}
	return nullptr;
}

string CActionCopyVariable::GetVerboseInfo() const
{
	return string().Format("From %s to %s", m_sourceVariable.GetVariableVerboseName().c_str(), m_targetVariable.GetVariableVerboseName().c_str());
}

void CActionCopyVariable::Serialize(Serialization::IArchive& ar)
{
	if (ar.openBlock("Source", " "))
	{
		m_sourceVariable._Serialize(ar, "^^SourceVariable", "^^>140>SourceCollection");
		ar.closeBlock();
	}

	if (ar.openBlock("Target", " "))
	{
		m_targetVariable._Serialize(ar, "^^TargetVariable", "^^>140>TargetCollection");
		ar.closeBlock();
	}
	
	if (ar.openBlock("Additional", " "))
	{
		ar(m_changeOperation, "operation", "^^>100>Operation");
		ar(Serialization::Decorators::Range<float>(m_cooldown, 0.0f, 120.0f), "Cooldown", "^^> Cooldown");
		ar.closeBlock();
	}

	if (ar.isEdit())
	{
		if (ar.isInput())
		{
			//for conveniences we copy the collection name from source to target and vice versa, when only one is specified yet.
			if (!m_targetVariable.m_collectionName.IsValid())
			{
				m_targetVariable.m_collectionName = m_sourceVariable.m_collectionName;
			}
			else if (!m_sourceVariable.m_collectionName.IsValid())
			{
				m_sourceVariable.m_collectionName = m_targetVariable.m_collectionName;
			}
		}
		else if (ar.isOutput())
		{
			//sanity check for cooldown
			if (m_cooldown < 0.0f)
			{
				ar.warning(m_cooldown, "Cooldown can`t be lower than 0.");
			}

			if (m_sourceVariable.m_collectionName.IsValid()
				&& m_sourceVariable.m_collectionName != CVariableCollection::s_localCollectionName
				&& m_sourceVariable.m_collectionName != CVariableCollection::s_contextCollectionName)
			{
				const CVariableCollectionManager* pVariableCollectionMgr = CResponseSystem::GetInstance()->GetVariableCollectionManager();
				CVariableCollection* pSourceVC = pVariableCollectionMgr->GetCollection(m_sourceVariable.m_collectionName);

				if (pSourceVC)
				{
					const CVariableValue& sourceValue = pSourceVC->GetVariableValue(m_sourceVariable.m_variableName);
					//checks for correct types
					if (m_changeOperation != CActionCopyVariable::eChangeOperation_Set
						&& sourceValue.GetType() != eDRVT_Float
						&& sourceValue.GetType() != eDRVT_Int)
					{
						ar.warning(m_cooldown, "Increment/Decrement is only supported for FLOAT and INTEGER variables");
					}

					//only check types if the target variable exists, otherwise it`s fine, we will create that variable with the correct time on execute
					CVariableCollection* pTargetVC = pVariableCollectionMgr->GetCollection(m_targetVariable.m_collectionName);
					if (pTargetVC
						&& !sourceValue.DoTypesMatch(pTargetVC->GetVariableValue(m_targetVariable.m_variableName).GetType()))
					{
						ar.warning(m_cooldown, "The type of the value to set and the type of the variable don't match!");
					}
				}
			}
		}
	}
}

namespace DRS
{
	SERIALIZATION_ENUM_BEGIN_NESTED(CActionCopyVariable, EChangeOperation, "OperationType")
		SERIALIZATION_ENUM(CActionCopyVariable::eChangeOperation_Set, "Set", "Set")
		SERIALIZATION_ENUM(CActionCopyVariable::eChangeOperation_Increment, "Increment", "Increment")
		SERIALIZATION_ENUM(CActionCopyVariable::eChangeOperation_Decrement, "Decrement", "Decrement")
		SERIALIZATION_ENUM(CActionCopyVariable::eChangeOperation_Swap, "Swap", "Swap")
		SERIALIZATION_ENUM_END()
}
