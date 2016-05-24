// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ConditionsCollection.h"
#include "VariableCollection.h"
#include <CryDynamicResponseSystem/IDynamicResponseCondition.h>
#include "ResponseSystem.h"
#include "SpecialConditionsImpl.h"
#include "ResponseInstance.h"
#include "ResponseManager.h"

using namespace CryDRS;

#if !defined(_RELEASE)
DRS::IResponseActor* CConditionsCollection::s_pActorForEvaluation = nullptr;
#endif

//--------------------------------------------------------------------------------------------------
bool CConditionsCollection::IsMet(CResponseInstance* pResponseInstance)
{
	for (SConditionInfo& current : m_conditions)
	{
		CRY_ASSERT(current.m_pCondition);

		if (!current.m_bNegated)
		{
			if (!current.m_pCondition->IsMet(pResponseInstance))
			{
				DRS_DEBUG_DATA_ACTION(AddConditionChecked(&current, m_bNegated));
				return m_bNegated;
			}
		}
		else
		{
			if (current.m_pCondition->IsMet(pResponseInstance))
			{
				DRS_DEBUG_DATA_ACTION(AddConditionChecked(&current, m_bNegated));
				return m_bNegated;
			}
		}
		DRS_DEBUG_DATA_ACTION(AddConditionChecked(&current, !m_bNegated));
	}

	return !m_bNegated;
}

//--------------------------------------------------------------------------------------------------
CConditionsCollection::~CConditionsCollection()
{
	m_conditions.clear();
}

//--------------------------------------------------------------------------------------------------
void CConditionsCollection::AddCondition(DRS::IConditionSharedPtr pCondition, bool Negated)
{
	if (pCondition)
	{
		SConditionInfo newCondition;
		newCondition.m_bNegated = Negated;
		newCondition.m_pCondition = pCondition;
		m_conditions.push_back(newCondition);
	}
}

//--------------------------------------------------------------------------------------------------
void CConditionsCollection::SConditionInfo::Serialize(Serialization::IArchive& ar)
{
	if (ar.isInput())
		m_pCondition = nullptr;

	ar(m_pCondition, "Condition", ">+^");

	if (!m_pCondition)
	{
		m_pCondition = DRS::IConditionSharedPtr(new CPlaceholderCondition());

		if (ar.isOutput())
		{
			ar(m_pCondition, "Condition", ">+^");
		}
	}
	ar(m_bNegated, "Negated", "^>Negated");

#if !defined(_RELEASE)
	if (ar.getFilter() & CResponseManager::eSH_EvaluateResponses)
	{
		bool bCurrentlyMet = false;
		if (s_pActorForEvaluation)
		{
			CryDRS::SSignal tempSignal(CHashedString::GetEmpty(), static_cast<CResponseActor*>(s_pActorForEvaluation), nullptr);
			CResponseInstance tempResponseInstance(tempSignal, nullptr);
			bCurrentlyMet = m_pCondition->IsMet(&tempResponseInstance);
		}
		ar(bCurrentlyMet, "currentlyMet", "^^Currently met");
	}
#endif
}

//--------------------------------------------------------------------------------------------------
void CConditionsCollection::Serialize(Serialization::IArchive& ar)
{
	if (ar.openBlock("Conditions", "Conditions"))
	{
		ar(m_conditions, "Conditions", "^+");
		if (m_conditions.empty())
		{
			ar(m_bNegated, "Negated", "");
		}
		else
		{
			ar(m_bNegated, "Negated", "^Negated as whole");
		}

		ar.closeBlock();
	}
}
