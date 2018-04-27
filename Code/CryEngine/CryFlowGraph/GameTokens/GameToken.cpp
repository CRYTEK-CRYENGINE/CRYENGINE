// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GameToken.cpp
//  Version:     v1.00
//  Created:     20/10/2005 by Timur.
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "GameToken.h"
#include "GameTokenSystem.h"
#include "FlowSystem/FlowSerialize.h"

//////////////////////////////////////////////////////////////////////////
CGameTokenSystem* CGameToken::g_pGameTokenSystem = 0;

//////////////////////////////////////////////////////////////////////////
CGameToken::CGameToken()
	: m_changed(0.0f)
{
	m_nFlags = 0;
}

//////////////////////////////////////////////////////////////////////////
CGameToken::~CGameToken()
{
	Notify(EGAMETOKEN_EVENT_DELETE);
}

//////////////////////////////////////////////////////////////////////////
void CGameToken::Notify(EGameTokenEvent event)
{
	// Notify all listeners about game token event.
	for (Listeneres::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
	{
		(*it)->OnGameTokenEvent(event, this);
	}
}

void CGameToken::TriggerAsChanged(bool bIsGameStart)
{
	m_changed = gEnv->pTimer->GetFrameStartTime();
	g_pGameTokenSystem->Notify(EGAMETOKEN_EVENT_CHANGE, this);
}

//////////////////////////////////////////////////////////////////////////
void CGameToken::SetName(const char* sName)
{
	m_name = sName;
}

//////////////////////////////////////////////////////////////////////////
void CGameToken::SetValue(const TFlowInputData& val)
{
	if (val != m_value)
	{
		m_value.SetValueWithConversion(val);

		#if (!defined(_RELEASE) && !defined(PERFORMANCE_BUILD))
		// make extra checks on forced data type conversion when not on release
		string valueToSetStr;
		val.GetValueWithConversion(valueToSetStr); // get string representation of the value to set in the token
		if (m_value.CheckIfForcedConversionOfCurrentValueWithString(valueToSetStr))
		{
			CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_ERROR,
				"GameToken Internal SetValue: Forced conversion of GameToken '%s' of type '%s' with value >%s<",
				m_name.c_str(),
				FlowTypeToName(m_value.GetType()),
				valueToSetStr.c_str()
			);
		}
		#endif

		TriggerAsChanged(false);
	}
}

void CGameToken::SetValueFromString(const char* valueStr)
{
	TFlowInputData valueFD;
	GetValue(valueFD); // get variant with correct type and lock

	if (valueFD.GetType() != eFDT_String && *valueStr == '\0')
	{
		CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_ERROR,
			"GameToken SetValue: Setting GameToken '%s' with an empty value.", m_name.c_str());
	}

	// bool convertWithSuccess = // ideally ret value of SetValueWithConversion could be used, but it accepts some conversions that we want to give a warning for
	valueFD.SetValueWithConversion(string(valueStr)); // set and convert the value from the string

	#if (!defined(_RELEASE) && !defined(PERFORMANCE_BUILD))
	// make extra checks on forced data type conversion when not on release
	if (valueFD.CheckIfForcedConversionOfCurrentValueWithString(valueStr))
	{
		CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_ERROR,
			"GameToken SetValueFromString: Forced conversion of GameToken '%s' of type '%s' with value >%s<",
			m_name.c_str(),
			FlowTypeToName(m_value.GetType()),
			valueStr
		);
	}
	#endif

	SetValue(valueFD); // set token from FD with correct type via the proper entry point
}

//////////////////////////////////////////////////////////////////////////
bool CGameToken::GetValue(TFlowInputData& val) const
{
	val = m_value;
	return true;
}

//////////////////////////////////////////////////////////////////////////
const char* CGameToken::GetValueAsString() const
{
	static string temp;
	m_value.GetValueWithConversion(temp);
	return temp.c_str();
}

void CGameToken::GetMemoryStatistics(ICrySizer* s)
{
	s->AddObject(this, sizeof(*this));
	s->AddObject(m_name);
	s->AddObject(m_value);
	s->AddObject(m_listeners);
}
