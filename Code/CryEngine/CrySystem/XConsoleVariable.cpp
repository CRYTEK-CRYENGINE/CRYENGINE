// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "XConsole.h"
#include "XConsoleVariable.h"

#include <CrySystem/IConsole.h>
#include <CrySystem/ISystem.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CXConsoleVariableBase::CXConsoleVariableBase(CXConsole* pConsole, const char* sName, int nFlags, const char* help)
{
	assert(pConsole);

	m_psHelp = (char*)help;
	m_pChangeFunc = NULL;

	m_pConsole = pConsole;

	m_nFlags = nFlags;

	if (nFlags & VF_COPYNAME)
	{
		m_szName = new char[strlen(sName) + 1];
		strcpy(m_szName, sName);
	}
	else m_szName = (char*)sName;

#if defined(DEDICATED_SERVER)
	m_pDataProbeString = NULL;
#endif
}

//////////////////////////////////////////////////////////////////////////
CXConsoleVariableBase::~CXConsoleVariableBase()
{
	if (m_nFlags & VF_COPYNAME)
		delete[] m_szName;

#if defined(DEDICATED_SERVER)
	if (m_pDataProbeString)
	{
		delete[] m_pDataProbeString;
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
void CXConsoleVariableBase::ForceSet(const char* s)
{
	int excludeFlags = (VF_CHEAT | VF_READONLY | VF_NET_SYNCED);
	int oldFlags = (m_nFlags & excludeFlags);
	m_nFlags &= ~(excludeFlags);
	Set(s);
	m_nFlags |= oldFlags;
}

//////////////////////////////////////////////////////////////////////////
void CXConsoleVariableBase::ClearFlags(int flags)
{
	m_nFlags &= ~flags;
}

//////////////////////////////////////////////////////////////////////////
int CXConsoleVariableBase::GetFlags() const
{
	return m_nFlags;
}

//////////////////////////////////////////////////////////////////////////
int CXConsoleVariableBase::SetFlags(int flags)
{
	m_nFlags = flags;
	return m_nFlags;
}

//////////////////////////////////////////////////////////////////////////
const char* CXConsoleVariableBase::GetName() const
{
	return m_szName;
}

//////////////////////////////////////////////////////////////////////////
const char* CXConsoleVariableBase::GetHelp()
{
	return m_psHelp;
}

void CXConsoleVariableBase::Release()
{
	m_pConsole->UnregisterVariable(m_szName);
}

void CXConsoleVariableBase::SetOnChangeCallback(ConsoleVarFunc pChangeFunc)
{
	m_pChangeFunc = pChangeFunc;
}

uint64 CXConsoleVariableBase::AddOnChangeFunctor(const SFunctor& pChangeFunctor)
{
	m_cpChangeFunctors.push_back(pChangeFunctor);

	return m_cpChangeFunctors.size() - 1;
}

uint64 CXConsoleVariableBase::GetNumberOfOnChangeFunctors() const
{
	return m_cpChangeFunctors.size();
}

const SFunctor& CXConsoleVariableBase::GetOnChangeFunctor(uint64 nFunctorIndex) const
{
	if (nFunctorIndex < m_cpChangeFunctors.size())
	{
		return m_cpChangeFunctors[(uint)nFunctorIndex];
	}

	static SFunctor sDummyFunctor;
	assert(false && "[CXConsoleVariableBase::GetOnChangeFunctor] Trying to get a functor past the end.");

	return sDummyFunctor;
}

bool CXConsoleVariableBase::RemoveOnChangeFunctor(const uint64 nElement)
{
	if (nElement < m_cpChangeFunctors.size())
	{
		m_cpChangeFunctors.erase(m_cpChangeFunctors.begin() + (size_t)nElement);
		return true;
	}
	return false;
}

ConsoleVarFunc CXConsoleVariableBase::GetOnChangeCallback() const
{
	return m_pChangeFunc;
}

void CXConsoleVariableBase::CallOnChangeFunctions()
{
	if (m_pChangeFunc)
	{
		m_pChangeFunc(this);
	}

	size_t nTotal(m_cpChangeFunctors.size());
	for (size_t nCount(0); nCount < nTotal; ++nCount)
	{
		m_cpChangeFunctors[nCount].Call();
	}
}

void CXConsoleVariableCVarGroup::OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup)
{
	assert(szGroup);
	assert(szKey);
	assert(szValue);

	bool bCheckIfInDefault = false;

	SCVarGroup* pGrp = 0;

	if (stricmp(szGroup, "default") == 0)       // needs to be before the other groups
	{
		pGrp = &m_CVarGroupDefault;

		//		if(stricmp(GetName(),szKey)==0)
		if (*szKey == 0)
		{
			m_sDefaultValue = szValue;
			int iGrpValue = atoi(szValue);

			// if default state is not part of the mentioned states generate this state, so GetIRealVal() can return this state as well
			if (m_CVarGroupStates.find(iGrpValue) == m_CVarGroupStates.end())
				m_CVarGroupStates[iGrpValue] = new SCVarGroup;

			return;
		}
	}
	else
	{
		int iGrp;

		if (sscanf(szGroup, "%d", &iGrp) == 1)
		{
			if (m_CVarGroupStates.find(iGrp) == m_CVarGroupStates.end())
				m_CVarGroupStates[iGrp] = new SCVarGroup;

			pGrp = m_CVarGroupStates[iGrp];
			bCheckIfInDefault = true;
		}
		else
		{
			gEnv->pLog->LogError("[CVARS]: [MISSING] [%s] is not a registered console variable group", szGroup);
#if LOG_CVAR_INFRACTIONS_CALLSTACK
			gEnv->pSystem->debug_LogCallStack();
#endif // LOG_CVAR_INFRACTIONS_CALLSTACK
			return;
		}

		if (*szKey == 0)
		{
			assert(0);      // =%d only expected in default section
			return;
		}
	}

	if (pGrp)
	{
		if (pGrp->m_KeyValuePair.find(szKey) != pGrp->m_KeyValuePair.end())
		{
			gEnv->pLog->LogError("[CVARS]: [DUPLICATE] [%s] specified multiple times in console variable group [%s] = [%s]", szKey, GetName(), szGroup);
		}

		pGrp->m_KeyValuePair[szKey] = szValue;

		if (bCheckIfInDefault)
			if (m_CVarGroupDefault.m_KeyValuePair.find(szKey) == m_CVarGroupDefault.m_KeyValuePair.end())
			{
				gEnv->pLog->LogError("[CVARS]: [MISSING] [%s] specified in console variable group [%s] = [%s], but missing from default group", szKey, GetName(), szGroup);
			}
	}
}

void CXConsoleVariableCVarGroup::OnLoadConfigurationEntry_End()
{
	if (!m_sDefaultValue.empty())
	{
		gEnv->pConsole->LoadConfigVar(GetName(), m_sDefaultValue);
		m_sDefaultValue.clear();
	}
}

CXConsoleVariableCVarGroup::CXConsoleVariableCVarGroup(CXConsole* pConsole, const char* sName, const char* szFileName, int nFlags)
	: CXConsoleVariableInt(pConsole, sName, 0, nFlags, 0)
{
	gEnv->pSystem->LoadConfiguration(szFileName, this, eLoadConfigSystemSpec);
}

string CXConsoleVariableCVarGroup::GetDetailedInfo() const
{
	string sRet = GetName();

	sRet += " [";

	{
		TCVarGroupStateMap::const_iterator it, end = m_CVarGroupStates.end();

		for (it = m_CVarGroupStates.begin(); it != end; ++it)
		{
			if (it != m_CVarGroupStates.begin())
				sRet += "/";

			char szNum[10];

			cry_sprintf(szNum, "%d", it->first);

			sRet += szNum;
		}
	}

	sRet += "/default] [current]:\n";

	std::map<string, string>::const_iterator it, end = m_CVarGroupDefault.m_KeyValuePair.end();

	for (it = m_CVarGroupDefault.m_KeyValuePair.begin(); it != end; ++it)
	{
		const string& rKey = it->first;

		sRet += " ... ";
		sRet += rKey;
		sRet += " = ";

		TCVarGroupStateMap::const_iterator it2, end2 = m_CVarGroupStates.end();

		for (it2 = m_CVarGroupStates.begin(); it2 != end2; ++it2)
		{
			SCVarGroup* pGrp = it2->second;

			sRet += GetValueSpec(rKey, &(it2->first));
			sRet += "/";
		}
		sRet += GetValueSpec(rKey);
		ICVar* pCVar = gEnv->pConsole->GetCVar(rKey);
		if (pCVar)
		{
			sRet += " [";
			sRet += pCVar->GetString();
			sRet += "]";
		}

		sRet += "\n";
	}

	return sRet;
}

const char* CXConsoleVariableCVarGroup::GetHelp()
{
	if (m_psHelp)
	{
		delete m_psHelp;
		m_psHelp = NULL;
	}

	// create help on demand
	string sRet = "Console variable group to apply settings to multiple variables\n\n";

	sRet += GetDetailedInfo();

	m_psHelp = new char[sRet.size() + 1];
	strcpy(m_psHelp, &sRet[0]);

	return m_psHelp;
}

void CXConsoleVariableCVarGroup::DebugLog(const int iExpectedValue, const ICVar::EConsoleLogMode mode) const
{
	TCVarGroupStateMap::const_iterator it, end = m_CVarGroupStates.end();

	SCVarGroup* pCurrentGrp = 0;
	{
		TCVarGroupStateMap::const_iterator itCurrentGrp = m_CVarGroupStates.find(iExpectedValue);

		if (itCurrentGrp != end)
			pCurrentGrp = itCurrentGrp->second;
	}

	// try the current state
	if (TestCVars(pCurrentGrp, mode))
		return;
}

int CXConsoleVariableCVarGroup::GetRealIVal() const
{
	TCVarGroupStateMap::const_iterator it, end = m_CVarGroupStates.end();

	int iValue = GetIVal();

	SCVarGroup* pCurrentGrp = 0;
	{
		TCVarGroupStateMap::const_iterator itCurrentGrp = m_CVarGroupStates.find(iValue);

		if (itCurrentGrp != end)
			pCurrentGrp = itCurrentGrp->second;
	}

	// first try the current state
	if (TestCVars(pCurrentGrp))
	{
		return iValue;
	}

	// then all other
	for (it = m_CVarGroupStates.begin(); it != end; ++it)
	{
		SCVarGroup* pLocalGrp = it->second;

		if (pLocalGrp == pCurrentGrp)
			continue;

		int iLocalState = it->first;

		if (TestCVars(pLocalGrp))
			return iLocalState;
	}

	return -1;    // no state found that represent the current one
}

void CXConsoleVariableCVarGroup::Set(const int i)
{
	if (i == m_iValue)
	{
		SCVarGroup* pCurrentGrp = 0;
		TCVarGroupStateMap::const_iterator itCurrentGrp = m_CVarGroupStates.find(m_iValue);

		if (itCurrentGrp != m_CVarGroupStates.end())
		{
			pCurrentGrp = itCurrentGrp->second;
		}

		if (TestCVars(pCurrentGrp))
		{
			// All cvars in this group match the current state - no further action is necessary
			return;
		}
	}

	char sTemp[128];
	cry_sprintf(sTemp, "%d", i);

	bool wasProcessingGroup = m_pConsole->GetIsProcessingGroup();
	m_pConsole->SetProcessingGroup(true);
	if (m_pConsole->OnBeforeVarChange(this, sTemp))
	{
		m_nFlags |= VF_MODIFIED;
		m_iValue = i;

		CallOnChangeFunctions();
		m_pConsole->OnAfterVarChange(this);
	}
	m_pConsole->SetProcessingGroup(wasProcessingGroup);

	// Useful for debugging cvar groups
	//CryLogAlways("[CVARS]: CXConsoleVariableCVarGroup::Set() Group %s in state %d (wanted %d)", GetName(), m_iValue, i);
}

CXConsoleVariableCVarGroup::~CXConsoleVariableCVarGroup()
{
	TCVarGroupStateMap::iterator it, end = m_CVarGroupStates.end();

	for (it = m_CVarGroupStates.begin(); it == end; ++it)
	{
		SCVarGroup* pGrp = it->second;

		delete pGrp;
	}

	delete m_psHelp;
}

void CXConsoleVariableCVarGroup::OnCVarChangeFunc(ICVar* pVar)
{
	CXConsoleVariableCVarGroup* pThis = (CXConsoleVariableCVarGroup*)pVar;

	int iValue = pThis->GetIVal();

	// all sys_spec_* should be clamped by the max available spec
	if (strnicmp(pThis->GetName(), "sys_spec", 8) == 0)
	{
		int iMaxSpec = gEnv->pSystem->GetMaxConfigSpec();

		if (iValue > iMaxSpec)
		{
			iValue = iMaxSpec;
			pThis->m_iValue = iValue;
		}
	}

	TCVarGroupStateMap::const_iterator itGrp = pThis->m_CVarGroupStates.find(iValue);

	SCVarGroup* pGrp = 0;

	if (itGrp != pThis->m_CVarGroupStates.end())
		pGrp = itGrp->second;

	if (pGrp)
		pThis->ApplyCVars(*pGrp);

	pThis->ApplyCVars(pThis->m_CVarGroupDefault, pGrp);
}

bool CXConsoleVariableCVarGroup::TestCVars(const SCVarGroup* pGroup, const ICVar::EConsoleLogMode mode) const
{
	if (pGroup)
		if (!_TestCVars(*pGroup, mode))
			return false;

	if (!_TestCVars(m_CVarGroupDefault, mode, pGroup))
		return false;

	return true;
}

bool CXConsoleVariableCVarGroup::_TestCVars(const SCVarGroup& rGroup, const ICVar::EConsoleLogMode mode, const SCVarGroup* pExclude) const
{
	bool bRet = true;
	std::map<string, string>::const_iterator it, end = rGroup.m_KeyValuePair.end();

	for (it = rGroup.m_KeyValuePair.begin(); it != end; ++it)
	{
		const string& rKey = it->first;
		const string& rValue = it->second;

		if (pExclude)
			if (pExclude->m_KeyValuePair.find(rKey) != pExclude->m_KeyValuePair.end())
				continue;

		ICVar* pVar = gEnv->pConsole->GetCVar(rKey.c_str());

		if (pVar)
		{
			if (pVar->GetFlags() & VF_CVARGRP_IGNOREINREALVAL) // Ignore the cvars which change often and shouldn't be used to determine state
			{
				continue;
			}

			bool bOk = true;

			// compare exact type,
			// simple string comparison would fail on some comparisons e.g. 2.0 == 2
			// and GetString() for int and float return pointer to shared array so this
			// can cause problems
			switch (pVar->GetType())
			{
			case CVAR_INT:
				{
					int iVal;
					if (sscanf(rValue.c_str(), "%d", &iVal) == 1)
						if (pVar->GetIVal() != atoi(rValue.c_str()))
						{ bOk = false; break; }

					if (pVar->GetIVal() != pVar->GetRealIVal())
					{ bOk = false; break; }
				}
				break;
			case CVAR_FLOAT:
				{
					float fVal;
					if (sscanf(rValue.c_str(), "%f", &fVal) == 1)
						if (pVar->GetFVal() != fVal)
						{ bOk = false; break; }
				}
				break;
			case CVAR_STRING:
				if (rValue != pVar->GetString())
				{ bOk = false; break; }
				break;
			default:
				assert(0);
			}

			if (!bOk)
			{
				if (mode == ICVar::eCLM_Off)
					return false;   // exit as early as possible

				bRet = false;       // exit with same return code but log all differences

				if (strcmp(pVar->GetString(), rValue.c_str()) != 0)
				{
					switch (mode)
					{
					case ICVar::eCLM_ConsoleAndFile:
						CryLog("[CVARS]: $3[FAIL] [%s] = $6[%s] $4(expected [%s] in group [%s] = [%s])", rKey.c_str(), pVar->GetString(), rValue.c_str(), GetName(), GetString());
						break;

					case ICVar::eCLM_FileOnly:
					case ICVar::eCLM_FullInfo:
						gEnv->pLog->LogToFile("[CVARS]: [FAIL] [%s] = [%s] (expected [%s] in group [%s] = [%s])", rKey.c_str(), pVar->GetString(), rValue.c_str(), GetName(), GetString());
						break;

					default:
						assert(0);
					}
				}
				else if (mode == ICVar::eCLM_FullInfo)
				{
					gEnv->pLog->LogToFile("[CVARS]: [FAIL] [%s] = [%s] (expected [%s] in group [%s] = [%s])", rKey.c_str(), pVar->GetString(), rValue.c_str(), GetName(), GetString());
				}

				pVar->DebugLog(pVar->GetIVal(), mode);   // recursion
			}

			if (pVar->GetFlags() & (VF_CHEAT | VF_CHEAT_ALWAYS_CHECK | VF_CHEAT_NOCHECK))
			{
				// either VF_CHEAT should be removed or the var should be not part of the CVarGroup
				gEnv->pLog->LogError("[CVARS]: [%s] is cheat protected; referenced in console variable group [%s] = [%s] ", rKey.c_str(), GetName(), GetString());
			}
		}
		else
		{
			if (gEnv->pSystem)
			{
				// Do not warn about D3D registered cvars, which carry the prefix "q_", as they are not actually registered with the cvar system.
				if (strcmp(rKey.c_str(), "q") == -1)
					gEnv->pLog->LogError("[CVARS]: [MISSING] [%s] is not a registered console variable; referenced when testing console variable group [%s] = [%s]", rKey.c_str(), GetName(), GetString());
			}
		}
	}

	return bRet;
}

string CXConsoleVariableCVarGroup::GetValueSpec(const string& sKey, const int* pSpec) const
{
	if (pSpec)
	{
		TCVarGroupStateMap::const_iterator itGrp = m_CVarGroupStates.find(*pSpec);

		if (itGrp != m_CVarGroupStates.end())
		{
			const SCVarGroup* pGrp = itGrp->second;

			// check in spec
			std::map<string, string>::const_iterator it = pGrp->m_KeyValuePair.find(sKey);

			if (it != pGrp->m_KeyValuePair.end())
				return it->second;
		}
	}

	// check in default
	std::map<string, string>::const_iterator it = m_CVarGroupDefault.m_KeyValuePair.find(sKey);

	if (it != m_CVarGroupDefault.m_KeyValuePair.end())
		return it->second;

	assert(0);    // internal error
	return "";
}

void CXConsoleVariableCVarGroup::ApplyCVars(const SCVarGroup& rGroup, const SCVarGroup* pExclude)
{
	std::map<string, string>::const_iterator it, end = rGroup.m_KeyValuePair.end();

	bool wasProcessingGroup = m_pConsole->GetIsProcessingGroup();
	m_pConsole->SetProcessingGroup(true);

	for (it = rGroup.m_KeyValuePair.begin(); it != end; ++it)
	{
		const string& rKey = it->first;

		if (pExclude)
			if (pExclude->m_KeyValuePair.find(rKey) != pExclude->m_KeyValuePair.end())
				continue;

		// Useful for debugging cvar groups
		//CryLogAlways("[CVARS]: [APPLY] ([%s]) [%s] = [%s]", GetName(), rKey.c_str(), it->second.c_str());

		m_pConsole->LoadConfigVar(rKey, it->second);
	}

	m_pConsole->SetProcessingGroup(wasProcessingGroup);
}
