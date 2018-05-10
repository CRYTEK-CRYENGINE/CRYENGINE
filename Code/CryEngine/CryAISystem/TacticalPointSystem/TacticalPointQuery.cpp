// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TacticalPointQuery.h"
#include "TacticalPointQueryEnum.h"
#include "TacticalPointSystem.h"

#include "Navigation/NavigationSystem/NavigationSystem.h"

using std::vector;

//------ CCriterion -------------------------------------------------------------------------------//

CCriterion::CCriterion()
{
	m_eQuery = eTPQ_None;
	m_eLimits = eTPQ_None;
	m_eObject = eTPQ_None;
	m_eObjectAux = eTPQ_None;
	m_nRelativeValueSource = eTPSRVS_Invalid;
}

bool CCriterion::IsValid() const
{
	CTacticalPointSystem* pTPS = gAIEnv.pTacticalPointSystem;

	// Check the necessary tokens all exist and that we haven't corrupted them in any way
	if (!pTPS->CheckToken(GetQuery())) return false;

	if (GetLimits())
	{
		if (!pTPS->CheckToken(GetLimits())) return false;
		if (!(GetQuery() & (eTPQ_FLAG_PROP_REAL | eTPQ_FLAG_MEASURE))) return false;
	}

	if (GetObject())
	{
		if (!pTPS->CheckToken(GetObject())) return false;
		if (!(GetQuery() & (eTPQ_FLAG_TEST | eTPQ_FLAG_MEASURE | eTPQ_FLAG_GENERATOR | eTPQ_FLAG_GENERATOR_O))) return false;
	}
	else
	{
		if (!(GetQuery() & (eTPQ_FLAG_PROP_BOOL | eTPQ_FLAG_PROP_REAL))) return false;
	}

	if (GetObjectAux())
	{
		if (!pTPS->CheckToken(GetObjectAux())) return false;
		if (!(GetQuery() & eTPQ_FLAG_GENERATOR_O)) return false;
	}
	else
	{
		if (GetQuery() & (eTPQ_FLAG_GENERATOR_O)) return false;    // Generator_O is a special case
	}

	return true;
}

//------ COptionCriteria --------------------------------------------------------------------------//

bool COptionCriteria::AddToParameters(const char* sSpec, float fValue)
{
	bool bRet = true;

	// get proper param index
	ETacticalPointQueryParameter nParam = gAIEnv.pTacticalPointSystem->TranslateParam(sSpec);

	// if its invalid - report it
	if (nParam == eTPQP_Invalid)
	{
		bRet = false;

		AIWarningID("<TacticalPointSystem> ", "AddToParameters - Parameter missing: \"%s\"", sSpec);
	}
	else
	{
		switch (nParam)
		{
		case eTPQP_ObjectsType:
			m_params.iObjectsType = (int)fValue;
			break;
		case eTPQP_Density:
			m_params.fDensity = fValue;
			break;
		case eTPQP_Height:
			m_params.fHeight = fValue;
			break;
		case eTPQP_HorizontalSpacing:
			m_params.fHorizontalSpacing = fValue;
			break;
		default:
			AIWarningID("<TacticalPointSystem> ", "AddToParameters - Unhandled parameter (code bug) \"%s\"", sSpec);
			break;
		}
	}

	return bRet;
}

bool COptionCriteria::AddToParameters(const char* sSpec, bool bValue)
{
	bool bRet = true;

	// get proper param index
	ETacticalPointQueryParameter nParam = gAIEnv.pTacticalPointSystem->TranslateParam(sSpec);

	// if its invalid - report it
	if (nParam == eTPQP_Invalid)
	{
		bRet = false;

		AIWarningID("<TacticalPointSystem> ", "AddToParameters - Parameter missing: \"%s\"", sSpec);
	}
	else
	{
		//		switch(nParam)
		//		{
		//		default:
		AIWarningID("<TacticalPointSystem> ", "AddToParameters - Unhandled parameter (code bug) \"%s\"", sSpec);
		//			break;
		//		}
	}

	return bRet;
}

bool COptionCriteria::AddToParameters(const char* sSpec, const char* sValue)
{
	bool bRet = true;

	// get proper param index
	ETacticalPointQueryParameter nParam = gAIEnv.pTacticalPointSystem->TranslateParam(sSpec);

	// if its invalid - report it
	if (nParam == eTPQP_Invalid)
	{
		bRet = false;

		AIWarningID("<TacticalPointSystem> ", "AddToParameters - Parameter missing: \"%s\"", sSpec);
	}
	else
	{
		switch (nParam)
		{
		case eTPQP_OptionLabel:
			m_params.sSignalToSend = sValue;
			break;
		case eTPQP_TagPointPostfix:
			m_params.tagPointPostfix = sValue;
			break;
		case eTPQP_ExtenderStringParameter:
			m_params.extenderStringParamenter = sValue;
			break;
		case eTPQP_NavigationAgentType:
			m_params.sNavigationAgentType = sValue;
			if (gAIEnv.pNavigationSystem != NULL)
			{
				if (gAIEnv.pNavigationSystem->GetAgentTypeID(m_params.sNavigationAgentType) == NavigationAgentTypeID())
				{
					AIWarningID("<TacticalPointSystem> ", "AddToParameters - NavigationAgentType '%s' is invalid! \"%s\"", m_params.sNavigationAgentType.c_str(), sSpec);
				}
			}
			break;
		default:
			AIWarningID("<TacticalPointSystem> ", "AddToParameters - Unhandled parameter (code bug) \"%s\"", sSpec);
			break;
		}
	}

	return bRet;
}

bool COptionCriteria::AddToGeneration(const char* sSpec, float fValue)
{
	int validFlags = (eTPQ_FLAG_GENERATOR | eTPQ_FLAG_GENERATOR_O);
	CCriterion criterion = GetCriterion(sSpec);

	// Check this is the right kind of query and that it is a valid one
	if (!(criterion.GetQuery() & validFlags) || !criterion.IsValid())
	{
		AIWarningID("<TacticalPointSystem> ", "AddToGeneration - Invalid query \"%s\"", sSpec);
		return false;
	}

	// Apply the value given (generation queries only take floats or ETPSRelativeValueSource)
	criterion.SetValue(fValue);

	// Add to list for this option
	m_vGeneration.push_back(criterion);
	return true;
}

bool COptionCriteria::AddToGeneration(const char* sSpec, ETPSRelativeValueSource nSource)
{
	assert(sSpec != NULL && "Empty parameter passed as sSpec to COptionCriteria::AddToGeneration!");

	int validFlags = (eTPQ_FLAG_GENERATOR | eTPQ_FLAG_GENERATOR_O);
	CCriterion criterion = GetCriterion(sSpec);

	// Check this is the right kind of query and that it is a valid one
	if (!(criterion.GetQuery() & validFlags) || !criterion.IsValid())
	{
		AIWarningID("<TacticalPointSystem> ", "AddToGeneration - Invalid query \"%s\"", sSpec);
		return false;
	}

	// Apply the value given (generation queries only take floats or ETPSRelativeValueSource)
	criterion.SetValue(nSource);

	// Add to list for this option
	m_vGeneration.push_back(criterion);
	return true;
}

bool COptionCriteria::AddToConditions(const char* sSpec, bool bValue)
{
	// Taking a boolean value so must be a Boolean Property or a Test (query returning a bool)
	int validFlags = eTPQ_FLAG_PROP_BOOL | eTPQ_FLAG_TEST;
	CCriterion criterion = GetCriterion(sSpec);

	// Check this is the right kind of query and that it is a valid one
	if (!(criterion.GetQuery() & validFlags) || !criterion.IsValid())
	{
		AIWarningID("<TacticalPointSystem> ", "AddToConditions(bool) - Invalid query \"%s\"", sSpec);
		return false;
	}

	// Furthermore, Bool Properties and Tests return bools
	// Hence must _not_ include a Limit
	if (criterion.GetLimits()) return false;

	// Apply the value given (generation queries only take floats)
	criterion.SetValue(bValue);

	// Add to list for this option
	m_vConditions.push_back(criterion);
	return true;
}

bool COptionCriteria::AddToConditions(const char* sSpec, float fValue)
{
	// Taking a float value so must be a Real Property or a Measure (query returning a float)
	int validFlags = eTPQ_FLAG_PROP_REAL | eTPQ_FLAG_MEASURE;
	CCriterion criterion = GetCriterion(sSpec);

	// Check this is the right kind of query and that it is a valid one
	if (!(criterion.GetQuery() & validFlags) || !criterion.IsValid())
	{
		AIWarningID("<TacticalPointSystem> ", "AddToConditions(float) - Invalid query \"%s\"", sSpec);
		return false;
	}

	// Furthermore, Real Properties and Measures return floats and we need to know how to compare against our float
	// Hence must include a Limit
	if (!criterion.GetLimits()) return false;

	// Apply the value given (generation queries only take floats)
	criterion.SetValue(fValue);

	// Add to list for this option
	m_vConditions.push_back(criterion);
	return true;
}

bool COptionCriteria::AddToWeights(const char* sSpec, float fWeight)
{
	// Note that although weights always end up producing a normalised [0-1] result, that can come from a boolean
	// Hence yes/no properties and tests are valid, as expressing a simple preference
	int validFlags = eTPQ_FLAG_PROP_REAL | eTPQ_FLAG_MEASURE | eTPQ_FLAG_PROP_BOOL | eTPQ_FLAG_TEST;
	CCriterion criterion = GetCriterion(sSpec);

	// Check this is the right kind of query and that it is a valid one
	if (!(criterion.GetQuery() & validFlags) || !criterion.IsValid())
	{
		AIWarningID("<TacticalPointSystem> ", "AddToWeights - Invalid query \"%s\"", sSpec);
		return false;
	}

	// Furthermore, weights can't include comparisons (because the value is used as the relative weight!)
	// Hence must _not_ include a Limit
	if (criterion.GetLimits()) return false;

	// Apply the value given - which for weights, is the relative weight of this query result
	criterion.SetValue(fWeight);

	// Add to list for this option
	m_vWeights.push_back(criterion);
	return true;
}

string COptionCriteria::GetDescription() const
{
	vector<CCriterion>::const_iterator itC;
	string sResult, sBuffer;
	CTacticalPointSystem* pTPS = gAIEnv.pTacticalPointSystem;
	bool bOk = true;
	sResult.reserve(256);
	sResult.append("Generation = { ");
	for (itC = m_vGeneration.begin(); itC != m_vGeneration.end(); ++itC)
	{
		bOk &= pTPS->Unparse(*itC, sBuffer);
		sResult.append(sBuffer);
		sBuffer.Format(" = %f, ", itC->GetValueAsFloat());
		sResult.append(sBuffer);
	}
	sResult.append(" }, ");
	sResult.append("Conditions = { ");
	for (itC = m_vConditions.begin(); itC != m_vConditions.end(); ++itC)
	{
		bOk &= pTPS->Unparse(*itC, sBuffer);
		sResult.append(sBuffer);
		if (itC->GetLimits())
			sBuffer.Format(" = %f, ", itC->GetValueAsFloat());
		else
			sBuffer.Format(" = %s, ", itC->GetValueAsBool() ? "true" : "false");
		sResult.append(sBuffer);
	}
	sResult.append(" }, ");
	sResult.append("Weights = { ");
	for (itC = m_vWeights.begin(); itC != m_vWeights.end(); ++itC)
	{
		bOk &= pTPS->Unparse(*itC, sBuffer);
		sResult.append(sBuffer);
		sBuffer.Format(" = %f, ", itC->GetValueAsFloat());
		sResult.append(sBuffer);
	}
	sResult.append(" }, ");

	return sResult;
}

bool COptionCriteria::IsValid() const
{
	// This is pretty redundant but might be handy - each criteria is checked when added.
	// What we _don't_ make sure of here is that each is valid for the role it is assigned.
	// We could refactor that out of the code that adds them.

	vector<CCriterion>::const_iterator itC;
	for (itC = m_vGeneration.begin(); itC != m_vGeneration.end(); ++itC)
		if (!itC->IsValid()) return false;
	for (itC = m_vConditions.begin(); itC != m_vConditions.end(); ++itC)
		if (!itC->IsValid()) return false;
	for (itC = m_vWeights.begin(); itC != m_vWeights.end(); ++itC)
		if (!itC->IsValid()) return false;

	return true;
}

CCriterion COptionCriteria::GetCriterion(const char* sSpec)
{
	TTacticalPointQuery query, limits, object, objectAux;

	// Call central system to parse to tokens
	bool bOK = gAIEnv.pTacticalPointSystem->Parse(sSpec, query, limits, object, objectAux);
	if (!bOK)
	{
		AIWarningID("<TacticalPointSystem> ", "Invalid criterion \"%s\"", (sSpec ? sSpec : "NULL"));
		return CCriterion();
	}

	// Build as a criterion
	CCriterion criterion;
	criterion.SetQuery(query);
	criterion.SetLimits(limits);
	criterion.SetObject(object);
	criterion.SetObjectAux(objectAux);

	return criterion;
}

//------ CTacticalPointQuery ----------------------------------------------------------------------//

CTacticalPointQuery::CTacticalPointQuery(const char* psName) : m_sName(psName)
{
	// Check there are no existing queries with that name
	assert(0 == gAIEnv.pTacticalPointSystem->GetQueryID(psName));
}

CTacticalPointQuery::~CTacticalPointQuery()
{
}

void CTacticalPointQuery::AddOption(const COptionCriteria& option)
{
	m_vOptions.push_back(option);
}

bool CTacticalPointQuery::IsValid() const
{
	vector<COptionCriteria>::const_iterator itO;
	for (itO = m_vOptions.begin(); itO != m_vOptions.end(); ++itO)
		if (!itO->IsValid()) return false;
	return true;
}

const COptionCriteria* CTacticalPointQuery::GetOption(int i) const
{
	if (i < 0 || i >= (int)m_vOptions.size())
		return NULL;
	return &(m_vOptions[i]);
}

COptionCriteria* CTacticalPointQuery::GetOption(int i)
{
	if (i < 0 || i >= (int)m_vOptions.size())
		return NULL;
	return &(m_vOptions[i]);
}
