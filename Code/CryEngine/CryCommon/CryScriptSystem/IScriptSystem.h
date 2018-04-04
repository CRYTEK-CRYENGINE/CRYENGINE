// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include <CryCore/functor.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/IEngineModule.h>

class ICrySizer;
struct IWeakScriptObject;
struct IScriptTable;
struct ISystem;
struct IFunctionHandler;
class SmartScriptTable;
struct ISerialize;

//! Script function reference.
struct SScriptFuncHandle {};
typedef SScriptFuncHandle* HSCRIPTFUNCTION;

//! ScriptHandle type used to pass pointers and handles to/from Lua script.
//! As Lua script do not natively support integers, full range integers used as handles must be stored in Lua using this class.
union ScriptHandle
{
	UINT_PTR n;
	void*    ptr;

	ScriptHandle() : ptr(0)
	{
	};
	ScriptHandle(int i) : n(i)
	{
	};

	ScriptHandle(void* p) : ptr(p)
	{
	};
};

//! Template to wrap an int into a ScriptHandle (to guarantee the proper handling of full-range ints).
template<typename IntType>
inline ScriptHandle IntToHandle(IntType const nInt) { ScriptHandle h; h.n = static_cast<UINT_PTR>(nInt); return h; }

//! Template to convert a ScriptHandle to an int type.
template<typename IntType>
inline IntType HandleToInt(ScriptHandle const hHandle) { return static_cast<IntType>(hHandle.n); }

struct ScriptUserData
{
	void* ptr;
	int   nRef;

	ScriptUserData() { ptr = 0; nRef = 0; }
};

typedef int HBREAKPOINT;

enum ELuaDebugMode
{
	eLDM_NoDebug    = 0,
	eLDM_FullDebug  = 1,
	eLDM_OnlyErrors = 2
};

enum BreakState
{
	bsStepNext,
	bsStepInto,
	bsStepOut,
	bsContinue,
	bsNoBreak
};

////////////////////////////////////////////////////////////////////////////
enum ScriptVarType
{
	svtNull = 0,
	svtString,
	svtNumber,
	svtBool,
	svtFunction,
	svtObject,
	svtPointer,
	svtUserData,
};

//! Any Script value.
enum ScriptAnyType
{
	ANY_ANY = 0,
	ANY_TNIL,
	ANY_TBOOLEAN,
	ANY_THANDLE,
	ANY_TNUMBER,
	ANY_TSTRING,
	ANY_TTABLE,
	ANY_TFUNCTION,
	ANY_TUSERDATA,
	ANY_TVECTOR,
	ANY_COUNT,
};

struct ScriptAnyValue
{
	ScriptAnyType type;
	union
	{
		bool                            b;
		float                           number;
		const char*                     str;
		IScriptTable*                   table;
		const void*                     ptr;
		HSCRIPTFUNCTION                 function;
		struct { float x, y, z; }       vec3;
		struct { void* ptr; int nRef; } ud;
	};

	~ScriptAnyValue();  // Implemented at the end of header.

	ScriptAnyValue() : type(ANY_ANY) { table = 0; };
	ScriptAnyValue(ScriptAnyType _type) : type(_type) { table = 0; };
	ScriptAnyValue(bool value) : type(ANY_TBOOLEAN) { b = value; };
	ScriptAnyValue(int value) : type(ANY_TNUMBER) { number = (float)value; };
	ScriptAnyValue(unsigned int value) : type(ANY_TNUMBER) { number = (float)value; };
	ScriptAnyValue(float value) : type(ANY_TNUMBER) { number = value; };
	ScriptAnyValue(const char* value) : type(ANY_TSTRING) { str = value; };
	ScriptAnyValue(ScriptHandle value) : type(ANY_THANDLE) { ptr = value.ptr; };
	ScriptAnyValue(HSCRIPTFUNCTION value);
	ScriptAnyValue(const Vec3& value) : type(ANY_TVECTOR) { vec3.x = value.x; vec3.y = value.y; vec3.z = value.z; };
	ScriptAnyValue(const Ang3& value) : type(ANY_TVECTOR) { vec3.x = value.x; vec3.y = value.y; vec3.z = value.z; };
	//ScriptAnyValue( const ScriptUserData &value ) : type(ANY_TUSERDATA) { ud.ptr = value.ptr; ud.nRef = value.nRef; };
	//ScriptAnyValue( USER_DATA value ) : type(ANY_TUSERDATA) { ud.nRef=(int)value;ud.nCookie=0;ud.nVal=0; }; // To Remove.
	ScriptAnyValue(IScriptTable* value);            // Implemented at the end of header.
	ScriptAnyValue(const SmartScriptTable& value);  // Implemented at the end of header.

	ScriptAnyValue(const ScriptAnyValue& value);  // Implemented at the end of header.
	void            Swap(ScriptAnyValue& value);  // Implemented at the end of header.

	ScriptAnyValue& operator=(const ScriptAnyValue& rhs)
	{
		ScriptAnyValue temp(rhs);
		Swap(temp);
		return *this;
	}

	//! Compares 2 values.
	bool operator==(const ScriptAnyValue& rhs) const;
	bool operator!=(const ScriptAnyValue& rhs) const { return !(*this == rhs); };

	bool CopyTo(bool& value) const                   { if (type == ANY_TBOOLEAN) { value = b; return true; }; return false; };
	bool CopyTo(int& value) const                    { if (type == ANY_TNUMBER) { value = (int)number; return true; }; return false; };
	bool CopyTo(unsigned int& value) const           { if (type == ANY_TNUMBER) { value = (unsigned int)number; return true; }; return false; };
	bool CopyTo(float& value) const                  { if (type == ANY_TNUMBER) { value = number; return true; }; return false; };
	bool CopyTo(const char*& value) const            { if (type == ANY_TSTRING) { value = str; return true; }; return false; };
	bool CopyTo(char*& value) const                  { if (type == ANY_TSTRING) { value = (char*)str; return true; }; return false; };
	bool CopyTo(string& value) const                 { if (type == ANY_TSTRING) { value = str; return true; }; return false; };
	bool CopyTo(ScriptHandle& value) const           { if (type == ANY_THANDLE) { value.ptr = const_cast<void*>(ptr); return true; }; return false; };
	bool CopyTo(HSCRIPTFUNCTION& value) const;
	bool CopyTo(Vec3& value) const                   { if (type == ANY_TVECTOR) { value.x = vec3.x; value.y = vec3.y; value.z = vec3.z; return true; }; return false; };
	bool CopyTo(Ang3& value) const                   { if (type == ANY_TVECTOR) { value.x = vec3.x; value.y = vec3.y; value.z = vec3.z; return true; }; return false; };
	//	bool CopyTo( ScriptUserData &value ) { if (type==ANY_TUSERDATA) { value.ptr = ud.ptr; value.nRef = ud.nRef; return true; }; return false; };
	//	bool CopyTo( USER_DATA &value ) { if (type==ANY_TUSERDATA) { value = (USER_DATA)ud.nRef; return true; }; return false; };
	bool CopyTo(IScriptTable*& value) const;    // Implemented at the end of header.
	bool CopyTo(SmartScriptTable& value) const; // Implemented at the end of header.
	bool CopyFromTableToXYZ(float& x, float& y, float& z) const;
	bool CopyFromTableTo(Vec3& value) const { return CopyFromTableToXYZ(value.x, value.y, value.z); }
	bool CopyFromTableTo(Ang3& value) const { return CopyFromTableToXYZ(value.x, value.y, value.z); }

	//! Clears any variable to uninitialized state.
	// Implemented at the end of header.
	void Clear();

	//! Only initialize type.
	ScriptAnyValue(bool, int) : type(ANY_TBOOLEAN) {};
	ScriptAnyValue(int, int) : type(ANY_TNUMBER) {};
	ScriptAnyValue(unsigned int, int) : type(ANY_TNUMBER) {};
	ScriptAnyValue(float&, int) : type(ANY_TNUMBER) {};
	ScriptAnyValue(const char*, int) : type(ANY_TSTRING) {};
	ScriptAnyValue(ScriptHandle, int) : type(ANY_THANDLE) {};
	ScriptAnyValue(HSCRIPTFUNCTION, int) : type(ANY_TFUNCTION) { function = 0; };
	ScriptAnyValue(Vec3&, int) : type(ANY_TVECTOR) {};
	ScriptAnyValue(Ang3&, int) : type(ANY_TVECTOR) {};
	ScriptAnyValue(ScriptUserData, int) : type(ANY_TUSERDATA) {};
	//ScriptAnyValue( USER_DATA,int ) : type(ANY_TUSERDATA) {};
	ScriptAnyValue(IScriptTable* _table, int);
	ScriptAnyValue(const SmartScriptTable& value, int);

	ScriptVarType GetVarType() const
	{
		switch (type)
		{
		case ANY_ANY:
			return svtNull;
		case ANY_TNIL:
			return svtNull;
		case ANY_TBOOLEAN:
			return svtBool;
		case ANY_THANDLE:
			return svtPointer;
		case ANY_TNUMBER:
			return svtNumber;
		case ANY_TSTRING:
			return svtString;
		case ANY_TTABLE:
			return svtObject;
		case ANY_TFUNCTION:
			return svtFunction;
		case ANY_TUSERDATA:
			return svtUserData;
		case ANY_TVECTOR:
			return svtObject;
		default:
			return svtNull;
		}
	}

	void GetMemoryUsage(ICrySizer* pSizer) const {}
};

struct IScriptSystemEngineModule : public Cry::IDefaultModule
{
	CRYINTERFACE_DECLARE_GUID(IScriptSystemEngineModule, "39b5373f-b298-4aa1-b691-88ed53cf5f9d"_cry_guid);
};

//! Scripting Engine interface.
//! This interface is mapped 1:1 on a script state.
//! All scripts loaded from the same interface instance are visible with each others'.
struct IScriptSystem
{
	// <interfuscator:shuffle>
	virtual ~IScriptSystem(){}

	//! Updates the system, per frame.
	virtual void Update(void) = 0;

	//! Sets the rate of Garbage Collection for script system.
	//! \param fRate Rate in seconds.
	virtual void SetGCFrequency(const float fRate) = 0;

	//! Sets the environment of the given function.
	//! \param scriptFunction Function to receive the environment.
	//! \param pEnv Environment to set.
	virtual void SetEnvironment(HSCRIPTFUNCTION scriptFunction, IScriptTable* pEnv) = 0;

	//! Gets the environment of the given function.
	//! \param scriptFunction Function to receive the environment.
	//! \return Pointer to a script table containing the environment
	virtual IScriptTable* GetEnvironment(HSCRIPTFUNCTION scriptFunction) = 0;

	//! Loads and runs a script file.
	//! \param sFileName Path of the script file.
	//! \param bRaiseError When set to true, the script engine will call CryWarning when an error in the script file occurs.
	//! \return false if the execution fails, otherwise it will be true.
	//! \note All global variables and functions declared in the executed script will persist for all the script system lifetime.
	virtual bool ExecuteFile(const char* sFileName, bool bRaiseError = true, bool bForceReload = false, IScriptTable* pEnv = 0) = 0;

	//! Executes an ASCII buffer.
	//! \param sBuffer An 8bit ASCII buffer containing the script that must be executed.
	//! \param bRaiseError When set to true, the script engine will call CryWarning when an error in the script file occurs.
	//! \param sBufferDescription Used as a name to describe the buffer.
	//! \return false if the execution fails, otherwise it will be true.
	//! \note All global variables and functions declared in the executed script will persist for all the script system lifetime.
	virtual bool ExecuteBuffer(const char* sBuffer, size_t nSize, const char* sBufferDescription = "", IScriptTable* pEnv = 0) = 0;

	//! Unloads a script.
	//! \param sFileName Path of the script file.
	//! \note The script engine never loads the same file twice because it internally stores a list of the loaded files. Calling this functions will remove the script file from this list.
	//! \see UnloadScripts.
	virtual void UnloadScript(const char* sFileName) = 0;

	//! Unloads all the scripts.
	virtual void UnloadScripts() = 0;

	//! Reloads a script.
	//! \param sFileName Path of the script file to reload.
	//! \param bRaiseError When set to true, the script engine will call CryWarning when an error in the script file occurs.
	//! \return False if the execution fails, otherwise it will be true.
	//! \see ReloadScripts
	virtual bool ReloadScript(const char* sFileName, bool bRaiseError = true) = 0;

	//! Reloads all the scripts previously loaded.
	//! \return False if the execution of one of the script fails, otherwise it will be true.
	virtual bool ReloadScripts() = 0;

	//! Generates an OnLoadedScriptDump() for every loaded script.
	virtual void DumpLoadedScripts() = 0;

	//! Creates a new IScriptTable table accessible to the scripts.
	//! \return Pointer to the created object, with the reference count of 0.
	virtual IScriptTable* CreateTable(bool bEmpty = false) = 0;

	//! Starts a call to script function.
	//! \param sTableName Name of the script table that contains the function.
	//! \param sFuncName Function name.
	//! \note To call a function in the script object you must: call BeginCall, push all parameters whit PushParam, then call EndCall.
	//! \code
	//! m_ScriptSystem->BeginCall("Player","OnInit");.
	//! m_ScriptSystem->PushParam(pObj);.
	//! m_ScriptSystem->PushParam(nTime);.
	//! m_ScriptSystem->EndCall();.
	//! \endcode
	virtual int BeginCall(HSCRIPTFUNCTION hFunc) = 0;

	//! Calls a named method inside specified table.
	virtual int BeginCall(const char* sFuncName) = 0;  // From void to int for error checking.
	virtual int BeginCall(const char* sTableName, const char* sFuncName) = 0;

	//! Calls a named method inside specified table.
	virtual int BeginCall(IScriptTable* pTable, const char* sFuncName) = 0;

	//! Ends a call to script function.
	virtual bool EndCall() = 0;

	//! Ends a call to script function.
	//!	\param[out] any Reference to the variable that will store the eventual return value.
	virtual bool EndCallAny(ScriptAnyValue& any) = 0;

	//! Ends a call to script function.
	//!	\param[out] anys Reference to the variable that will store the eventual returning values.
	virtual bool EndCallAnyN(int n, ScriptAnyValue* anys) = 0;

	//! Gets reference to the Lua function.
	//! \note This reference must be released with IScriptSystem::ReleaseFunc().
	//! \see IScriptSystem::ReleaseFunc()
	//! @{.
	virtual HSCRIPTFUNCTION GetFunctionPtr(const char* sFuncName) = 0;
	virtual HSCRIPTFUNCTION GetFunctionPtr(const char* sTableName, const char* sFuncName) = 0;
	//! @}.

	//! Adds new reference to function referenced by HSCRIPTFUNCTION.
	virtual HSCRIPTFUNCTION AddFuncRef(HSCRIPTFUNCTION f) = 0;

	//! Adds new reference to function referenced by HSCRIPTFUNCTION.
	virtual bool CompareFuncRef(HSCRIPTFUNCTION f1, HSCRIPTFUNCTION f2) = 0;

	//! Frees references created with GetFunctionPtr or GetValue for HSCRIPTFUNCTION.
	virtual void ReleaseFunc(HSCRIPTFUNCTION f) = 0;

	//! Properly clones a ScriptAnyValue. It will create new references to objects if appropriate.
	virtual ScriptAnyValue CloneAny(const ScriptAnyValue& any) = 0;

	//! Properly releases a ScriptAnyValue. It will release references to objects if appropriate.
	virtual void ReleaseAny(const ScriptAnyValue& any) = 0;

	//! Push a parameter during a function call.
	virtual void PushFuncParamAny(const ScriptAnyValue& any) = 0;

	//! Set Global value.
	virtual void SetGlobalAny(const char* sKey, const ScriptAnyValue& any) = 0;

	//! Get Global value.
	virtual bool GetGlobalAny(const char* sKey, ScriptAnyValue& any) = 0;

	//! Set Global value to Null.
	virtual void          SetGlobalToNull(const char* sKey) { SetGlobalAny(sKey, ScriptAnyValue(ANY_TNIL)); }

	virtual IScriptTable* CreateUserData(void* ptr, size_t size) = 0;

	//! Forces a Garbage collection cycle.
	//! \note In the current status of the engine the automatic GC is disabled so this function must be called explicitly.
	virtual void ForceGarbageCollection() = 0;

	//! Gets number of "garbaged" object.
	virtual int GetCGCount() = 0;

	//! \deprecated This is a legacy function.
	virtual void SetGCThreshhold(int nKb) = 0;

	//! Releases and destroys the script system.
	virtual void Release() = 0;

	//! \note Debug functions.
	//! @{.
	virtual void          ShowDebugger(const char* pszSourceFile, int iLine, const char* pszReason) = 0;

	virtual HBREAKPOINT   AddBreakPoint(const char* sFile, int nLineNumber) = 0;
	virtual IScriptTable* GetLocalVariables(int nLevel, bool bRecursive) = 0;

	//! \return A table containing 1 entry per stack level(aka per call). An entry will look like this table.
	//! \code
	//! [1]={
	//!     Description="function bau()",.
	//!     Line=234,.
	//!     Sourcefile="/scripts/bla/bla/bla.lua".
	//! }
	//! \endcode
	virtual IScriptTable* GetCallsStack() = 0;

	//! Dump callstack to log, can be used during exception handling.
	virtual void       DumpCallStack() = 0;

	virtual void       DebugContinue() = 0;
	virtual void       DebugStepNext() = 0;
	virtual void       DebugStepInto() = 0;
	virtual void       DebugDisable() = 0;
	virtual BreakState GetBreakState() = 0;
	//! @}.

	virtual void GetMemoryStatistics(ICrySizer* pSizer) const = 0;

	//! \note Is not recursive but combines the hash values of the whole table when the specifies variable is a table, otherwise has to be a Lua function.
	//! \param sPath Zero terminated path to the variable (e.g. _localplayer.cnt), max 255 characters.
	//! \param szKey Zero terminated name of the variable (e.g. luaFunc), max 255 characters.
	//! \param dwHash It is used as input and output.
	virtual void GetScriptHash(const char* sPath, const char* szKey, unsigned int& dwHash) = 0;

	virtual void RaiseError(const char* format, ...) PRINTF_PARAMS(2, 3) = 0;

	//! \note Called one time after initialization of system to register script system console vars.
	virtual void PostInit() = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual void LoadScriptedSurfaceTypes(const char* sFolder, bool bReload) = 0;

	//! Serializes script timers.
	virtual void SerializeTimers(ISerialize* pSer) = 0;

	//! Resets all the script timers.
	virtual void ResetTimers() = 0;

	virtual int  GetStackSize() const = 0;

	//! Retrieves size of memory allocated in script.
	virtual uint32 GetScriptAllocSize() = 0;

	//! Facility to pre-catch any Lua buffer.
	virtual HSCRIPTFUNCTION CompileBuffer(const char* sBuffer, size_t nSize, const char* sBufferDesc) = 0;
	virtual int             PreCacheBuffer(const char* sBuffer, size_t nSize, const char* sBufferDesc) = 0;
	virtual int             BeginPreCachedBuffer(int iIndex) = 0;
	virtual void            ClearPreCachedBuffer() = 0;

	//! Allocate or deallocate through the script system's allocator.
	virtual void*  Allocate(size_t sz) = 0;
	virtual size_t Deallocate(void* ptr) = 0;
	// </interfuscator:shuffle>

	template<class T>
	bool EndCall(T& value)
	{
		ScriptAnyValue any(value, 0);
		return EndCallAny(any) && any.CopyTo(value);
	}

	template<class T> void PushFuncParam(const T& value)                    { PushFuncParamAny(value); }

	template<class T> void SetGlobalValue(const char* sKey, const T& value) { SetGlobalAny(sKey, ScriptAnyValue(value)); }
	template<class T> bool GetGlobalValue(const char* sKey, T& value)
	{
		ScriptAnyValue any(value, 0);
		return GetGlobalAny(sKey, any) && any.CopyTo(value);
	}
};

class CCheckScriptStack
{
public:
	CCheckScriptStack(IScriptSystem* pSS, const char* file, int line)
	{
		m_pSS = pSS;
		m_stackSize = pSS->GetStackSize();
		m_file = file;
		m_line = line;
	}

	~CCheckScriptStack()
	{
#if defined(_DEBUG)
		int stackSize = m_pSS->GetStackSize();
		assert(stackSize == m_stackSize);
#endif
	}

private:
	IScriptSystem* m_pSS;
	int            m_stackSize;
	const char*    m_file;
	int            m_line;
};

#define CHECK_SCRIPT_STACK_2(x, y) x ## y
#define CHECK_SCRIPT_STACK_1(x, y) CHECK_SCRIPT_STACK_2(x, y)
#define CHECK_SCRIPT_STACK CCheckScriptStack CHECK_SCRIPT_STACK_1(css_, __COUNTER__)(gEnv->pScriptSystem, __FILE__, __LINE__)

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

struct IScriptTableDumpSink
{
	// <interfuscator:shuffle>
	virtual ~IScriptTableDumpSink(){}
	virtual void OnElementFound(const char* sName, ScriptVarType type) = 0;
	virtual void OnElementFound(int nIdx, ScriptVarType type) = 0;
	// </interfuscator:shuffle>
};

// Description:
//	 Interface to the iterator of values in script table.
// Notes:
//	 This is reference counted interface, when last reference to this interface
//	 is release, object is deleted.
//	 Used together with smart_ptr.
// See also:
//	 smart_ptr
//////////////////////////////////////////////////////////////////////////
struct IScriptTableIterator
{
	// <interfuscator:shuffle>
	virtual ~IScriptTableIterator(){}
	virtual void AddRef();
	// Summary:
	//	 Decrements reference delete script table iterator.
	virtual void Release();

	//! Get next value in the table.
	virtual bool Next(ScriptAnyValue& var);
	// </interfuscator:shuffle>
};

////////////////////////////////////////////////////////////////////////////
struct IScriptTable
{
	typedef Functor1wRet<IFunctionHandler*, int> FunctionFunctor;
	typedef int (*                               UserDataFunction)(IFunctionHandler* pH, void* pBuffer, int nSize);

	//! Iteration over table parameters.
	struct Iterator
	{
		const char*    sKey; // This is now redundant.
		int            nKey; // This is now redundant.
		ScriptAnyValue value;
		ScriptAnyValue key;
		struct
		{
			bool resolvePrototypeTableAsWell;
			int  nStackMarker1; //!< Used for traversing our own table (this is typically the table that overrides properties from prototype tables).
			int  nStackMarker2; //!< Used after our own table is traversed; we then try to traverse the prototype table (gets retrieved via a potential metatable).
		} internal;
	};

	//! Structure that describe user data function.
	struct SUserFunctionDesc
	{
		const char*      sFunctionName;   //!< Name of function.
		const char*      sFunctionParams; //!< List of parameters (ex "nSlot,vDirection").
		const char*      sGlobalName;     //!< Name of global table (ex "System").
		FunctionFunctor  pFunctor;        //!< Pointer to simple function.
		int              nParamIdOffset;  //!< Offset of the parameter to accept as 1st function argument.
		UserDataFunction pUserDataFunc;   //!< Pointer to function with associated data buffer.
		void*            pDataBuffer;     //!< Pointer to the data buffer associated to the user data function.
		int              nDataSize;       //!< Size of data associated with user data function.

		//! Constructor that initialize all data members to initial state.
		SUserFunctionDesc() : sFunctionName(""), sFunctionParams(""), sGlobalName(""), nParamIdOffset(0), pUserDataFunc(0), pDataBuffer(0), nDataSize(0) {}
	};

	// <interfuscator:shuffle>
	virtual ~IScriptTable(){}

	//! Gets script system of this table.
	virtual IScriptSystem* GetScriptSystem() const = 0;

	//! Increments reference count to the script table.
	virtual void AddRef() = 0;

	//! Decrements reference count for script table.
	//! \note When reference count reaches zero, table will be deleted.
	virtual void  Release() = 0;

	virtual void  Delegate(IScriptTable* pObj) = 0;
	virtual void* GetUserDataValue() = 0;

	//! Sets the value of a table member.
	virtual void SetValueAny(const char* sKey, const ScriptAnyValue& any, bool bChain = false) = 0;

	//! Gets the value of a table member.
	virtual bool GetValueAny(const char* sKey, ScriptAnyValue& any, bool bChain = false) = 0;

	//! Start a Set/Get chain.
	//! \note This is faster than calling SetValue/GetValue a large number of times.
	virtual bool BeginSetGetChain() = 0;

	//! Completes a Set/Get chain.
	//! \note This is faster than calling SetValue/GetValue a large number of times.
	virtual void EndSetGetChain() = 0;

	//! Gets the value type of a table member.
	//! \param sKey Variable name.
	//! \return The value type or svtNull if doesn't exist.
	virtual ScriptVarType GetValueType(const char* sKey) = 0;

	virtual ScriptVarType GetAtType(int nIdx) = 0;

	//! Sets the value of a member variable at the specified index this means that you will use the object as vector into the script.
	virtual void SetAtAny(int nIndex, const ScriptAnyValue& any) = 0;

	//! Gets the value of a member variable at the specified index.
	virtual bool                   GetAtAny(int nIndex, ScriptAnyValue& any) = 0;

	virtual IScriptTable::Iterator BeginIteration(bool resolvePrototypeTableAsWell = false) = 0;
	virtual bool                   MoveNext(Iterator& iter) = 0;
	virtual void                   EndIteration(const Iterator& iter) = 0;

	//! Clears the table,removes all the entries in the table.
	virtual void Clear() = 0;

	//! Gets the count of elements into the object.
	virtual int Count() = 0;

	//! Produces a copy of the src table.
	//! \param pSrcTable Source table to clone from.
	//! \param bDeepCopy Defines if source table is cloned recursively or not,
	//! \note If bDeepCopy is false Only does shallow copy (no deep copy, table entries are not cloned hierarchically).
	//! \note If bDeepCopy is true, all sub tables are also cloned recursively.
	//! \note If bDeepCopy is true and bCopyByReference is true, the table structure is copied but the tables are left empty and the metatable is set to point at the original table.
	virtual bool Clone(IScriptTable* pSrcTable, bool bDeepCopy = false, bool bCopyByReference = false) = 0;

	//! Dumps all table entries to the IScriptTableDumpSink interface.
	virtual void Dump(IScriptTableDumpSink* p) = 0;

	//! Adds a C++ callback function to the table.
	//! \note The function is a standard function that returns number of arguments and accept IFunctionHandler as argument.
	//! \see IFunctionHandler
	virtual bool AddFunction(const SUserFunctionDesc& fd) = 0;
	// </interfuscator:shuffle>

	//! Set value of a table member.
	template<class T> void SetValue(const char* sKey, const T& value) { SetValueAny(sKey, value); }

	//! Get value of a table member.
	template<class T> bool GetValue(const char* sKey, T& value)
	{
		ScriptAnyValue any(value, 0);
		return GetValueAny(sKey, any) && any.CopyTo(value);
	}
	bool HaveValue(const char* sKey)
	{
		ScriptAnyValue any;
		GetValueAny(sKey, any);

		switch (any.type)
		{
		case ANY_TTABLE:
			if (any.table)
			{
				any.table->Release();
				any.table = 0;
			}
			return true;
		case ANY_TFUNCTION:
			if (any.function)
			{
				gEnv->pScriptSystem->ReleaseFunc(any.function);
				any.function = 0;
			}
			return true;
		case ANY_TNIL:
			return false;
		default:
			return true;
		}
	}
	//! Set member value to nil.
	void SetToNull(const char* sKey) { SetValueAny(sKey, ScriptAnyValue(ANY_TNIL)); }

	//! Set value of a table member.
	template<class T> void SetValueChain(const char* sKey, const T& value) { SetValueAny(sKey, value, true); }

	//! Get value of a table member.
	template<class T> bool GetValueChain(const char* sKey, T& value)
	{
		ScriptAnyValue any(value, 0);
		return GetValueAny(sKey, any, true) && any.CopyTo(value);
	}
	void SetToNullChain(const char* sKey) { SetValueChain(sKey, ScriptAnyValue(ANY_TNIL)); }

	//! Set the value of a member variable at the specified index.
	template<class T> void SetAt(int nIndex, const T& value) { SetAtAny(nIndex, value); }

	//! Get the value of a member variable at the specified index.
	template<class T> bool GetAt(int nIndex, T& value)
	{
		ScriptAnyValue any(value, 0);
		return GetAtAny(nIndex, any) && any.CopyTo(value);
	}
	bool HaveAt(int elem)
	{
		ScriptAnyValue any;
		GetAtAny(elem, any);
		return any.type != ANY_TNIL;
	}
	//! Set the value of a member variable to nil at the specified index.
	void SetNullAt(int nIndex) { SetAtAny(nIndex, ScriptAnyValue(ANY_TNIL)); }

	//! Add value at next available index.
	template<class T> void PushBack(const T& value)
	{
		int nNextPos = Count() + 1;
		SetAtAny(nNextPos, value);
	}

private:
	//! Prevent using one of these as the output parameter will end up in a dangling pointer if it was set to NULL before the call.
	//! Instead, use of GetValue<SmartScriptTable> and GetValueChain<SmartScriptTable> is encouraged.
	//! @{
	bool GetValue(const char* sKey, IScriptTable*& value);
	bool GetValue(const char* sKey, const IScriptTable*& value);
	bool GetValueChain(const char* sKey, IScriptTable*& value);
	bool GetValueChain(const char* sKey, const IScriptTable*& value);
	//! @}
};

//! This interface is used by the C++ function mapped to the script to retrieve the function parameters passed by the script and to return an optional result value to the script.
struct IFunctionHandler
{
	// <interfuscator:shuffle>
	virtual ~IFunctionHandler(){}

	//! Returns pointer to the script system.
	virtual IScriptSystem* GetIScriptSystem() = 0;

	//! Gets this pointer of table which called C++ method.
	//! This pointer is assigned to key "__this" in the table.
	virtual void* GetThis() = 0;

	//! Retrieves the value of the self passed when calling the table.
	//! \note Always the 1st argument of the function.
	virtual bool GetSelfAny(ScriptAnyValue& any) = 0;

	//! Returns the function name of the currently called function.
	//! \note Use this only used for error reporting.
	virtual const char* GetFuncName() = 0;

	//! Gets the number of parameter at specified index passed by the Lua.
	virtual int GetParamCount() = 0;

	//! Gets the type of the parameter at specified index passed by the Lua.
	virtual ScriptVarType GetParamType(int nIdx) = 0;

	//! Gets the nIdx param passed by the script.
	//! \param nIdx 1-based index of the parameter.
	//! \param val Reference to the C++ variable that will store the value.
	virtual bool GetParamAny(int nIdx, ScriptAnyValue& any) = 0;

	virtual int  EndFunctionAny(const ScriptAnyValue& any) = 0;
	virtual int  EndFunctionAny(const ScriptAnyValue& any1, const ScriptAnyValue& any2) = 0;
	virtual int  EndFunctionAny(const ScriptAnyValue& any1, const ScriptAnyValue& any2, const ScriptAnyValue& any3) = 0;
	virtual int  EndFunction() = 0;
	// </interfuscator:shuffle>

	//! Retrieves the value of the self passed when calling the table.
	//! \note Always the 1st argument of the function.
	template<class T>
	bool GetSelf(T& value)
	{
		ScriptAnyValue any(value, 0);
		return GetSelfAny(any) && any.CopyTo(value);
	}

	//! Get the nIdx param passed by the script.
	//! \param nIdx 1-based index of the parameter.
	//! \param val Reference to the C++ variable that will store the value.
	template<typename T>
	bool GetParam(int nIdx, T& value)
	{
		ScriptAnyValue any(value, 0);
		return GetParamAny(nIdx, any) && any.CopyTo(value);
	}

	template<class T>
	int EndFunction(const T& value)                                       { return EndFunctionAny(value); }
	template<class T1, class T2>
	int EndFunction(const T1& value1, const T2& value2)                   { return EndFunctionAny(value1, value2); }
	template<class T1, class T2, class T3>
	int EndFunction(const T1& value1, const T2& value2, const T3& value3) { return EndFunctionAny(value1, value2, value3); }
	int EndFunctionNull()                                                 { return EndFunction(); }

	//! Template methods to get multiple parameters.
	template<class P1>
	bool GetParams(P1& p1)
	{
		if (!GetParam(1, p1)) return false;
		return true;
	}
	template<class P1, class P2>
	bool GetParams(P1& p1, P2& p2)
	{
		if (!GetParam(1, p1) || !GetParam(2, p2)) return false;
		return true;
	}
	template<class P1, class P2, class P3>
	bool GetParams(P1& p1, P2& p2, P3& p3)
	{
		if (!GetParam(1, p1) || !GetParam(2, p2) || !GetParam(3, p3)) return false;
		return true;
	}
	template<class P1, class P2, class P3, class P4>
	bool GetParams(P1& p1, P2& p2, P3& p3, P4& p4)
	{
		if (!GetParam(1, p1) || !GetParam(2, p2) || !GetParam(3, p3) || !GetParam(4, p4)) return false;
		return true;
	}
	template<class P1, class P2, class P3, class P4, class P5>
	bool GetParams(P1& p1, P2& p2, P3& p3, P4& p4, P5& p5)
	{
		if (!GetParam(1, p1) || !GetParam(2, p2) || !GetParam(3, p3) || !GetParam(4, p4)) return false;
		if (!GetParam(5, p5)) return false;
		return true;
	}
	template<class P1, class P2, class P3, class P4, class P5, class P6>
	bool GetParams(P1& p1, P2& p2, P3& p3, P4& p4, P5& p5, P6& p6)
	{
		if (!GetParam(1, p1) || !GetParam(2, p2) || !GetParam(3, p3) || !GetParam(4, p4)) return false;
		if (!GetParam(5, p5) || !GetParam(6, p6)) return false;
		return true;
	}
	template<class P1, class P2, class P3, class P4, class P5, class P6, class P7>
	bool GetParams(P1& p1, P2& p2, P3& p3, P4& p4, P5& p5, P6& p6, P7& p7)
	{
		if (!GetParam(1, p1) || !GetParam(2, p2) || !GetParam(3, p3) || !GetParam(4, p4)) return false;
		if (!GetParam(5, p5) || !GetParam(6, p6) || !GetParam(7, p7)) return false;
		return true;
	}
	template<class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8>
	bool GetParams(P1& p1, P2& p2, P3& p3, P4& p4, P5& p5, P6& p6, P7& p7, P8& p8)
	{
		if (!GetParam(1, p1) || !GetParam(2, p2) || !GetParam(3, p3) || !GetParam(4, p4)) return false;
		if (!GetParam(5, p5) || !GetParam(6, p6) || !GetParam(7, p7) || !GetParam(8, p8)) return false;
		return true;
	}
	template<class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9>
	bool GetParams(P1& p1, P2& p2, P3& p3, P4& p4, P5& p5, P6& p6, P7& p7, P8& p8, P9& p9)
	{
		if (!GetParam(1, p1) || !GetParam(2, p2) || !GetParam(3, p3) || !GetParam(4, p4)) return false;
		if (!GetParam(5, p5) || !GetParam(6, p6) || !GetParam(7, p7) || !GetParam(8, p8)) return false;
		if (!GetParam(9, p9)) return false;
		return true;
	}
	template<class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10>
	bool GetParams(P1& p1, P2& p2, P3& p3, P4& p4, P5& p5, P6& p6, P7& p7, P8& p8, P9& p9, P10& p10)
	{
		if (!GetParam(1, p1) || !GetParam(2, p2) || !GetParam(3, p3) || !GetParam(4, p4)) return false;
		if (!GetParam(5, p5) || !GetParam(6, p6) || !GetParam(7, p7) || !GetParam(8, p8)) return false;
		if (!GetParam(9, p9) || !GetParam(10, p10)) return false;
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	// To be removed later (FC Compatability).
	//////////////////////////////////////////////////////////////////////////
	/*
	   bool GetParamUDVal(int nIdx,ULONG_PTR &nValue,int &nCookie)
	   {
	    ScriptUserData ud;
	    if (!GetParam( nIdx,ud ))
	      return false;
	    nValue = ud.nVal;
	    nCookie = ud.nCookie;
	    return true;
	   }
	   bool GetParamUDVal(int nIdx,INT_PTR &nValue,int &nCookie)
	   {
	    ScriptUserData ud;
	    if (!GetParam( nIdx,ud ))
	      return false;
	    nValue = ud.nVal;
	    nCookie = ud.nCookie;
	    return true;
	   }
	 */
};

// Under development.
struct ScriptDebugInfo
{
	const char* sSourceName;
	int         nCurrentLine;
};

// Under development.
struct IScriptDebugSink
{
	// <interfuscator:shuffle>
	virtual ~IScriptDebugSink(){}
	virtual void OnLoadSource(const char* sSourceName, unsigned char* sSource, long nSourceSize) = 0;
	virtual void OnExecuteLine(ScriptDebugInfo& sdiDebugInfo) = 0;
	// </interfuscator:shuffle>
};

// Utility classes.

//! Helper for faster Set/Gets on the table.
class CScriptSetGetChain
{
public:
	CScriptSetGetChain(IScriptTable* pTable)
	{
		m_pTable = pTable;
		m_pTable->BeginSetGetChain();
	}
	~CScriptSetGetChain() { m_pTable->EndSetGetChain(); }

	void                         SetToNull(const char* sKey)                      { m_pTable->SetToNull(sKey); }
	template<class T> ILINE void SetValue(const char* sKey, const T& value) const { m_pTable->SetValueChain(sKey, value); }
	template<class T> ILINE bool GetValue(const char* sKey, T& value) const       { return m_pTable->GetValueChain(sKey, value); }

private:
	IScriptTable* m_pTable;
};

#include "ScriptHelpers.h"

//! After SmartScriptTable defined, now implement ScriptAnyValue constructor for it.
inline ScriptAnyValue::ScriptAnyValue(IScriptTable* value) : type(ANY_TTABLE)
{
	table = value;
	if (table)
		table->AddRef();
};

inline ScriptAnyValue::ScriptAnyValue(const SmartScriptTable& value) : type(ANY_TTABLE)
{
	table = value;
	if (table)
		table->AddRef();
};

inline ScriptAnyValue::ScriptAnyValue(IScriptTable* _table, int) : type(ANY_TTABLE)
{
	table = _table;
	if (table)
		table->AddRef();
};

inline ScriptAnyValue::ScriptAnyValue(const SmartScriptTable& value, int) : type(ANY_TTABLE)
{
	table = value;
	if (table)
		table->AddRef();
};

inline bool ScriptAnyValue::CopyTo(IScriptTable*& value) const
{
	if (type == ANY_TTABLE) { value = table; return true; };
	return false;
};

inline bool ScriptAnyValue::CopyTo(SmartScriptTable& value) const
{
	if (type == ANY_TTABLE) { value = table; return true; };
	return false;
};

inline bool ScriptAnyValue::CopyFromTableToXYZ(float& x, float& y, float& z) const
{
	if (type == ANY_TTABLE)
	{
		const char* const coords[3] = { "x", "y", "z" };
		float xyz[3];
		ScriptAnyValue anyValue;

		for (size_t i = 0; i < 3; ++i)
		{
			if (table->GetValueAny(coords[i], anyValue) && anyValue.type == ANY_TNUMBER)
			{
				anyValue.CopyTo(xyz[i]);
			}
			else
			{
				return false;
			}
		}
		x = xyz[0];
		y = xyz[1];
		z = xyz[2];
		return true;
	}
	return false;
};

inline void ScriptAnyValue::Clear()
{
	if (type == ANY_TTABLE && table)
	{
		table->Release();
	}
	else if (type == ANY_TFUNCTION && function)
	{
		gEnv->pScriptSystem->ReleaseFunc(function);
	}
	function = 0;
	table = 0;
	type = ANY_ANY;
}

inline ScriptAnyValue::~ScriptAnyValue()
{
	Clear();
}

inline ScriptAnyValue::ScriptAnyValue(const ScriptAnyValue& rhs)
{
	type = rhs.type;
	switch (type)
	{
	case ANY_ANY:
		table = 0;
		break;
	case ANY_TBOOLEAN:
		b = rhs.b;
		break;
	case ANY_TFUNCTION:
		function = gEnv->pScriptSystem->AddFuncRef(rhs.function);
		break;
	case ANY_THANDLE:
		ptr = rhs.ptr;
		break;
	case ANY_TNIL:
		table = 0;
		break;
	case ANY_TNUMBER:
		number = rhs.number;
		break;
	case ANY_TSTRING:
		str = rhs.str;
		break;
	case ANY_TTABLE:
		table = rhs.table;
		if (table)
			table->AddRef();
		break;
	case ANY_TUSERDATA:
		break;
	case ANY_TVECTOR:
		vec3.x = rhs.vec3.x;
		vec3.y = rhs.vec3.y;
		vec3.z = rhs.vec3.z;
		break;
	}
}
inline void ScriptAnyValue::Swap(ScriptAnyValue& value)
{
	char temp[sizeof(ScriptAnyValue)];
	memcpy(temp, this, sizeof(ScriptAnyValue));
	memcpy(this, &value, sizeof(ScriptAnyValue));
	memcpy(&value, temp, sizeof(ScriptAnyValue));
}

inline ScriptAnyValue::ScriptAnyValue(HSCRIPTFUNCTION value) : type(ANY_TFUNCTION)
{
	function = gEnv->pScriptSystem->AddFuncRef(value);
}

inline bool ScriptAnyValue::CopyTo(HSCRIPTFUNCTION& value) const
{
	if (type == ANY_TFUNCTION)
	{
		value = gEnv->pScriptSystem->AddFuncRef(function);
		return true;
	}
	;
	return false;
}

inline bool ScriptAnyValue::operator==(const ScriptAnyValue& rhs) const
{
	// Comparing memory of union is a bad idea.
	bool result = type == rhs.type;
	if (result)
	{
		switch (type)
		{
		case ANY_TBOOLEAN:
			result = b == rhs.b;
			break;
		case ANY_TNUMBER:
			result = number == rhs.number;
			break;
		case ANY_TSTRING:
			result = str == rhs.str;
			break;
		case ANY_TVECTOR:
			result = vec3.x == rhs.vec3.x && vec3.y == rhs.vec3.y && vec3.z == rhs.vec3.z;
			break;
		case ANY_TTABLE:
			result = table == rhs.table;
			break;
		case ANY_THANDLE:
			result = ptr == rhs.ptr;
			break;
		case ANY_TFUNCTION:
			result = gEnv->pScriptSystem->CompareFuncRef(function, rhs.function);
			break;
		case ANY_TUSERDATA:
			result = ud.nRef == rhs.ud.nRef && ud.ptr == rhs.ud.ptr;
			break;
		}
	}
	return result;
}

#ifdef CRYSCRIPTSYSTEM_EXPORTS
	#define CRYSCRIPTSYSTEM_API DLL_EXPORT
#else // CRYSCRIPTSYSTEM_EXPORTS
	#define CRYSCRIPTSYSTEM_API DLL_IMPORT
#endif // CRYSCRIPTSYSTEM_EXPORTS

extern "C"
{
	CRYSCRIPTSYSTEM_API IScriptSystem* CreateScriptSystem(ISystem* pSystem, bool bStdLibs);
}
typedef IScriptSystem*(* CREATESCRIPTSYSTEM_FNCPTR)(ISystem* pSystem, bool bStdLibs);
