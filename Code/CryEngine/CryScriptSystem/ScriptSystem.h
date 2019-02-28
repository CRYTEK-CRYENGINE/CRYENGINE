// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/IConsole.h>
#include <CryScriptSystem/IScriptSystem.h>
#pragma warning(push) // Because lua.h touches warning C4996
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#pragma warning(pop)

#include "StackGuard.h"
#include "ScriptBindings/ScriptBinding.h"
#include "ScriptTimerMgr.h"

class CLUADbg;

struct SLuaStackEntry
{
	int    line;
	string source;
	string description;
};

// Returns literal representation of the type value
inline const char* ScriptAnyTypeToString(EScriptAnyType type)
{
	switch (type)
	{
	case EScriptAnyType::Any:
		return "Any";
	case EScriptAnyType::Nil:
		return "Null";
	case EScriptAnyType::Boolean:
		return "Boolean";
	case EScriptAnyType::String:
		return "String";
	case EScriptAnyType::Number:
		return "Number";
	case EScriptAnyType::Function:
		return "Function";
	case EScriptAnyType::Table:
		return "Table";
	case EScriptAnyType::UserData:
		return "UserData";
	case EScriptAnyType::Handle:
		return "Pointer";
	case EScriptAnyType::Vector:
		return "Vec3";
	default:
		return "Unknown";
	}
}

// Returns literal representation of the type value
inline const char* ScriptVarTypeAsCStr(ScriptVarType t)
{
	switch (t)
	{
	case svtNull:
		return "Null";
	case svtBool:
		return "Boolean";
	case svtString:
		return "String";
	case svtNumber:
		return "Number";
	case svtFunction:
		return "Function";
	case svtObject:
		return "Table";
	case svtUserData:
		return "UserData";
	case svtPointer:
		return "Pointer";
	default:
		return "Unknown";
	}
}

typedef std::set<string, stl::less_stricmp<string>> ScriptFileList;
typedef ScriptFileList::iterator                    ScriptFileListItor;

//////////////////////////////////////////////////////////////////////////
// forwarde declarations.
class CScriptSystem;
class CScriptTable;

#define SCRIPT_OBJECT_POOL_SIZE 15000

/*! IScriptSystem implementation
   @see IScriptSystem
 */
class CScriptSystem : public IScriptSystem, public ISystemEventListener
{
public:
	//! constructor
	CScriptSystem();
	//! destructor
	virtual ~CScriptSystem();
	//!
	bool          Init(ISystem* pSystem, bool bStdLibs, int nStackSize);

	void          Update();
	void          SetGCFrequency(const float fRate);

	void          SetEnvironment(HSCRIPTFUNCTION scriptFunction, IScriptTable* pEnv);
	IScriptTable* GetEnvironment(HSCRIPTFUNCTION scriptFunction);

	//!
	void RegisterErrorHandler(void);

	//!
	bool _ExecuteFile(const char* sFileName, bool bRaiseError, IScriptTable* pEnv = 0);
	//!

	// interface IScriptSystem -----------------------------------------------------------

	virtual bool          ExecuteFile(const char* sFileName, bool bRaiseError, bool bForceReload, IScriptTable* pEnv = 0);
	virtual bool          ExecuteBuffer(const char* sBuffer, size_t nSize, const char* sBufferDescription, IScriptTable* pEnv = 0);
	virtual void          UnloadScript(const char* sFileName);
	virtual void          UnloadScripts();
	virtual bool          ReloadScript(const char* sFileName, bool bRaiseError);
	virtual bool          ReloadScripts();
	virtual void          DumpLoadedScripts();

	virtual IScriptTable* CreateTable(bool bEmpty = false);

	//////////////////////////////////////////////////////////////////////////
	// Begin Call.
	//////////////////////////////////////////////////////////////////////////
	virtual int BeginCall(HSCRIPTFUNCTION hFunc);
	virtual int BeginCall(const char* sFuncName);
	virtual int BeginCall(const char* sTableName, const char* sFuncName);
	virtual int BeginCall(IScriptTable* pTable, const char* sFuncName);

	//////////////////////////////////////////////////////////////////////////
	// End Call.
	//////////////////////////////////////////////////////////////////////////
	virtual bool EndCall();
	virtual bool EndCallAny(ScriptAnyValue& any);
	virtual bool EndCallAnyN(int n, ScriptAnyValue* anys);

	//////////////////////////////////////////////////////////////////////////
	// Get function pointer.
	//////////////////////////////////////////////////////////////////////////
	virtual HSCRIPTFUNCTION GetFunctionPtr(const char* sFuncName);
	virtual HSCRIPTFUNCTION GetFunctionPtr(const char* sTableName, const char* sFuncName);
	virtual void            ReleaseFunc(HSCRIPTFUNCTION f);
	virtual HSCRIPTFUNCTION AddFuncRef(HSCRIPTFUNCTION f);
	virtual bool            CompareFuncRef(HSCRIPTFUNCTION f1, HSCRIPTFUNCTION f2);

	virtual ScriptAnyValue  CloneAny(const ScriptAnyValue& any);
	virtual void            ReleaseAny(const ScriptAnyValue& any);

	//////////////////////////////////////////////////////////////////////////
	// Push function param.
	//////////////////////////////////////////////////////////////////////////
	virtual void PushFuncParamAny(const ScriptAnyValue& any);

	//////////////////////////////////////////////////////////////////////////
	// Set global value.
	//////////////////////////////////////////////////////////////////////////
	virtual void          SetGlobalAny(const char* sKey, const ScriptAnyValue& any);
	virtual bool          GetGlobalAny(const char* sKey, ScriptAnyValue& any);

	virtual IScriptTable* CreateUserData(void* ptr, size_t size);
	virtual void          ForceGarbageCollection();
	virtual int           GetCGCount();
	virtual void          SetGCThreshhold(int nKb);
	virtual void          Release();
	virtual void          ShowDebugger(const char* pszSourceFile, int iLine, const char* pszReason);
	virtual HBREAKPOINT   AddBreakPoint(const char* sFile, int nLineNumber);
	virtual IScriptTable* GetLocalVariables(int nLevel, bool bRecursive);
	virtual IScriptTable* GetCallsStack();// { return 0; };
	virtual void          DumpCallStack();

	virtual void          DebugContinue() {}
	virtual void          DebugStepNext() {}
	virtual void          DebugStepInto() {}
	virtual void          DebugDisable()  {}

	virtual BreakState    GetBreakState() { return bsNoBreak; }
	virtual void          GetMemoryStatistics(ICrySizer* pSizer) const;
	virtual void          GetScriptHash(const char* sPath, const char* szKey, unsigned int& dwHash);
	virtual void          PostInit();
	virtual void          RaiseError(const char* format, ...) PRINTF_PARAMS(2, 3);
	virtual void          LoadScriptedSurfaceTypes(const char* sFolder, bool bReload);
	virtual void          SerializeTimers(ISerialize* pSer);
	virtual void          ResetTimers();

	virtual int           GetStackSize() const;
	virtual uint32        GetScriptAllocSize();

	virtual void*         Allocate(size_t sz);
	virtual size_t        Deallocate(void* ptr);

	void                  PushAny(const ScriptAnyValue& var);
	bool                  PopAny(ScriptAnyValue& var);
	// Convert top stack item to Any.
	bool                  ToAny(ScriptAnyValue& var, int index);
	void                  PushVec3(const Vec3& vec);
	bool                  ToVec3(Vec3& vec, int index);
	// Push table reference
	void                  PushTable(IScriptTable* pTable);
	// Attach reference at the top of the stack to the specified table pointer.
	void                  AttachTable(IScriptTable* pTable);
	bool                  GetRecursiveAny(IScriptTable* pTable, const char* sKey, ScriptAnyValue& any);

	lua_State*            GetLuaState() const { return L; }
	void                  TraceScriptError(const char* file, int line, const char* errorStr);
	void                  LogStackTrace();

	CScriptTimerMgr*      GetScriptTimerMgr() { return m_pScriptTimerMgr; };

	void                  GetCallStack(std::vector<SLuaStackEntry>& callstack);
	bool                  IsCallStackEmpty(void);
	void                  DumpStateToFile(const char* filename);

	//////////////////////////////////////////////////////////////////////////
	// Facility to pre-catch any lua buffer
	//////////////////////////////////////////////////////////////////////////
	virtual HSCRIPTFUNCTION CompileBuffer(const char* sBuffer, size_t nSize, const char* sBufferDesc);
	virtual int             PreCacheBuffer(const char* sBuffer, size_t nSize, const char* sBufferDesc);
	virtual int             BeginPreCachedBuffer(int iIndex);
	virtual void            ClearPreCachedBuffer();

	//////////////////////////////////////////////////////////////////////////
	// ISystemEventListener
	//////////////////////////////////////////////////////////////////////////
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
	//////////////////////////////////////////////////////////////////////////

	// Used by Lua debugger to maintain callstack exposed to C++
	void ExposedCallstackClear();
	void ExposedCallstackPush(const char* sFunction, int nLine, const char* sSource);
	void ExposedCallstackPop();

private: // ---------------------------------------------------------------------
	//!
	bool       EndCallN(int nReturns);

	static int ErrorHandler(lua_State* L);

	// Create default metatables.
	void CreateMetatables();

	// Now private, doesn't need to be updated by main thread
	void        EnableDebugger(ELuaDebugMode eDebugMode);
	void        EnableCodeCoverage(bool enable);

	static void DebugModeChange(ICVar* cvar);
	static void CodeCoverageChange(ICVar* cvar);

	// Loaded file tracking helpers
	void AddFileToList(const char* sName);
	void RemoveFileFromList(const ScriptFileListItor& itor);

	// ----------------------------------------------------------------------------
private:
	static CScriptSystem* s_mpScriptSystem;
	lua_State*            L;
	ICVar*                m_cvar_script_debugger; // Stores current debugging mode
	ICVar*                m_cvar_script_coverage;
	int                   m_nTempArg;
	int                   m_nTempTop;

	IScriptTable*         m_pUserDataMetatable;
	IScriptTable*         m_pPreCacheBufferTable;
	std::vector<string>   m_vecPreCached;

	HSCRIPTFUNCTION       m_pErrorHandlerFunc;

	ScriptFileList        m_dqLoadedFiles;

	CScriptBindings       m_stdScriptBinds;
	ISystem*              m_pSystem;

	float                 m_fGCFreq;      //!< relative time in seconds
	float                 m_lastGCTime;   //!< absolute time in seconds
	int                   m_nLastGCCount; //!<
	int                   m_forceReloadCount;

	CScriptTimerMgr*      m_pScriptTimerMgr;

	// Store a simple callstack that can be inspected in C++ debugger
	const static int MAX_CALLDEPTH = 32;
	int              m_nCallDepth;
	stack_string     m_sCallDescriptions[MAX_CALLDEPTH];

public: // -----------------------------------------------------------------------

	string   m_sLastBreakSource; //!
	int      m_nLastBreakLine;   //!
	CLUADbg* m_pLuaDebugger;
};
