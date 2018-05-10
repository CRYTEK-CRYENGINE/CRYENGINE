// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   ScriptBind_AI.cpp
   Description: Goal Op Factory interface and management classes

   -------------------------------------------------------------------------
   History:
   -24:02:2008   - Created by Matthew

 *********************************************************************/

#include "StdAfx.h"
#include "GoalOpFactory.h"

IGoalOp* CGoalOpFactoryOrdering::GetGoalOp(const char* sGoalOpName, IFunctionHandler* pH, int nFirstParam, GoalParameters& params) const
{
	IGoalOp* pResult = NULL;
	const TFactoryVector::const_iterator itEnd = m_Factories.end();
	for (TFactoryVector::const_iterator it = m_Factories.begin(); !pResult && it != itEnd; ++it)
		pResult = (*it)->GetGoalOp(sGoalOpName, pH, nFirstParam, params);
	return pResult;
}

IGoalOp* CGoalOpFactoryOrdering::GetGoalOp(EGoalOperations op, GoalParameters& params) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	IGoalOp* pResult = NULL;
	const TFactoryVector::const_iterator itEnd = m_Factories.end();
	for (TFactoryVector::const_iterator it = m_Factories.begin(); !pResult && it != itEnd; ++it)
		pResult = (*it)->GetGoalOp(op, params);
	return pResult;
}

void CGoalOpFactoryOrdering::AddFactory(IGoalOpFactory* pFactory)
{
	m_Factories.push_back(pFactory);
}

void CGoalOpFactoryOrdering::PrepareForFactories(size_t count)
{
	m_Factories.reserve(count);
}

void CGoalOpFactoryOrdering::DestroyAll(void)
{
	const TFactoryVector::const_iterator itEnd = m_Factories.end();
	for (TFactoryVector::const_iterator it = m_Factories.begin(); it != itEnd; ++it)
		delete (*it);
	m_Factories.clear();
}
