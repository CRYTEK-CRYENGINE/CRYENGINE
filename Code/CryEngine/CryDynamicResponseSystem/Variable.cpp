// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Variable.h"
#include "ResponseSystem.h"
#include "ResponseInstance.h"
#include "VariableCollection.h"

using namespace CryDRS;

bool IVariableUsingBase::s_bDoDisplayCurrentValueInDebugOutput = false;

void CVariable::Serialize(Serialization::IArchive& ar)
{
	ar(m_name, "name", "^!>150>");
	m_value.Serialize(ar);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

IVariableUsingBase::IVariableUsingBase() {}

//--------------------------------------------------------------------------------------------------
CVariableCollection* IVariableUsingBase::GetCurrentCollection(CResponseInstance* pResponseInstance)
{
	if (m_collectionName == CVariableCollection::s_localCollectionName)  //local variable collection
	{
		CResponseActor* const pCurrentActor = pResponseInstance->GetCurrentActor();
#if defined(DRS_COLLECT_DEBUG_DATA)
		s_lastTestedObjectName = pCurrentActor->GetName().GetText();
#endif
		return pCurrentActor->GetLocalVariables();
	}
	else if (m_collectionName == CVariableCollection::s_contextCollectionName)  //context variable collection
	{
		return pResponseInstance->GetContextVariablesImpl().get();
	}
	else
	{
		static CVariableCollectionManager* const pVariableCollectionMgr = CResponseSystem::GetInstance()->GetVariableCollectionManager();
		return pVariableCollectionMgr->GetCollection(m_collectionName);  //todo: cache
	}
	return nullptr;
}

//--------------------------------------------------------------------------------------------------
CVariable* IVariableUsingBase::GetCurrentVariable(CResponseInstance* pResponseInstance)
{
	CVariableCollection* pCollection = GetCurrentCollection(pResponseInstance);
	if (pCollection)
	{
		CVariable* pVariable = pCollection->GetVariable(m_variableName);
#if defined(DRS_COLLECT_DEBUG_DATA)
		if (pVariable)
		{
			s_lastTestedValueAsString = pVariable->m_value.GetValueAsString();
		}
#endif
		return pVariable;
	}
	return nullptr;
}

//--------------------------------------------------------------------------------------------------
const CVariableValue& IVariableUsingBase::GetCurrentVariableValue(CResponseInstance* pResponseInstance)
{
	CVariableCollection* pCollection = GetCurrentCollection(pResponseInstance);
	const CVariableValue& value = (pCollection) ? pCollection->GetVariableValue(m_variableName) : CVariableCollection::s_newVariableValue;
#if defined(DRS_COLLECT_DEBUG_DATA)
	s_lastTestedValueAsString = value.GetValueAsString();
#endif
	return value;
}

//--------------------------------------------------------------------------------------------------
string IVariableUsingBase::GetVariableVerboseName() const
{
#if defined(DRS_COLLECT_DEBUG_DATA)
	string out = "'";
	if (m_collectionName == CVariableCollection::s_localCollectionName && !s_lastTestedObjectName.empty())
		out += m_collectionName.GetText() + "(" + s_lastTestedObjectName + ")::";
	else if (!m_collectionName.IsValid())
		out += "MISSING_Collection::";
	else
		out += m_collectionName.GetText() + "::";

	if (!m_variableName.IsValid())
		out += "MISSING_Variable";
	else
		out += m_variableName.GetText();

	if (s_bDoDisplayCurrentValueInDebugOutput && !s_lastTestedValueAsString.empty())
		out += "' (value:'" + s_lastTestedValueAsString + "')";
	else
		out += "'";
	return out;
#else
	return m_collectionName.GetText() + "::" + m_variableName.GetText();
#endif
}

//--------------------------------------------------------------------------------------------------
void IVariableUsingBase::_Serialize(Serialization::IArchive& ar, const char* szVariableName /* = "^Variable" */, const char* szCollectionName /* = "^Collection" */)
{
	ar(m_collectionName, "collection", szCollectionName);
	ar(m_variableName, "variable", szVariableName);

#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS) && defined(HASHEDSTRING_STORES_SOURCE_STRING)
	if (ar.isEdit())
	{
		if (!m_collectionName.IsValid())
		{
			ar.warning(m_collectionName.m_textCopy, "No variable collection specified");
		}
		else if (!m_variableName.IsValid())
		{
			ar.warning(m_collectionName.m_textCopy, "No variable specified");
		}
		else if (!m_variableName.m_textCopy.empty() && (m_variableName.m_textCopy[0] == ' ' || m_variableName.m_textCopy[m_variableName.m_textCopy.size() - 1] == ' '))
		{
			ar.warning(m_variableName.m_textCopy, "Variable name starts or ends with a space. Check if this is really wanted");
		}
	}
#endif
}
