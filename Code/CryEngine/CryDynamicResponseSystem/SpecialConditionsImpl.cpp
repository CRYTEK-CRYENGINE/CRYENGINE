// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SpecialConditionsImpl.h"

#include <CryDynamicResponseSystem/IDynamicResponseCondition.h>
#include <CryMath/Random.h>
#include <CryString/StringUtils.h>
#include <CryGame/IGameFramework.h>
#include <CryMemory/CrySizer.h>
#include <CrySerialization/Decorators/Range.h>
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>

#include "VariableCollection.h"
#include "ResponseSystem.h"
#include "Response.h"
#include "ResponseManager.h"
#include "ResponseInstance.h"

using namespace CryDRS;

//--------------------------------------------------------------------------------------------------
bool CRandomCondition::IsMet(DRS::IResponseInstance* pResponseInstance)
{
	return cry_random(0, 100) < m_randomFactor;
}

//--------------------------------------------------------------------------------------------------
void CRandomCondition::Serialize(Serialization::IArchive& ar)
{
	ar(Serialization::Range(m_randomFactor, 0, 100), "RandomFactor", "^ Probability");

#if !defined(_RELEASE)
	if (ar.isEdit())
	{
		if (m_randomFactor <= 0)
		{
			ar.warning(m_randomFactor, "Will never be true.");
		}
		else if (m_randomFactor >= 100)
		{
			ar.warning(m_randomFactor, "Will always be true.");
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CGameTokenCondition::~CGameTokenCondition()
{
	if (m_pCachedToken)
	{
		IGameTokenSystem* pGTSys = gEnv->pGameFramework->GetIGameTokenSystem();
		if (pGTSys)
		{
			pGTSys->UnregisterListener(m_pCachedToken->GetName(), this);
		}
	}
}

//--------------------------------------------------------------------------------------------------
void CGameTokenCondition::OnGameTokenEvent(EGameTokenEvent event, IGameToken* pGameToken)
{
	if (event == EGAMETOKEN_EVENT_DELETE)
	{
		m_pCachedToken = nullptr;
	}
}

//--------------------------------------------------------------------------------------------------
void CGameTokenCondition::GetMemoryUsage(class ICrySizer* pSizer) const
{
	pSizer->Add(*this);
}

//--------------------------------------------------------------------------------------------------
void CGameTokenCondition::Serialize(Serialization::IArchive& ar)
{
	ar(m_tokenName, "Token", "Token");

	m_pCachedToken = nullptr;
	if (gEnv->pGameFramework)
	{
		if (auto* pGameTokenSystem = gEnv->pGameFramework->GetIGameTokenSystem())
		{
			m_pCachedToken = pGameTokenSystem->FindToken(m_tokenName.c_str());
		}
	}

	if (ar.isEdit() &&
	    ((m_pCachedToken && m_pCachedToken->GetType() == eFDT_Bool) || (ar.isOutput() && (m_minValue.GetType() == eDRVT_Boolean || m_minValue.GetType() == eDRVT_String)))) //for booleans/strings range-tests would not make much sense
	{
		ar(m_minValue, "MinVal", " EQUALS ");
		m_maxValue = m_minValue;
	}
	else
	{
		ar(m_minValue, "MinVal", "GREATER than or equal to");
		if (m_minValue.GetType() == eDRVT_Boolean || m_minValue.GetType() == eDRVT_String || (m_pCachedToken && m_pCachedToken->GetType() == eFDT_Bool))
		{
			m_maxValue = m_minValue;
			if (ar.isOutput())
			{
				ar(m_maxValue, "MaxVal", "and LESS than or equal to");
			}
		}
		else
		{
			ar(m_maxValue, "MaxVal", "and LESS than or equal to");
		}
	}

#if !defined(_RELEASE)
	if (ar.isEdit() && ar.isOutput())
	{
		ar(GetVerboseInfo(), "ConditionDesc", "!^<");

		if (m_tokenName.empty())
		{
			ar.warning(m_tokenName, "You need to specify a GameToken Name");
		}
		else if (!m_tokenName.empty() && (m_tokenName.front() == ' ' || m_tokenName.back() == ' '))
		{
			ar.warning(m_tokenName, "GameToken name starts or ends with a space. Check if this is really wanted.");
		}
		else if (!m_minValue.DoTypesMatch(m_maxValue))
		{
			ar.warning(m_tokenName, "The type of min and max value do not match!");
		}
		else if (m_maxValue == CVariableValue::POS_INFINITE && m_minValue == CVariableValue::NEG_INFINITE)
		{
			ar.warning(m_tokenName, "This Condition will always be true!");
		}
		if (m_pCachedToken) //check if the type of the compare-to-values and the game-token type match
		{
			switch (m_pCachedToken->GetType())
			{
			case eFDT_Int:
				{
					if (m_minValue.GetType() != eDRVT_Int && m_minValue.GetType() != eDRVT_NegInfinite)
					{
						ar.warning(m_tokenName, "Gametoken has type Int and the compare-to-value has a different type.");
					}
					break;
				}
			case eFDT_Float:
				{
					if (m_minValue.GetType() != eDRVT_Float && m_minValue.GetType() != eDRVT_NegInfinite)
					{
						ar.warning(m_tokenName, "Gametoken has type Float and the compare-to-value has a different type.");
					}
					break;
				}
			case eFDT_String:
				{
					if (m_minValue.GetType() != eDRVT_String)
					{
						ar.warning(m_tokenName, "Gametoken has type String and the compare-to-value has a different type.");
					}
					break;
				}
			case eFDT_Bool:
				{
					if (m_minValue.GetType() != eDRVT_Boolean)
					{
						ar.warning(m_tokenName, "Gametoken has type Bool and the compare-to-value has a different type.");
					}
					break;
				}
			default:
				{
					ar.warning(m_tokenName, "Only Int/Float/String and Bool GameTokens can be used in DRS-Conditions");
				}
			}
		}
	}
#endif
}

//--------------------------------------------------------------------------------------------------
bool CGameTokenCondition::IsMet(DRS::IResponseInstance* pResponseInstance)
{
	if (!m_pCachedToken)
	{
		IGameTokenSystem* pGTSys = gEnv->pGameFramework->GetIGameTokenSystem();

		m_pCachedToken = pGTSys->FindToken(m_tokenName.c_str());
		if (!m_pCachedToken)
		{
			CryWarning(VALIDATOR_MODULE_DRS, VALIDATOR_ERROR, "DRS: Could not find GameToken for condition-check, token-name: '%s'", m_tokenName.c_str());
			return false;
		}
		else
		{
			pGTSys->RegisterListener(m_pCachedToken->GetName(), this);
		}
	}

	CVariableValue currentTokenValue = CVariableValue::NEG_INFINITE;

	switch (m_pCachedToken->GetType())
	{
	case eFDT_Int:
		{
			int value;
			if (m_pCachedToken->GetValueAs(value))
			{
				currentTokenValue = value;
			}
			break;
		}
	case eFDT_Float:
		{
			float value;
			if (m_pCachedToken->GetValueAs(value))
			{
				currentTokenValue = value;
			}
			break;
		}
	case eFDT_String:
		{
			currentTokenValue = CHashedString(m_pCachedToken->GetValueAsString());
			break;
		}
	case eFDT_Bool:
		{
			bool bValue;
			if (m_pCachedToken->GetValueAs(bValue))
			{
				currentTokenValue = bValue;
			}
			break;
		}
	default:
		{
			CryWarning(VALIDATOR_MODULE_DRS, VALIDATOR_ERROR, "Only Int/Float/String and Bool GameTokens can be used in DRS-Conditions. TokenName: %s", m_tokenName.c_str());
			return false;
		}
	}

	return currentTokenValue <= m_maxValue && currentTokenValue >= m_minValue;
}

//////////////////////////////////////////////////////////////////////////
string CGameTokenCondition::GetVerboseInfo() const
{
	string tokenInfo;
	if (m_pCachedToken)
		tokenInfo.Format("found, value: '%s'", m_pCachedToken->GetValueAsString());
	else
		tokenInfo = "not found in current level";

	if (m_minValue == m_maxValue)
	{
		return string().Format("Token '%s' EQUALS '%s' (token %s)", m_tokenName.c_str(), m_minValue.GetValueAsString().c_str(), tokenInfo.c_str());
	}
	else
	{
		return string().Format("Token '%s' is BETWEEN '%s' AND '%s' (token %s)", m_tokenName.c_str(), m_minValue.GetValueAsString().c_str(), m_maxValue.GetValueAsString().c_str(), tokenInfo.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CPlaceholderCondition::Serialize(Serialization::IArchive& ar)
{
	ar(m_Label, "Label", "^ Text:");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct SDepthCounterHelper
{
	SDepthCounterHelper() { ++s_depthCounter; }
	~SDepthCounterHelper() { --s_depthCounter; }

	static int s_depthCounter;
};
int SDepthCounterHelper::s_depthCounter = 0;

bool CInheritConditionsCondition::IsMet(DRS::IResponseInstance* pResponseInstance)
{
	SDepthCounterHelper temp;
	if (temp.s_depthCounter >= 3)
	{
		DrsLogWarning("Max depth of 'inherited conditions' is 2. Example: Response1 inherites from Response2 which inherites from Response3, would be the maximum. Also make sure not to inherit from yourself.");
		return false;
	}

	if (!m_pCachedResponse)
	{
		m_pCachedResponse = CResponseSystem::GetInstance()->GetResponseManager()->GetResponse(m_responseToReuse);
	}
	if (m_pCachedResponse)
	{
		return m_pCachedResponse->GetBaseSegment().AreConditionsMet(static_cast<CResponseInstance*>(pResponseInstance));
	}

	return true;
}

//--------------------------------------------------------------------------------------------------
void CInheritConditionsCondition::Serialize(Serialization::IArchive& ar)
{
	ar(m_responseToReuse, "ResponseToCopyConditionsFrom", "^ Response to inherit from");
	m_pCachedResponse = nullptr; // we cannot cache the response in most cases here, because the response to cache needs to be serialized before hand, and we cannot guarantee that.

#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
	if (ar.isEdit())
	{
		if (!m_responseToReuse.IsValid())
		{
			ar.warning(m_responseToReuse.m_textCopy, "No Response to inherit conditions from specified");
		}
		else if (!CResponseSystem::GetInstance()->GetResponseManager()->HasMappingForSignal(m_responseToReuse))
		{
			ar.warning(m_responseToReuse.m_textCopy, "No response specified from which the conditions should be inherited");
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CTimeSinceCondition::IsMet(DRS::IResponseInstance* pResponseInstance)
{
	CVariable* pVariable = GetCurrentVariable(static_cast<CResponseInstance*>(pResponseInstance));
	if (pVariable && pVariable->m_value.GetType() != eDRVT_Undefined)
	{
		const float timeSince = CResponseSystem::GetInstance()->GetCurrentDrsTime() - pVariable->GetValueAsFloat();
		return timeSince >= m_minTime && ((timeSince <= m_maxTime) || m_maxTime < 0.0f);
	}
	else
	{
		return true;  //if the timer-variable is not even existing, we assume it never happened, and therefore the time since then is infinite
	}
}

//--------------------------------------------------------------------------------------------------
void CTimeSinceCondition::Serialize(Serialization::IArchive& ar)
{
	IVariableUsingBase::_Serialize(ar, "^TimerVariable");
	ar(m_minTime, "minTime", "^MinElapsedTime");
	ar(m_maxTime, "maxTime", "^MaxElapsedTime");

#if defined (ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	CVariableCollection* pCollection = CResponseSystem::GetInstance()->GetCollection(m_collectionName);
	if (pCollection)
	{
		if (pCollection->GetVariableValue(m_variableName).GetType() != eDRVT_Float && pCollection->GetVariableValue(m_variableName).GetType() != eDRVT_Undefined)
		{
			ar.warning(m_collectionName.m_textCopy, "Variable to check needs to hold a time value as float");
		}
	}
#endif
}

//--------------------------------------------------------------------------------------------------
string CTimeSinceCondition::GetVerboseInfo() const
{
	if (IVariableUsingBase::s_bDoDisplayCurrentValueInDebugOutput)
	{
		return GetVariableVerboseName() + string(" more than ") + CryStringUtils::toString(m_minTime) + " seconds away from current time: "
		       + CryStringUtils::toString(CResponseSystem::GetInstance()->GetCurrentDrsTime()) + " and less then " + CryStringUtils::toString(m_maxTime);
	}
	else
	{
		return GetVariableVerboseName() + string(" more than ") + CryStringUtils::toString(m_minTime) + " seconds and less then " + CryStringUtils::toString(m_maxTime);
	}
}

//--------------------------------------------------------------------------------------------------
bool CExecutionLimitCondition::IsMet(DRS::IResponseInstance* pResponseInstance)
{
	CResponse* pResponse = nullptr;
	if (!m_ResponseToTest.IsValid())
	{
		pResponse = static_cast<CResponseInstance*>(pResponseInstance)->GetResponse();
	}
	else
	{
		ResponsePtr pOverrideResponse = CResponseSystem::GetInstance()->GetResponseManager()->GetResponse(m_ResponseToTest);
		if (pOverrideResponse)
		{
			pResponse = pOverrideResponse.get();
		}
	}
	if (pResponse)
	{
		const uint32 executionCounter = pResponse->GetExecutionCounter();
		return executionCounter >= m_minExecutions && ((executionCounter <= m_maxExecutions) || m_maxExecutions < 0);
	}
	else
	{
		return false;
	}
}

//--------------------------------------------------------------------------------------------------
void CExecutionLimitCondition::Serialize(Serialization::IArchive& ar)
{
	ar(m_minExecutions, "minExecutions", "^ MinExecutions");
	ar(m_maxExecutions, "maxExecutions", "^MaxExecutions");
	ar(m_ResponseToTest, "response", "^Response");

#if !defined(_RELEASE)
	if (ar.isEdit())
	{
		if (m_minExecutions > m_maxExecutions)
		{
			ar.warning(m_minExecutions, "Will never be true. Because Min is larger than Max");
		}
		else if (m_maxExecutions <= 0)
		{
			ar.warning(m_maxExecutions, "Will never be true. Because the maximum execution count is set to 0");
		}
	}
#endif
}

//--------------------------------------------------------------------------------------------------
string CExecutionLimitCondition::GetVerboseInfo() const
{
	return string("min: ") + CryStringUtils::toString(m_minExecutions) + string(" max: ") + CryStringUtils::toString(m_maxExecutions);
}

//--------------------------------------------------------------------------------------------------
bool CTimeSinceResponseCondition::IsMet(DRS::IResponseInstance* pResponseInstance)
{
	CResponse* pResponse = nullptr;
	if (!m_responseId.IsValid())
	{
		pResponse = static_cast<CResponseInstance*>(pResponseInstance)->GetResponse();
	}
	else
	{
		ResponsePtr pOverrideResponse = CResponseSystem::GetInstance()->GetResponseManager()->GetResponse(m_responseId);
		if (pOverrideResponse)
		{
			pResponse = pOverrideResponse.get();
		}
	}
	if (pResponse)
	{
		const float lastStartOrEndTime = std::max(pResponse->GetLastEndTime(), pResponse->GetLastStartTime());  //if the response was started recently, but has not ended yet, then the LastStartTime is actually higher than the LastFinishedTime.
		const float timeSince = (lastStartOrEndTime > 0) ? CResponseSystem::GetInstance()->GetCurrentDrsTime() - lastStartOrEndTime : std::numeric_limits<float>::max();
		return timeSince >= m_minTime && ((timeSince <= m_maxTime) || m_maxTime < 0.0f);
	}
	else
	{
		return false;
	}
}

//--------------------------------------------------------------------------------------------------
void CTimeSinceResponseCondition::Serialize(Serialization::IArchive& ar)
{
	ar(m_responseId, "response", "^ Response");
	ar(m_minTime, "minTime", "^MinElpasedTime");
	ar(m_maxTime, "maxTime", "^MaxElpasedTime");
}

//--------------------------------------------------------------------------------------------------
string CTimeSinceResponseCondition::GetVerboseInfo() const
{
	string responseName = (m_responseId.IsValid()) ? m_responseId.GetText() : "CurrentResponse";
	return "Execution of '" + responseName + string("' more than ") + CryStringUtils::toString(m_minTime) + " seconds and less then " + CryStringUtils::toString(m_maxTime);
}
