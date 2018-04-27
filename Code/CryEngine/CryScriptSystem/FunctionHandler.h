// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(AFX_FUNCTIONHANDLER_H__CB02D9A1_DFAA_4DA3_8DF7_E2E8769F4ECE__INCLUDED_)
#define AFX_FUNCTIONHANDLER_H__CB02D9A1_DFAA_4DA3_8DF7_E2E8769F4ECE__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include "ScriptSystem.h"

/*! IFunctionHandler implementation
   @see IFunctionHandler
 */
class CFunctionHandler : public IFunctionHandler
{
public:
	CFunctionHandler(CScriptSystem* pSS, lua_State* lState, const char* sFuncName, int paramIdOffset)
	{
		m_pSS = pSS;
		L = lState;
		m_sFuncName = sFuncName;
		m_paramIdOffset = paramIdOffset;
	}
	~CFunctionHandler() {}

public:
	int                 GetParamCount();
	ScriptVarType       GetParamType(int nIdx);
	IScriptSystem*      GetIScriptSystem();

	virtual void*       GetThis();
	virtual bool        GetSelfAny(ScriptAnyValue& any);

	virtual const char* GetFuncName();

	//////////////////////////////////////////////////////////////////////////
	virtual bool GetParamAny(int nIdx, ScriptAnyValue& any);
	virtual int  EndFunctionAny(const ScriptAnyValue& any);
	virtual int  EndFunctionAny(const ScriptAnyValue& any1, const ScriptAnyValue& any2);
	virtual int  EndFunctionAny(const ScriptAnyValue& any1, const ScriptAnyValue& any2, const ScriptAnyValue& any3);
	virtual int  EndFunction();
	//////////////////////////////////////////////////////////////////////////

private:
	lua_State*     L;
	CScriptSystem* m_pSS;
	const char*    m_sFuncName;
	int            m_paramIdOffset;
};

#endif // !defined(AFX_FUNCTIONHANDLER_H__CB02D9A1_DFAA_4DA3_8DF7_E2E8769F4ECE__INCLUDED_)
