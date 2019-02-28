// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CryCore/CryVariant.h>
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

	ScriptHandle() : ptr(0) {}
	ScriptHandle(int i) : n(i) {}
	ScriptHandle(void* p) : ptr(p) {}

	bool operator==(const ScriptHandle& rhs) const { return ptr == rhs.ptr; }
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

	ScriptUserData() : ptr(nullptr), nRef(0) {}
	ScriptUserData(void* const _ptr, int ref) : ptr(_ptr), nRef(ref) {}

	bool operator==(const ScriptUserData& rhs) const { return ptr == rhs.ptr && nRef == rhs.nRef; }
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
enum class EScriptAnyType
{
	Any = 0,
	Nil,
	Boolean,
	Handle,
	Number,
	String,
	Table,
	Function,
	UserData,
	Vector,
};

struct ScriptAnyValue
{
	~ScriptAnyValue();  // Implemented at the end of header.

	ScriptAnyValue() {}
	ScriptAnyValue(EScriptAnyType type); // Implemented at the end of header.
	ScriptAnyValue(bool bValue) : m_data(bValue) {}
	ScriptAnyValue(int value) : m_data(static_cast<float>(value)) {}
	ScriptAnyValue(unsigned int value) : m_data(static_cast<float>(value)) {}
	ScriptAnyValue(float value) : m_data(value) {}
	ScriptAnyValue(const char* value) : m_data(value) {}
	ScriptAnyValue(ScriptHandle value) : m_data(value) {}
	ScriptAnyValue(HSCRIPTFUNCTION value);
	ScriptAnyValue(const Vec3& value) : m_data(value) {}
	ScriptAnyValue(const Ang3& value) : m_data(static_cast<Vec3>(value)) {}
	ScriptAnyValue(const ScriptUserData& value) : m_data(value) {}
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

	bool CopyTo(bool& value) const             { if (GetType() == EScriptAnyType::Boolean)  { value = GetBool(); return true; } return false; }
	bool CopyTo(int& value) const              { if (GetType() == EScriptAnyType::Number)   { value = static_cast<int>(GetNumber()); return true; } return false; }
	bool CopyTo(unsigned int& value) const     { if (GetType() == EScriptAnyType::Number)   { value = static_cast<unsigned int>(GetNumber()); return true; } return false; }
	bool CopyTo(float& value) const            { if (GetType() == EScriptAnyType::Number)   { value = GetNumber(); return true; } return false; }
	bool CopyTo(const char*& value) const      { if (GetType() == EScriptAnyType::String)   { value = GetString(); return true; } return false; }
	bool CopyTo(char*& value) const            { if (GetType() == EScriptAnyType::String)   { value = const_cast<char*>(GetString()); return true; } return false; }
	bool CopyTo(string& value) const           { if (GetType() == EScriptAnyType::String)   { value = GetString(); return true; } return false; }
	bool CopyTo(ScriptHandle& value) const     { if (GetType() == EScriptAnyType::Handle)   { value = GetScriptHandle(); return true; } return false; }
	bool CopyTo(HSCRIPTFUNCTION& value) const;
	bool CopyTo(Vec3& value) const             { if (GetType() == EScriptAnyType::Vector)   { value = GetVector(); return true; } return false; }
	bool CopyTo(Ang3& value) const             { if (GetType() == EScriptAnyType::Vector)   { value = GetVector(); return true; } return false; }
	bool CopyTo(ScriptUserData &value)         { if (GetType() == EScriptAnyType::UserData) { value = GetUserData(); return true; } return false; }
	bool CopyTo(IScriptTable*& value) const;    // Implemented at the end of header.
	bool CopyTo(SmartScriptTable& value) const; // Implemented at the end of header.
	bool CopyFromTableToXYZ(float& x, float& y, float& z) const;
	bool CopyFromTableTo(Vec3& value) const    { return CopyFromTableToXYZ(value.x, value.y, value.z); }
	bool CopyFromTableTo(Ang3& value) const    { return CopyFromTableToXYZ(value.x, value.y, value.z); }

	//! Clears any variable to uninitialized state.
	// Implemented at the end of header.
	void Clear();

	//! Only initialize type.
	ScriptAnyValue(bool, int) { m_data.emplace<bool>(); }
	ScriptAnyValue(int, int) { m_data.emplace<float>(); }
	ScriptAnyValue(unsigned int, int) { m_data.emplace<float>(); }
	ScriptAnyValue(float&, int) { m_data.emplace<float>(); }
	ScriptAnyValue(const char*, int) { m_data.emplace<const char*>(); }
	ScriptAnyValue(ScriptHandle, int) { m_data.emplace<ScriptHandle>(); }
	ScriptAnyValue(HSCRIPTFUNCTION, int) { m_data.emplace<HSCRIPTFUNCTION>(); }
	ScriptAnyValue(Vec3&, int) { m_data.emplace<Vec3>(); }
	ScriptAnyValue(Ang3&, int) { m_data.emplace<Vec3>(); }
	ScriptAnyValue(ScriptUserData, int) { m_data.emplace<ScriptUserData>(); }
	ScriptAnyValue(IScriptTable* _table, int);
	ScriptAnyValue(const SmartScriptTable& value, int);

	ScriptVarType GetVarType() const
	{
		switch (GetType())
		{
		case EScriptAnyType::Any:
			return svtNull;
		case EScriptAnyType::Nil:
			return svtNull;
		case EScriptAnyType::Boolean:
			return svtBool;
		case EScriptAnyType::Handle:
			return svtPointer;
		case EScriptAnyType::Number:
			return svtNumber;
		case EScriptAnyType::String:
			return svtString;
		case EScriptAnyType::Table:
			return svtObject;
		case EScriptAnyType::Function:
			return svtFunction;
		case EScriptAnyType::UserData:
			return svtUserData;
		case EScriptAnyType::Vector:
			return svtObject;
		default:
			return svtNull;
		}
	}

	void GetMemoryUsage(ICrySizer* pSizer) const {}

	EScriptAnyType GetType() const
	{
		return static_cast<EScriptAnyType>(m_data.index());;
	}

	void SetAny()
	{
		m_data.emplace<static_cast<size_t>(EScriptAnyType::Any)>(nullptr);
	}

	void SetNil()
	{
		m_data.emplace<static_cast<size_t>(EScriptAnyType::Nil)>(nullptr);
	}

	bool GetBool() const
	{
		CRY_ASSERT(GetType() == EScriptAnyType::Boolean && stl::holds_alternative<bool>(m_data));
		return stl::get<bool>(m_data);
	}
	void SetBool(bool bValue)
	{
		m_data = bValue;
	}

	ScriptHandle GetScriptHandle() const
	{
		CRY_ASSERT(GetType() == EScriptAnyType::Handle && stl::holds_alternative<ScriptHandle>(m_data));
		return stl::get<ScriptHandle>(m_data);
	}
	void SetScriptHandle(ScriptHandle value)
	{
		m_data = value;
	}

	float GetNumber() const
	{
		CRY_ASSERT(GetType() == EScriptAnyType::Number && stl::holds_alternative<float>(m_data));
		return stl::get<float>(m_data);
	}
	void SetNumber(float value)
	{
		m_data = value;
	}

	const char* GetString() const
	{
		CRY_ASSERT(GetType() == EScriptAnyType::String && stl::holds_alternative<const char*>(m_data));
		return stl::get<const char*>(m_data);
	}
	void SetString(const char* szValue)
	{
		m_data = szValue;
	}

	IScriptTable* GetScriptTable()
	{
		CRY_ASSERT(GetType() == EScriptAnyType::Table && stl::holds_alternative<IScriptTable*>(m_data));
		return stl::get<IScriptTable*>(m_data);
	}
	IScriptTable* GetScriptTable() const
	{
		return const_cast<ScriptAnyValue*>(this)->GetScriptTable();
	}
	void SetScriptTable(IScriptTable* const pValue)
	{
		m_data = pValue;
	}

	HSCRIPTFUNCTION GetScriptFunction()
	{
		CRY_ASSERT(GetType() == EScriptAnyType::Function && stl::holds_alternative<HSCRIPTFUNCTION>(m_data));
		return stl::get<HSCRIPTFUNCTION>(m_data);
	}
	const HSCRIPTFUNCTION GetScriptFunction() const
	{
		return const_cast<ScriptAnyValue*>(this)->GetScriptFunction();
	}
	void SetScriptFunction(HSCRIPTFUNCTION value)
	{
		m_data = value;
	}

	const ScriptUserData& GetUserData() const
	{
		CRY_ASSERT(GetType() == EScriptAnyType::UserData && stl::holds_alternative<ScriptUserData>(m_data));
		return stl::get<ScriptUserData>(m_data);
	}
	void SetUserData(const ScriptUserData& value)
	{
		m_data = value;
	}

	const Vec3& GetVector() const
	{
		CRY_ASSERT(GetType() == EScriptAnyType::Vector && stl::holds_alternative<Vec3>(m_data));
		return stl::get<Vec3>(m_data);
	}
	void SetVector(const Vec3& value)
	{
		m_data = value;
	}

	private:
		typedef CryVariant<
			void*,
			std::nullptr_t,
			bool,
			ScriptHandle,
			float,
			const char*,
			IScriptTable*,
			HSCRIPTFUNCTION,
			ScriptUserData,
			Vec3
		> TScriptVariant;

		static_assert(std::is_same<stl::variant_alternative<static_cast<size_t>(EScriptAnyType::Any), TScriptVariant>::type, void*>::value, "Enum value and variant index do not match!");
		static_assert(std::is_same<stl::variant_alternative<static_cast<size_t>(EScriptAnyType::Nil), TScriptVariant>::type, std::nullptr_t>::value, "Enum value and variant index do not match!");
		static_assert(std::is_same<stl::variant_alternative<static_cast<size_t>(EScriptAnyType::Boolean), TScriptVariant>::type, bool>::value, "Enum value and variant index do not match!");
		static_assert(std::is_same<stl::variant_alternative<static_cast<size_t>(EScriptAnyType::Number), TScriptVariant>::type, float>::value, "Enum value and variant index do not match!");
		static_assert(std::is_same<stl::variant_alternative<static_cast<size_t>(EScriptAnyType::String), TScriptVariant>::type, const char*>::value, "Enum value and variant index do not match!");
		static_assert(std::is_same<stl::variant_alternative<static_cast<size_t>(EScriptAnyType::Table), TScriptVariant>::type, IScriptTable*>::value, "Enum value and variant index do not match!");
		static_assert(std::is_same<stl::variant_alternative<static_cast<size_t>(EScriptAnyType::Function), TScriptVariant>::type, HSCRIPTFUNCTION>::value, "Enum value and variant index do not match!");
		static_assert(std::is_same<stl::variant_alternative<static_cast<size_t>(EScriptAnyType::UserData), TScriptVariant>::type, ScriptUserData>::value, "Enum value and variant index do not match!");
		static_assert(std::is_same<stl::variant_alternative<static_cast<size_t>(EScriptAnyType::Vector), TScriptVariant>::type, Vec3>::value, "Enum value and variant index do not match!");

		TScriptVariant m_data;
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
	virtual void          SetGlobalToNull(const char* sKey) { SetGlobalAny(sKey, ScriptAnyValue(EScriptAnyType::Nil)); }

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

		switch (any.GetType())
		{
		case EScriptAnyType::Table:
			if (any.GetScriptTable())
			{
				any.GetScriptTable()->Release();
				any.SetScriptTable(nullptr);
			}
			return true;
		case EScriptAnyType::Function:
			if (any.GetScriptFunction())
			{
				gEnv->pScriptSystem->ReleaseFunc(any.GetScriptFunction());
				any.SetScriptFunction(nullptr);
			}
			return true;
		case EScriptAnyType::Nil:
			return false;
		default:
			return true;
		}
	}
	//! Set member value to nil.
	void SetToNull(const char* sKey) { SetValueAny(sKey, ScriptAnyValue(EScriptAnyType::Nil)); }

	//! Set value of a table member.
	template<class T> void SetValueChain(const char* sKey, const T& value) { SetValueAny(sKey, value, true); }

	//! Get value of a table member.
	template<class T> bool GetValueChain(const char* sKey, T& value)
	{
		ScriptAnyValue any(value, 0);
		return GetValueAny(sKey, any, true) && any.CopyTo(value);
	}
	void SetToNullChain(const char* sKey) { SetValueChain(sKey, ScriptAnyValue(EScriptAnyType::Nil)); }

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
		return any.GetType() != EScriptAnyType::Nil;
	}
	//! Set the value of a member variable to nil at the specified index.
	void SetNullAt(int nIndex) { SetAtAny(nIndex, ScriptAnyValue(EScriptAnyType::Nil)); }

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

inline ScriptAnyValue::ScriptAnyValue(EScriptAnyType type)
{
	switch (type)
	{
	case EScriptAnyType::Any:
		SetAny();
		break;
	case EScriptAnyType::Nil:
		SetNil();
		break;
	case EScriptAnyType::Boolean:
		m_data.emplace<bool>();
		break;
	case EScriptAnyType::Handle:
		m_data.emplace<ScriptHandle>();
		break;
	case EScriptAnyType::Number:
		m_data.emplace<float>();
		break;
	case EScriptAnyType::String:
		m_data.emplace<const char*>();
		break;
	case EScriptAnyType::Table:
		m_data.emplace<IScriptTable*>();
		break;
	case EScriptAnyType::Function:
		m_data.emplace<HSCRIPTFUNCTION>();
		break;
	case EScriptAnyType::UserData:
		m_data.emplace<ScriptUserData>();
		break;
	case EScriptAnyType::Vector:
		m_data.emplace<Vec3>();
		break;
	default:
		CRY_ASSERT(false);
		break;
	}
}

//! After SmartScriptTable defined, now implement ScriptAnyValue constructor for it.
inline ScriptAnyValue::ScriptAnyValue(IScriptTable* value)
{
	if (value)
		value->AddRef();
	SetScriptTable(value);
};

inline ScriptAnyValue::ScriptAnyValue(const SmartScriptTable& value)
{
	if (value)
		value->AddRef();
	SetScriptTable(value);
};

inline ScriptAnyValue::ScriptAnyValue(IScriptTable* _table, int)
{
	if (_table)
		_table->AddRef();
	SetScriptTable(_table);
};

inline ScriptAnyValue::ScriptAnyValue(const SmartScriptTable& value, int)
{
	if (value)
		value->AddRef();
	SetScriptTable(value);
};

inline bool ScriptAnyValue::CopyTo(IScriptTable*& value) const
{
	if (GetType() == EScriptAnyType::Table)
	{
		value = GetScriptTable();
		return true;
	}
	return false;
};

inline bool ScriptAnyValue::CopyTo(SmartScriptTable& value) const
{
	if (GetType() == EScriptAnyType::Table)
	{
		value = GetScriptTable();
		return true;
	}
	return false;
};

inline bool ScriptAnyValue::CopyFromTableToXYZ(float& x, float& y, float& z) const
{
	if (GetType() == EScriptAnyType::Table)
	{
		const char* const coords[3] = { "x", "y", "z" };
		float xyz[3];
		ScriptAnyValue anyValue;

		for (size_t i = 0; i < 3; ++i)
		{
			if (GetScriptTable()->GetValueAny(coords[i], anyValue) && anyValue.GetType() == EScriptAnyType::Number)
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
	if (GetType() == EScriptAnyType::Table && GetScriptTable())
	{
		GetScriptTable()->Release();
	}
	else if (GetType() == EScriptAnyType::Function && GetScriptFunction())
	{
		gEnv->pScriptSystem->ReleaseFunc(GetScriptFunction());
	}
	SetAny();
}

inline ScriptAnyValue::~ScriptAnyValue()
{
	Clear();
}

inline ScriptAnyValue::ScriptAnyValue(const ScriptAnyValue& rhs)
{
	switch (rhs.GetType())
	{
	case EScriptAnyType::Any:
		break;
	case EScriptAnyType::Boolean:
		SetBool(rhs.GetBool());
		break;
	case EScriptAnyType::Function:
		SetScriptFunction(gEnv->pScriptSystem->AddFuncRef(rhs.GetScriptFunction()));
		break;
	case EScriptAnyType::Handle:
		SetScriptHandle(rhs.GetScriptHandle());
		break;
	case EScriptAnyType::Nil:
		SetNil();
		break;
	case EScriptAnyType::Number:
		SetNumber(rhs.GetNumber());
		break;
	case EScriptAnyType::String:
		SetString(rhs.GetString());
		break;
	case EScriptAnyType::Table:
		SetScriptTable(rhs.GetScriptTable());
		if (GetScriptTable())
			GetScriptTable()->AddRef();
		break;
	case EScriptAnyType::UserData:
		break;
	case EScriptAnyType::Vector:
		SetVector(rhs.GetVector());
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

inline ScriptAnyValue::ScriptAnyValue(HSCRIPTFUNCTION value)
{
	SetScriptFunction(gEnv->pScriptSystem->AddFuncRef(value));
}

inline bool ScriptAnyValue::CopyTo(HSCRIPTFUNCTION& value) const
{
	if (GetType() == EScriptAnyType::Function)
	{
		value = gEnv->pScriptSystem->AddFuncRef(GetScriptFunction());
		return true;
	}

	return false;
}

inline bool ScriptAnyValue::operator==(const ScriptAnyValue& rhs) const
{
	// Comparing memory of union is a bad idea.
	bool result = GetType() == rhs.GetType();
	if (result)
	{
		switch (GetType())
		{
		case EScriptAnyType::Boolean:
		case EScriptAnyType::Number:
		case EScriptAnyType::String:
		case EScriptAnyType::Vector:
		case EScriptAnyType::Handle:
		case EScriptAnyType::Table:
		case EScriptAnyType::UserData:
			result = (m_data == rhs.m_data);
			break;

		case EScriptAnyType::Function:
			result = gEnv->pScriptSystem->CompareFuncRef(GetScriptFunction(), rhs.GetScriptFunction());
			break;

		default:
			CRY_ASSERT(false);
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

//! \endcond