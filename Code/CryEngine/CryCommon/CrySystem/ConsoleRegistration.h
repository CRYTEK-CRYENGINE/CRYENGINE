// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ISystem.h"
#include "IConsole.h"

#if (defined(_LAUNCHER) && defined(CRY_IS_MONOLITHIC_BUILD)) || !defined(_LIB)
extern std::vector<const char*> g_moduleCommands;
extern std::vector<const char*> g_moduleCVars;
	#define MODULE_REGISTER_COMMAND(name) g_moduleCommands.push_back(name)
	#define MODULE_REGISTER_CVAR(name)    g_moduleCVars.push_back(name)
#else
	#define MODULE_REGISTER_COMMAND(name) static_cast<void>(0)
	#define MODULE_REGISTER_CVAR(name)    static_cast<void>(0)
#endif

struct ConsoleRegistrationHelper
{
	static void AddCommand(const char* szCommand, ConsoleCommandFunc func, int flags = 0, const char* szHelp = nullptr, bool bIsManagedExternally = false)
	{
		CRY_ASSERT(gEnv && gEnv->pConsole);
		MODULE_REGISTER_COMMAND(szCommand);
		gEnv->pConsole->AddCommand(szCommand, func, flags, szHelp, bIsManagedExternally);
	}
	static void AddCommand(const char* szName, const char* szScriptFunc, int flags = 0, const char* szHelp = nullptr)
	{
		CRY_ASSERT(gEnv && gEnv->pConsole);
		MODULE_REGISTER_COMMAND(szName);
		gEnv->pConsole->AddCommand(szName, szScriptFunc, flags, szHelp);
	}

	static ICVar* RegisterString(const char* szName, const char* szValue, int flags, const char* szHelp = "", ConsoleVarFunc pChangeFunc = nullptr)
	{
		CRY_ASSERT(gEnv && gEnv->pConsole);
		if (gEnv && gEnv->pConsole)
		{
			MODULE_REGISTER_CVAR(szName);
			return gEnv->pConsole->RegisterString(szName, szValue, flags, szHelp, pChangeFunc);
		}
		else
		{
			return nullptr;
		}
	}
	static ICVar* RegisterInt(const char* szName, int value, int flags, const char* szHelp = "", ConsoleVarFunc pChangeFunc = nullptr)
	{
		CRY_ASSERT(gEnv && gEnv->pConsole);
		if (gEnv && gEnv->pConsole)
		{
			MODULE_REGISTER_CVAR(szName);
			return gEnv->pConsole->RegisterInt(szName, value, flags, szHelp, pChangeFunc);
		}
		else
		{
			return nullptr;
		}
	}
	static ICVar* RegisterInt64(const char* szName, int64 value, int flags, const char* szHelp = "", ConsoleVarFunc pChangeFunc = nullptr)
	{
		CRY_ASSERT(gEnv && gEnv->pConsole);
		if (gEnv && gEnv->pConsole)
		{
			MODULE_REGISTER_CVAR(szName);
			return gEnv->pConsole->RegisterInt64(szName, value, flags, szHelp, pChangeFunc);
		}
		else
		{
			return nullptr;
		}
	}
	static ICVar* RegisterFloat(const char* szName, float value, int flags, const char* szHelp = "", ConsoleVarFunc pChangeFunc = nullptr)
	{
		CRY_ASSERT(gEnv && gEnv->pConsole);
		if (gEnv && gEnv->pConsole)
		{
			MODULE_REGISTER_CVAR(szName);
			return gEnv->pConsole->RegisterFloat(szName, value, flags, szHelp, pChangeFunc);
		}
		else
		{
			return nullptr;
		}
	}

	template<class T, class U>
	static ICVar* Register(const char* szName, T* pSrc, U defaultValue, int flags = 0, const char* szHelp = "", ConsoleVarFunc pChangeFunc = nullptr, bool bAllowModify = true)
	{
		return RegisterImpl(get_enum_tag<T>(), szName, pSrc, defaultValue, flags, szHelp, pChangeFunc, bAllowModify);
	}

	static ICVar* Register(ICVar* pVar)
	{
		CRY_ASSERT(gEnv && gEnv->pConsole);
		if (gEnv && gEnv->pConsole)
		{
			MODULE_REGISTER_CVAR(pVar->GetName());
			return gEnv->pConsole->Register(pVar);
		}
		else
		{
			return nullptr;
		}
	}

private:
	struct enum_tag {};
	struct non_enum_tag {};

	template<class T>
	struct get_enum_tag : std::conditional<std::is_enum<T>::value, enum_tag, non_enum_tag>::type {};

	template<class T, class U>
	static ICVar* RegisterImpl(enum_tag, const char* szName, T* pSrc, U defaultValue, int flags = 0, const char* szHelp = "", ConsoleVarFunc pChangeFunc = nullptr, bool bAllowModify = true)
	{
		using ET = typename std::underlying_type<T>::type;
		static_assert(std::is_same<ET, int>::value, "Invalid template type!");
		return RegisterImpl(non_enum_tag(), szName, reinterpret_cast<ET*>(pSrc), static_cast<ET>(defaultValue), flags, szHelp, pChangeFunc, bAllowModify);
	}

	template<class T, class U>
	static ICVar* RegisterImpl(non_enum_tag, const char* szName, T* pSrc, U defaultValue, int flags = 0, const char* szHelp = "", ConsoleVarFunc pChangeFunc = nullptr, bool bAllowModify = true)
	{
		static_assert(std::is_same<T, int>::value || std::is_same<T, float>::value || std::is_same<T, const char*>::value, "Invalid template type!");
		static_assert(std::is_convertible<U, T>::value, "Invalid default value type!");
		CRY_ASSERT(gEnv && gEnv->pConsole);
		if (gEnv && gEnv->pConsole)
		{
			MODULE_REGISTER_CVAR(szName);
			return gEnv->pConsole->Register(szName, pSrc, static_cast<T>(defaultValue), flags, szHelp, pChangeFunc, bAllowModify);
		}
		else
		{
			return nullptr;
		}
	}
};

#undef MODULE_REGISTER_COMMAND
#undef MODULE_REGISTER_CVAR

#if defined(RELEASE) && !CRY_PLATFORM_DESKTOP
	#ifndef LOG_CONST_CVAR_ACCESS
		#error LOG_CONST_CVAR_ACCESS should be defined in ProjectDefines.h
	#endif

#include <CrySystem/CVarOverride.h>

namespace Detail
{
template<typename T>
struct SQueryTypeEnum;
template<>
struct SQueryTypeEnum<int>
{
	static const ECVarType type = ECVarType::Int;
	static int ParseString(const char* s) { return atoi(s); }
};
template<>
struct SQueryTypeEnum<float>
{
	static const ECVarType type = ECVarType::Float;
	static float ParseString(const char* s) { return (float)atof(s); }
};

template<typename T>
struct SDummyCVar : ICVar
{
	const T      value;
	#if LOG_CONST_CVAR_ACCESS
	mutable bool bWasRead;
	mutable bool bWasChanged;
	SDummyCVar(T value) : value(value), bWasChanged(false), bWasRead(false) {}
	#else
	SDummyCVar(T value) : value(value) {}
	#endif

	void WarnUse() const
	{
	#if LOG_CONST_CVAR_ACCESS
		if (!bWasRead)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[CVAR] Read from const CVar '%s' via name look-up, this is non-optimal", GetName());
			bWasRead = true;
		}
	#endif
	}

	void InvalidAccess() const
	{
	#if LOG_CONST_CVAR_ACCESS
		if (!bWasChanged)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[CVAR] Write to const CVar '%s' with wrong value '%f' was ignored. This indicates a bug in code or a config file", GetName(), GetFVal());
			bWasChanged = true;
		}
	#endif
	}

	void                         Release() override                                              {}
	int                          GetIVal() const override                                        { WarnUse(); return static_cast<int>(value); }
	int64                        GetI64Val() const override                                      { WarnUse(); return static_cast<int64>(value); }
	float                        GetFVal() const override                                        { WarnUse(); return static_cast<float>(value); }
	const char*                  GetString() const override                                      { return ""; }
	const char*                  GetDataProbeString() const override                             { return ""; }
	void                         Set(const char* s) override                                     { if (SQueryTypeEnum<T>::ParseString(s) != value) InvalidAccess(); }
	void                         ForceSet(const char* s) override                                { Set(s); }
	void                         Set(float f) override                                           { if (static_cast<T>(f) != value) InvalidAccess(); }
	void                         Set(int i) override                                             { if (static_cast<T>(i) != value) InvalidAccess(); }
	void                         Set(int64 i) override                                           { if (static_cast<T>(i) != value) InvalidAccess(); }
	void                         SetFromString(const char* szValue) override                     { Set(szValue); }
	void                         ClearFlags(int flags) override                                  {}
	int                          GetFlags() const override                                       { return VF_CONST_CVAR | VF_READONLY; }
	int                          SetFlags(int flags) override                                    { return 0; }
	ECVarType                    GetType() const override                                        { return SQueryTypeEnum<T>::type; }
	const char*                  GetHelp() const override                                        { return NULL; }
	bool                         IsConstCVar() const override                                    { return true; }
	uint64                       AddOnChange(SmallFunction<void()> /*changeFunctor*/) override   { return 0; }
	uint64                       GetNumberOfOnChangeFunctors() const override                    { return 0; }
	const SmallFunction<void()>& GetOnChangeFunctor(uint64 nFunctorIndex) const override         { InvalidAccess();  static SmallFunction<void()> oDummy; return oDummy; }
	bool                         RemoveOnChangeFunctor(const uint64 nElement) override           { return true; }
	void                         GetMemoryUsage(class ICrySizer* pSizer) const override          {}
	int                          GetRealIVal() const override                                    { return GetIVal(); }
	void                         SetDataProbeString(const char* pDataProbeString)                { InvalidAccess(); }
	void                         SetMinValue(int min) override                                   {}
	void                         SetMinValue(int64 min) override                                 {}
	void                         SetMinValue(float min) override                                 {}
	void                         SetMaxValue(int max) override                                   {}
	void                         SetMaxValue(int64 max) override                                 {}
	void                         SetMaxValue(float max) override                                 {}
	void                         SetAllowedValues(std::initializer_list<int> values) override    {}
	void                         SetAllowedValues(std::initializer_list<int64> values) override  {}
	void                         SetAllowedValues(std::initializer_list<float> values) override  {}
	void                         SetAllowedValues(std::initializer_list<string> values) override {}
	bool                         IsOwnedByConsole() const override                               { return true; }
};
}

	#define REGISTER_DUMMY_CVAR(type, name, value)                                                      \
	do {                                                                                                \
		static struct DummyCVar : Detail::SDummyCVar<type>                                                \
		{                                                                                                 \
			DummyCVar() : Detail::SDummyCVar<type>(value) {}                                                \
			const char* GetName() const { return name; }                                                    \
		} DummyStaticInstance;                                                                            \
		if (!(gEnv->pConsole != nullptr ? ConsoleRegistrationHelper::Register(&DummyStaticInstance) : 0)) \
		{                                                                                                 \
			__debugbreak();                                                                                 \
			CryFatalError("Can not register dummy CVar");                                                   \
		}                                                                                                 \
	} while (0)

	#define CONSOLE_CONST_CVAR_MODE
	#define DeclareConstIntCVar(name, defaultValue)                          enum : int { name = GetCVarOverride( # name, defaultValue) }
	#define DeclareStaticConstIntCVar(name, defaultValue)                    enum : int { name = GetCVarOverride( # name, defaultValue) }
	#define DefineConstIntCVarName(strname, name, defaultValue, flags, help) { static_assert(static_cast<int>(GetCVarOverride( # name, defaultValue)) == static_cast<int>(name), "Unexpected value!"); REGISTER_DUMMY_CVAR(int, strname, GetCVarOverride( # name, defaultValue)); }
	#define DefineConstIntCVar(name, defaultValue, flags, help)              { static_assert(static_cast<int>(GetCVarOverride( # name, defaultValue)) == static_cast<int>(name), "Unexpected value!"); REGISTER_DUMMY_CVAR(int, ( # name), GetCVarOverride( # name, defaultValue)); }

//! DefineConstIntCVar2 is deprecated, any such instance can be converted to the 3 variant by removing the quotes around the first parameter.
	#define DefineConstIntCVar3(name, _var_, defaultValue, flags, help) { static_assert(static_cast<int>(GetCVarOverride( # name, defaultValue)) == static_cast<int>(_var_), "Unexpected value!"); REGISTER_DUMMY_CVAR(int, name, GetCVarOverride( # name, defaultValue)); }
	#define AllocateConstIntCVar(scope, name)

	#define DefineConstFloatCVar(name, flags, help) { REGISTER_DUMMY_CVAR(float, ( # name), GetCVarOverride( # name, name ## Default) ); }
	#define DeclareConstFloatCVar(name) static constexpr float name = GetCVarOverride( # name, name ## Default)
	#define DeclareStaticConstFloatCVar(name) DeclareConstFloatCVar(name)
	#define AllocateConstFloatCVar(scope, name)

#else

	#define DeclareConstIntCVar(name, defaultValue)                          int name
	#define DeclareStaticConstIntCVar(name, defaultValue)                    static int name
	#define DefineConstIntCVarName(strname, name, defaultValue, flags, help) ConsoleRegistrationHelper::Register(strname, &name, defaultValue, flags | CONST_CVAR_FLAGS, CVARHELP(help))
	#define DefineConstIntCVar(name, defaultValue, flags, help)              ConsoleRegistrationHelper::Register(( # name), &name, defaultValue, flags | CONST_CVAR_FLAGS, CVARHELP(help), nullptr, false)

//! DefineConstIntCVar2 is deprecated, any such instance can be converted to the 3 variant by removing the quotes around the first parameter.
	#define DefineConstIntCVar3(_name, _var, _def_val, _flags, help) ConsoleRegistrationHelper::Register(_name, &(_var), (_def_val), (_flags) | CONST_CVAR_FLAGS, CVARHELP(help), nullptr, false)
	#define AllocateConstIntCVar(scope, name)                        int scope::name

	#define DefineConstFloatCVar(name, flags, help)                  ConsoleRegistrationHelper::Register(( # name), &name, name ## Default, flags | CONST_CVAR_FLAGS, CVARHELP(help), nullptr, false)
	#define DeclareConstFloatCVar(name)                              float name
	#define DeclareStaticConstFloatCVar(name)                        static float name
	#define AllocateConstFloatCVar(scope, name)                      float scope::name
#endif

//! The following macros allow the help text to be easily stripped out.

//! Preferred way to register a CVar
#define REGISTER_CVAR(_var, _def_val, _flags, _comment) ConsoleRegistrationHelper::Register(( # _var), &(_var), (_def_val), (_flags), CVARHELP(_comment))

//! Preferred way to register a CVar with a callback
#define REGISTER_CVAR_CB(_var, _def_val, _flags, _comment, _onchangefunction) ConsoleRegistrationHelper::Register(( # _var), &(_var), (_def_val), (_flags), CVARHELP(_comment), _onchangefunction)

//! Preferred way to register a string CVar
#define REGISTER_STRING(_name, _def_val, _flags, _comment) ConsoleRegistrationHelper::RegisterString(_name, (_def_val), (_flags), CVARHELP(_comment))

//! Preferred way to register a string CVar with a callback
#define REGISTER_STRING_CB(_name, _def_val, _flags, _comment, _onchangefunction) ConsoleRegistrationHelper::RegisterString(_name, (_def_val), (_flags), CVARHELP(_comment), _onchangefunction)

//! Preferred way to register an int CVar
#define REGISTER_INT(_name, _def_val, _flags, _comment) ConsoleRegistrationHelper::RegisterInt(_name, (_def_val), (_flags), CVARHELP(_comment))

//! Preferred way to register an int CVar with a callback
#define REGISTER_INT_CB(_name, _def_val, _flags, _comment, _onchangefunction) ConsoleRegistrationHelper::RegisterInt(_name, (_def_val), (_flags), CVARHELP(_comment), _onchangefunction)

//! Preferred way to register an int64 CVar
#define REGISTER_INT64(_name, _def_val, _flags, _comment) ConsoleRegistrationHelper::RegisterInt64(_name, (_def_val), (_flags), CVARHELP(_comment))

//! Preferred way to register an int64 CVar with a callback
#define REGISTER_INT64_CB(_name, _def_val, _flags, _comment, _onchangefunction) ConsoleRegistrationHelper::RegisterInt64(_name, (_def_val), (_flags), CVARHELP(_comment), _onchangefunction)

//! Preferred way to register a float CVar
#define REGISTER_FLOAT(_name, _def_val, _flags, _comment) ConsoleRegistrationHelper::RegisterFloat(_name, (_def_val), (_flags), CVARHELP(_comment))

//! Offers more flexibility but more code is required
#define REGISTER_CVAR2(_name, _var, _def_val, _flags, _comment) ConsoleRegistrationHelper::Register(_name, _var, (_def_val), (_flags), CVARHELP(_comment))

//! Offers more flexibility but more code is required
#define REGISTER_CVAR2_CB(_name, _var, _def_val, _flags, _comment, _onchangefunction) ConsoleRegistrationHelper::Register(_name, _var, (_def_val), (_flags), CVARHELP(_comment), _onchangefunction)

//! Offers more flexibility but more code is required, explicit address taking of destination variable
#define REGISTER_CVAR3(_name, _var, _def_val, _flags, _comment) ConsoleRegistrationHelper::Register(_name, &(_var), (_def_val), (_flags), CVARHELP(_comment))

//! Offers more flexibility but more code is required, explicit address taking of destination variable
#define REGISTER_CVAR3_CB(_name, _var, _def_val, _flags, _comment, _onchangefunction) ConsoleRegistrationHelper::Register(_name, &(_var), (_def_val), (_flags), CVARHELP(_comment), _onchangefunction)

//! Preferred way to register a console command
#define REGISTER_COMMAND(_name, _func, _flags, _comment) ConsoleRegistrationHelper::AddCommand(_name, _func, (_flags), CVARHELP(_comment))

//! Preferred way to register an externally managed console command
#define REGISTER_MANAGED_COMMAND(_name, _func, _flags, _comment) ConsoleRegistrationHelper::AddCommand(_name, _func, (_flags), CVARHELP(_comment), true)

////////////////////////////////////////////////////////////////////////////////
//! Development only cvars.
//! (1) Registered as real cvars *in non release builds only*.
//! (2) Can still be manipulated in release by the mapped variable, so not the same as const cvars.
//! (3) Any 'OnChanged' callback will need guarding against in release builds since the cvar won't exist.
//! (4) Any code that tries to get ICVar* will need guarding against in release builds since the cvar won't exist.
//! ILLEGAL_DEV_FLAGS is a mask of all those flags which make no sense in a _DEV_ONLY or _DEDI_ONLY cvar since the.
//! Cvar potentially won't exist in a release build.
#define ILLEGAL_DEV_FLAGS (VF_NET_SYNCED | VF_CHEAT | VF_CHEAT_ALWAYS_CHECK | VF_CHEAT_NOCHECK | VF_READONLY | VF_CONST_CVAR)

#if defined(RELEASE)
	#define REGISTER_CVAR_DEV_ONLY(_var, _def_val, _flags, _comment)                               NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!"); _var = _def_val
	#define REGISTER_CVAR_CB_DEV_ONLY(_var, _def_val, _flags, _comment, _onchangefunction)         NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!"); _var = _def_val /* _onchangefunction consumed; callback not available */
	#define REGISTER_STRING_DEV_ONLY(_name, _def_val, _flags, _comment)                            NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")                  /* consumed; pure cvar not available */
	#define REGISTER_STRING_CB_DEV_ONLY(_name, _def_val, _flags, _comment, _onchangefunction)      NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")                  /* consumed; pure cvar not available */
	#define REGISTER_INT_DEV_ONLY(_name, _def_val, _flags, _comment)                               NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")                  /* consumed; pure cvar not available */
	#define REGISTER_INT_CB_DEV_ONLY(_name, _def_val, _flags, _comment, _onchangefunction)         NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")                  /* consumed; pure cvar not available */
	#define REGISTER_INT64_DEV_ONLY(_name, _def_val, _flags, _comment)                             NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")                  /* consumed; pure cvar not available */
	#define REGISTER_FLOAT_DEV_ONLY(_name, _def_val, _flags, _comment)                             NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")                  /* consumed; pure cvar not available */
	#define REGISTER_CVAR2_DEV_ONLY(_name, _var, _def_val, _flags, _comment)                       NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!"); *(_var) = _def_val
	#define REGISTER_CVAR2_CB_DEV_ONLY(_name, _var, _def_val, _flags, _comment, _onchangefunction) NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!"); *(_var) = _def_val
	#define REGISTER_CVAR3_DEV_ONLY(_name, _var, _def_val, _flags, _comment)                       NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!"); _var = _def_val
	#define REGISTER_CVAR3_CB_DEV_ONLY(_name, _var, _def_val, _flags, _comment, _onchangefunction) NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!"); _var = _def_val
	#define REGISTER_COMMAND_DEV_ONLY(_name, _func, _flags, _comment)                              /* consumed; command not available */
#else
	#define REGISTER_CVAR_DEV_ONLY(_var, _def_val, _flags, _comment)                               REGISTER_CVAR(_var, _def_val, ((_flags) | VF_DEV_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR_CB_DEV_ONLY(_var, _def_val, _flags, _comment, _onchangefunction)         REGISTER_CVAR_CB(_var, _def_val, ((_flags) | VF_DEV_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_STRING_DEV_ONLY(_name, _def_val, _flags, _comment)                            REGISTER_STRING(_name, _def_val, ((_flags) | VF_DEV_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_STRING_CB_DEV_ONLY(_name, _def_val, _flags, _comment, _onchangefunction)      REGISTER_STRING_CB(_name, _def_val, ((_flags) | VF_DEV_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_INT_DEV_ONLY(_name, _def_val, _flags, _comment)                               REGISTER_INT(_name, _def_val, ((_flags) | VF_DEV_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_INT_CB_DEV_ONLY(_name, _def_val, _flags, _comment, _onchangefunction)         REGISTER_INT_CB(_name, _def_val, ((_flags) | VF_DEV_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_INT64_DEV_ONLY(_name, _def_val, _flags, _comment)                             REGISTER_INT64(_name, _def_val, ((_flags) | VF_DEV_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_FLOAT_DEV_ONLY(_name, _def_val, _flags, _comment)                             REGISTER_FLOAT(_name, _def_val, ((_flags) | VF_DEV_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR2_DEV_ONLY(_name, _var, _def_val, _flags, _comment)                       REGISTER_CVAR2(_name, _var, _def_val, ((_flags) | VF_DEV_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR2_CB_DEV_ONLY(_name, _var, _def_val, _flags, _comment, _onchangefunction) REGISTER_CVAR2_CB(_name, _var, _def_val, ((_flags) | VF_DEV_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR3_DEV_ONLY(_name, _var, _def_val, _flags, _comment)                       REGISTER_CVAR3(_name, _var, _def_val, ((_flags) | VF_DEV_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR3_CB_DEV_ONLY(_name, _var, _def_val, _flags, _comment, _onchangefunction) REGISTER_CVAR3_CB(_name, _var, _def_val, ((_flags) | VF_DEV_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_COMMAND_DEV_ONLY(_name, _func, _flags, _comment)                              REGISTER_COMMAND(_name, _func, ((_flags) | VF_DEV_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
#endif // defined(RELEASE)
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//! Dedicated server only cvars
//! (1) Registered as real cvars in all non release builds
//! (2) Registered as real cvars in release on dedi servers only, otherwise treated as DEV_ONLY type cvars (see above)
#if defined(RELEASE) && defined(DEDICATED_SERVER)
	#define REGISTER_CVAR_DEDI_ONLY(_var, _def_val, _flags, _comment)                               REGISTER_CVAR(_var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR_CB_DEDI_ONLY(_var, _def_val, _flags, _comment, _onchangefunction)         REGISTER_CVAR_CB(_var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_STRING_DEDI_ONLY(_name, _def_val, _flags, _comment)                            REGISTER_STRING(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_STRING_CB_DEDI_ONLY(_name, _def_val, _flags, _comment, _onchangefunction)      REGISTER_STRING_CB(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_INT_DEDI_ONLY(_name, _def_val, _flags, _comment)                               REGISTER_INT(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_INT_CB_DEDI_ONLY(_name, _def_val, _flags, _comment, _onchangefunction)         REGISTER_INT_CB(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_INT64_DEDI_ONLY(_name, _def_val, _flags, _comment)                             REGISTER_INT64(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_FLOAT_DEDI_ONLY(_name, _def_val, _flags, _comment)                             REGISTER_FLOAT(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR2_DEDI_ONLY(_name, _var, _def_val, _flags, _comment)                       REGISTER_CVAR2(_name, _var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR2_CB_DEDI_ONLY(_name, _var, _def_val, _flags, _comment, _onchangefunction) REGISTER_CVAR2_CB(_name, _var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR3_DEDI_ONLY(_name, _var, _def_val, _flags, _comment)                       REGISTER_CVAR3(_name, _var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR3_CB_DEDI_ONLY(_name, _var, _def_val, _flags, _comment, _onchangefunction) REGISTER_CVAR3_CB(_name, _var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_COMMAND_DEDI_ONLY(_name, _func, _flags, _comment)                              REGISTER_COMMAND(_name, _func, ((_flags) | VF_DEDI_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
#else
	#define REGISTER_CVAR_DEDI_ONLY(_var, _def_val, _flags, _comment)                               REGISTER_CVAR_DEV_ONLY(_var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment)
	#define REGISTER_CVAR_CB_DEDI_ONLY(_var, _def_val, _flags, _comment, _onchangefunction)         REGISTER_CVAR_CB_DEV_ONLY(_var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction)
	#define REGISTER_STRING_DEDI_ONLY(_name, _def_val, _flags, _comment)                            REGISTER_STRING_DEV_ONLY(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment)
	#define REGISTER_STRING_CB_DEDI_ONLY(_name, _def_val, _flags, _comment, _onchangefunction)      REGISTER_STRING_CB_DEV_ONLY(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction)
	#define REGISTER_INT_DEDI_ONLY(_name, _def_val, _flags, _comment)                               REGISTER_INT_DEV_ONLY(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment)
	#define REGISTER_INT_CB_DEDI_ONLY(_name, _def_val, _flags, _comment, _onchangefunction)         REGISTER_INT_CB_DEV_ONLY(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction)
	#define REGISTER_INT64_DEDI_ONLY(_name, _def_val, _flags, _comment)                             REGISTER_INT64_DEV_ONLY(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment)
	#define REGISTER_FLOAT_DEDI_ONLY(_name, _def_val, _flags, _comment)                             REGISTER_FLOAT_DEV_ONLY(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment)
	#define REGISTER_CVAR2_DEDI_ONLY(_name, _var, _def_val, _flags, _comment)                       REGISTER_CVAR2_DEV_ONLY(_name, _var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment)
	#define REGISTER_CVAR2_CB_DEDI_ONLY(_name, _var, _def_val, _flags, _comment, _onchangefunction) REGISTER_CVAR2_CB_DEV_ONLY(_name, _var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction)
	#define REGISTER_CVAR3_DEDI_ONLY(_name, _var, _def_val, _flags, _comment)                       REGISTER_CVAR3_DEV_ONLY(_name, _var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment)
	#define REGISTER_CVAR3_CB_DEDI_ONLY(_name, _var, _def_val, _flags, _comment, _onchangefunction) REGISTER_CVAR3_CB_DEV_ONLY(_name, _var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction)
	#define REGISTER_COMMAND_DEDI_ONLY(_name, _func, _flags, _comment)                              REGISTER_COMMAND_DEV_ONLY(_name, _func, ((_flags) | VF_DEDI_ONLY), _comment)
#endif // defined(RELEASE)
//////////////////////////////////////////////////////////////////////////////
