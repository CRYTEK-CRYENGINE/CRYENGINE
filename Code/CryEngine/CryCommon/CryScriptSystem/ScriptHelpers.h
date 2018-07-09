// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CryString/StringUtils.h>  // cry_strcpy()
#include <CryCore/AlignmentTools.h>

// forward declarations
class CScriptBind_GameStatistics;

//! Helper template class.
//! Handles dispatching of Lua to C script function callbacks to the specified
//! member functions of the CScriptableBase.
// See Also: CScriptableBase::RegisterTemplateFunction
struct ScriptTemplateCallHelper
{
	template<typename Callee>
	static int Call(Callee* callee, int (Callee::* func)(IFunctionHandler*), IFunctionHandler* pH)
	{
		return (callee->*func)(pH);
	}
	template<typename Callee, typename P1>
	static int Call(Callee* callee, int (Callee::* func)(IFunctionHandler*, P1), IFunctionHandler* pH)
	{
		P1 p1;
		if (!pH->GetParams(p1))
			return pH->EndFunction();
		return (callee->*func)(pH, p1);
	}
	template<typename Callee, typename P1, typename P2>
	static int Call(Callee* callee, int (Callee::* func)(IFunctionHandler*, P1, P2), IFunctionHandler* pH)
	{
		P1 p1;
		P2 p2;
		if (!pH->GetParams(p1, p2))
			return pH->EndFunction();
		return (callee->*func)(pH, p1, p2);
	}
	template<typename Callee, typename P1, typename P2, typename P3>
	static int Call(Callee* callee, int (Callee::* func)(IFunctionHandler*, P1, P2, P3), IFunctionHandler* pH)
	{
		P1 p1;
		P2 p2;
		P3 p3;
		if (!pH->GetParams(p1, p2, p3))
			return pH->EndFunction();
		return (callee->*func)(pH, p1, p2, p3);
	}
	template<typename Callee, typename P1, typename P2, typename P3, typename P4>
	static int Call(Callee* callee, int (Callee::* func)(IFunctionHandler*, P1, P2, P3, P4), IFunctionHandler* pH)
	{
		P1 p1;
		P2 p2;
		P3 p3;
		P4 p4;
		if (!pH->GetParams(p1, p2, p3, p4))
			return pH->EndFunction();
		return (callee->*func)(pH, p1, p2, p3, p4);
	}
	template<typename Callee, typename P1, typename P2, typename P3, typename P4, typename P5>
	static int Call(Callee* callee, int (Callee::* func)(IFunctionHandler*, P1, P2, P3, P4, P5), IFunctionHandler* pH)
	{
		P1 p1;
		P2 p2;
		P3 p3;
		P4 p4;
		P5 p5;
		if (!pH->GetParams(p1, p2, p3, p4, p5))
			return pH->EndFunction();
		return (callee->*func)(pH, p1, p2, p3, p4, p5);
	}
	template<typename Callee, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
	static int Call(Callee* callee, int (Callee::* func)(IFunctionHandler*, P1, P2, P3, P4, P5, P6), IFunctionHandler* pH)
	{
		P1 p1;
		P2 p2;
		P3 p3;
		P4 p4;
		P5 p5;
		P6 p6;
		if (!pH->GetParams(p1, p2, p3, p4, p5, p6))
			return pH->EndFunction();
		return (callee->*func)(pH, p1, p2, p3, p4, p5, p6);
	}
	template<typename Callee, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
	static int Call(Callee* callee, int (Callee::* func)(IFunctionHandler*, P1, P2, P3, P4, P5, P6, P7), IFunctionHandler* pH)
	{
		P1 p1;
		P2 p2;
		P3 p3;
		P4 p4;
		P5 p5;
		P6 p6;
		P7 p7;
		if (!pH->GetParams(p1, p2, p3, p4, p5, p6, p7))
			return pH->EndFunction();
		return (callee->*func)(pH, p1, p2, p3, p4, p5, p6, p7);
	}
	template<typename Callee, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
	static int Call(Callee* callee, int (Callee::* func)(IFunctionHandler*, P1, P2, P3, P4, P5, P6, P7, P8), IFunctionHandler* pH)
	{
		P1 p1;
		P2 p2;
		P3 p3;
		P4 p4;
		P5 p5;
		P6 p6;
		P7 p7;
		P8 p8;
		if (!pH->GetParams(p1, p2, p3, p4, p5, p6, p7, p8))
			return pH->EndFunction();
		return (callee->*func)(pH, p1, p2, p3, p4, p5, p6, p7, p8);
	}
	template<typename Callee, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9>
	static int Call(Callee* callee, int (Callee::* func)(IFunctionHandler*, P1, P2, P3, P4, P5, P6, P7, P8, P9), IFunctionHandler* pH)
	{
		P1 p1;
		P2 p2;
		P3 p3;
		P4 p4;
		P5 p5;
		P6 p6;
		P7 p7;
		P8 p8;
		P9 p9;
		if (!pH->GetParams(p1, p2, p3, p4, p5, p6, p7, p8, p9))
			return pH->EndFunction();
		return (callee->*func)(pH, p1, p2, p3, p4, p5, p6, p7, p8, p9);
	}
	template<typename Callee, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10>
	static int Call(Callee* callee, int (Callee::* func)(IFunctionHandler*, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10), IFunctionHandler* pH)
	{
		P1 p1;
		P2 p2;
		P3 p3;
		P4 p4;
		P5 p5;
		P6 p6;
		P7 p7;
		P8 p8;
		P9 p9;
		P10 p10;
		if (!pH->GetParams(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10))
			return pH->EndFunction();
		return (callee->*func)(pH, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
	}

	template<typename Callee, typename Func>
	struct CallDispatcher
	{
		static int Dispatch(IFunctionHandler* pH, void* pBuffer, int nSize)
		{
			unsigned char* buffer = (unsigned char*)pBuffer;
			Callee* pCallee;
			Func func;
			LoadUnaligned(buffer, pCallee);
			LoadUnaligned(buffer + sizeof(Callee*), func);
			return ScriptTemplateCallHelper::Call(pCallee, func, pH);
		}
	};
};

//! Classes that want to register functions to the script must derive from this interface.
class CScriptableBase
{
public:
	friend class CScriptBind_GameStatistics;

	virtual ~CScriptableBase() { Done(); };
	virtual void Init(IScriptSystem* pSS, ISystem* pSystem, int nParamIdOffset = 0)
	{
		m_pSS = pSS;
		m_pMethodsTable = m_pSS->CreateTable();
		m_pMethodsTable->AddRef();
		m_nParamIdOffset = nParamIdOffset;
	}
	void Done()
	{
		if (*m_sGlobalName != 0 && m_pSS)
			m_pSS->SetGlobalToNull(m_sGlobalName);
		SAFE_RELEASE(m_pMethodsTable);
	}
	virtual void GetMemoryStatistics(ICrySizer* pSizer) {};

	void         SetGlobalName(const char* sGlobalName)
	{
		assert(strlen(sGlobalName) < sizeof(m_sGlobalName));
		cry_strcpy(m_sGlobalName, sGlobalName);
		if (m_pMethodsTable)
			m_pSS->SetGlobalValue(sGlobalName, m_pMethodsTable);
	}

	IScriptTable* GetMethodsTable() { return m_pMethodsTable; };

protected:
	CScriptableBase() { m_pSS = NULL; m_pMethodsTable = NULL; m_sGlobalName[0] = 0; }

	void RegisterFunction(const char* sFuncName, IScriptTable::FunctionFunctor function)
	{
		if (m_pMethodsTable)
		{
			IScriptTable::SUserFunctionDesc fd;
			fd.sGlobalName = m_sGlobalName;
			fd.sFunctionName = sFuncName;
			fd.pFunctor = function;
			fd.nParamIdOffset = m_nParamIdOffset;
			m_pMethodsTable->AddFunction(fd);
		}
	}
	template<typename Callee, typename Func>
	void RegisterTemplateFunction(const char* sFuncName, const char* sFuncParams, Callee& callee, const Func& func)
	{
		typedef Callee* Callee_pointer;
		if (m_pMethodsTable)
		{
			Callee_pointer pCalleePtr = &callee;
			unsigned char pBuffer[sizeof(Callee_pointer) + sizeof(func)];
			memcpy(pBuffer, &pCalleePtr, sizeof(Callee_pointer));
			memcpy(pBuffer + sizeof(Callee_pointer), &func, sizeof(func));

			IScriptTable::SUserFunctionDesc fd;
			fd.sGlobalName = m_sGlobalName;
			fd.sFunctionName = sFuncName;
			fd.sFunctionParams = sFuncParams;
			fd.pUserDataFunc = ScriptTemplateCallHelper::CallDispatcher<Callee, Func>::Dispatch;
			fd.nDataSize = sizeof(Callee_pointer) + sizeof(func);
			fd.pDataBuffer = pBuffer;
			fd.nParamIdOffset = m_nParamIdOffset;

			m_pMethodsTable->AddFunction(fd);
		}
	}

protected:
	//////////////////////////////////////////////////////////////////////////
	char           m_sGlobalName[64];
	IScriptTable*  m_pMethodsTable;
	IScriptSystem* m_pSS;
	int            m_nParamIdOffset;
	//////////////////////////////////////////////////////////////////////////
};

#define SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_FUNC(func)                   RegisterFunction( # func, functor_ret(*this, SCRIPT_REG_CLASSNAME func));
#define SCRIPT_REG_GLOBAL(var)                  gEnv->pScriptSystem->SetGlobalValue( # var, var);
#define SCRIPT_REG_TEMPLFUNC(func, sFuncParams) RegisterTemplateFunction( # func, sFuncParams, *this, SCRIPT_REG_CLASSNAME func);

#define SCRIPT_CHECK_PARAMETERS(_n)                                                                                                                  \
  if (pH->GetParamCount() != _n)                                                                                                                     \
  { CryWarning(VALIDATOR_MODULE_SCRIPTSYSTEM, VALIDATOR_WARNING, "[%s] %d arguments passed, " # _n " expected)", __FUNCTION__, pH->GetParamCount()); \
    return pH->EndFunction(); }

#define SCRIPT_CHECK_PARAMETERS_MIN(_n)                                                                                                                       \
  if (pH->GetParamCount() < _n)                                                                                                                               \
  { CryWarning(VALIDATOR_MODULE_SCRIPTSYSTEM, VALIDATOR_WARNING, "[%s] %d arguments passed, at least " # _n " expected)", __FUNCTION__, pH->GetParamCount()); \
    return pH->EndFunction(); }

#define SCRIPT_CHECK_PARAMETERS2(_n, _n2)                                                                                                                         \
  if ((pH->GetParamCount() != _n) && (pH->GetParamCount() != _n2))                                                                                                \
  { CryWarning(VALIDATOR_MODULE_SCRIPTSYSTEM, VALIDATOR_WARNING, "[%s] %d arguments passed, " # _n " or " # _n2 " expected)", __FUNCTION__, pH->GetParamCount()); \
    return pH->EndFunction(); }

#define SCRIPT_CHECK_PARAMETERS3(_n, _n2, _n3)                                                                                                                               \
  if ((pH->GetParamCount() != _n) && (pH->GetParamCount() != _n2) && (pH->GetParamCount() != _n3))                                                                           \
  { CryWarning(VALIDATOR_MODULE_SCRIPTSYSTEM, VALIDATOR_WARNING, "[%s] %d arguments passed, " # _n ", " # _n2 " or " # _n3 " expected)", __FUNCTION__, pH->GetParamCount()); \
    return pH->EndFunction(); }

//! Auto Wrapper on IScriptTable interface, will destroy table when exiting scope.
class SmartScriptTable
{
public:
	SmartScriptTable() : p(NULL) {};
	SmartScriptTable(const SmartScriptTable& st)
	{
		p = st.p;
		if (p) p->AddRef();
	}
	SmartScriptTable(IScriptTable* newp)
	{
		if (newp) newp->AddRef();
		p = newp;
	}

	//! Copy operator.
	SmartScriptTable& operator=(IScriptTable* newp)
	{
		if (newp) newp->AddRef();
		if (p) p->Release();
		p = newp;
		return *this;
	}
	//! Copy operator.
	SmartScriptTable& operator=(const SmartScriptTable& st)
	{
		if (st.p) st.p->AddRef();
		if (p) p->Release();
		p = st.p;
		return *this;
	}

	explicit SmartScriptTable(IScriptSystem* pSS, bool bCreateEmpty = false)
	{
		p = pSS->CreateTable(bCreateEmpty);
		p->AddRef();
	}
	~SmartScriptTable() { if (p) p->Release(); }

	// Casts
	IScriptTable* operator->() const { return p; }
	IScriptTable* operator*() const  { return p; }
	operator const IScriptTable*() const { return p; }
	operator IScriptTable*() const { return p; }
	operator bool() const { return (p != NULL); }

	// Boolean comparisons.
	bool operator!() const                        { return p == NULL; };
	bool operator==(const IScriptTable* p2) const { return p == p2; };
	bool operator==(IScriptTable* p2) const       { return p == p2; };
	bool operator!=(const IScriptTable* p2) const { return p != p2; };
	bool operator!=(IScriptTable* p2) const       { return p != p2; };
	bool operator<(const IScriptTable* p2) const  { return p < p2; };
	bool operator>(const IScriptTable* p2) const  { return p > p2; };

	//////////////////////////////////////////////////////////////////////////
	IScriptTable* GetPtr() const { return p; }
	//////////////////////////////////////////////////////////////////////////

	bool Create(IScriptSystem* pSS, bool bCreateEmpty = false)
	{
		SAFE_RELEASE(p);
		p = pSS->CreateTable(bCreateEmpty);
		if (p == NULL)
			return false;

		p->AddRef();
		return true;
	}

private:
	IScriptTable* p;
};

// Smart wrapper on top of script function handle.
class SmartScriptFunction
{
public:
	SmartScriptFunction()
		: m_func(0)
		, m_pSS(0)
	{
	};

	SmartScriptFunction(IScriptSystem* pSS, HSCRIPTFUNCTION func)
		: m_func(func)
		, m_pSS(pSS)
	{
	}

	SmartScriptFunction(const SmartScriptFunction& other)
		: m_func(0)
		, m_pSS(0)
	{
		if (other.m_func)
		{
			ScriptAnyValue func(
			  other.m_pSS->CloneAny(ScriptAnyValue((HSCRIPTFUNCTION)other.m_func)));
			func.CopyTo(m_func);
			m_pSS = other.m_pSS;
		}
	}

	~SmartScriptFunction()
	{
		if (m_func)
			m_pSS->ReleaseFunc(m_func);

		m_func = 0;
		m_pSS = 0;
	}

	SmartScriptFunction& operator=(const SmartScriptFunction& other)
	{
		SmartScriptFunction(other).swap(*this);

		return *this;
	}

	operator HSCRIPTFUNCTION() const
	{
		return m_func;
	}

	HSCRIPTFUNCTION get() const
	{
		return m_func;
	}

	void swap(SmartScriptFunction& other)
	{
		HSCRIPTFUNCTION otherFunc = other.m_func;
		IScriptSystem* otherSS = other.m_pSS;
		other.m_func = m_func;
		other.m_pSS = m_pSS;
		m_func = otherFunc;
		m_pSS = otherSS;
	}

	void reset()
	{
		SmartScriptFunction().swap(*this);
	}

	void reset(IScriptSystem* pSS, HSCRIPTFUNCTION func)
	{
		SmartScriptFunction(pSS, func).swap(*this);
	}

private:
	HSCRIPTFUNCTION m_func;
	IScriptSystem*  m_pSS;
};

// Description:
//		This class map an 3d vector to a LUA table with x,y,z members.
class CScriptVector : public SmartScriptTable
{
public:
	CScriptVector() {}
	CScriptVector(IScriptSystem* pSS, bool bCreateEmpty = false) : SmartScriptTable(pSS, bCreateEmpty) {}
	void Set(const Vec3& v)
	{
		CScriptSetGetChain chain(*this);
		chain.SetValue("x", v.x);
		chain.SetValue("y", v.y);
		chain.SetValue("z", v.z);
	}
	Vec3 Get()
	{
		Vec3 v(0, 0, 0);
		CScriptSetGetChain chain(*this);
		chain.GetValue("x", v.x);
		chain.GetValue("y", v.y);
		chain.GetValue("z", v.z);
		return v;
	}
	CScriptVector& operator=(const Vec3& v3) { Set(v3); return *this; }
};

//! This class map an "color" to a LUA table with indexed 3 numbers [1],[2],[3] members.
class CScriptColor : public SmartScriptTable
{
public:
	CScriptColor() {}
	CScriptColor(IScriptSystem* pSS, bool bCreateEmpty = false) : SmartScriptTable(pSS, bCreateEmpty) {}
	void Set(const Vec3& v)
	{
		IScriptTable* pTable = *this;
		pTable->SetAt(1, v.x);
		pTable->SetAt(2, v.y);
		pTable->SetAt(3, v.z);
	}
	Vec3 Get()
	{
		IScriptTable* pTable = *this;
		Vec3 v(0, 0, 0);
		pTable->GetAt(1, v.x);
		pTable->GetAt(2, v.y);
		pTable->GetAt(3, v.z);
		return v;
	}
	CScriptColor& operator=(const Vec3& v3) { Set(v3); return *this; }
};

//! Script call helper.
struct Script
{
	static SmartScriptTable GetCachedTable(IFunctionHandler* pH, int funcParam)
	{
		SmartScriptTable out;
		if (pH->GetParamCount() >= funcParam && pH->GetParamType(funcParam) == svtObject)
			pH->GetParam(funcParam, out);
		if (!out.GetPtr())
			out = SmartScriptTable(gEnv->pScriptSystem);
		return out;
	}

	static SmartScriptTable SetCachedVector(const Vec3& value, IFunctionHandler* pH, int funcParam)
	{
		SmartScriptTable out = GetCachedTable(pH, funcParam);
		{
			CScriptSetGetChain chain(out);
			chain.SetValue("x", value.x);
			chain.SetValue("y", value.y);
			chain.SetValue("z", value.z);
		}
		return out;
	}

	static SmartScriptTable GetCachedTable(SmartScriptTable& table, const char* fieldName)
	{
		SmartScriptTable out;

		if (table->GetValue(fieldName, out))
			return out;

		out = SmartScriptTable(gEnv->pScriptSystem);
		table->SetValue(fieldName, out);
		return out;
	}

	static SmartScriptTable SetCachedVector(const Vec3& value, SmartScriptTable& table, const char* fieldName)
	{
		SmartScriptTable out = GetCachedTable(table, fieldName);
		{
			CScriptSetGetChain chain(out);
			chain.SetValue("x", value.x);
			chain.SetValue("y", value.y);
			chain.SetValue("z", value.z);
		}
		return out;
	}

	static SmartScriptTable GetCachedTable(CScriptSetGetChain& chain, const char* fieldName)
	{
		SmartScriptTable out;

		if (chain.GetValue(fieldName, out))
			return out;

		out = SmartScriptTable(gEnv->pScriptSystem);
		chain.SetValue(fieldName, out);

		return out;
	}

	static SmartScriptTable SetCachedVector(const Vec3& value, CScriptSetGetChain& chain, const char* fieldName)
	{
		SmartScriptTable out = GetCachedTable(chain, fieldName);
		{
			CScriptSetGetChain vecChain(out);
			vecChain.SetValue("x", value.x);
			vecChain.SetValue("y", value.y);
			vecChain.SetValue("z", value.z);
		}
		return out;
	}

	//////////////////////////////////////////////////////////////////////////
	static bool Call(IScriptSystem* pSS, HSCRIPTFUNCTION func)
	{
		if (!pSS->BeginCall(func)) return false;
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	static bool Call(IScriptSystem* pSS, const char* funcName)
	{
		if (!pSS->BeginCall(funcName)) return false;
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template<class P1>
	static bool Call(IScriptSystem* pSS, HSCRIPTFUNCTION func, const P1& p1)
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS, p1);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template<class P1, class P2>
	static bool Call(IScriptSystem* pSS, HSCRIPTFUNCTION func, const P1& p1, const P2& p2)
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS, p1, p2);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template<class P1, class P2, class P3>
	static bool Call(IScriptSystem* pSS, HSCRIPTFUNCTION func, const P1& p1, const P2& p2, const P3& p3)
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS, p1, p2, p3);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template<class P1, class P2, class P3, class P4>
	static bool Call(IScriptSystem* pSS, HSCRIPTFUNCTION func, const P1& p1, const P2& p2, const P3& p3, const P4& p4)
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS, p1, p2, p3, p4);
		return pSS->EndCall();
	}

	template<class P1, class P2, class P3, class P4, class P5>
	static bool Call(IScriptSystem* pSS, HSCRIPTFUNCTION func, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5)
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS, p1, p2, p3, p4, p5);
		return pSS->EndCall();
	}

	template<class P1, class P2, class P3, class P4, class P5, class P6>
	static bool Call(IScriptSystem* pSS, HSCRIPTFUNCTION func, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5, const P6& p6)
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS, p1, p2, p3, p4, p5, p6);
		return pSS->EndCall();
	}

	//! Call to table.
	//! \param pTable Must not be 0.
	static bool CallMethod(IScriptTable* pTable, const char* sMethod)
	{
		MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_ScriptCall, 0, "LUA call (%s)", sMethod);

		assert(pTable);
		IScriptSystem* pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(pTable, sMethod)) return false;
		PushParams(pSS, pTable);
		return pSS->EndCall();
	}
	//! \param pTable Must not be 0.
	template<class P1>
	static bool CallMethod(IScriptTable* pTable, const char* sMethod, const P1& p1)
	{
		MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_ScriptCall, 0, "LUA call (%s)", sMethod);

		assert(pTable);
		IScriptSystem* pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(pTable, sMethod)) return false;
		PushParams(pSS, pTable, p1);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template<class P1, class P2>
	static bool CallMethod(IScriptTable* pTable, const char* sMethod, const P1& p1, const P2& p2)
	{
		MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_ScriptCall, 0, "LUA call (%s)", sMethod);

		IScriptSystem* pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(pTable, sMethod)) return false;
		PushParams(pSS, pTable, p1, p2);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template<class P1, class P2, class P3>
	static bool CallMethod(IScriptTable* pTable, const char* sMethod, const P1& p1, const P2& p2, const P3& p3)
	{
		MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_ScriptCall, 0, "LUA call (%s)", sMethod);

		IScriptSystem* pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(pTable, sMethod)) return false;
		PushParams(pSS, pTable, p1, p2, p3);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template<class P1, class P2, class P3, class P4>
	static bool CallMethod(IScriptTable* pTable, const char* sMethod, const P1& p1, const P2& p2, const P3& p3, const P4& p4)
	{
		MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_ScriptCall, 0, "LUA call (%s)", sMethod);

		IScriptSystem* pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(pTable, sMethod)) return false;
		PushParams(pSS, pTable, p1, p2, p3, p4);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template<class P1, class P2, class P3, class P4, class P5>
	static bool CallMethod(IScriptTable* pTable, const char* sMethod, const P1& p1, const P2& p2, const P3& p3, const P4& p4,
	                       const P5& p5)
	{
		MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_ScriptCall, 0, "LUA call (%s)", sMethod);

		IScriptSystem* pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(pTable, sMethod)) return false;
		PushParams(pSS, pTable, p1, p2, p3, p4, p5);
		return pSS->EndCall();
	}

	//! Call to table.
	static bool CallMethod(IScriptTable* pTable, HSCRIPTFUNCTION func)
	{
		IScriptSystem* pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS, pTable);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template<class P1>
	static bool CallMethod(IScriptTable* pTable, HSCRIPTFUNCTION func, const P1& p1)
	{
		IScriptSystem* pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS, pTable, p1);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template<class P1, class P2>
	static bool CallMethod(IScriptTable* pTable, HSCRIPTFUNCTION func, const P1& p1, const P2& p2)
	{
		IScriptSystem* pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS, pTable, p1, p2);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template<class P1, class P2, class P3>
	static bool CallMethod(IScriptTable* pTable, HSCRIPTFUNCTION func, const P1& p1, const P2& p2, const P3& p3)
	{
		IScriptSystem* pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS, pTable, p1, p2, p3);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template<class P1, class P2, class P3, class P4>
	static bool CallMethod(IScriptTable* pTable, HSCRIPTFUNCTION func, const P1& p1, const P2& p2, const P3& p3, const P4& p4)
	{
		IScriptSystem* pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS, pTable, p1, p2, p3, p4);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template<class P1, class P2, class P3, class P4, class P5>
	static bool CallMethod(IScriptTable* pTable, HSCRIPTFUNCTION func, const P1& p1, const P2& p2, const P3& p3, const P4& p4,
	                       const P5& p5)
	{
		IScriptSystem* pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS, pTable, p1, p2, p3, p4, p5);
		return pSS->EndCall();
	}

	//////////////////////////////////////////////////////////////////////////
	template<class Ret>
	static bool CallReturn(IScriptSystem* pSS, HSCRIPTFUNCTION func, Ret& ret)
	{
		if (!pSS->BeginCall(func)) return false;
		return pSS->EndCall(ret);
	}
	//////////////////////////////////////////////////////////////////////////
	template<class P1, class Ret>
	static bool CallReturn(IScriptSystem* pSS, HSCRIPTFUNCTION func, const P1& p1, Ret& ret)
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS, p1);
		return pSS->EndCall(ret);
	}
	//////////////////////////////////////////////////////////////////////////
	template<class P1, class P2, class Ret>
	static bool CallReturn(IScriptSystem* pSS, HSCRIPTFUNCTION func, const P1& p1, const P2& p2, Ret& ret)
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS, p1, p2);
		return pSS->EndCall(ret);
	}
	//////////////////////////////////////////////////////////////////////////
	template<class P1, class P2, class P3, class Ret>
	static bool CallReturn(IScriptSystem* pSS, HSCRIPTFUNCTION func, const P1& p1, const P2& p2, const P3& p3, Ret& ret)
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS, p1, p2, p3);
		return pSS->EndCall(ret);
	}
	//////////////////////////////////////////////////////////////////////////
	template<class P1, class P2, class P3, class P4, class Ret>
	static bool CallReturn(IScriptSystem* pSS, HSCRIPTFUNCTION func, const P1& p1, const P2& p2, const P3& p3, const P4& p4, Ret& ret)
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS, p1, p2, p3, p4);
		return pSS->EndCall(ret);
	}
	template<class P1, class P2, class P3, class P4, class P5, class Ret>
	static bool CallReturn(IScriptSystem* pSS, HSCRIPTFUNCTION func, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5, Ret& ret)
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS, p1, p2, p3, p4, p5);
		return pSS->EndCall(ret);
	}

	//////////////////////////////////////////////////////////////////////////
private:
	template<class P1>
	static void PushParams(IScriptSystem* pSS, const P1& p1)
	{
		pSS->PushFuncParam(p1);
	}
	template<class P1, class P2>
	static void PushParams(IScriptSystem* pSS, const P1& p1, const P2& p2)
	{
		pSS->PushFuncParam(p1);
		pSS->PushFuncParam(p2);
	}
	template<class P1, class P2, class P3>
	static void PushParams(IScriptSystem* pSS, const P1& p1, const P2& p2, const P3& p3)
	{
		pSS->PushFuncParam(p1);
		pSS->PushFuncParam(p2);
		pSS->PushFuncParam(p3);
	}
	template<class P1, class P2, class P3, class P4>
	static void PushParams(IScriptSystem* pSS, const P1& p1, const P2& p2, const P3& p3, const P4& p4)
	{
		pSS->PushFuncParam(p1);
		pSS->PushFuncParam(p2);
		pSS->PushFuncParam(p3);
		pSS->PushFuncParam(p4);
	}
	template<class P1, class P2, class P3, class P4, class P5>
	static void PushParams(IScriptSystem* pSS, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5)
	{
		pSS->PushFuncParam(p1);
		pSS->PushFuncParam(p2);
		pSS->PushFuncParam(p3);
		pSS->PushFuncParam(p4);
		pSS->PushFuncParam(p5);
	}
	template<class P1, class P2, class P3, class P4, class P5, class P6>
	static void PushParams(IScriptSystem* pSS, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5, const P6& p6)
	{
		pSS->PushFuncParam(p1);
		pSS->PushFuncParam(p2);
		pSS->PushFuncParam(p3);
		pSS->PushFuncParam(p4);
		pSS->PushFuncParam(p5);
		pSS->PushFuncParam(p6);
	}
};

//! \endcond