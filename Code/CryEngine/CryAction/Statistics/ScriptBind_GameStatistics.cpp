// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ScriptBind_GameStatistics.h"
#include "GameStatistics.h"
#include "CryActionCVars.h"

static const char* GLOBAL_NAME = "GameStatistics";
static const size_t MAX_SCRIPT_MATCH_FEATURES = 20;

void SaveLuaStatToXML(XmlNodeRef& node, const ScriptAnyValue& value);
void SaveLuaToXML(XmlNodeRef node, const char* name, const ScriptAnyValue& value);

CScriptBind_GameStatistics::CScriptBind_GameStatistics(CGameStatistics* pGS) : m_pGS(pGS), m_pSS(gEnv->pScriptSystem)
{
	Init(m_pSS, gEnv->pSystem, 1);
	RegisterMethods();
	RegisterGlobals();

	SmartScriptTable t(m_pSS);
	t->SetValue("__this", ScriptHandle(m_pGS));
	t->Delegate(GetMethodsTable());
	m_pSS->SetGlobalValue(GLOBAL_NAME, t);
}

CScriptBind_GameStatistics::~CScriptBind_GameStatistics()
{
	m_pSS->SetGlobalToNull(GLOBAL_NAME);
}

void CScriptBind_GameStatistics::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_GameStatistics::
	SCRIPT_REG_TEMPLFUNC(PushGameScope, "scopeID");
	SCRIPT_REG_TEMPLFUNC(PopGameScope, "");
	SCRIPT_REG_TEMPLFUNC(AddGameElement, "");
	SCRIPT_REG_TEMPLFUNC(CurrentScope, "");
	SCRIPT_REG_TEMPLFUNC(RemoveGameElement, "");
	SCRIPT_REG_TEMPLFUNC(Event, "event, params");
	SCRIPT_REG_TEMPLFUNC(StateValue, "state, value");
#undef SCRIPT_REG_CLASSNAME
}

void CScriptBind_GameStatistics::RegisterGlobals()
{
	size_t numEventTypes = m_pGS->GetEventCount();
	for (size_t i = 0; i != numEventTypes; ++i)
		gEnv->pScriptSystem->SetGlobalValue(m_pGS->GetEventDesc(i)->scriptName, static_cast<int>(i));

	size_t numStateTypes = m_pGS->GetStateCount();
	for (size_t i = 0; i != numStateTypes; ++i)
		gEnv->pScriptSystem->SetGlobalValue(m_pGS->GetStateDesc(i)->scriptName, static_cast<int>(i));

	size_t numScopeTypes = m_pGS->GetScopeCount();
	for (size_t i = 0; i != numScopeTypes; ++i)
		gEnv->pScriptSystem->SetGlobalValue(m_pGS->GetScopeDesc(i)->scriptName, static_cast<int>(i));

	size_t numElemTypes = m_pGS->GetElementCount();
	for (size_t i = 0; i != numElemTypes; ++i)
	{
		const SGameElementDesc* desc = m_pGS->GetElementDesc(i);
		gEnv->pScriptSystem->SetGlobalValue(desc->scriptName, static_cast<int>(i));
		gEnv->pScriptSystem->SetGlobalValue(desc->locatorName, static_cast<int>(desc->locatorID));
	}
}

IGameStatistics* CScriptBind_GameStatistics::GetStats(IFunctionHandler* pH)
{
	if (void* pThis = pH->GetThis())
		return static_cast<IGameStatistics*>(pThis);
	return 0;
}

bool CScriptBind_GameStatistics::BindTracker(IScriptTable* pT, const char* name, IStatsTracker* tracker)
{
#if STATISTICS_CHECK_BINDING
	m_boundTrackers.insert(tracker);
#endif
	SmartScriptTable t(m_pSS);
	t->SetValue("__this", ScriptHandle(tracker));
	t->Delegate(GetMethodsTable());
	pT->SetValue(name, t);
	return true;
}

bool CScriptBind_GameStatistics::UnbindTracker(IScriptTable* pT, const char* name, IStatsTracker* tracker)
{
#if STATISTICS_CHECK_BINDING
	stl::member_find_and_erase(m_boundTrackers, tracker);
#endif
	if (pT)
	{
		ScriptHandle h;
		SmartScriptTable tr;
		if (pT->GetValue(name, tr) && tr->GetValue("__this", h) && tracker == h.ptr)
		{
			pT->SetToNull(name);
			return true;
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "[Telemetry] Error unbinding tracker");
			return false;
		}
	}
	return false;
}

int CScriptBind_GameStatistics::PushGameScope(IFunctionHandler* pH, int scopeID)
{
	if (GetStats(pH) == m_pGS)
	{
		m_pGS->PushGameScope(static_cast<size_t>(scopeID));
	}
	else
	{
		m_pSS->RaiseError("[Telemetry] Error: PushGameScope() method is supported only by global %s object", GLOBAL_NAME);
	}

	return pH->EndFunction();
}

int CScriptBind_GameStatistics::PopGameScope(IFunctionHandler* pH)
{
	if (GetStats(pH) != m_pGS)
	{
		m_pSS->RaiseError("[Telemetry] Error: PopGameScope() method is supported only by global %s object", GLOBAL_NAME);
	}
	else if (pH->GetParamCount() == 0)
	{
		m_pGS->PopGameScope(INVALID_STAT_ID);
	}
	else if (pH->GetParamCount() == 1)
	{
		int scopeID;
		pH->GetParam(1, scopeID);
		m_pGS->PopGameScope(static_cast<size_t>(scopeID));
	}
	else
	{
		m_pSS->RaiseError("[Telemetry] Error: PopGameScope() expects 0 or 1 params.");
	}

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////

int CScriptBind_GameStatistics::CurrentScope(IFunctionHandler* pH)
{
	int scope = m_pGS->GetScopeStackSize() != 0 ? (int)m_pGS->GetScopeID() : -1;
	return pH->EndFunction(scope);
}

//////////////////////////////////////////////////////////////////////////

// Syntax: GameStatistics.AddGameElement( scopeID, elementID, locatorID, locatorValue, table )
int CScriptBind_GameStatistics::AddGameElement(IFunctionHandler* pH)
{
	if (GetStats(pH) != m_pGS)
	{
		m_pSS->RaiseError("[Telemetry] Error: AddGameElement() method is supported only by global %s object", GLOBAL_NAME);
	}
	else if (pH->GetParamCount() != 4 && pH->GetParamCount() != 5)
	{
		m_pSS->RaiseError("[Telemetry] Error: AddGameElement() expected 4 or 5 params.");
	}
	else
	{
		if (pH->GetParamType(1) != svtNumber)
		{
			m_pSS->RaiseError("[Telemetry] AddGameElement() Param #1 expected Number.");
		}
		else if (pH->GetParamType(2) != svtNumber)
		{
			m_pSS->RaiseError("[Telemetry] AddGameElement() Param #2 expected Number.");
		}
		else if (pH->GetParamType(3) != svtNumber)
		{
			m_pSS->RaiseError("[Telemetry] AddGameElement() Param #3 expected Number.");
		}
		else if (pH->GetParamType(4) != svtNumber && pH->GetParamType(4) != svtPointer)
		{
			m_pSS->RaiseError("[Telemetry] AddGameElement() Param #4 expected Number or EntityID.");
		}
		else
		{
			uint32 scope, elem, locID;
			ScriptAnyValue anydata;
			pH->GetParam(1, scope);
			pH->GetParam(2, elem);
			pH->GetParam(3, locID);

			ScriptAnyValue locVal;
			pH->GetParamAny(4, locVal);
			uint32 lv = locVal.GetType() == EScriptAnyType::Number ? static_cast<uint32>(locVal.GetNumber()) : static_cast<uint32>(locVal.GetScriptHandle().n);

			if (pH->GetParamCount() == 5)
				pH->GetParamAny(5, anydata);

			if (pH->GetParamCount() == 4 || anydata.GetType() == EScriptAnyType::Nil)
			{
				m_pGS->AddGameElement(SNodeLocator(elem, scope, locID, lv), 0);
			}
			else if (anydata.GetType() == EScriptAnyType::Table)
			{
				IStatsTracker* tracker = m_pGS->AddGameElement(SNodeLocator(elem, scope, locID, lv), anydata.GetScriptTable());
			}
			else
			{
				m_pSS->RaiseError("[Telemetry] AddGameElement() Param #5 expected Table or Nil.");
			}
		}
	}

	return pH->EndFunction();
}

// Syntax: GameStatistics.RemoveGameElement( scopeID, elementID, locatorID, locatorValue )
int CScriptBind_GameStatistics::RemoveGameElement(IFunctionHandler* pH)
{
	if (GetStats(pH) != m_pGS)
	{
		m_pSS->RaiseError("[Telemetry] Error: RemoveGameElement() method is supported only by global %s object", GLOBAL_NAME);
	}
	else if (pH->GetParamCount() != 4 && pH->GetParamCount() != 5)
	{
		m_pSS->RaiseError("[Telemetry] Error: RemoveGameElement() expected 4 params.");
	}
	else
	{
		if (pH->GetParamType(1) != svtNumber)
		{
			m_pSS->RaiseError("[Telemetry] RemoveGameElement() Param #1 expected Number.");
		}
		else if (pH->GetParamType(2) != svtNumber)
		{
			m_pSS->RaiseError("[Telemetry] RemoveGameElement() Param #2 expected Number.");
		}
		else if (pH->GetParamType(3) != svtNumber)
		{
			m_pSS->RaiseError("[Telemetry] RemoveGameElement() Param #3 expected Number.");
		}
		else if (pH->GetParamType(4) != svtNumber && pH->GetParamType(4) != svtPointer)
		{
			m_pSS->RaiseError("[Telemetry] RemoveGameElement() Param #4 expected Number or EntityID.");
		}
		else
		{
			uint32 scope, elem, locID;
			pH->GetParam(1, scope);
			pH->GetParam(2, elem);
			pH->GetParam(3, locID);

			ScriptAnyValue locVal;
			pH->GetParamAny(4, locVal);
			uint32 lv = locVal.GetType() == EScriptAnyType::Number ? static_cast<uint32>(locVal.GetNumber()) : static_cast<uint32>(locVal.GetScriptHandle().n);

			m_pGS->RemoveElement(SNodeLocator(elem, scope, locID, lv));
		}
	}

	return pH->EndFunction();
}

IStatsTracker* CScriptBind_GameStatistics::GetTracker(IFunctionHandler* pH)
{
	if (void* pThis = pH->GetThis())
		return static_cast<IStatsTracker*>(pThis);

	return 0;
}

int CScriptBind_GameStatistics::Event(IFunctionHandler* pH)
{
#if STATS_MODE_CVAR
	if (CCryActionCVars::Get().g_statisticsMode != 2)
		return pH->EndFunction();
#endif

	IStatsTracker* tracker = GetTracker(pH);
	if (!tracker)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "[Telemetry] Event() tracker is nil");
	}
#if STATISTICS_CHECK_BINDING
	else if (m_boundTrackers.find(tracker) == m_boundTrackers.end())
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "[Telemetry] Event() tracker is not bound");
	}
#endif
	else if (pH->GetParamCount() != 2)
	{
		m_pSS->RaiseError("[Telemetry] Error: Event() expected 2 params.");
	}
	else
	{
		if (pH->GetParamType(1) != svtNumber && pH->GetParamType(1) != svtString)
		{
			m_pSS->RaiseError("[Telemetry] Event() Param #1 expected String or Number.");
		}
		else if (pH->GetParamType(2) == svtNull)
		{
			m_pSS->RaiseError("[Telemetry] Event() Param #2 is Null");
		}
		else if (pH->GetParamType(1) == svtNumber)
		{
			int id;
			pH->GetParam(1, id);
			ScriptAnyValue p;
			pH->GetParamAny(2, p);

			if ((p.GetType() == EScriptAnyType::Table || p.GetType() == EScriptAnyType::Vector))
			{
				XmlNodeRef node = m_pGS->CreateStatXMLNode();
				SaveLuaStatToXML(node, p);
				if (XmlNodeRef ch = (node->getChildCount() > 0) ? node->getChild(0) : XmlNodeRef(0))
				{
					SStatAnyValue statVal(m_pGS->WrapXMLNode(ch));
					m_pGS->PreprocessScriptedEventParameter(id, statVal);
					tracker->Event(id, statVal);
				}
			}
			else
			{
				SStatAnyValue statVal(p);
				m_pGS->PreprocessScriptedEventParameter(id, statVal);
				tracker->Event(id, statVal);
			}
		}
		else if (pH->GetParamType(1) == svtString)
		{
			const char* name = 0;
			pH->GetParam(1, name);
			ScriptAnyValue p;
			pH->GetParamAny(2, p);

			size_t ev = m_pGS->GetEventID(name);
			if (ev == static_cast<size_t>(-1))
			{
				ev = m_pGS->RegisterGameEvent(name, name);
				if (ev == static_cast<size_t>(-1))
					return pH->EndFunction();
			}

			if ((p.GetType() == EScriptAnyType::Table || p.GetType() == EScriptAnyType::Vector))
			{
				XmlNodeRef node = m_pGS->CreateStatXMLNode();
				SaveLuaStatToXML(node, p);
				if (XmlNodeRef ch = (node->getChildCount() > 0) ? node->getChild(0) : XmlNodeRef(0))
				{
					SStatAnyValue statVal(m_pGS->WrapXMLNode(ch));
					m_pGS->PreprocessScriptedEventParameter(ev, statVal);
					tracker->Event(ev, statVal);
				}
			}
			else
			{
				SStatAnyValue statVal(p);
				m_pGS->PreprocessScriptedEventParameter(ev, statVal);
				tracker->Event(ev, statVal);
			}
		}
	}
	return pH->EndFunction();
}

int CScriptBind_GameStatistics::StateValue(IFunctionHandler* pH)
{
#if STATS_MODE_CVAR
	if (CCryActionCVars::Get().g_statisticsMode != 2)
		return pH->EndFunction();
#endif
	IStatsTracker* tracker = GetTracker(pH);
	if (!tracker)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "[Telemetry] StateValue() tracker is nil");
	}
#if STATISTICS_CHECK_BINDING
	else if (m_boundTrackers.find(tracker) == m_boundTrackers.end())
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "[Telemetry] StateValue() tracker is not bound");
	}
#endif
	else if (pH->GetParamCount() != 2)
	{
		m_pSS->RaiseError("[Telemetry] Error: StateValue() expected 2 params.");
	}
	else
	{
		if (pH->GetParamType(1) != svtNumber && pH->GetParamType(1) != svtString)
		{
			m_pSS->RaiseError("[Telemetry] StateValue() Param #1 expected String or Number.");
		}
		else if (pH->GetParamType(2) == svtNull)
		{
			m_pSS->RaiseError("[Telemetry] StateValue() Param #2 is Null");
		}
		else if (pH->GetParamType(1) == svtNumber)
		{
			int id;
			pH->GetParam(1, id);
			ScriptAnyValue p;
			pH->GetParamAny(2, p);

			if ((p.GetType() == EScriptAnyType::Table || p.GetType() == EScriptAnyType::Vector))
			{
				XmlNodeRef node = m_pGS->CreateStatXMLNode();
				SaveLuaStatToXML(node, p);
				if (XmlNodeRef ch = (node->getChildCount() > 0) ? node->getChild(0) : XmlNodeRef(0))
				{
					SStatAnyValue statVal(m_pGS->WrapXMLNode(ch));
					m_pGS->PreprocessScriptedStateParameter(id, statVal);
					tracker->StateValue(id, statVal);
				}
			}
			else
			{
				SStatAnyValue statVal(p);
				m_pGS->PreprocessScriptedStateParameter(id, statVal);
				tracker->StateValue(id, statVal);
			}
		}
		else if (pH->GetParamType(1) == svtString)
		{
			const char* name = 0;
			pH->GetParam(1, name);
			ScriptAnyValue p;
			pH->GetParamAny(2, p);

			size_t st = m_pGS->GetStateID(name);
			if (st == static_cast<size_t>(-1))
			{
				st = m_pGS->RegisterGameState(name, name);
				if (st == static_cast<size_t>(-1))
					return pH->EndFunction();
			}

			if ((p.GetType() == EScriptAnyType::Table || p.GetType() == EScriptAnyType::Vector))
			{
				XmlNodeRef node = m_pGS->CreateStatXMLNode();
				SaveLuaStatToXML(node, p);
				if (XmlNodeRef ch = (node->getChildCount() > 0) ? node->getChild(0) : XmlNodeRef(0))
				{
					SStatAnyValue statVal(m_pGS->WrapXMLNode(ch));
					m_pGS->PreprocessScriptedStateParameter(st, statVal);
					tracker->StateValue(st, m_pGS->WrapXMLNode(ch));
				}
			}
			else
			{
				SStatAnyValue statVal(p);
				m_pGS->PreprocessScriptedStateParameter(st, statVal);
				tracker->StateValue(st, statVal);
			}
		}
	}
	return pH->EndFunction();
}

void SaveLuaToXML(XmlNodeRef node, const char* name, const SmartScriptTable& table)
{
	XmlNodeRef c = gEnv->pSystem->CreateXmlNode(name);
	IScriptTable::Iterator it = table->BeginIteration();
	bool attributes = false;
	while (table->MoveNext(it))
	{
		if (it.sKey)
		{
			SaveLuaToXML(c, it.sKey, it.value);
			attributes = true;
		}
		else
		{
			//XmlNodeRef sub = gEnv->pSystem->CreateXmlNode(name);
			SmartScriptTable t;
			it.value.CopyTo(t);
			SaveLuaToXML(node, name, it.value);
			//node->addChild(sub);
		}
	}
	table->EndIteration(it);
	if (attributes)
		node->addChild(c);
}

void SaveVec3ToXML(XmlNodeRef node, const Vec3& a)
{
	node->setAttr("x", a.x);
	node->setAttr("y", a.y);
	node->setAttr("z", a.z);
}

void SaveLuaToXML(XmlNodeRef node, const char* name, const ScriptAnyValue& value)
{
	switch (value.GetType())
	{
	case EScriptAnyType::Boolean:
		node->setAttr(name, value.GetBool());
		break;
	case EScriptAnyType::Number:
		node->setAttr(name, value.GetNumber());
		break;
	case EScriptAnyType::String:
		node->setAttr(name, value.GetString());
		break;
	case EScriptAnyType::Handle:
		node->setAttr(name, (UINT_PTR)(value.GetUserData().ptr)); //[SergiyY] : ud.nRef contains trash atm; FIXME
		break;
	case EScriptAnyType::Vector:
		{
			XmlNodeRef c = gEnv->pSystem->CreateXmlNode(name);
			SaveVec3ToXML(c, value.GetVector());
			node->addChild(c);
		}
		break;
	case EScriptAnyType::Table:
		{
			SmartScriptTable t;
			value.CopyTo(t);
			SaveLuaToXML(node, name, t);
		}
		break;
	}
}

void SaveLuaStatToXML(XmlNodeRef& node, const ScriptAnyValue& value)
{
	if (!node)
		return;

	SaveLuaToXML(node, "param", value);
}
