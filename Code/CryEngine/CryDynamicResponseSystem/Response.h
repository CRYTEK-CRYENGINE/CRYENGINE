// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   The dynamic Response itself is responsible for managing its Response segments
   and checking base conditions

   /************************************************************************/

#pragma once

#include "ResponseSegment.h"

namespace CryDRS
{
class CVariableValue;
class CVariableCollection;
struct SSignal;

class CConditionParserHelper
{
public:
	static void GetResponseVariableValueFromString(const char* szValueString, CVariableValue* pOutValue);
};

class CResponse
{
public:
	friend class CDataImportHelper;

	CResponse() : m_executionCounter(0), m_lastStartTime(0), m_lastEndTime(0) {}

	CResponseInstance* StartExecution(SSignal& signal);   //the execution itself will/might take some time, therefore we return a ResponseInstance object, which needs to be updated, until the execution has finished.
	CResponseSegment& GetBaseSegment()                      { return m_baseSegment; }
	void              Reset()                               { m_executionCounter = 0; m_lastStartTime.SetSeconds(0); m_lastEndTime.SetSeconds(0); }
	uint32            GetExecutionCounter()                 { return m_executionCounter; }
	void              SetExecutionCounter(uint32 newAmount) { m_executionCounter = newAmount; }
	const CTimeValue& GetLastStartTime()                    { return m_lastStartTime; }
	void              SetLastStartTime(const CTimeValue& newValue) { m_lastStartTime = newValue; }
	const CTimeValue& GetLastEndTime()										{ return m_lastEndTime; }
	void              SetLastEndTime(const CTimeValue& newValue)   { m_lastEndTime = newValue; }

	void              Serialize(Serialization::IArchive& ar);

private:
	CTimeValue       m_lastStartTime;
	CTimeValue       m_lastEndTime;
	uint32           m_executionCounter;
	CResponseSegment m_baseSegment;
};
}
