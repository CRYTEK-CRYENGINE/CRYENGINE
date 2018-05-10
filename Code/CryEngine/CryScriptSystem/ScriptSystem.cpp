// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#ifdef DEBUG_LUA_STATE
	#include <CryEntitySystem/IEntity.h>
	#include <CryEntitySystem/IEntitySystem.h>
#endif

#include <string.h>
#include <stdio.h>
#include "ScriptSystem.h"
#include "ScriptTable.h"
#include "StackGuard.h"
#include "BucketAllocator.h"

#include <CrySystem/ISystem.h> // For warning and errors.
// needed for crypak
#include <CrySystem/File/ICryPak.h>
#include <CrySystem/IConsole.h>
#include <CrySystem/ICmdLine.h>
#include <CrySystem/ILog.h>
#include <CrySystem/ITimer.h>
#include <CryAISystem/IAISystem.h>
#include <CryMath/Random.h>

#include <CryNetwork/ISerialize.h>

#pragma warning(push) // Because lua.h touches warning C4996
extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lobject.h"
	LUALIB_API void lua_bitlibopen(lua_State* L);
}
#pragma warning(pop)

#if CRY_PLATFORM_WINDOWS
	#include "LuaDebugger/LUADBG.h"
	#include "LuaDebugger/Coverage.h"
extern HANDLE gDLLHandle;
#endif

#include "LuaRemoteDebug/LuaRemoteDebug.h"

// Fine tune this value for optimal performance/memory
#define PER_FRAME_LUA_GC_STEP        2

#define LUA_NODE_ALLOCATOR_BLOCKSIZE 128 * 1024
#define LUA_NODE_ALLOCATOR_TYPE      eCryDefaultMalloc

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
int g_dumpStackOnAlloc = 0;
int g_nPrecaution = 0; // will cause delayed crash, will make engine extremelly unstable.

// Global Lua debugger pointer (if initialized)
CLUADbg* g_pLuaDebugger = 0;

// Script memory allocator.
//PageBucketAllocatorForLua gLuaAlloc;

#if LUA_REMOTE_DEBUG_ENABLED
CLuaRemoteDebug* g_pLuaRemoteDebug = 0;
#endif

namespace
{
void LuaDebuggerShow(IConsoleCmdArgs* /* pArgs */)
{
	gEnv->pScriptSystem->ShowDebugger(NULL, 0, "");
}
void LuaDumpState(IConsoleCmdArgs* /* pArgs */)
{
	((CScriptSystem*)gEnv->pScriptSystem)->DumpStateToFile("LuaState.txt");
}
void ReloadScriptCmd(IConsoleCmdArgs* pArgs)
{
	int nArgs = pArgs->GetArgCount();
	// First arg is the console command itself
	if (nArgs < 2)
	{
		ScriptWarning("No file specified for reload");
	}
	for (int i = 1; i < nArgs; i++)
	{
		const char* sFilename = pArgs->GetArg(i);
		if (((CScriptSystem*)gEnv->pScriptSystem)->ExecuteFile(sFilename, true, true))
		{
			CryLog("Loaded %s", sFilename);
		}
		// No need to give an error on fail, ExecuteFile already does this
	}
}

void LuaDumpCoverage(IConsoleCmdArgs*)
{
#if CRY_PLATFORM_WINDOWS
	g_pLuaDebugger->GetCoverage()->DumpCoverage();
#endif
}

void LuaGarbargeCollect(IConsoleCmdArgs*)
{
	gEnv->pScriptSystem->ForceGarbageCollection();
}

inline CScriptTable* AllocTable() { return new CScriptTable; }
//	inline void FreeTable( CScriptTable *pTable ) { /*delete pTable;*/ }
}

// Module-level overrides to the allocator in use.

#include <CryMemory/CryMemoryAllocator.h>
#include <CryMemory/BucketAllocatorImpl.h> // needs full implementation before instantiation

#if USE_RAW_LUA_ALLOCS

extern void* _LuaAlloc(size_t size);
extern void  _LuaFree(void* p);
extern void* _LuaRealloc(void* p, size_t size);

#else

class lua_allocator :
	#if defined(USE_GLOBAL_BUCKET_ALLOCATOR)
	public BucketAllocator<BucketAllocatorDetail::DefaultTraits<12*1024*1024, BucketAllocatorDetail::SyncPolicyUnlocked>>
	#else
	public node_alloc<LUA_NODE_ALLOCATOR_TYPE, false, LUA_NODE_ALLOCATOR_BLOCKSIZE>
	#endif
{
public:
	lua_allocator()
	#if defined(USE_GLOBAL_BUCKET_ALLOCATOR)
		: m_iAllocated(0)
	#else
		: m_iAllocated(0)
	#endif
	{
	}

	void* re_alloc(void* ptr, size_t osize, size_t nsize)
	{

		if (NULL == ptr)
		{
			if (nsize)
			{
				m_iAllocated += nsize;
				return alloc(nsize);
			}

			return 0;
		}
		else
		{
			void* nptr = 0;
			if (nsize)
			{
				nptr = (char*)alloc(nsize) + g_nPrecaution;
				memcpy(nptr, ptr, nsize > osize ? osize : nsize);
				m_iAllocated += nsize;
			}
			dealloc(ptr, osize);
			m_iAllocated -= osize;
			return nptr;
		}
	}

	size_t get_alloc_size() { return m_iAllocated; }

	void   GetMemoryUsage(ICrySizer* pSizer) const
	{
	}
private:
	int m_iAllocated;
};

static lua_allocator gLuaAlloc;

#endif // USE_RAW_LUA_ALLOCS

CScriptSystem* CScriptSystem::s_mpScriptSystem = NULL;

//////////////////////////////////////////////////////////////////////
extern "C"
{
	int vl_initvectorlib(lua_State* L);

	static lua_State* g_LStack = 0;

	//////////////////////////////////////////////////////////////////////////
	void DumpCallStack(lua_State* L)
	{
		lua_Debug ar;

		memset(&ar, 0, sizeof(lua_Debug));

		//////////////////////////////////////////////////////////////////////////
		// Print callstack.
		//////////////////////////////////////////////////////////////////////////
		int level = 0;
		while (lua_getstack(L, level++, &ar))
		{
			const char* slevel = "";
			if (level == 1)
				slevel = "  ";
			int nRes = lua_getinfo(L, "lnS", &ar);
			if (ar.name)
				CryLog("$6%s    > %s, (%s: %d)", slevel, ar.name, ar.short_src, ar.currentline);
			else
				CryLog("$6%s    > (null) (%s: %d)", slevel, ar.short_src, ar.currentline);
		}
	}

	void* custom_lua_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
	{
#if USE_RAW_LUA_ALLOCS
		return _LuaRealloc(ptr, nsize);
#else
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_LUA, 0, "Lua");

		if (g_dumpStackOnAlloc)
			DumpCallStack(g_LStack);

	#if !defined(NOT_USE_CRY_MEMORY_MANAGER) && !defined(_DEBUG)
		void* ret = gLuaAlloc.re_alloc(ptr, osize, nsize);
		return ret;
	#endif

		(void)ud;
		(void)osize;
		if (nsize == 0)
		{
			free(ptr);
			return NULL;
		}
		else
			return realloc(ptr, nsize);

#endif // USE_RAW_LUA_ALLOCS
	}
	static int cutsom_lua_panic(lua_State* L)
	{
		ScriptWarning("PANIC: unprotected error during Lua-API call\nLua error string: '%s'", lua_tostring(L, -1));
		DumpCallStack(L);

		// from lua docs:
		//
		// "Lua calls a panic function and then calls exit(EXIT_FAILURE), thus exiting the host application."
		//
		//	so we might as well fatal error here - at least we can inform the user, create a crash dump and fill out a jira bug etc...
		CryFatalError("PANIC: unprotected error during Lua-API call\nLua error string: '%s'", lua_tostring(L, -1));

		return 0;
	}

	// Random function used by lua.
	float script_frand0_1()
	{
		return cry_random(0.0f, 1.0f);
	}

	void script_randseed(unsigned int seed)
	{
		gEnv->pSystem->GetRandomGenerator().Seed(seed);
	}
}

//////////////////////////////////////////////////////////////////////////
// For debugger.
//////////////////////////////////////////////////////////////////////////
IScriptTable* CScriptSystem::GetLocalVariables(int nLevel, bool bRecursive)
{
	lua_Debug ar;
	const char* name;
	IScriptTable* pObj = CreateTable(true);
	pObj->AddRef();

	// Attach a new table
	const int checkStack = lua_gettop(L);
	lua_newtable(L);
	lua_pushvalue(L, -1);
	AttachTable(pObj);

	// Get the stack frame
	while (lua_getstack(L, nLevel, &ar) != 0)
	{
		// Push a sub-table for this frame (recursive only)
		if (bRecursive)
		{
			assert(lua_istable(L, -1) && "The result table should be on the top of the stack");
			lua_pushinteger(L, nLevel);
			lua_newtable(L);
			lua_pushvalue(L, -1);
			lua_insert(L, -3);
			lua_rawset(L, -4);
			assert(lua_istable(L, -1) && lua_istable(L, -2) && lua_gettop(L) == checkStack + 2 && "There should now be two tables on the top of the stack");
		}

		// Assign variable names and values for the current frame to the table on top of the stack
		int i = 1;
		const int checkInner = lua_gettop(L);
		assert(checkInner == checkStack + 1 + bRecursive && "Too much stack space used");
		assert(lua_istable(L, -1) && "The target table must be on the top of the stack");
		while ((name = lua_getlocal(L, &ar, i++)) != NULL)
		{
			if (strcmp(name, "(*temporary)") == 0)
			{
				// Not interested in temporaries
				lua_pop(L, 1);
			}
			else
			{
				// Push the name, swap the top two items, and set in the table
				lua_pushstring(L, name);
				lua_insert(L, -2);
				assert(lua_gettop(L) == checkInner + 2 && "There should be a key-value pair on top of the stack");
				lua_rawset(L, -3);
			}
			assert(lua_gettop(L) == checkInner && "Unbalanced algorithm problem");
			assert(lua_istable(L, -1) && "The target table should be on the top of the stack");
		}

		// Pop the sub-table (recursive only)
		if (bRecursive)
		{
			assert(lua_istable(L, -1) && lua_istable(L, -2) && "There should now be two tables on the top of the stack");
			lua_pop(L, 1);
		}
		else break;

		nLevel++;
	}

	// Pop the result table from the stack
	lua_pop(L, 1);
	assert(lua_gettop(L) == checkStack && "Unbalanced algorithm problem");
	return pObj;
}

const int LEVELS1 = 12; /* size of the first part of the stack */
const int LEVELS2 = 10; /* size of the second part of the stack */

void CScriptSystem::GetCallStack(std::vector<SLuaStackEntry>& callstack)
{
	callstack.clear();

	int level = 0;
	int firstpart = 1;  /* still before eventual `...' */
	lua_Debug ar;
	while (lua_getstack(L, level++, &ar))
	{

		char buff[512];  /* enough to fit following `cry_sprintf's */
		if (level == 2)
		{
			//luaL_addstring(&b, ("stack traceback:\n"));
		}
		else if (level > LEVELS1 && firstpart)
		{
			/* no more than `LEVELS2' more levels? */
			if (!lua_getstack(L, level + LEVELS2, &ar))
				level--;  /* keep going */
			else
			{
				//		luaL_addstring(&b, ("       ...\n"));  /* too many levels */
				while (lua_getstack(L, level + LEVELS2, &ar))  /* find last levels */
					level++;
			}
			firstpart = 0;
			continue;
		}

		cry_sprintf(buff, ("%4d:  "), level - 1);

		lua_getinfo(L, ("Snl"), &ar);
		switch (*ar.namewhat)
		{
		case 'l':
			cry_sprintf(buff, "function[local] `%.50s'", ar.name);
			break;
		case 'g':
			cry_sprintf(buff, "function[global] `%.50s'", ar.name);
			break;
		case 'f':  /* field */
			cry_sprintf(buff, "field `%.50s'", ar.name);
			break;
		case 'm':  /* field */
			cry_sprintf(buff, "method `%.50s'", ar.name);
			break;
		case 't':  /* tag method */
			cry_sprintf(buff, "`%.50s' tag method", ar.name);
			break;
		default:
			cry_strcpy(buff, "");
			break;
		}

		SLuaStackEntry se;
		se.description = buff;
		se.line = ar.currentline;
		se.source = ar.source;

		callstack.push_back(se);
	}
}

IScriptTable* CScriptSystem::GetCallsStack()
{
	std::vector<SLuaStackEntry> stack;

	IScriptTable* pCallsStack = CreateTable();
	assert(pCallsStack);

	pCallsStack->AddRef();
	GetCallStack(stack);

	for (size_t i = 0; i < stack.size(); ++i)
	{
		SmartScriptTable pEntry(this);

		pEntry->SetValue("description", stack[i].description.c_str());
		pEntry->SetValue("line", stack[i].line);
		pEntry->SetValue("sourcefile", stack[i].source.c_str());

		pCallsStack->PushBack(pEntry);
	}
	return pCallsStack;
}

void CScriptSystem::DumpCallStack()
{
	CryLogAlways("LUA call stack : ============================================================");
	LogStackTrace();
	CryLogAlways("=============================================================================");
}

bool CScriptSystem::IsCallStackEmpty(void)
{
	lua_Debug ar;
	return (lua_getstack(L, 0, &ar) == 0);
}

int listvars(lua_State* L, int level)
{
	char sTemp[1000];
	lua_Debug ar;
	int i = 1;
	const char* name;
	if (lua_getstack(L, level, &ar) == 0)
		return 0; /* failure: no such level in the stack */
	while ((name = lua_getlocal(L, &ar, i)) != NULL)
	{
		cry_sprintf(sTemp, "%s =", name);
		OutputDebugString(sTemp);

		if (lua_isnumber(L, i))
		{
			int n = (int)lua_tonumber(L, i);
			itoa(n, sTemp, 10);
			OutputDebugString(sTemp);
		}
		else if (lua_isstring(L, i))
		{
			OutputDebugString(lua_tostring(L, i));
		}
		else if (lua_isnil(L, i))
		{
			OutputDebugString("nil");
		}
		else
		{
			OutputDebugString("<<unknown>>");
		}
		OutputDebugString("\n");
		i++;
		lua_pop(L, 1);   /* remove variable value */
	}
	/*lua_getglobals(L);
	   i = 1;
	   while ((name = lua_getlocal(L, &ar, i)) != NULL)
	   {
	   cry_sprintf(sTemp, "%s =", name);
	   OutputDebugString(sTemp);


	   if (lua_isnumber(L, i))
	   {
	   int n = (int)lua_tonumber(L, i);
	   itoa(n, sTemp, 10);
	   OutputDebugString(sTemp);
	   }
	   else if (lua_isstring(L, i))
	   {
	   OutputDebugString(lua_tostring(L, i));
	   }else
	   if (lua_isnil(L, i))
	   {
	   OutputDebugString("nil");
	   }
	   else
	   {
	   OutputDebugString("<<unknown>>");
	   }
	   OutputDebugString("\n");
	   i++;
	   lua_pop(L, 1);
	   }*/

	return 1;
}

static void LuaDebugHook(lua_State* L, lua_Debug* ar)
{
#if CRY_PLATFORM_WINDOWS

	if (g_pLuaDebugger)
		g_pLuaDebugger->OnDebugHook(L, ar);
#endif

#if LUA_REMOTE_DEBUG_ENABLED
	if (g_pLuaRemoteDebug)
		g_pLuaRemoteDebug->OnDebugHook(L, ar);
#endif
}

/*
   static void callhook(lua_State *L, lua_Debug *ar)
   {
   CScriptSystem *pSS = (CScriptSystem*)gEnv->pScriptSystem;
   if(pSS->m_bsBreakState!=bsStepInto)return;
   lua_getinfo(L, "Sl", ar);
   ScriptDebugInfo sdi;
   pSS->m_sLastBreakSource = sdi.sSourceName = ar->source;
   pSS->m_nLastBreakLine =	sdi.nCurrentLine = ar->currentline;

   pSS->ShowDebugger( sdi.sSourceName,sdi.nCurrentLine, "Breakpoint Hit");
   }

   static void linehook(lua_State *L, lua_Debug *ar)
   {
   CScriptSystem *pSS=(CScriptSystem*)gEnv->pScriptSystem;

   if(pSS->m_bsBreakState!=bsNoBreak)
   {
    switch(pSS->m_bsBreakState)
    {
    case bsContinue:
      if(pSS->m_BreakPoint.nLine==ar->currentline)
      {
        lua_getinfo(L, "Sl", ar);
        if(ar->source)
        {
          if(stricmp(ar->source,pSS->m_BreakPoint.sSourceFile.c_str())==0)
            break;
        }
      }
      return;
    case bsStepNext:
    case bsStepInto:
      if(pSS->m_BreakPoint.nLine!=ar->currentline)
      {
        lua_getinfo(L, "Sl", ar);
        if((stricmp(pSS->m_sLastBreakSource.c_str(),ar->source)==0)){
          break;
        }
      }
      return;

      /*lua_getinfo(L, "S", ar);
      if(ar->source)
      {
      if(pSS->m_BreakPoint.nLine!=ar->currentline && (stricmp(pSS->m_sLastBreakSource.c_str(),ar->source)!=0))
      break;
      }
      return;*//*
    default:
      return;
    };
    ScriptDebugInfo sdi;
    pSS->m_sLastBreakSource = sdi.sSourceName = ar->source;
    pSS->m_nLastBreakLine =	sdi.nCurrentLine = ar->currentline;
    pSS->ShowDebugger( sdi.sSourceName,sdi.nCurrentLine, "Breakpoint Hit");
   }

   }
 */

//////////////////////////////////////////////////////////////////////////
const char* FormatPath(char* const sLowerName, const char* sPath)
{
	strcpy(sLowerName, sPath);
	int i = 0;
	while (sLowerName[i] != 0)
	{
		if (sLowerName[i] == '\\')
			sLowerName[i] = '/';
		i++;
	}
	return sLowerName;
}

/* For LuaJIT
   static void l_message (const char *pname, const char *msg) {
   if (pname) fprintf(stderr, "%s: ", pname);
   fprintf(stderr, "%s\n", msg);
   fflush(stderr);
   }

   static const char *progname = "CryScriptSystem";

   static int report (lua_State *L, int status) {
   if (status && !lua_isnil(L, -1)) {
    const char *msg = lua_tostring(L, -1);
    if (msg == NULL) msg = "(error object is not a string)";
    l_message(progname, msg);
    lua_pop(L, 1);
   }
   return status;
   }

   // ---- start of LuaJIT extensions
   static int loadjitmodule (lua_State *L, const char *notfound)
   {
   lua_getglobal(L, "require");
   lua_pushliteral(L, "jit.");
   lua_pushvalue(L, -3);
   lua_concat(L, 2);
   if (lua_pcall(L, 1, 1, 0)) {
    const char *msg = lua_tostring(L, -1);
    if (msg && !strncmp(msg, "module ", 7)) {
      l_message(progname, notfound);
      return 1;
    }
    else
      return report(L, 1);
   }
   lua_getfield(L, -1, "start");
   lua_remove(L, -2);  // drop module table
   return 0;
   }

   // start optimizer
   static int dojitopt (lua_State *L, const char *opt) {
   lua_pushliteral(L, "opt");
   if (loadjitmodule(L, "LuaJIT optimizer module not installed"))
    return 1;
   lua_remove(L, -2);  // drop module name
   if (*opt) lua_pushstring(L, opt);
   return report(L, lua_pcall(L, *opt ? 1 : 0, 0, 0));
   }

   // JIT engine control command: try jit library first or load add-on module
   static int dojitcmd (lua_State *L, const char *cmd) {
   const char *val = strchr(cmd, '=');
   lua_pushlstring(L, cmd, val ? val - cmd : strlen(cmd));
   lua_getglobal(L, "jit");  // get jit.* table
   lua_pushvalue(L, -2);
   lua_gettable(L, -2);  // lookup library function
   if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);  // drop non-function and jit.* table, keep module name
    if (loadjitmodule(L, "unknown luaJIT command"))
      return 1;
   }
   else {
    lua_remove(L, -2);  // drop jit.* table
   }
   lua_remove(L, -2);  // drop module name
   if (val) lua_pushstring(L, val+1);
   return report(L, lua_pcall(L, val ? 1 : 0, 0, 0));
   }
 */

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
CScriptSystem::CScriptSystem()
	: L(nullptr)
	, m_cvar_script_debugger(nullptr)
	, m_cvar_script_coverage(nullptr)
	, m_nTempArg(0)
	, m_nTempTop(0)
	, m_pUserDataMetatable(nullptr)
	, m_pPreCacheBufferTable(nullptr)
	, m_pErrorHandlerFunc(nullptr)
	, m_pSystem(nullptr)
	, m_fGCFreq(10.0f)
	, m_lastGCTime(0.0f)
	, m_nLastGCCount(0)
	, m_forceReloadCount(0)
	, m_pScriptTimerMgr(nullptr)
	, m_nCallDepth(0)
	, m_nLastBreakLine(0)
	, m_pLuaDebugger(nullptr)
{
	s_mpScriptSystem = this; // Should really be checking here...
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
CScriptSystem::~CScriptSystem()
{
	m_pSystem->GetISystemEventDispatcher()->RemoveListener(this);

	delete m_pScriptTimerMgr;

#if CRY_PLATFORM_WINDOWS
	if (m_pLuaDebugger)
	{
		delete m_pLuaDebugger;
		m_pLuaDebugger = NULL;
		g_pLuaDebugger = NULL;
	}
#endif

	m_stdScriptBinds.Done();

	if (L)
	{
		lua_close(L);

		L = NULL;
	}
}

//////////////////////////////////////////////////////////////////////
bool CScriptSystem::Init(ISystem* pSystem, bool bStdLibs, int nStackSize)
{
	assert(gEnv->pConsole);

	m_pSystem = pSystem;
	m_pScriptTimerMgr = new CScriptTimerMgr(this);

	m_pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CScriptSystem");

	//L = lua_open();
	L = lua_newstate(custom_lua_alloc, NULL);
	lua_atpanic(L, &cutsom_lua_panic);

	g_LStack = L;
	CScriptTable::L = L; // Set lua state for script table class.
	CScriptTable::m_pSS = this;

#if defined(_RELEASE)
	lua_storedebuginfo(L, 0);
#endif // !defined(_RELEASE)

	if (bStdLibs)
	{
		luaL_openlibs(L);
	}
	lua_bitlibopen(L);
	vl_initvectorlib(L);

	// For LuaJIT
	//dojitopt(L,"2");
	//dojitcmd(L,"trace");
	//dojitcmd(L,"dumphints=ljit_dumphints.txt");
	//dojitcmd(L,"dump=ljit_dump.txt");

	CreateMetatables();

	m_stdScriptBinds.Init(GetISystem(), this);

	// Set global time variable into the script.
	SetGlobalValue("_time", 0);
	SetGlobalValue("_frametime", 0);
	SetGlobalValue("_aitick", 0);
	m_nLastGCCount = GetCGCount();

	// Make the error handler available to LUA
	RegisterErrorHandler();

	// Register the command for showing the lua debugger
	REGISTER_COMMAND("lua_debugger_show", LuaDebuggerShow, VF_NULL, "Shows the Lua debugger window");
	REGISTER_COMMAND("lua_dump_state", LuaDumpState, VF_NULL, "Dumps the current state of the lua memory (defined symbols and values) into the file LuaState.txt");
	REGISTER_COMMAND("lua_dump_coverage", LuaDumpCoverage, VF_NULL,
	                 "Dumps lua states");
	REGISTER_COMMAND("lua_garbagecollect", LuaGarbargeCollect, VF_NULL, "Forces a garbage collection of the lua state");

	// Publish the debugging mode as console variable
	m_cvar_script_debugger = REGISTER_INT_CB("lua_debugger", 0, VF_CHEAT,
	                                         "Enables the script debugger.\n"
	                                         "1 to trigger on breakpoints and errors\n"
	                                         "2 to only trigger on errors\n"
	                                         "Usage: lua_debugger [0/1/2]",
	                                         CScriptSystem::DebugModeChange);

	// Publish the debugging mode as console variable
	REGISTER_INT("lua_StopOnError", 0, VF_CHEAT, "Stops on error");
	m_cvar_script_coverage = REGISTER_INT_CB("lua_CodeCoverage", 0, VF_CHEAT, "Enables code coverage", CScriptSystem::CodeCoverageChange);

	// Ensure the debugger is in the correct mode
	EnableDebugger((ELuaDebugMode) m_cvar_script_debugger->GetIVal());

	// Register the command for reloading script files
	REGISTER_COMMAND("lua_reload_script", ReloadScriptCmd, VF_CHEAT, "Reloads given script files and their dependencies");

	//DumpStateToFile( "LuaState.txt" );

	//////////////////////////////////////////////////////////////////////////
	// Execute common lua file.
	//////////////////////////////////////////////////////////////////////////
	ExecuteFile("scripts/common.lua", true, false);

#if CAPTURE_REPLAY_LOG && defined(USE_GLOBAL_BUCKET_ALLOCATOR)
	gLuaAlloc.ReplayRegisterAddressRange("Lua Buckets");
#endif

#if LUA_REMOTE_DEBUG_ENABLED
	g_pLuaRemoteDebug = new CLuaRemoteDebug(this);
#endif

	//initvectortag(L);
	return L ? true : false;
}

void CScriptSystem::DebugModeChange(ICVar* cvar)
{
	// Update the mode of the debugger
	if (s_mpScriptSystem)
		s_mpScriptSystem->EnableDebugger((ELuaDebugMode) cvar->GetIVal());
}

void CScriptSystem::CodeCoverageChange(ICVar* cvar)
{
	// Update the mode of the debugger
	if (s_mpScriptSystem)
		s_mpScriptSystem->EnableCodeCoverage(cvar->GetIVal() != 0);
}

void CScriptSystem::PostInit()
{
	//////////////////////////////////////////////////////////////////////////
	// Register console vars.
	//////////////////////////////////////////////////////////////////////////
	if (gEnv->pConsole)
	{
		REGISTER_CVAR2("lua_stackonmalloc", &g_dumpStackOnAlloc, 0, VF_NULL, "Enables/disables logging of the called lua functions and respective callstacks, whenever a new lua object is instantiated.");
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::EnableDebugger(ELuaDebugMode eDebugMode)
{
#if CRY_PLATFORM_WINDOWS
	// Create the debugger if need be
	if (eDebugMode != eLDM_NoDebug && !m_pLuaDebugger)
	{
		m_pLuaDebugger = new CLUADbg(this);
		m_pLuaDebugger->EnableCodeCoverage(m_cvar_script_coverage->GetIVal() != 0);
		g_pLuaDebugger = m_pLuaDebugger;
	}
#endif

	// Set hooks
	if (eDebugMode == eLDM_FullDebug)
		// Enable
		lua_sethook(L, LuaDebugHook, LUA_MASKCALL | LUA_MASKLINE | LUA_MASKRET, 0);
	else
		// Disable
		lua_sethook(L, LuaDebugHook, 0, 0);

	// Clear the debugging call stack
	ExposedCallstackClear();

	// Error handler takes care of itself by checking the cvar
}

void CScriptSystem::EnableCodeCoverage(bool enable)
{
#if CRY_PLATFORM_WINDOWS
	if (g_pLuaDebugger)
		g_pLuaDebugger->EnableCodeCoverage(enable);
	// Error handler takes care of itself by checking the cvar
#endif
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::TraceScriptError(const char* file, int line, const char* errorStr)
{
	lua_Debug ar;

	// If in debug mode, try to enable debugger.
	if ((ELuaDebugMode) m_cvar_script_debugger->GetIVal() != eLDM_NoDebug)
	{
		if (lua_getstack(L, 1, &ar))
		{
			if (lua_getinfo(L, "lnS", &ar))
			{
#if LUA_REMOTE_DEBUG_ENABLED
				if (g_pLuaRemoteDebug && g_pLuaRemoteDebug->IsClientConnected())
				{
					g_pLuaRemoteDebug->OnScriptError(L, &ar, errorStr);
				}
				else
#endif
				{
					ShowDebugger(file, line, errorStr);
				}
			}
		}
	}
	else
	{
		LogStackTrace();
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::LogStackTrace()
{
	::DumpCallStack(L);
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
int CScriptSystem::ErrorHandler(lua_State* L)
{
	if (!lua_isstoredebuginfo(L))
		return 0; // ignore script errors if engine is running without game

	// Handle error
	lua_Debug ar;
	CScriptSystem* pThis = (CScriptSystem*)gEnv->pScriptSystem;

	memset(&ar, 0, sizeof(lua_Debug));

	const char* sErr = lua_tostring(L, 1);

	if (sErr)
	{
		ScriptWarning("[Lua Error] %s", sErr);
	}

	//////////////////////////////////////////////////////////////////////////
	// Print error callstack.
	//////////////////////////////////////////////////////////////////////////
	int level = 1;
	while (lua_getstack(L, level++, &ar))
	{
		lua_getinfo(L, "lnS", &ar);
		if (ar.name)
			CryLog("$6    > %s, (%s: %d)", ar.name, ar.short_src, ar.currentline);
		else
			CryLog("$6    > (null) (%s: %d)", ar.short_src, ar.currentline);
	}

	if (sErr)
	{
		ICVar* lua_StopOnError = gEnv->pConsole->GetCVar("lua_StopOnError");
		if (lua_StopOnError && lua_StopOnError->GetIVal() != 0)
		{
			ScriptWarning("![Lua Error] %s", sErr);
		}
	}

	pThis->TraceScriptError(ar.source, ar.currentline, sErr);

	return 0;
}

void CScriptSystem::RegisterErrorHandler()
{
	// Legacy approach
	/*
	   if(bDebugger)
	   {
	   //lua_register(L, LUA_ERRORMESSAGE, CScriptSystem::ErrorHandler );
	   //lua_setglobal(L, LUA_ERRORMESSAGE);
	   }
	   else
	   {
	   //lua_register(L, LUA_ERRORMESSAGE, CScriptSystem::ErrorHandler );

	   //lua_newuserdatabox(L, this);
	   //lua_pushcclosure(L, CScriptSystem::ErrorHandler, 1);
	   //lua_setglobal(L, LUA_ALERT);
	   //lua_pushcclosure(L, errorfb, 0);
	   //lua_setglobal(L, LUA_ERRORMESSAGE);
	   }
	 */

	// Register global error handler.
	// This just makes it available - when we call LUA, we insert it so it will be called
	if (!m_pErrorHandlerFunc)
	{
		lua_pushcfunction(L, CScriptSystem::ErrorHandler);
		m_pErrorHandlerFunc = (HSCRIPTFUNCTION)(INT_PTR)lua_ref(L, 1);
	}
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void CScriptSystem::CreateMetatables()
{
	m_pUserDataMetatable = CreateTable();
	m_pUserDataMetatable->AddRef();

	m_pPreCacheBufferTable = CreateTable();
	m_pPreCacheBufferTable->AddRef();

	/*
	   m_pUserDataMetatable->AddFunction( )

	   // Make Garbage collection for user data metatable.
	   lua_newuserdatabox(L, this);
	   lua_pushcclosure(L, CScriptSystem::GCTagHandler, 1);
	   lua_settagmethod(L, m_nGCTag, "gc");
	 */
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
IScriptTable* CScriptSystem::CreateTable(bool bEmpty)
{
	CScriptTable* pObj = AllocTable();
	if (!bEmpty)
	{
		pObj->CreateNew();
	}
	return pObj;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
bool CScriptSystem::_ExecuteFile(const char* sFileName, bool bRaiseError, IScriptTable* pEnv)
{
	CCryFile file;

	if (!file.Open(sFileName, "rb"))
	{
		if (bRaiseError)
			ScriptWarning("[Lua Error] Failed to load script file %s", sFileName);

		return false;
	}

	uint32 fileSize = file.GetLength();
	if (!fileSize)
		return false;

	std::vector<char> buffer(fileSize);

	if (file.ReadRaw(&buffer.front(), fileSize) == 0)
		return false;

	// Translate pak alias filenames
	char translatedBuf[_MAX_PATH + 1];
	const char* translated = gEnv->pCryPak->AdjustFileName(sFileName, translatedBuf, ICryPak::FLAGS_NO_FULL_PATH);

	stack_string fileName("@");
	fileName.append(translated);
	fileName.replace('\\', '/');

	CRY_DEFINE_ASSET_SCOPE("LUA", sFileName);

	return ExecuteBuffer(&buffer.front(), fileSize, fileName.c_str(), pEnv);
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
bool CScriptSystem::ExecuteFile(const char* sFileName, bool bRaiseError, bool bForceReload, IScriptTable* pEnv)
{
	if (strlen(sFileName) <= 0)
		return false;

	LOADING_TIME_PROFILE_SECTION_ARGS(sFileName);

	INDENT_LOG_DURING_SCOPE(true, "Executing file '%s' (raiseErrors=%d%s)", sFileName, bRaiseError, bForceReload ? ", force reload" : "");

	if (bForceReload)
		m_forceReloadCount++;

	char sTemp[_MAX_PATH];
	char lowerName[_MAX_PATH];
	cry_strcpy(sTemp, FormatPath(lowerName, sFileName));
	//ScriptFileListItor itor = std::find(m_dqLoadedFiles.begin(), m_dqLoadedFiles.end(), sTemp.c_str());
	ScriptFileListItor itor = m_dqLoadedFiles.find(CONST_TEMP_STRING(sTemp));
	if (itor == m_dqLoadedFiles.end() || bForceReload || m_forceReloadCount > 0)
	{
#if CRY_PLATFORM_WINDOWS
		if (g_pLuaDebugger)
		{
			string name = sFileName;
			char sTempStr[_MAX_PATH];
			assert(name.size() < _MAX_PATH);
			for (uint32 i = 0; i < name.size(); ++i)
				sTempStr[i] = tolower(name[i]);
			sTempStr[name.size()] = 0;
			g_pLuaDebugger->GetCoverage()->ResetFile(sTempStr);
		}
#endif
		if (!_ExecuteFile(sTemp, bRaiseError, pEnv))
		{
			if (itor != m_dqLoadedFiles.end())
				RemoveFileFromList(itor);
			if (bForceReload)
				m_forceReloadCount--;
			return false;
		}
		if (itor == m_dqLoadedFiles.end())
			AddFileToList(sTemp);
	}
	if (bForceReload)
		m_forceReloadCount--;
	return true;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void CScriptSystem::UnloadScript(const char* sFileName)
{
	if (strlen(sFileName) <= 0)
		return;

	char lowerName[_MAX_PATH];
	const char* sTemp = FormatPath(lowerName, sFileName);
	//ScriptFileListItor itor = std::find(m_dqLoadedFiles.begin(), m_dqLoadedFiles.end(), sTemp.c_str());
	ScriptFileListItor itor = m_dqLoadedFiles.find(CONST_TEMP_STRING(sTemp));
	if (itor != m_dqLoadedFiles.end())
	{
		RemoveFileFromList(itor);
	}
}
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void CScriptSystem::UnloadScripts()
{
	m_dqLoadedFiles.clear();
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
bool CScriptSystem::ReloadScripts()
{
	ScriptFileListItor itor;
	itor = m_dqLoadedFiles.begin();
	while (itor != m_dqLoadedFiles.end())
	{
		ReloadScript(itor->c_str(), true);
		++itor;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
bool CScriptSystem::ReloadScript(const char* sFileName, bool bRaiseError)
{
	return ExecuteFile(sFileName, bRaiseError, gEnv->pSystem->IsDevMode());
}

void CScriptSystem::DumpLoadedScripts()
{
	ScriptFileListItor itor;
	itor = m_dqLoadedFiles.begin();
	while (itor != m_dqLoadedFiles.end())
	{
		CryLogAlways("%s", itor->c_str());
		++itor;
	}
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void CScriptSystem::AddFileToList(const char* sName)
{
	m_dqLoadedFiles.insert(sName);
}

void CScriptSystem::RemoveFileFromList(const ScriptFileListItor& itor)
{
	m_dqLoadedFiles.erase(itor);
}

/*
   void CScriptSystem::GetScriptHashFunction( IScriptTable &Current, unsigned int &dwHash)
   {
   unsigned int *pCode=0;
   int iSize=0;

   if(pCurrent.GetCurrentFuncData(pCode,iSize))						// function ?
   {
   if(pCode)                                            // lua function ?
   GetScriptHashFunction(pCode,iSize,dwHash);
   }
   }
 */

void CScriptSystem::GetScriptHash(const char* sPath, const char* szKey, unsigned int& dwHash)
{
	//	IScriptTable *pCurrent;

	//	GetGlobalValue(szKey,pCurrent);

	//	if(!pCurrent)		// is not a table
	//	{
	//	}
	/*
	   else if(lua_isfunction(L, -1))
	   {
	   GetScriptHashFunction(*pCurrent,dwHash);

	   return;
	   }
	   else
	   {
	   lua_pop(L, 1);
	   return;
	   }
	 */
	/*
	   pCurrent->BeginIteration();

	   while(pCurrent->MoveNext())
	   {
	   char *szKeyName;

	   if(!pCurrent->GetCurrentKey(szKeyName))
	   szKeyName="NO";

	   ScriptVarType type=pCurrent->GetCurrentType();

	   if(type==svtObject)		// svtNull,svtFunction,svtString,svtNumber,svtUserData,svtObject
	   {
	   void *pVis;

	   pCurrent->GetCurrentPtr(pVis);

	   gEnv->pLog->Log("  table '%s/%s'",sPath.c_str(),szKeyName);

	   if(setVisited.count(pVis)!=0)
	   {
	   gEnv->pLog->Log("    .. already processed ..");
	   continue;
	   }

	   setVisited.insert(pVis);

	   {
	   IScriptTable *pNewObject = m_pScriptSystem->CreateEmptyObject();

	   pCurrent->GetCurrent(pNewObject);

	   Debug_Full_recursive(pNewObject,sPath+string("/")+szKeyName,setVisited);

	   pNewObject->Release();
	   }
	   }
	   else if(type==svtFunction)		// svtNull,svtFunction,svtString,svtNumber,svtUserData,svtObject
	   {
	   GetScriptHashFunction(*pCurrent,dwHash);
	   }
	   }

	   pCurrent->EndIteration();
	 */
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
bool CScriptSystem::ExecuteBuffer(const char* sBuffer, size_t nSize, const char* sBufferDescription, IScriptTable* pEnv)
{
	int status;

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Lua LoadScript");
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_ScriptCall, 0, "%s", sBufferDescription);

	{
		status = luaL_loadbuffer(L, sBuffer, nSize, sBufferDescription);
	}

	if (status == 0)
	{
		// parse OK?
		int base = lua_gettop(L);  // function index.
		lua_getref(L, (int)(INT_PTR)m_pErrorHandlerFunc);
		lua_insert(L, base);  // put it under chunk and args

		if (pEnv)
		{
			PushTable(pEnv);
			lua_setfenv(L, -2);
		}

		status = lua_pcall(L, 0, LUA_MULTRET, base); // call main
		lua_remove(L, base);                         // remove error handler function.
	}
	if (status != 0)
	{
		const char* sErr = lua_tostring(L, -1);
		if (sBufferDescription && strlen(sBufferDescription) != 0)
			GetISystem()->Warning(VALIDATOR_MODULE_SCRIPTSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE | VALIDATOR_FLAG_SCRIPT, sBufferDescription,
			                      "[Lua Error] Failed to execute file %s: %s", sBufferDescription, sErr);
		else
			ScriptWarning("[Lua Error] Error executing lua %s", sErr);
		assert(GetStackSize() > 0);
		lua_pop(L, 1);
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void CScriptSystem::Release()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
int CScriptSystem::BeginCall(HSCRIPTFUNCTION hFunc)
{
	assert(hFunc != 0);
	if (!hFunc)
		return 0;

	lua_getref(L, (int)(INT_PTR)hFunc);
	if (!lua_isfunction(L, -1))
	{
#if defined(__GNUC__)
		ScriptWarning("[CScriptSystem::BeginCall] Function Ptr:%d not found", (int)(INT_PTR)hFunc);
#else
		ScriptWarning("[CScriptSystem::BeginCall] Function Ptr:%d not found", hFunc);
#endif
		m_nTempArg = -1;
		lua_pop(L, 1);
		return 0;
	}
	m_nTempArg = 0;

	return 1;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
int CScriptSystem::BeginCall(const char* sTableName, const char* sFuncName)
{
	lua_getglobal(L, sTableName);

	if (!lua_istable(L, -1))
	{
		ScriptWarning("[CScriptSystem::BeginCall] Tried to call %s:%s(), Table %s not found (check for syntax errors or if the file wasn't loaded)", sTableName, sFuncName, sTableName);
		m_nTempArg = -1;
		lua_pop(L, 1);
		return 0;
	}

	lua_pushstring(L, sFuncName);
	lua_gettable(L, -2);
	lua_remove(L, -2);  // Remove table global.
	m_nTempArg = 0;

	if (!lua_isfunction(L, -1))
	{
		ScriptWarning("[CScriptSystem::BeginCall] Function %s:%s not found(check for syntax errors or if the file wasn't loaded)", sTableName, sFuncName);
		m_nTempArg = -1;
		lua_pop(L, 1);
		return 0;
	}

	return 1;
}

//////////////////////////////////////////////////////////////////////
int CScriptSystem::BeginCall(IScriptTable* pTable, const char* sFuncName)
{
	PushTable(pTable);

	lua_pushstring(L, sFuncName);
	lua_gettable(L, -2);
	lua_remove(L, -2);  // Remove table global.
	m_nTempArg = 0;

	if (!lua_isfunction(L, -1))
	{
		ScriptWarning("[CScriptSystem::BeginCall] Function %s not found in the table", sFuncName);
		m_nTempArg = -1;
		lua_pop(L, 1);
		return 0;
	}

	return 1;
}

//////////////////////////////////////////////////////////////////////////
int CScriptSystem::BeginCall(const char* sFuncName)
{
	if (L)
	{
		lua_getglobal(L, sFuncName);
		m_nTempArg = 0;

		if (!lua_isfunction(L, -1))
		{
			ScriptWarning("[CScriptSystem::BeginCall] Function %s not found(check for syntax errors or if the file wasn't loaded)", sFuncName);
			m_nTempArg = -1;
			lua_pop(L, 1);
			return 0;
		}

		return 1;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
HSCRIPTFUNCTION CScriptSystem::GetFunctionPtr(const char* sFuncName)
{
	CHECK_STACK(L);
	HSCRIPTFUNCTION func;
	lua_getglobal(L, sFuncName);
	if (lua_isnil(L, -1) || (!lua_isfunction(L, -1)))
	{
		lua_pop(L, 1);
		return NULL;
	}
	func = (HSCRIPTFUNCTION)(INT_PTR)lua_ref(L, 1);

	return func;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
HSCRIPTFUNCTION CScriptSystem::GetFunctionPtr(const char* sTableName, const char* sFuncName)
{
	CHECK_STACK(L);
	HSCRIPTFUNCTION func;
	lua_getglobal(L, sTableName);
	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		return 0;
	}
	lua_pushstring(L, sFuncName);
	lua_gettable(L, -2);
	lua_remove(L, -2);  // Remove table global.
	if (lua_isnil(L, -1) || (!lua_isfunction(L, -1)))
	{
		lua_pop(L, 1);
		return FALSE;
	}
	func = (HSCRIPTFUNCTION)(INT_PTR)lua_ref(L, 1);
	return func;
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::PushAny(const ScriptAnyValue& var)
{
	switch (var.GetType())
	{
	case EScriptAnyType::Any:
	case EScriptAnyType::Nil:
		lua_pushnil(L);
		break;
		;
	case EScriptAnyType::Boolean:
		lua_pushboolean(L, var.GetBool());
		break;
		;
	case EScriptAnyType::Handle:
		lua_pushlightuserdata(L, var.GetScriptHandle().ptr);
		break;
	case EScriptAnyType::Number:
		lua_pushnumber(L, var.GetNumber());
		break;
	case EScriptAnyType::String:
		lua_pushstring(L, var.GetString());
		break;
	case EScriptAnyType::Table:
		if (var.GetScriptTable())
			PushTable(var.GetScriptTable());
		else
			lua_pushnil(L);
		break;
	case EScriptAnyType::Function:
		lua_getref(L, (int)(INT_PTR)var.GetScriptFunction());
		assert(lua_type(L, -1) == LUA_TFUNCTION);
		break;
	case EScriptAnyType::UserData:
		lua_getref(L, var.GetUserData().nRef);
		break;
	case EScriptAnyType::Vector:
		PushVec3(var.GetVector());
		break;
	default:
		// Must handle everything.
		assert(0);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CScriptSystem::ToAny(ScriptAnyValue& var, int index)
{
	if (!lua_gettop(L))
		return false;

	CHECK_STACK(L);

	if (var.GetType() == EScriptAnyType::Any)
	{
		switch (lua_type(L, index))
		{
		case LUA_TNIL:
			var.SetNil();
			break;
		case LUA_TBOOLEAN:
			var.SetBool(lua_toboolean(L, index) != 0);
			break;
		case LUA_TLIGHTUSERDATA:
			var.SetScriptHandle(const_cast<void*>(lua_topointer(L, index)));
			break;
		case LUA_TNUMBER:
			var.SetNumber(static_cast<float>(lua_tonumber(L, index)));
			break;
		case LUA_TSTRING:
			var.SetString(lua_tostring(L, index));
			break;
		case LUA_TTABLE:
		case LUA_TUSERDATA:
			if (var.GetType() != EScriptAnyType::Table || !var.GetScriptTable())
			{
				var.SetScriptTable(AllocTable());
				var.GetScriptTable()->AddRef();
			}
			lua_pushvalue(L, index);
			AttachTable(var.GetScriptTable());
			break;
		case LUA_TFUNCTION:
			{
				// Make reference to function.
				lua_pushvalue(L, index);
				var.SetScriptFunction((HSCRIPTFUNCTION)(INT_PTR)lua_ref(L, 1));
			}
			break;
		case LUA_TTHREAD:
		default:
			return false;
		}
		return true;
	}
	else
	{
		bool res = false;
		switch (lua_type(L, index))
		{
		case LUA_TNIL:
			if (var.GetType() == EScriptAnyType::Nil)
				res = true;
			break;
		case LUA_TBOOLEAN:
			if (var.GetType() == EScriptAnyType::Boolean)
			{
				var.SetBool(lua_toboolean(L, index) != 0);
				res = true;
			}
			break;
		case LUA_TLIGHTUSERDATA:
			if (var.GetType() == EScriptAnyType::Handle)
			{
				var.SetScriptHandle(const_cast<void*>(lua_topointer(L, index)));
				res = true;
			}
			break;
		case LUA_TNUMBER:
			if (var.GetType() == EScriptAnyType::Number)
			{
				var.SetNumber(static_cast<float>(lua_tonumber(L, index)));
				res = true;
			}
			else if (var.GetType() == EScriptAnyType::Boolean)
			{
				var.SetBool(lua_tonumber(L, index) != 0);
				res = true;
			}
			break;
		case LUA_TSTRING:
			if (var.GetType() == EScriptAnyType::String)
			{
				var.SetString(lua_tostring(L, index));
				res = true;
			}
			break;
		case LUA_TTABLE:
			if (var.GetType() == EScriptAnyType::Table)
			{
				if (!var.GetScriptTable())
				{
					var.SetScriptTable(AllocTable());
					var.GetScriptTable()->AddRef();
				}
				lua_pushvalue(L, index);
				AttachTable(var.GetScriptTable());
				res = true;
			}
			else if (var.GetType() == EScriptAnyType::Vector)
			{
				Vec3 v(0, 0, 0);
				if (res = ToVec3(v, index))
				{
					var.SetVector(v);
				}
			}
			break;
		case LUA_TUSERDATA:
			if (var.GetType() == EScriptAnyType::Table)
			{
				if (!var.GetScriptTable())
				{
					var.SetScriptTable(AllocTable());
					var.GetScriptTable()->AddRef();
				}
				lua_pushvalue(L, index);
				AttachTable(var.GetScriptTable());
				res = true;
			}
			break;
		case LUA_TFUNCTION:
			if (var.GetType() == EScriptAnyType::Function)
			{
				// Make reference to function.
				lua_pushvalue(L, index);
				var.SetScriptFunction((HSCRIPTFUNCTION)(INT_PTR)lua_ref(L, 1));
				res = true;
			}
			break;
		case LUA_TTHREAD:
			break;
		}
		return res;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CScriptSystem::PopAny(ScriptAnyValue& var)
{
	bool res = ToAny(var, -1);
	lua_pop(L, 1);
	return res;
}

/*
   #include <signal.h>
   static void cry_lstop (lua_State *l, lua_Debug *ar) {
   (void)ar;  // unused arg.
   lua_sethook(l, NULL, 0, 0);
   luaL_error(l, "interrupted!");
   }
   static void cry_laction (int i)
   {
   CScriptSystem *pThis = (CScriptSystem *)gEnv->pScriptSystem;
   lua_State *L = pThis->GetLuaState();
   signal(i, SIG_DFL); // if another SIGINT happens before lstop, terminate process (default action)
   lua_sethook(L, cry_lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
   }
 */

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
bool CScriptSystem::EndCallN(int nReturns)
{
	if (m_nTempArg < 0 || !L)
		return false;

	int base = lua_gettop(L) - m_nTempArg;  // function index.
	lua_getref(L, (int)(INT_PTR)m_pErrorHandlerFunc);
	lua_insert(L, base);  // put it under chunk and args

	//signal(SIGINT, cry_laction);
	int status = lua_pcall(L, m_nTempArg, nReturns, base);
	//signal(SIGINT, SIG_DFL);
	lua_remove(L, base);  // remove error handler function.

	return status == 0;
}

//////////////////////////////////////////////////////////////////////////
bool CScriptSystem::EndCall()
{
	return EndCallN(0);
}

//////////////////////////////////////////////////////////////////////////
bool CScriptSystem::EndCallAny(ScriptAnyValue& any)
{
	//CHECK_STACK(L);
	if (!EndCallN(1))
		return false;
	return PopAny(any);
}

//////////////////////////////////////////////////////////////////////
bool CScriptSystem::EndCallAnyN(int n, ScriptAnyValue* anys)
{
	if (!EndCallN(n))
		return false;
	for (int i = 0; i < n; i++)
	{
		if (!PopAny(anys[i]))
			return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////
void CScriptSystem::PushFuncParamAny(const ScriptAnyValue& any)
{
	if (m_nTempArg == -1)
		return;
	PushAny(any);
	m_nTempArg++;
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::SetGlobalAny(const char* sKey, const ScriptAnyValue& any)
{
	CHECK_STACK(L);
	PushAny(any);
	lua_setglobal(L, sKey);
}

//////////////////////////////////////////////////////////////////////////
bool CScriptSystem::GetRecursiveAny(IScriptTable* pTable, const char* sKey, ScriptAnyValue& any)
{
	char key1[256];
	char key2[256];

	const char* const sep = strchr(sKey, '.');
	if (sep)
	{
		cry_strcpy(key1, sKey, (size_t)(sep - sKey));
		cry_strcpy(key2, sep + 1);
	}
	else
	{
		cry_strcpy(key1, sKey);
		key2[0] = 0;
	}

	ScriptAnyValue localAny;
	if (!pTable->GetValueAny(key1, localAny))
		return false;

	if (localAny.GetType() == EScriptAnyType::Function && NULL == sep)
	{
		any = localAny;
		return true;
	}
	else if (localAny.GetType() == EScriptAnyType::Table && NULL != sep)
	{
		return GetRecursiveAny(localAny.GetScriptTable(), key2, any);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CScriptSystem::GetGlobalAny(const char* sKey, ScriptAnyValue& any)
{
	CHECK_STACK(L);
	const char* sep = strchr(sKey, '.');
	if (sep)
	{
		ScriptAnyValue globalAny;
		char key1[256];
		cry_strcpy(key1, sKey);
		key1[sep - sKey] = 0;
		GetGlobalAny(key1, globalAny);
		if (globalAny.GetType() == EScriptAnyType::Table)
		{
			return GetRecursiveAny(globalAny.GetScriptTable(), sep + 1, any);
		}
		return false;
	}

	lua_getglobal(L, sKey);
	if (!PopAny(any))
	{
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void CScriptSystem::ForceGarbageCollection()
{
	int beforeUsage = lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0);

	// Do a full garbage collection cycle.
	lua_gc(L, LUA_GCCOLLECT, 0);

	int fracUsage = lua_gc(L, LUA_GCCOUNTB, 0);
	int totalUsage = lua_gc(L, LUA_GCCOUNT, 0) * 1024 + fracUsage;

#if USE_RAW_LUA_ALLOCS
	// Nothing to do.
	#ifdef USE_GLOBAL_BUCKET_ALLOCATOR
	gLuaAlloc.cleanup();
	#endif
#endif

	CryComment("Lua garbage collection %i -> %i", beforeUsage, totalUsage);

	/*char sTemp[200];
	   lua_StateStats lss;
	   lua_getstatestats(L,&lss);
	   cry_sprintf(sTemp,"protos=%d closures=%d tables=%d udata=%d strings=%d\n",lss.nProto,lss.nClosure,lss.nHash,lss.nUdata,lss.nString);
	   OutputDebugString("BEFORE GC STATS :");
	   OutputDebugString(sTemp);*/

	//lua_setgcthreshold(L, 0);

	/*lua_getstatestats(L,&lss);
	   cry_sprintf(sTemp,"protos=%d closures=%d tables=%d udata=%d strings=%d\n",lss.nProto,lss.nClosure,lss.nHash,lss.nUdata,lss.nString);
	   OutputDebugString("AFTER GC STATS :");
	   OutputDebugString(sTemp);*/
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
int CScriptSystem::GetCGCount()
{
	return lua_getgccount(L);
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void CScriptSystem::SetGCThreshhold(int nKb)
{
	//lua_setgcthreshold(L, nKb);
}

IScriptTable* CScriptSystem::CreateUserData(void* ptr, size_t size)   //AMD Port
{
	CHECK_STACK(L);

	void* nptr = lua_newuserdata(L, size);
	memcpy(nptr, ptr, size);
	CScriptTable* pNewTbl = new CScriptTable();
	pNewTbl->Attach();

	return pNewTbl;
}

//////////////////////////////////////////////////////////////////////
HBREAKPOINT CScriptSystem::AddBreakPoint(const char* sFile, int nLineNumber)
{
#if CRY_PLATFORM_WINDOWS
	if (m_pLuaDebugger)
		m_pLuaDebugger->AddBreakPoint(sFile, nLineNumber);
#endif
	return 0;
}

HSCRIPTFUNCTION CScriptSystem::AddFuncRef(HSCRIPTFUNCTION f)
{
	CHECK_STACK(L);

	if (f)
	{
		int ret;
		lua_getref(L, (int)(INT_PTR)f);
		assert(lua_type(L, -1) == LUA_TFUNCTION);
		ret = lua_ref(L, 1);
		if (ret != LUA_REFNIL)
		{
			return (HSCRIPTFUNCTION)(EXPAND_PTR)ret;
		}
		assert(0);
	}
	return 0;
}

bool CScriptSystem::CompareFuncRef(HSCRIPTFUNCTION f1, HSCRIPTFUNCTION f2)
{
	CHECK_STACK(L);
	if (f1 == f2)
		return true;

	lua_getref(L, (int)(INT_PTR)f1);
	assert(lua_type(L, -1) == LUA_TFUNCTION);
	const void* f1p = lua_topointer(L, -1);
	lua_pop(L, 1);
	lua_getref(L, (int)(INT_PTR)f2);
	assert(lua_type(L, -1) == LUA_TFUNCTION);
	const void* f2p = lua_topointer(L, -1);
	lua_pop(L, 1);
	if (f1p == f2p)
		return true;
	return false;
}

void CScriptSystem::ReleaseFunc(HSCRIPTFUNCTION f)
{
	CHECK_STACK(L);

	if (f)
	{
#ifdef _DEBUG
		lua_getref(L, (int)(INT_PTR)f);
		assert(lua_type(L, -1) == LUA_TFUNCTION);
		lua_pop(L, 1);
#endif
		lua_unref(L, (int)(INT_PTR)f);
	}
}

ScriptAnyValue CScriptSystem::CloneAny(const ScriptAnyValue& any)
{
	CHECK_STACK(L);

	ScriptAnyValue result(any.GetType());

	switch (any.GetType())
	{
	case EScriptAnyType::Any:
	case EScriptAnyType::Nil:
		// TODO_janh
		break;

	case EScriptAnyType::Boolean:
		result.SetBool(any.GetBool());
		break;
	case EScriptAnyType::Number:
		result.SetNumber(any.GetNumber());
		break;
	case EScriptAnyType::Handle:
		result.SetScriptHandle(any.GetScriptHandle());
		break;
	case EScriptAnyType::String:
		result.SetString(any.GetString());
		break;
	case EScriptAnyType::Table:
		if (any.GetScriptTable())
		{
			result.SetScriptTable(any.GetScriptTable());
			result.GetScriptTable()->Release();
		}
		break;
	case EScriptAnyType::Function:
		lua_getref(L, (int)(INT_PTR)any.GetScriptFunction());
		assert(lua_type(L, -1) == LUA_TFUNCTION);
		result.SetScriptFunction((HSCRIPTFUNCTION)(EXPAND_PTR)lua_ref(L, 1));
		break;
	case EScriptAnyType::UserData:
		{
			ScriptUserData ud;
			ud.ptr = any.GetUserData().ptr;
			lua_getref(L, any.GetUserData().nRef);
			ud.nRef = lua_ref(L, 1);
			result.SetUserData(ud);
			break;
		}
	case EScriptAnyType::Vector:
		result.SetVector(any.GetVector());
		break;
	default:
		assert(0);
	}

	return result;
}

void CScriptSystem::ReleaseAny(const ScriptAnyValue& any)
{
	CHECK_STACK(L);

	switch (any.GetType())
	{
	case EScriptAnyType::Any:
	case EScriptAnyType::Nil:
	case EScriptAnyType::Boolean:
	case EScriptAnyType::Number:
	case EScriptAnyType::Handle:
	case EScriptAnyType::String:
	case EScriptAnyType::Vector:
		break;
	case EScriptAnyType::Table:
	case EScriptAnyType::Function:
		// will be freed on delete
		break;
	case EScriptAnyType::UserData:
		lua_unref(L, any.GetUserData().nRef);
		break;
	default:
		assert(0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::PushTable(IScriptTable* pTable)
{
	((CScriptTable*)pTable)->PushRef();
};

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::AttachTable(IScriptTable* pTable)
{
	((CScriptTable*)pTable)->Attach();
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::PushVec3(const Vec3& vec)
{
	lua_newtable(L);
	lua_pushlstring(L, "x", 1);
	lua_pushnumber(L, vec.x);
	lua_settable(L, -3);
	lua_pushlstring(L, "y", 1);
	lua_pushnumber(L, vec.y);
	lua_settable(L, -3);
	lua_pushlstring(L, "z", 1);
	lua_pushnumber(L, vec.z);
	lua_settable(L, -3);
}

//////////////////////////////////////////////////////////////////////////
bool CScriptSystem::ToVec3(Vec3& vec, int tableIndex)
{
	CHECK_STACK(L);

	if (tableIndex < 0)
	{
		tableIndex = lua_gettop(L) + tableIndex + 1;
	}

	if (lua_type(L, tableIndex) != LUA_TTABLE)
	{
		return false;
	}

	//int num = luaL_getn(L,index);
	//if (num != 3)
	//return false;

	float x, y, z;
	lua_pushlstring(L, "x", 1);
	lua_gettable(L, tableIndex);
	if (!lua_isnumber(L, -1))
	{
		lua_pop(L, 1); // pop x value.
		//////////////////////////////////////////////////////////////////////////
		// Try an indexed table.
		lua_pushnumber(L, 1);
		lua_gettable(L, tableIndex);
		if (!lua_isnumber(L, -1))
		{
			lua_pop(L, 1); // pop value.
			return false;
		}
		x = lua_tonumber(L, -1);
		lua_pushnumber(L, 2);
		lua_gettable(L, tableIndex);
		if (!lua_isnumber(L, -1))
		{
			lua_pop(L, 2); // pop value.
			return false;
		}
		y = lua_tonumber(L, -1);
		lua_pushnumber(L, 3);
		lua_gettable(L, tableIndex);
		if (!lua_isnumber(L, -1))
		{
			lua_pop(L, 3); // pop value.
			return false;
		}
		z = lua_tonumber(L, -1);
		lua_pop(L, 3); // pop value.

		vec.x = x;
		vec.y = y;
		vec.z = z;
		return true;
		//////////////////////////////////////////////////////////////////////////
	}
	x = lua_tonumber(L, -1);
	lua_pop(L, 1); // pop value.

	lua_pushlstring(L, "y", 1);
	lua_gettable(L, tableIndex);
	if (!lua_isnumber(L, -1))
	{
		lua_pop(L, 1); // pop table.
		return false;
	}
	y = lua_tonumber(L, -1);
	lua_pop(L, 1); // pop value.

	lua_pushlstring(L, "z", 1);
	lua_gettable(L, tableIndex);
	if (!lua_isnumber(L, -1))
	{
		lua_pop(L, 1); // pop table.
		return false;
	}
	z = lua_tonumber(L, -1);
	lua_pop(L, 1); // pop value.

	vec.x = x;
	vec.y = y;
	vec.z = z;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::ShowDebugger(const char* pszSourceFile, int iLine, const char* pszReason)
{
#if CRY_PLATFORM_WINDOWS
	// Create the debugger if need be
	if (!m_pLuaDebugger)
		EnableDebugger(eLDM_OnlyErrors);

	if (m_pLuaDebugger)
	{
		m_pLuaDebugger->InvokeDebugger(pszSourceFile, iLine, pszReason);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CScriptSystem::Update()
{
	CRY_PROFILE_REGION(PROFILE_SCRIPT, "ScriptSystem: Update");
	CRYPROFILE_SCOPE_PROFILE_MARKER("ScriptSystem: Update");

	//CryGetScr
	//L->l_G->totalbytes =
	ITimer* pTimer = gEnv->pTimer;
	CTimeValue nCurTime = pTimer->GetFrameStartTime();

	// Enable debugger if needed.
	// Some code is executed even if the debugMode doesn't change, including clearing the exposed stack
	ELuaDebugMode debugMode = (ELuaDebugMode) m_cvar_script_debugger->GetIVal();
	EnableDebugger(debugMode);

	// Might need to check for new lua code needing hooks

	float currTime = pTimer->GetCurrTime();
	float frameTime = pTimer->GetFrameTime();

	IScriptSystem* pScriptSystem = m_pSystem->GetIScriptSystem();

	// Set global time variable into the script.
	pScriptSystem->SetGlobalValue("_time", currTime);
	pScriptSystem->SetGlobalValue("_frametime", frameTime);

	{
		int aiTicks = 0;

		IAISystem* pAISystem = gEnv->pAISystem;

		if (pAISystem)
			aiTicks = pAISystem->GetAITickCount();
		pScriptSystem->SetGlobalValue("_aitick", aiTicks);
	}

	//TRACE("GC DELTA %d",m_pScriptSystem->GetCGCount()-nStartGC);
	//int nStartGC = pScriptSystem->GetCGCount();

	bool bKickIn = false;                             // Invoke Gargabe Collector

	if (currTime - m_lastGCTime > m_fGCFreq)  // g_GC_Frequence->GetIVal())
		bKickIn = true;

	int nGCCount = pScriptSystem->GetCGCount();

	bool bNoLuaGC = false;

	if (nGCCount - m_nLastGCCount > 2000 && !bNoLuaGC)   //
		bKickIn = true;

	//if(bKickIn)
	{
		CRY_PROFILE_REGION(PROFILE_SCRIPT, "Lua GC");

		//CryLog( "Lua GC=%d",GetCGCount() );

		//float fTimeBefore=pTimer->GetAsyncCurTime()*1000;

		// Do a full garbage collection cycle.
		//pScriptSystem->ForceGarbageCollection();

		// Do incremental Garbage Collection
		lua_gc(L, LUA_GCSTEP, (PER_FRAME_LUA_GC_STEP));

		m_nLastGCCount = pScriptSystem->GetCGCount();
		m_lastGCTime = currTime;
		//float fTimeAfter=pTimer->GetAsyncCurTime()*1000;
		//CryLog("--[after coll]GC DELTA %d ",pScriptSystem->GetCGCount()-nGCCount);
		//TRACE("--[after coll]GC DELTA %d [time =%f]",m_pScriptSystem->GetCGCount()-nStartGC,fTimeAfter-fTimeBefore);
	}

	m_pScriptTimerMgr->Update(nCurTime.GetMilliSecondsAsInt64());
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::SetGCFrequency(const float fRate)
{
	if (fRate >= 0)
		m_fGCFreq = fRate;
	else if (g_nPrecaution == 0)
	{
		g_nPrecaution = cry_random(1, 3); // lets get nasty.
	}
}

void CScriptSystem::SetEnvironment(HSCRIPTFUNCTION scriptFunction, IScriptTable* pEnv)
{
	CHECK_STACK(L);

	lua_getref(L, (int)(INT_PTR)scriptFunction);
	if (!lua_isfunction(L, -1))
	{
#if defined(__GNUC__)
		ScriptWarning("[CScriptSystem::SetEnvironment] Function %d not found", (int)(INT_PTR)scriptFunction);
#else
		ScriptWarning("[CScriptSystem::SetEnvironment] Function %d not found", scriptFunction);
#endif
	}

	PushTable(pEnv);
	lua_setfenv(L, -2);
	lua_pop(L, 1);
}

IScriptTable* CScriptSystem::GetEnvironment(HSCRIPTFUNCTION scriptFunction)
{
	CHECK_STACK(L);

	lua_getref(L, (int)(INT_PTR)scriptFunction);

	if (!lua_isfunction(L, -1))
	{
#if defined(__GNUC__)
		ScriptWarning("[CScriptSystem::SetEnvironment] Function %d not found", (int)(INT_PTR)scriptFunction);
#else
		ScriptWarning("[CScriptSystem::SetEnvironment] Function %d not found", scriptFunction);
#endif
	}
	IScriptTable* env = 0;
	lua_getfenv(L, -1);
	if (lua_istable(L, -1))
	{
		// The method AttachTable will cause an element to be poped from the lua stack.
		// Therefore, we need to pop only one element instead of two if we enter this
		// block.
		AttachTable(env = CreateTable(true));
		lua_pop(L, 1);
	}
	else
	{
		lua_pop(L, 2);
	}
	return env;
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::RaiseError(const char* format, ...)
{
	va_list arglist;
	char sBuf[2048];
	int nCurrentLine = 0;
	const char* sSourceFile = "undefined";

	va_start(arglist, format);
	cry_vsprintf(sBuf, format, arglist);
	va_end(arglist);

	ScriptWarning("[Lua Error] %s", sBuf);

	TraceScriptError(sSourceFile, nCurrentLine, sBuf);
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::LoadScriptedSurfaceTypes(const char* sFolder, bool bReload)
{
	m_stdScriptBinds.LoadScriptedSurfaceTypes(sFolder, bReload);
}

//////////////////////////////////////////////////////////////////////////
void* CScriptSystem::Allocate(size_t sz)
{
#if USE_RAW_LUA_ALLOCS
	_LuaAlloc(sz);
#else
	void* ret = gLuaAlloc.alloc(sz);
	#ifndef _RELEASE
	if (!ret)
	{
		CryFatalError("Lua Allocator has run out of memory.");
	}
	#endif
	return ret;
#endif

}

size_t CScriptSystem::Deallocate(void* ptr)
{
#if USE_RAW_LUA_ALLOCS
	_LuaFree(ptr);
#else
	return gLuaAlloc.dealloc(ptr);
#endif
}

//////////////////////////////////////////////////////////////////////////
int CScriptSystem::GetStackSize() const
{
	return lua_gettop(L);
}

//////////////////////////////////////////////////////////////////////////
uint32 CScriptSystem::GetScriptAllocSize()
{
#if USE_RAW_LUA_ALLOCS
	return 0;
#else
	return gLuaAlloc.get_alloc_size();
#endif
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::GetMemoryStatistics(ICrySizer* pSizer) const
{
	{
		SIZER_COMPONENT_NAME(pSizer, "Self");
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_pScriptTimerMgr);
		pSizer->AddObject(m_dqLoadedFiles);
		pSizer->AddObject(m_vecPreCached);
	}

#ifndef _LIB // Only when compiling as dynamic library
	{
		SIZER_COMPONENT_NAME(pSizer, "Strings");
		pSizer->AddObject((this + 1), string::_usedMemory(0));
	}
	{
		SIZER_COMPONENT_NAME(pSizer, "STL Allocator Waste");
		CryModuleMemoryInfo meminfo;
		ZeroStruct(meminfo);
		CryGetMemoryInfoForModule(&meminfo);
		pSizer->AddObject((this + 2), (size_t)meminfo.STL_wasted);
	}
#endif
	{
#if USE_RAW_LUA_ALLOCS
		// Nothing to do.
#else
		SIZER_COMPONENT_NAME(pSizer, "Lua");
	#ifdef _LIB
		pSizer->AddObject(&gLuaAlloc, gLuaAlloc.get_alloc_size());
	#else
		pSizer->AddObject(&gLuaAlloc, gLuaAlloc.get_alloc_size() + gLuaAlloc.get_wasted_in_blocks() + gLuaAlloc.get_wasted_in_allocation());
	#endif
#endif // USE_RAW_LUA_ALLOCS
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
		ForceGarbageCollection();
		break;

	case ESYSTEM_EVENT_LEVEL_LOAD_START:
		ForceGarbageCollection();
		break;
	}

#if USE_RAW_LUA_ALLOCS
	// Nothing to do.
#else
	#if defined(USE_GLOBAL_BUCKET_ALLOCATOR)
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_UNLOAD:
		gLuaAlloc.EnableExpandCleanups(true);
		gLuaAlloc.cleanup();
		break;

	case ESYSTEM_EVENT_LEVEL_LOAD_START:
		gLuaAlloc.EnableExpandCleanups(true);
		gLuaAlloc.cleanup();
		break;

	case ESYSTEM_EVENT_LEVEL_LOAD_END:
		gLuaAlloc.EnableExpandCleanups(false);
		gLuaAlloc.cleanup();
		break;

	case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
		gLuaAlloc.EnableExpandCleanups(true);
		gLuaAlloc.cleanup();
		break;
	}
	#endif
#endif // USE_RAW_LUA_ALLOCS
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::ExposedCallstackClear()
{
	m_nCallDepth = 0;
	for (int i = 0; i < MAX_CALLDEPTH; i++)
		m_sCallDescriptions[i].clear();
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::ExposedCallstackPush(const char* sFunction, int nLine, const char* sSource)
{
	assert(sFunction);
	assert(sSource);
	if (m_nCallDepth < CScriptSystem::MAX_CALLDEPTH)
	{
		stack_string& callDescription = m_sCallDescriptions[m_nCallDepth];
		callDescription.Format("%s,%i,%s", sFunction, nLine, sSource);
		m_nCallDepth++;
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::ExposedCallstackPop()
{
	if (m_nCallDepth > 0)
	{
		m_nCallDepth--;
		stack_string& callDescription = m_sCallDescriptions[m_nCallDepth];
		callDescription.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
struct IRecursiveLuaDump
{
	virtual ~IRecursiveLuaDump(){}
	virtual void OnElement(int nLevel, const char* sKey, int nKey, ScriptAnyValue& value) = 0;
	virtual void OnBeginTable(int nLevel, const char* sKey, int nKey) = 0;
	virtual void OnEndTable(int nLevel) = 0;
};

struct SRecursiveLuaDumpToFile : public IRecursiveLuaDump
{
	FILE* file;
	char  sLevelOffset[1024];
	char  sKeyStr[32];
	int   nSize;

	const char* GetOffsetStr(int nLevel)
	{
		if (nLevel > sizeof(sLevelOffset) - 1)
			nLevel = sizeof(sLevelOffset) - 1;
		memset(sLevelOffset, '\t', nLevel);
		sLevelOffset[nLevel] = 0;
		return sLevelOffset;
	}
	const char* GetKeyStr(const char* sKey, int nKey)
	{
		if (sKey)
			return sKey;
		cry_sprintf(sKeyStr, "[%02d]", nKey);
		return sKeyStr;
	}
	SRecursiveLuaDumpToFile(const char* filename)
		: nSize(0)
	{
		file = fxopen(filename, "wt");
		ZeroArray(sLevelOffset);
		ZeroArray(sKeyStr);
	}
	~SRecursiveLuaDumpToFile()
	{
		if (file)
			fclose(file);
	}
	virtual void OnElement(int nLevel, const char* sKey, int nKey, ScriptAnyValue& value)
	{
		nSize += sizeof(Node);
		if (sKey)
			nSize += strlen(sKey) + 1;
		else
			nSize += sizeof(int);
		switch (value.GetType())
		{
		case EScriptAnyType::Boolean:
			if (value.GetBool())
				fprintf(file, "[%6d] %s %s=true\n", nSize, GetOffsetStr(nLevel), GetKeyStr(sKey, nKey));
			else
				fprintf(file, "[%6d] %s %s=false\n", nSize, GetOffsetStr(nLevel), GetKeyStr(sKey, nKey));
			break;
		case EScriptAnyType::Handle:
			fprintf(file, "[%6d] %s %s=%p\n", nSize, GetOffsetStr(nLevel), GetKeyStr(sKey, nKey), value.GetScriptHandle().ptr);
			break;
		case EScriptAnyType::Number:
			fprintf(file, "[%6d] %s %s=%g\n", nSize, GetOffsetStr(nLevel), GetKeyStr(sKey, nKey), value.GetNumber());
			break;
		case EScriptAnyType::String:
			fprintf(file, "[%6d] %s %s=%s\n", nSize, GetOffsetStr(nLevel), GetKeyStr(sKey, nKey), value.GetString());
			nSize += strlen(value.GetString()) + 1;
			break;
		//case ANY_TTABLE:
		case EScriptAnyType::Function:
			fprintf(file, "[%6d] %s %s()\n", nSize, GetOffsetStr(nLevel), GetKeyStr(sKey, nKey));
			break;
		case EScriptAnyType::UserData:
			fprintf(file, "[%6d] %s [userdata] %s\n", nSize, GetOffsetStr(nLevel), GetKeyStr(sKey, nKey));
			break;
		case EScriptAnyType::Vector:
			fprintf(file, "[%6d] %s %s=%g,%g,%g\n", nSize, GetOffsetStr(nLevel), GetKeyStr(sKey, nKey), value.GetVector().x, value.GetVector().y, value.GetVector().z);
			nSize += sizeof(Vec3);
			break;
		}
	}
	virtual void OnBeginTable(int nLevel, const char* sKey, int nKey)
	{
		nSize += sizeof(Node);
		nSize += sizeof(Table);
		fprintf(file, "[%6d] %s %s = {\n", nSize, GetOffsetStr(nLevel), GetKeyStr(sKey, nKey));
	}
	virtual void OnEndTable(int nLevel)
	{
		fprintf(file, "[%6d] %s }\n", nSize, GetOffsetStr(nLevel));
	}
};

//////////////////////////////////////////////////////////////////////////
static void RecursiveTableDump(CScriptSystem* pSS, lua_State* L, int idx, int nLevel, IRecursiveLuaDump* sink, std::set<void*>& tables)
{
	const char* sKey = 0;
	int nKey = 0;

	CHECK_STACK(L);

	void* pTable = (void*)lua_topointer(L, idx);
	if (tables.find(pTable) != tables.end())
	{
		// This table was already dumped.
		return;
	}
	tables.insert(pTable);

	lua_pushnil(L);
	while (lua_next(L, idx) != 0)
	{
		// `key' is at index -2 and `value' at index -1
		if (lua_type(L, -2) == LUA_TSTRING)
			sKey = lua_tostring(L, -2);
		else
		{
			sKey = 0;
			nKey = (int)lua_tonumber(L, -2); // key index.
		}
		int type = lua_type(L, -1);
		switch (type)
		{
		case LUA_TNIL:
			break;
		case LUA_TTABLE:
			{
				if (!(sKey != 0 && nLevel == 0 && strcmp(sKey, "_G") == 0))
				{
					sink->OnBeginTable(nLevel, sKey, nKey);
					RecursiveTableDump(pSS, L, lua_gettop(L), nLevel + 1, sink, tables);
					sink->OnEndTable(nLevel);
				}
			}
			break;
		default:
			{
				ScriptAnyValue any;
				pSS->ToAny(any, -1);
				sink->OnElement(nLevel, sKey, nKey, any);
			}
			break;
		}
		lua_pop(L, 1);
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::DumpStateToFile(const char* filename)
{
	CHECK_STACK(L);
	SRecursiveLuaDumpToFile sink(filename);
	if (sink.file)
	{
		std::set<void*> tables;
		RecursiveTableDump(this, L, LUA_GLOBALSINDEX, 0, &sink, tables);

#ifdef DEBUG_LUA_STATE
		{
			CHECK_STACK(L);
			for (std::set<CScriptTable*>::iterator it = gAllScriptTables.begin(); it != gAllScriptTables.end(); ++it)
			{
				CScriptTable* pTable = *it;

				ScriptHandle handle;
				if (pTable->GetValue("id", handle) && gEnv->pEntitySystem)
				{
					EntityId id = handle.n;
					IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
					char str[256];
					cry_sprintf(str, "*Entity: %s", pEntity->GetEntityTextDescription().c_str());
					sink.OnBeginTable(0, str, 0);
				}
				else
					sink.OnBeginTable(0, "*Unknown Table", 0);

				pTable->PushRef();
				RecursiveTableDump(this, L, lua_gettop(L), 1, &sink, tables);
				lua_pop(L, 1);
				sink.OnEndTable(0);
			}
		}
#endif
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::SerializeTimers(ISerialize* pSer)
{
	TSerialize ser(pSer);
	m_pScriptTimerMgr->Serialize(ser);
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::ResetTimers()
{
	m_pScriptTimerMgr->Reset();
}

//////////////////////////////////////////////////////////////////////////
HSCRIPTFUNCTION CScriptSystem::CompileBuffer(const char* sBuffer, size_t nSize, const char* sBufferDesc)
{
	int iIndex = -1;
	const char* sBufferDescription = "Pre Compiled Code";
	if (sBufferDesc)
		sBufferDescription = sBufferDesc;

	int status = luaL_loadbuffer(L, sBuffer, nSize, sBufferDescription);

	if (status == 0)
	{
		return (HSCRIPTFUNCTION)(INT_PTR)lua_ref(L, 1);
	}
	else
	{
		const char* sErr = lua_tostring(L, -2);
		GetISystem()->Warning(VALIDATOR_MODULE_SCRIPTSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE | VALIDATOR_FLAG_SCRIPT, sBufferDescription,
		                      "[Lua Error] Failed to compile code [%s]: %s", sBuffer, sErr);

		return 0;
	}
}

//////////////////////////////////////////////////////////////////////////
int CScriptSystem::PreCacheBuffer(const char* sBuffer, size_t nSize, const char* sBufferDesc)
{
	const char* sBufferDescription = "Pre Cached Code";
	if (sBufferDesc)
		sBufferDescription = sBufferDesc;

	int iIndex = -1;
	if (HSCRIPTFUNCTION scriptFunction = CompileBuffer(sBuffer, nSize, sBufferDesc))
	{
		iIndex = m_vecPreCached.size();
		m_pPreCacheBufferTable->SetAt(iIndex, scriptFunction);
		m_vecPreCached.push_back(sBuffer);
	}

	return iIndex;
}

//////////////////////////////////////////////////////////////////////////
int CScriptSystem::BeginPreCachedBuffer(int iIndex)
{
	assert(iIndex >= 0);

	int iRet = 0;

	if (iIndex >= 0 && iIndex < (int)m_vecPreCached.size())
	{
		ScriptAnyValue ScriptFunction;
		m_pPreCacheBufferTable->GetAtAny(iIndex, ScriptFunction);

		if (ScriptFunction.GetVarType() == svtFunction)
		{
			iRet = BeginCall(ScriptFunction.GetScriptFunction());
		}

		if (iRet == 0)
		{
			GetISystem()->Warning(VALIDATOR_MODULE_SCRIPTSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE | VALIDATOR_FLAG_SCRIPT, "Precached",
			                      "[Lua] Error executing: [%s]", m_vecPreCached[iIndex].c_str());
		}
	}
	else
	{
		GetISystem()->Warning(VALIDATOR_MODULE_SCRIPTSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE | VALIDATOR_FLAG_SCRIPT, "Precached",
		                      "[Lua] Precached buffer %d does not exist (last valid is %d)", iIndex, m_vecPreCached.size());
	}

	return (iRet);
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::ClearPreCachedBuffer()
{
	m_pPreCacheBufferTable->Clear();
	m_vecPreCached.clear();
}
