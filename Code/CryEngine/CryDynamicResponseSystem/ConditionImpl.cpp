// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ConditionImpl.h"
#include "Variable.h"
#include "VariableCollection.h"
#include "VariableValue.h"
#include "ResponseSystem.h"
#include <CryDynamicResponseSystem/IDynamicResponseCondition.h>
#include "ResponseInstance.h"

using namespace CryDRS;

#if defined(DRS_COLLECT_DEBUG_DATA)
string IVariableUsingBase::s_lastTestedValueAsString;
string IVariableUsingBase::s_lastTestedObjectName;
#endif

//--------------------------------------------------------------------------------------------------
bool CVariableAgainstVariablesCondition::IsMet(DRS::IResponseInstance* pResponseInstance)
{
	//todo: cache
	CResponseInstance* pInstanceImpl = static_cast<CResponseInstance*>(pResponseInstance);
	const CVariable* pToTestVariable = GetVariable(m_collectionNameToTest, m_variableNameToTest, pInstanceImpl);
	if (!pToTestVariable)
	{
		return false;
	}
	const CVariable* pSmallerVariable = GetVariable(m_collectionNameSmaller, m_variableNameSmaller, pInstanceImpl);
	const CVariable* pLargerVariable = GetVariable(m_collectionNameLarger, m_variableNameLarger, pInstanceImpl);
	return ((!pSmallerVariable) || (*pToTestVariable) >= (*pSmallerVariable)) && ((!pLargerVariable) || (*pToTestVariable) <= (*pLargerVariable));
}

//--------------------------------------------------------------------------------------------------
void CVariableAgainstVariablesCondition::Serialize(Serialization::IArchive& ar)
{
	ar(m_collectionNameSmaller, "smallerCollection", "LessOrEqualCollection");
	ar(m_variableNameSmaller, "smaller", "LessOrEqualVariable");
	ar(m_collectionNameToTest, "toTestCollection", "ToTestCollection");
	ar(m_variableNameToTest, "toTest", "ToTestVariable");
	ar(m_collectionNameLarger, "largerCollection", "GreaterOrEqualCollection");
	ar(m_variableNameLarger, "larger", "GreaterOrEqualVariable");

#if defined(ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS) && defined(HASHEDSTRING_STORES_SOURCE_STRING)
	if (ar.isEdit())
	{
		if (!m_collectionNameToTest.IsValid() || !m_variableNameToTest.IsValid())
		{
			ar.warning(m_collectionNameToTest.m_textCopy, "No variable to compare specified!");
		}

		if (!m_collectionNameSmaller.IsValid() && !m_collectionNameLarger.IsValid())
		{
			ar.warning(m_collectionNameSmaller.m_textCopy, "You need to specify at least a larger or a smaller variable to compare to!");
			ar.warning(m_collectionNameLarger.m_textCopy, "You need to specify at least a larger or a smaller variable to compare to!");
		}
	}
#endif

#if !defined(_RELEASE)
	if (ar.isEdit() && ar.isOutput())
	{
		ar(GetVerboseInfo(), "ConditionDesc", "!^<");
	}
#endif
}

//--------------------------------------------------------------------------------------------------
string CVariableAgainstVariablesCondition::GetVerboseInfo() const
{
	if (m_collectionNameSmaller.IsValid() && m_collectionNameLarger.IsValid())
	{
		return m_collectionNameSmaller.GetText() + "::" + m_variableNameSmaller.GetText() + " <= " + m_collectionNameToTest.GetText() + "::" + m_variableNameToTest.GetText() + " <= " + m_collectionNameLarger.GetText() + "::" + m_variableNameLarger.GetText();
	}
	else if (m_collectionNameSmaller.IsValid())
	{
		return m_collectionNameSmaller.GetText() + "::" + m_variableNameSmaller.GetText() + " <= " + m_collectionNameToTest.GetText() + "::" + m_variableNameToTest.GetText();
	}
	else if (m_collectionNameLarger.IsValid())
	{
		return m_collectionNameToTest.GetText() + "::" + m_variableNameToTest.GetText() + " <= " + m_collectionNameLarger.GetText() + "::" + m_variableNameLarger.GetText();
	}
	else
	{
		return "missing parameter";
	}
}

//--------------------------------------------------------------------------------------------------
const CVariable* CVariableAgainstVariablesCondition::GetVariable(const CHashedString& collection, const CHashedString& variable, CResponseInstance* pResponseInstance) const
{
	CVariableCollection* pCollection;
	if (collection == CVariableCollection::s_localCollectionName)
	{
		pCollection = pResponseInstance->GetCurrentActor()->GetLocalVariables();
	}
	else if (collection == CVariableCollection::s_contextCollectionName)
	{
		pCollection = pResponseInstance->GetContextVariablesImpl().get();
	}
	else
	{
		static CVariableCollectionManager* const pVariableCollectionMgr = CResponseSystem::GetInstance()->GetVariableCollectionManager();
		pCollection = pVariableCollectionMgr->GetCollection(collection);
	}
	if (pCollection)
	{
		return pCollection->GetVariable(variable);
	}
	return nullptr;
}

//--------------------------------------------------------------------------------------------------
void CVariableSmallerCondition::Serialize(Serialization::IArchive& ar)
{
	IVariableUsingBase::_Serialize(ar);
	ar(m_value, "compareValue", "^Less than");

#if !defined(_RELEASE)
	if (ar.isEdit() && ar.isOutput() && m_value.GetType() != eDRVT_Undefined && m_value.GetType() != eDRVT_Int && m_value.GetType() != eDRVT_Float)
	{
		ar.warning(m_collectionName.m_textCopy, "Only Integer or Float types should be tested");
	}
#endif
}

//--------------------------------------------------------------------------------------------------
bool CVariableSmallerCondition::IsMet(DRS::IResponseInstance* pResponseInstance)
{
	return GetCurrentVariableValue(static_cast<CResponseInstance*>(pResponseInstance)) < m_value;
}

//--------------------------------------------------------------------------------------------------
string CVariableSmallerCondition::GetVerboseInfo() const
{
	return "Is " + GetVariableVerboseName() + " LESS than '" + m_value.GetValueAsString() + "'";
}

//--------------------------------------------------------------------------------------------------
void CVariableLargerCondition::Serialize(Serialization::IArchive& ar)
{
	IVariableUsingBase::_Serialize(ar);
	ar(m_value, "compareValue", "^Greater than");

#if !defined(_RELEASE)
	if (ar.isEdit() && ar.isOutput() && m_value.GetType() != eDRVT_Undefined && m_value.GetType() != eDRVT_Int && m_value.GetType() != eDRVT_Float)
	{
		ar.warning(m_collectionName.m_textCopy, "Only Integer or Float types should be tested");
	}
#endif
}

//--------------------------------------------------------------------------------------------------
bool CVariableLargerCondition::IsMet(DRS::IResponseInstance* pResponseInstance)
{
	return GetCurrentVariableValue(static_cast<CResponseInstance*>(pResponseInstance)) > m_value;
}

//--------------------------------------------------------------------------------------------------
string CVariableLargerCondition::GetVerboseInfo() const
{
	return "Is " + GetVariableVerboseName() + " GREATER than '" + m_value.GetValueAsString() + "'";
}

//--------------------------------------------------------------------------------------------------
void CVariableEqualCondition::Serialize(Serialization::IArchive& ar)
{
	IVariableUsingBase::_Serialize(ar);
	ar(m_value, "compareValue", "^Equal to");
}

//--------------------------------------------------------------------------------------------------
bool CVariableEqualCondition::IsMet(DRS::IResponseInstance* pResponseInstance)
{
	return GetCurrentVariableValue(static_cast<CResponseInstance*>(pResponseInstance)) == m_value;
}

//--------------------------------------------------------------------------------------------------
string CVariableEqualCondition::GetVerboseInfo() const
{
	return "Is " + GetVariableVerboseName() + " EQUAL to '" + m_value.GetValueAsString() + "'";
}

//--------------------------------------------------------------------------------------------------
void CVariableRangeCondition::Serialize(Serialization::IArchive& ar)
{
	IVariableUsingBase::_Serialize(ar);
	ar(m_value, "compareValue", "Greater than or equal to");
	ar(m_value2, "compareValue2", "Less than or equal to");

#if !defined(_RELEASE)
	if (ar.isEdit() && ar.isOutput())
	{
		if (!m_value.DoTypesMatch(m_value2))
		{
			ar.warning(m_collectionName.m_textCopy, "The type of min and max value do not match");
		}
		if (m_value2.GetType() == eDRVT_Undefined)
		{
			ar.warning(m_collectionName.m_textCopy, "Max value undefined");
		}
		if (m_value.GetType() != eDRVT_Undefined && m_value.GetType() != eDRVT_Int && m_value.GetType() != eDRVT_Float)
		{
			ar.warning(m_collectionName.m_textCopy, "Only Integer or Float types should be tested");
		}

		string shortdesc = "between " + m_value.GetValueAsString() + " and " + m_value2.GetValueAsString();
		ar(shortdesc, "ConditionDesc", "!^<");
	}
#endif
}

//--------------------------------------------------------------------------------------------------
bool CVariableRangeCondition::IsMet(DRS::IResponseInstance* pResponseInstance)
{
	const CVariableValue& value = GetCurrentVariableValue(static_cast<CResponseInstance*>(pResponseInstance));
	return value >= m_value && value <= m_value2;
}

//--------------------------------------------------------------------------------------------------
string CVariableRangeCondition::GetVerboseInfo() const
{
	return "Is " + GetVariableVerboseName() + " BETWEEN '" + m_value.GetValueAsString() + "' AND '" + m_value2.GetValueAsString() + "'";
}
