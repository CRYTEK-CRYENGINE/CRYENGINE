// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   Some additional base conditions

   /************************************************************************/

#pragma once

#include <CryDynamicResponseSystem/IDynamicResponseCondition.h>
#include <CryGame/IGameTokens.h>
#include <CryString/CryString.h>
#include "VariableValue.h"
#include "ConditionsCollection.h"
#include "ResponseManager.h"
#include "Variable.h"

struct IGameToken;

namespace CryDRS
{
class CPlaceholderCondition final : public DRS::IResponseCondition
{
public:
	CPlaceholderCondition() { m_Label = "Empty"; }

	//////////////////////////////////////////////////////////
	// IResponseCondition implementation
	virtual bool        IsMet(DRS::IResponseInstance* pResponseInstance) override { return true; }
	virtual void        Serialize(Serialization::IArchive& ar) override;
	virtual string      GetVerboseInfo() const override                           { return "\"" + m_Label + "\""; }
	virtual const char* GetType() const override                                  { return "Placeholder"; }
	//////////////////////////////////////////////////////////

private:
	string m_Label;
};

//////////////////////////////////////////////////////////////////////////
class CRandomCondition final : public DRS::IResponseCondition
{
public:
	CRandomCondition() : m_randomFactor(50) {}
	CRandomCondition(int factor) : m_randomFactor(factor) {}

	//////////////////////////////////////////////////////////
	// IResponseCondition implementation
	virtual bool        IsMet(DRS::IResponseInstance* pResponseInstance) override;
	virtual void        Serialize(Serialization::IArchive& ar) override;
	virtual string      GetVerboseInfo() const override { return string("Chance: ") + CryStringUtils::toString(m_randomFactor); }
	virtual const char* GetType() const override        { return "Random"; }
	//////////////////////////////////////////////////////////

private:
	int m_randomFactor;   //0 - 100
};

//////////////////////////////////////////////////////////////////////////
class CExecutionLimitCondition final : public DRS::IResponseCondition
{
public:
	CExecutionLimitCondition() : m_minExecutions(1), m_maxExecutions(1) {}
	CExecutionLimitCondition(int minExecutions, int maxExecutions) : m_minExecutions(minExecutions), m_maxExecutions(maxExecutions) {}

	//////////////////////////////////////////////////////////
	// IResponseCondition implementation
	virtual bool        IsMet(DRS::IResponseInstance* pResponseInstance) override;
	virtual void        Serialize(Serialization::IArchive& ar) override;
	virtual string      GetVerboseInfo() const override;
	virtual const char* GetType() const override { return "Execution Limit"; }
	//////////////////////////////////////////////////////////

private:
	CHashedString m_ResponseToTest;
	int           m_minExecutions;
	int           m_maxExecutions;
};

//////////////////////////////////////////////////////////////////////////
class CInheritConditionsCondition final : public DRS::IResponseCondition
{
public:
	CInheritConditionsCondition() {}
	CInheritConditionsCondition(const CHashedString& signalToReuse) : m_responseToReuse(signalToReuse) {}

	//////////////////////////////////////////////////////////
	// IResponseCondition implementation
	virtual bool        IsMet(DRS::IResponseInstance* pResponseInstance) override;
	virtual void        Serialize(Serialization::IArchive& ar) override;
	virtual string      GetVerboseInfo() const override { return string("From ") + m_responseToReuse.GetText(); }
	virtual const char* GetType() const override        { return "Inherit Conditions"; }
	//////////////////////////////////////////////////////////

private:
	CHashedString m_responseToReuse;
	ResponsePtr   m_pCachedResponse;
};

//////////////////////////////////////////////////////////////////////////
// Checks if the time (stored in variable) is in the specified range
class CTimeSinceCondition final : public IVariableUsingBase, public DRS::IResponseCondition
{
public:
	CTimeSinceCondition() { m_minTime = 5.0f; m_maxTime = -1.0f; }
	//////////////////////////////////////////////////////////
	// IResponseCondition implementation
	virtual bool        IsMet(DRS::IResponseInstance* pResponseInstance) override;
	virtual void        Serialize(Serialization::IArchive& ar) override;
	virtual string      GetVerboseInfo() const override;
	virtual const char* GetType() const override { return "TimeSince"; }
	//////////////////////////////////////////////////////////

protected:
	float m_minTime;
	float m_maxTime;
};

//////////////////////////////////////////////////////////////////////////
// Checks if the time since execution of the specified response is in the specified range
class CTimeSinceResponseCondition final : public DRS::IResponseCondition
{
public:
	CTimeSinceResponseCondition() { m_minTime = 5.0f; m_maxTime = -1.0f; }

	//////////////////////////////////////////////////////////
	// IResponseCondition implementation
	virtual bool        IsMet(DRS::IResponseInstance* pResponseInstance) override;
	virtual void        Serialize(Serialization::IArchive& ar) override;
	virtual string      GetVerboseInfo() const override;
	virtual const char* GetType() const override { return "TimeSinceResponse"; }
	//////////////////////////////////////////////////////////

protected:
	CHashedString m_responseId;
	float         m_minTime;
	float         m_maxTime;
};

//////////////////////////////////////////////////////////////////////////
class CGameTokenCondition final : public DRS::IResponseCondition, public IGameTokenEventListener
{
public:
	CGameTokenCondition() : m_pCachedToken(nullptr), m_minValue(CVariableValue::NEG_INFINITE), m_maxValue(CVariableValue::POS_INFINITE) {}
	CGameTokenCondition(const string& tokenName, const CVariableValue& minValue, const CVariableValue& maxValue)
		: m_tokenName(tokenName), m_pCachedToken(nullptr), m_minValue(minValue), m_maxValue(maxValue) {}
	virtual ~CGameTokenCondition() override;

	//////////////////////////////////////////////////////////
	// IResponseCondition implementation
	virtual bool        IsMet(DRS::IResponseInstance* pResponseInstance) override;
	virtual void        Serialize(Serialization::IArchive& ar) override;
	virtual string      GetVerboseInfo() const override;
	virtual const char* GetType() const override { return "GameTokenCheck"; }
	//////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////
	// IGameTokenEventListener implementation
	virtual void OnGameTokenEvent(EGameTokenEvent event, IGameToken* pGameToken) override;
	virtual void GetMemoryUsage(class ICrySizer* pSizer) const override;
	//////////////////////////////////////////////////////////

private:
	IGameToken*    m_pCachedToken;
	string         m_tokenName;

	CVariableValue m_minValue;
	CVariableValue m_maxValue;
};

}  //end namespace CryDRS
