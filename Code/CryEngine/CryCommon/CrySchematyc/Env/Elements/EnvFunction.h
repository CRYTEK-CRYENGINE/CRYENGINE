// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Reduce code duplication using variadic templates or pre-processor macros.

#pragma once

#include "CrySchematyc/FundamentalTypes.h"
#include "CrySchematyc/Env/EnvElementBase.h"
#include "CrySchematyc/Env/Elements/IEnvFunction.h"
#include "CrySchematyc/Reflection/TypeDesc.h"
#include "CrySchematyc/Runtime/RuntimeParamMap.h"
#include "CrySchematyc/Utils/Assert.h"
#include "CrySchematyc/Utils/Any.h"
#include "CrySchematyc/Utils/EnumFlags.h"
#include "CrySchematyc/Utils/StringUtils.h"
#include "CrySchematyc/Utils/TypeName.h"

#define SCHEMATYC_MAKE_ENV_FUNCTION(functionPtr, guid, name) Schematyc::EnvFunction::MakeShared(functionPtr, guid, name, SCHEMATYC_SOURCE_FILE_INFO)

namespace Schematyc
{

// Forward declare interfaces.
struct IEnvAction;
struct IEnvComponent;

namespace EnvFunctionUtils
{

struct SBinding;

enum class EParamFlags
{
	AvailableInput  = BIT(0), // Parameter can be bound to input.
	BoundToInput    = BIT(1), // Parameter is bound to input.
	AvailableOutput = BIT(2), // Parameter can be bound to output.
	BoundToOutput   = BIT(3)  // Parameter is bound to output.
};

typedef CEnumFlags<EParamFlags> ParamFlags;

typedef void (* StubPtr)(const SBinding&, CRuntimeParamMap& params, void* pObject);

enum : uint32
{
	MaxParams = 10u
};

template<typename TYPE> struct SParamTraits
{
	typedef TYPE        UnqualifiedType;
	typedef const TYPE& ProxyType;

	static const bool IsSupported = true;
	static const bool IsConst = false;
	static const bool IsReference = false;
};

template<typename TYPE> struct SParamTraits<const TYPE>
{
	typedef TYPE        UnqualifiedType;
	typedef const TYPE& ProxyType;

	static const bool IsSupported = true;
	static const bool IsConst = true;
	static const bool IsReference = false;
};

template<typename TYPE> struct SParamTraits<TYPE&>
{
	typedef TYPE  UnqualifiedType;
	typedef TYPE& ProxyType;

	static const bool IsSupported = true;
	static const bool IsConst = false;
	static const bool IsReference = true;
};

template<typename TYPE> struct SParamTraits<const TYPE&>
{
	typedef TYPE        UnqualifiedType;
	typedef const TYPE& ProxyType;

	static const bool IsSupported = true;
	static const bool IsConst = true;
	static const bool IsReference = true;
};

template<typename TYPE> struct SParamTraits<TYPE*>
{
	typedef void UnqualifiedType;
	typedef void ProxyType;

	static const bool IsSupported = false;
	static const bool IsConst = false;
	static const bool IsReference = false;
};

template<typename TYPE> struct SParamTraits<const TYPE*>
{
	typedef void UnqualifiedType;
	typedef void ProxyType;

	static const bool IsSupported = false;
	static const bool IsConst = false;
	static const bool IsReference = false;
};

struct SParamBinding
{
	inline SParamBinding()
		: idx(InvalidIdx)
		, id(InvalidId)
	{}

	ParamFlags   flags;
	uint32       idx;
	uint32       id;
	string       name;
	string       description;
	CAnyValuePtr pData;
};

struct SBinding
{
	inline SBinding()
		: flags(EEnvFunctionFlags::None)
		, pObjectTypeDesc(nullptr)
		, inputCount(0)
		, outputCount(0)
	{}

	uint8                  pFunction[sizeof(void*) * 4];
	StubPtr                pStub;

	EnvFunctionFlags       flags;
	const CCommonTypeDesc* pObjectTypeDesc;
	SParamBinding          params[MaxParams];
	uint32                 inputCount;
	uint32                 inputs[MaxParams];
	uint32                 outputCount;
	uint32                 outputs[MaxParams];
};

template<typename TYPE> inline void InitParamBinding(SParamBinding& paramBinding)
{
	typedef SParamTraits<TYPE> Traits;

	static_assert(Traits::IsSupported, "Parameter type is not supported!");
	static_assert(std::is_default_constructible<typename Traits::UnqualifiedType>::value, "Parameter type must provide default constructor!");

	if (Traits::IsConst || !Traits::IsReference)
	{
		paramBinding.flags = EParamFlags::AvailableInput;
	}
	else
	{
		paramBinding.flags = EParamFlags::AvailableOutput;
	}

	paramBinding.pData = CAnyValue::MakeShared(typename Traits::UnqualifiedType());
}

template<typename TYPE> inline void InitReturnParamBinding(SParamBinding& paramBinding)
{
	typedef SParamTraits<TYPE> Traits;

	static_assert(Traits::IsSupported, "Parameter type is not supported");
	static_assert(std::is_default_constructible<typename Traits::UnqualifiedType>::value, "Parameter type must provide default constructor!");

	paramBinding.flags = EParamFlags::AvailableOutput;

	paramBinding.pData = CAnyValue::MakeShared(typename Traits::UnqualifiedType());
}

inline void BindInput(SBinding& binding, uint32 paramIdx, uint32 id, const char* szName, const char* szDescription, const CAnyValuePtr& pData)
{
	// #SchematycTODO : Make sure id is unique and name is valid!!!

	SCHEMATYC_CORE_ASSERT_FATAL(paramIdx < MaxParams);
	SCHEMATYC_CORE_ASSERT_FATAL(szName);

	SParamBinding& param = binding.params[paramIdx];
	SCHEMATYC_CORE_ASSERT_FATAL(param.flags.Check(EParamFlags::AvailableInput));

	param.flags.Remove({ EParamFlags::AvailableInput, EParamFlags::AvailableOutput });
	param.flags.Add(EParamFlags::BoundToInput);

	param.idx = binding.inputCount;
	param.id = id;
	param.name = szName;
	param.description = szDescription;

	if (pData)
	{
		param.pData = pData;
	}

	binding.inputs[binding.inputCount] = paramIdx;
	++binding.inputCount;
}

inline void BindOutput(SBinding& binding, uint32 paramIdx, uint32 id, const char* szName, const char* szDescription)
{
	// #SchematycTODO : Make sure id is unique and name is valid!!!

	SCHEMATYC_CORE_ASSERT_FATAL(paramIdx < MaxParams);
	SCHEMATYC_CORE_ASSERT_FATAL(szName);

	SParamBinding& param = binding.params[paramIdx];
	SCHEMATYC_CORE_ASSERT_FATAL(param.flags.Check(EParamFlags::AvailableOutput));

	param.flags.Remove({ EParamFlags::AvailableInput, EParamFlags::AvailableOutput });
	param.flags.Add(EParamFlags::BoundToOutput);

	param.idx = binding.outputCount;
	param.id = id;
	param.name = szName;
	param.description = szDescription;

	binding.outputs[binding.outputCount] = paramIdx;
	++binding.outputCount;
}

inline bool ValidateBinding(const SBinding& binding)
{
	for (uint32 paramIdx = 0; paramIdx < MaxParams; ++paramIdx)
	{
		if (binding.params[paramIdx].flags.CheckAny({ EParamFlags::AvailableInput, EParamFlags::AvailableOutput }))
		{
			return false;
		}
	}
	return true;
}

template<typename TYPE> struct SParamReader
{
	typedef const TYPE& ReturnType;

	static inline ReturnType ReadParam(const SBinding& binding, uint32 paramIdx, CRuntimeParamMap& params)
	{
		const SParamBinding& param = binding.params[paramIdx];
		SCHEMATYC_CORE_ASSERT_FATAL(param.flags.Check(EParamFlags::BoundToInput));
		return DynamicCast<TYPE>(*params.GetInput(CUniqueId::FromUInt32(param.id)));
	}
};

template<typename TYPE> struct SParamReader<TYPE&>
{
	typedef TYPE& ReturnType;

	static inline ReturnType ReadParam(const SBinding& binding, uint32 paramIdx, CRuntimeParamMap& params)
	{
		const SParamBinding& param = binding.params[paramIdx];
		SCHEMATYC_CORE_ASSERT_FATAL(param.flags.Check(EParamFlags::BoundToOutput));
		return DynamicCast<TYPE>(*params.GetOutput(CUniqueId::FromUInt32(param.id)));
	}
};

template<typename TYPE> struct SParamReader<const TYPE&>
{
	typedef const TYPE& ReturnType;

	static inline ReturnType ReadParam(const SBinding& binding, uint32 paramIdx, CRuntimeParamMap& params)
	{
		const SParamBinding& param = binding.params[paramIdx];
		SCHEMATYC_CORE_ASSERT_FATAL(param.flags.Check(EParamFlags::BoundToInput));
		return DynamicCast<TYPE>(*params.GetInput(CUniqueId::FromUInt32(param.id)));
	}
};

template<typename TYPE> inline typename SParamReader<TYPE>::ReturnType ReadParam(const SBinding& binding, uint32 paramIdx, CRuntimeParamMap& params)
{
	return SParamReader<TYPE>::ReadParam(binding, paramIdx, params);
}

template<typename TYPE> inline void WriteParam(const SBinding& binding, uint32 paramIdx, CRuntimeParamMap& params, const TYPE& value)
{
	const SParamBinding& param = binding.params[paramIdx];
	SCHEMATYC_CORE_ASSERT_FATAL(param.flags.Check(EParamFlags::BoundToOutput));
	DynamicCast<typename SParamTraits<TYPE>::UnqualifiedType>(*params.GetOutput(CUniqueId::FromUInt32(param.id))) = value;
}

template<typename FUNCTION_PTR_TYPE> struct SBinder
{
	static const bool IsSupported = false;
};

// void (*)()
template<>
struct SBinder<void (*)()>
{
	typedef void (* FunctionPtr)();

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		binding.pStub = &SBinder::Stub;
		*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(*pFunction)();
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)()
template<typename OBJECT>
struct SBinder<void (OBJECT::*)()>
{
	typedef void (OBJECT::* FunctionPtr)();

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<OBJECT*>(pObject)->*pFunction)();
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)() const
template<typename OBJECT>
struct SBinder<void (OBJECT::*)() const>
{
	typedef void (OBJECT::* FunctionPtr)() const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<const OBJECT*>(pObject)->*pFunction)();
	}

	static const bool IsSupported = true;
};

// PARAM0 (*)()
template<typename PARAM0>
struct SBinder<PARAM0 (*)()>
{
	typedef PARAM0 (* FunctionPtr)();

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitReturnParamBinding<PARAM0>(binding.params[0]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (*pFunction)());
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)()
template<typename OBJECT, typename PARAM0>
struct SBinder<PARAM0 (OBJECT::*)()>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)();

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<OBJECT*>(pObject)->*pFunction)());
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)() const
template<typename OBJECT, typename PARAM0>
struct SBinder<PARAM0 (OBJECT::*)() const>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)() const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<const OBJECT*>(pObject)->*pFunction)());
	}

	static const bool IsSupported = true;
};

// void (*)(PARAM1)
template<typename PARAM1>
struct SBinder<void (*)(PARAM1)>
{
	typedef void (* FunctionPtr)(PARAM1);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitParamBinding<PARAM1>(binding.params[1]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(*pFunction)(param1);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1)
template<typename OBJECT, typename PARAM1>
struct SBinder<void (OBJECT::*)(PARAM1)>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitParamBinding<PARAM1>(binding.params[1]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<OBJECT*>(pObject)->*pFunction)(param1);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1) const
template<typename OBJECT, typename PARAM1>
struct SBinder<void (OBJECT::*)(PARAM1) const>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;

		InitParamBinding<PARAM1>(binding.params[1]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<const OBJECT*>(pObject)->*pFunction)(param1);
	}

	static const bool IsSupported = true;
};

// PARAM0 (*)(PARAM1)
template<typename PARAM0, typename PARAM1>
struct SBinder<PARAM0 (*)(PARAM1)>
{
	typedef PARAM0 (* FunctionPtr)(PARAM1);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (*pFunction)(param1));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1)
template<typename OBJECT, typename PARAM0, typename PARAM1>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1)>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<OBJECT*>(pObject)->*pFunction)(param1));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1) const
template<typename OBJECT, typename PARAM0, typename PARAM1>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1) const>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<const OBJECT*>(pObject)->*pFunction)(param1));
	}

	static const bool IsSupported = true;
};

// void (*)(PARAM1, PARAM2)
template<typename PARAM1, typename PARAM2>
struct SBinder<void (*)(PARAM1, PARAM2)>
{
	typedef void (* FunctionPtr)(PARAM1, PARAM2);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(*pFunction)(param1, param2);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1, PARAM2)
template<typename OBJECT, typename PARAM1, typename PARAM2>
struct SBinder<void (OBJECT::*)(PARAM1, PARAM2)>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1, PARAM2);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<OBJECT*>(pObject)->*pFunction)(param1, param2);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1, PARAM2) const
template<typename OBJECT, typename PARAM1, typename PARAM2>
struct SBinder<void (OBJECT::*)(PARAM1, PARAM2) const>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1, PARAM2) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<const OBJECT*>(pObject)->*pFunction)(param1, param2);
	}

	static const bool IsSupported = true;
};

// PARAM0 (*)(PARAM1, PARAM2)
template<typename PARAM0, typename PARAM1, typename PARAM2>
struct SBinder<PARAM0 (*)(PARAM1, PARAM2)>
{
	typedef PARAM0 (* FunctionPtr)(PARAM1, PARAM2);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (*pFunction)(param1, param2));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1, PARAM2)
template<typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1, PARAM2)>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1, PARAM2);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<OBJECT*>(pObject)->*pFunction)(param1, param2));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1, PARAM2) const
template<typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1, PARAM2) const>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1, PARAM2) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<const OBJECT*>(pObject)->*pFunction)(param1, param2));
	}

	static const bool IsSupported = true;
};

// void (*)(PARAM1, PARAM2, PARAM3);
template<typename PARAM1, typename PARAM2, typename PARAM3>
struct SBinder<void (*)(PARAM1, PARAM2, PARAM3)>
{
	typedef void (* FunctionPtr)(PARAM1, PARAM2, PARAM3);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(*pFunction)(param1, param2, param3);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1, PARAM2, PARAM3)
template<typename OBJECT, typename PARAM1, typename PARAM2, typename PARAM3>
struct SBinder<void (OBJECT::*)(PARAM1, PARAM2, PARAM3)>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<OBJECT*>(pObject)->*pFunction)(param1, param2, param3);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1, PARAM2, PARAM3) const
template<typename OBJECT, typename PARAM1, typename PARAM2, typename PARAM3>
struct SBinder<void (OBJECT::*)(PARAM1, PARAM2, PARAM3) const>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<const OBJECT*>(pObject)->*pFunction)(param1, param2, param2);
	}

	static const bool IsSupported = true;
};

// PARAM0 (*)(PARAM1, PARAM2, PARAM3)
template<typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3>
struct SBinder<PARAM0 (*)(PARAM1, PARAM2, PARAM3)>
{
	typedef PARAM0 (* FunctionPtr)(PARAM1, PARAM2, PARAM3);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (*pFunction)(param1, param2, param3));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3)
template<typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3)>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<OBJECT*>(pObject)->*pFunction)(param1, param2, param3));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3) const
template<typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3) const>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<const OBJECT*>(pObject)->*pFunction)(param1, param2, param2));
	}

	static const bool IsSupported = true;
};

// void (*)(PARAM1, PARAM2, PARAM3, PARAM4)
template<typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4>
struct SBinder<void (*)(PARAM1, PARAM2, PARAM3, PARAM4)>
{
	typedef void (* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(*pFunction)(param1, param2, param3, param4);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4)
template<typename OBJECT, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4>
struct SBinder<void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4)>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4) const
template<typename OBJECT, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4>
struct SBinder<void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4) const>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<const OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4);
	}

	static const bool IsSupported = true;
};

// PARAM0 (*)(PARAM1, PARAM2, PARAM3, PARAM4)
template<typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4>
struct SBinder<PARAM0 (*)(PARAM1, PARAM2, PARAM3, PARAM4)>
{
	typedef PARAM0 (* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (*pFunction)(param1, param2, param3, param4));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4)
template<typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4)>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4) const
template<typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4) const>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<const OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4));
	}

	static const bool IsSupported = true;
};

// void (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)
template<typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5>
struct SBinder<void (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)>
{
	typedef void (* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(*pFunction)(param1, param2, param3, param4, param5);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)
template<typename OBJECT, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5>
struct SBinder<void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5) const
template<typename OBJECT, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5>
struct SBinder<void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5) const>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<const OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5);
	}

	static const bool IsSupported = true;
};

// PARAM0 (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)
template<typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5>
struct SBinder<PARAM0 (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)>
{
	typedef PARAM0 (* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (*pFunction)(param1, param2, param3, param4, param5));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)
template<typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5) const
template<typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5) const>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<const OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5));
	}

	static const bool IsSupported = true;
};

// void (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)
template<typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6>
struct SBinder<void (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)>
{
	typedef void (* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(*pFunction)(param1, param2, param3, param4, param5, param6);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)
template<typename OBJECT, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6>
struct SBinder<void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5, param6);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6) const
template<typename OBJECT, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6>
struct SBinder<void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6) const>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<const OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5, param6);
	}

	static const bool IsSupported = true;
};

// PARAM0 (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)
template<typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6>
struct SBinder<PARAM0 (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)>
{
	typedef PARAM0 (* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (*pFunction)(param1, param2, param3, param4, param5, param6));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)
template<typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5, param6));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6) const
template<typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6) const>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<const OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5, param6));
	}

	static const bool IsSupported = true;
};

// void (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)
template<typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7>
struct SBinder<void (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)>
{
	typedef void (* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(*pFunction)(param1, param2, param3, param4, param5, param6, param7);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)
template<typename OBJECT, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7>
struct SBinder<void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5, param6, param7);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7) const
template<typename OBJECT, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7>
struct SBinder<void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7) const>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<const OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5, param6, param7);
	}

	static const bool IsSupported = true;
};

// PARAM0 (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)
template<typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7>
struct SBinder<PARAM0 (*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)>
{
	typedef PARAM0 (* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (*pFunction)(param1, param2, param3, param4, param5, param6, param7));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)
template<typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5, param6, param7));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7) const
template<typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7) const>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<const OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5, param6, param7));
	}

	static const bool IsSupported = true;
};

// void (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)
template<typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8>
struct SBinder<void (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)>
{
	typedef void (* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
		InitParamBinding<PARAM8>(binding.params[8]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);
		typename SParamTraits<PARAM8>::ProxyType param8 = ReadParam<PARAM8>(binding, 8, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(*pFunction)(param1, param2, param3, param4, param5, param6, param7, param8);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)
template<typename OBJECT, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8>
struct SBinder<void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
		InitParamBinding<PARAM8>(binding.params[8]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);
		typename SParamTraits<PARAM8>::ProxyType param8 = ReadParam<PARAM8>(binding, 8, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5, param6, param7, param8);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8) const
template<typename OBJECT, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8>
struct SBinder<void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8) const>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
		InitParamBinding<PARAM8>(binding.params[8]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);
		typename SParamTraits<PARAM8>::ProxyType param8 = ReadParam<PARAM8>(binding, 8, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<const OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5, param6, param7, param8);
	}

	static const bool IsSupported = true;
};

// PARAM0 (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)
template<typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8>
struct SBinder<PARAM0 (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)>
{
	typedef PARAM0 (* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
		InitParamBinding<PARAM8>(binding.params[8]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);
		typename SParamTraits<PARAM8>::ProxyType param8 = ReadParam<PARAM8>(binding, 8, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (*pFunction)(param1, param2, param3, param4, param5, param6, param7, param8));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)
template<typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
		InitParamBinding<PARAM8>(binding.params[8]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);
		typename SParamTraits<PARAM8>::ProxyType param8 = ReadParam<PARAM8>(binding, 8, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5, param6, param7, param8));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8) const
template<typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8) const>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
		InitParamBinding<PARAM8>(binding.params[8]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);
		typename SParamTraits<PARAM8>::ProxyType param8 = ReadParam<PARAM8>(binding, 8, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<const OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5, param6, param7, param8));
	}

	static const bool IsSupported = true;
};

// void (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)
template<typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9>
struct SBinder<void (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)>
{
	typedef void (* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
		InitParamBinding<PARAM8>(binding.params[8]);
		InitParamBinding<PARAM9>(binding.params[9]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);
		typename SParamTraits<PARAM8>::ProxyType param8 = ReadParam<PARAM8>(binding, 8, params);
		typename SParamTraits<PARAM9>::ProxyType param9 = ReadParam<PARAM9>(binding, 9, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(*pFunction)(param1, param2, param3, param4, param5, param6, param7, param8, param9);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)
template<typename OBJECT, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9>
struct SBinder<void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
		InitParamBinding<PARAM8>(binding.params[8]);
		InitParamBinding<PARAM9>(binding.params[9]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);
		typename SParamTraits<PARAM8>::ProxyType param8 = ReadParam<PARAM8>(binding, 8, params);
		typename SParamTraits<PARAM9>::ProxyType param9 = ReadParam<PARAM9>(binding, 9, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5, param6, param7, param8, param9);
	}

	static const bool IsSupported = true;
};

// void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9) const
template<typename OBJECT, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9>
struct SBinder<void (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9) const>
{
	typedef void (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
		InitParamBinding<PARAM8>(binding.params[8]);
		InitParamBinding<PARAM9>(binding.params[9]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);
		typename SParamTraits<PARAM8>::ProxyType param8 = ReadParam<PARAM8>(binding, 8, params);
		typename SParamTraits<PARAM9>::ProxyType param9 = ReadParam<PARAM9>(binding, 9, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		(static_cast<const OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5, param6, param7, param8, param9);
	}

	static const bool IsSupported = true;
};

// PARAM0 (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)
template<typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9>
struct SBinder<PARAM0 (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)>
{
	typedef PARAM0 (* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
		InitParamBinding<PARAM8>(binding.params[8]);
		InitParamBinding<PARAM9>(binding.params[9]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);
		typename SParamTraits<PARAM8>::ProxyType param8 = ReadParam<PARAM8>(binding, 8, params);
		typename SParamTraits<PARAM9>::ProxyType param9 = ReadParam<PARAM9>(binding, 9, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (*pFunction)(param1, param2, param3, param4, param5, param6, param7, param8, param9));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)
template<typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9);

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = EEnvFunctionFlags::Member;
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
		InitParamBinding<PARAM8>(binding.params[8]);
		InitParamBinding<PARAM9>(binding.params[9]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);
		typename SParamTraits<PARAM8>::ProxyType param8 = ReadParam<PARAM8>(binding, 8, params);
		typename SParamTraits<PARAM9>::ProxyType param9 = ReadParam<PARAM9>(binding, 9, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5, param6, param7, param8, param9));
	}

	static const bool IsSupported = true;
};

// PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9) const
template<typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9>
struct SBinder<PARAM0 (OBJECT::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9) const>
{
	typedef PARAM0 (OBJECT::* FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9) const;

	static inline void Bind(SBinding& binding, FunctionPtr pFunction)
	{
		static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

		memcpy(binding.pFunction, &pFunction, sizeof(FunctionPtr));

		binding.pStub = &SBinder::Stub;
		binding.flags = { EEnvFunctionFlags::Member, EEnvFunctionFlags::Const };
		binding.pObjectTypeDesc = &GetTypeDesc<OBJECT>();

		InitReturnParamBinding<PARAM0>(binding.params[0]);

		InitParamBinding<PARAM1>(binding.params[1]);
		InitParamBinding<PARAM2>(binding.params[2]);
		InitParamBinding<PARAM3>(binding.params[3]);
		InitParamBinding<PARAM4>(binding.params[4]);
		InitParamBinding<PARAM5>(binding.params[5]);
		InitParamBinding<PARAM6>(binding.params[6]);
		InitParamBinding<PARAM7>(binding.params[7]);
		InitParamBinding<PARAM8>(binding.params[8]);
		InitParamBinding<PARAM9>(binding.params[9]);
	}

	static void Stub(const SBinding& binding, CRuntimeParamMap& params, void* pObject)
	{
		typename SParamTraits<PARAM1>::ProxyType param1 = ReadParam<PARAM1>(binding, 1, params);
		typename SParamTraits<PARAM2>::ProxyType param2 = ReadParam<PARAM2>(binding, 2, params);
		typename SParamTraits<PARAM3>::ProxyType param3 = ReadParam<PARAM3>(binding, 3, params);
		typename SParamTraits<PARAM4>::ProxyType param4 = ReadParam<PARAM4>(binding, 4, params);
		typename SParamTraits<PARAM5>::ProxyType param5 = ReadParam<PARAM5>(binding, 5, params);
		typename SParamTraits<PARAM6>::ProxyType param6 = ReadParam<PARAM6>(binding, 6, params);
		typename SParamTraits<PARAM7>::ProxyType param7 = ReadParam<PARAM7>(binding, 7, params);
		typename SParamTraits<PARAM8>::ProxyType param8 = ReadParam<PARAM8>(binding, 8, params);
		typename SParamTraits<PARAM9>::ProxyType param9 = ReadParam<PARAM9>(binding, 9, params);

		FunctionPtr pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
		WriteParam<PARAM0>(binding, 0, params, (static_cast<const OBJECT*>(pObject)->*pFunction)(param1, param2, param3, param4, param5, param6, param7, param8, param9));
	}

	static const bool IsSupported = true;
};

} // EnvFunctionUtils

class CEnvFunction : public CEnvElementBase<IEnvFunction>
{
public:

	template<typename FUNCTION_PTR_TYPE> inline CEnvFunction(FUNCTION_PTR_TYPE pFunction, const CryGUID& guid, const char* szName, const SSourceFileInfo& sourceFileInfo)
		: CEnvElementBase(guid, szName, sourceFileInfo)
	{
		typedef EnvFunctionUtils::SBinder<FUNCTION_PTR_TYPE> FunctionBinder;

		static_assert(FunctionBinder::IsSupported, "Unsupported function signature!");

		FunctionBinder::Bind(m_binding, pFunction);
	}

	// IEnvElement

	virtual bool IsValidScope(IEnvElement& scope) const override
	{
		switch (scope.GetType())
		{
		case EEnvElementType::Module:
		case EEnvElementType::DataType:
		case EEnvElementType::Class:
		case EEnvElementType::Component:
		case EEnvElementType::Action:
			{
				return true;
			}
		default:
			{
				return false;
			}
		}
	}

	// ~IEnvElement

	// IEnvFunction

	virtual bool Validate() const override
	{
		return EnvFunctionUtils::ValidateBinding(m_binding);
	}

	virtual EnvFunctionFlags GetFunctionFlags() const override
	{
		return m_binding.flags;
	}

	virtual const CCommonTypeDesc* GetObjectTypeDesc() const override
	{
		return m_binding.pObjectTypeDesc;
	}

	virtual uint32 GetInputCount() const override
	{
		return m_binding.inputCount;
	}

	virtual uint32 GetInputId(uint32 inputIdx) const override
	{
		return inputIdx < m_binding.inputCount ? m_binding.params[m_binding.inputs[inputIdx]].id : InvalidId;
	}

	virtual const char* GetInputName(uint32 inputIdx) const override
	{
		return inputIdx < m_binding.inputCount ? m_binding.params[m_binding.inputs[inputIdx]].name.c_str() : "";
	}

	virtual const char* GetInputDescription(uint32 inputIdx) const override
	{
		return inputIdx < m_binding.inputCount ? m_binding.params[m_binding.inputs[inputIdx]].description.c_str() : "";
	}

	virtual CAnyConstPtr GetInputData(uint32 inputIdx) const override
	{
		return inputIdx < m_binding.inputCount ? m_binding.params[m_binding.inputs[inputIdx]].pData.get() : nullptr;
	}

	virtual uint32 GetOutputCount() const override
	{
		return m_binding.outputCount;
	}

	virtual uint32 GetOutputId(uint32 outputIdx) const override
	{
		return outputIdx < m_binding.outputCount ? m_binding.params[m_binding.outputs[outputIdx]].id : InvalidId;
	}

	virtual const char* GetOutputName(uint32 outputIdx) const override
	{
		return outputIdx < m_binding.outputCount ? m_binding.params[m_binding.outputs[outputIdx]].name.c_str() : "";
	}

	virtual const char* GetOutputDescription(uint32 outputIdx) const override
	{
		return outputIdx < m_binding.outputCount ? m_binding.params[m_binding.outputs[outputIdx]].description.c_str() : "";
	}

	virtual CAnyConstPtr GetOutputData(uint32 outputIdx) const override
	{
		return outputIdx < m_binding.outputCount ? m_binding.params[m_binding.outputs[outputIdx]].pData.get() : nullptr;
	}

	virtual void Execute(CRuntimeParamMap& params, void* pObject) const override
	{
		if (m_binding.pStub)
		{
			(*m_binding.pStub)(m_binding, params, pObject);
		}
	}

	// ~IEnvFunction

	inline void SetFlags(EnvFunctionFlags flags)
	{
		const EnvFunctionFlags filter = { EEnvFunctionFlags::Pure, EEnvFunctionFlags::Construction };
		m_binding.flags.SetWithFilter(flags, filter);
	}

	inline void BindInput(uint32 paramIdx, uint32 id, const char* szName, const char* szDescription = nullptr)
	{
		EnvFunctionUtils::BindInput(m_binding, paramIdx, id, szName, szDescription, CAnyValuePtr());
	}

	template<typename TYPE> inline void BindInput(uint32 paramIdx, uint32 id, const char* szName, const char* szDescription, const TYPE& value)
	{
		EnvFunctionUtils::BindInput(m_binding, paramIdx, id, szName, szDescription, CAnyValue::MakeShared(value));
	}

	inline void BindOutput(uint32 paramIdx, uint32 id, const char* szName, const char* szDescription = nullptr)
	{
		EnvFunctionUtils::BindOutput(m_binding, paramIdx, id, szName, szDescription);
	}

private:

	EnvFunctionUtils::SBinding m_binding;
};

namespace EnvFunction
{

template<typename FUNCTION_PTR_TYPE> inline std::shared_ptr<CEnvFunction> MakeShared(FUNCTION_PTR_TYPE pFunction, const CryGUID& guid, const char* szName, const SSourceFileInfo& sourceFileInfo)
{
	return std::make_shared<CEnvFunction>(pFunction, guid, szName, sourceFileInfo);
}

} // EnvFunction
} // Schematyc
