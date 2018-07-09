// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Move EnvFunctionDescriptorUtils to separate header?
// #SchematycTODO : Can we reduce code duplication (using pre-processor macros for example)?
// #SchematycTODO : Can we optimize execution further (e.g. make references point to outputs)?

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/Any.h"
#include "CrySchematyc2/Env/IEnvFunctionDescriptor.h"
#include "CrySchematyc2/Utils/DefaultInit.h"
#include "CrySchematyc2/Utils/StringUtils.h"

namespace Schematyc2
{
	namespace EnvFunctionDescriptorUtils
	{
		struct SBinding;

		enum class EParamFlags
		{
			Void            = 0,
			AvailableInput  = BIT(0), // Parameter can be bound to input.
			BoundToInput    = BIT(1), // Parameter is bound to input.
			AvailableOutput = BIT(2), // Parameter can be bound to output.
			BoundToOutput   = BIT(3)  // Parameter is bound to output.
		};

		DECLARE_ENUM_CLASS_FLAGS(EParamFlags)

		typedef SEnvFunctionResult (*StubPtr)(const SBinding&, const SEnvFunctionContext&, void*, const EnvFunctionInputs&, const EnvFunctionOutputs&);

		template <typename TYPE> struct SParamTraits
		{
			static const bool IsSupported = true;

			typedef TYPE        UnqualifiedType;
			typedef const TYPE& ProxyType;

			static inline EParamFlags GetFlags()
			{
				return EParamFlags::AvailableInput;
			}
		};

		template <typename TYPE> struct SParamTraits<const TYPE>
		{
			static const bool IsSupported = true;

			typedef TYPE        UnqualifiedType;
			typedef const TYPE& ProxyType;

			static inline EParamFlags GetFlags()
			{
				return EParamFlags::AvailableInput;
			}
		};

		template <typename TYPE> struct SParamTraits<TYPE&>
		{
			static const bool IsSupported = true;

			typedef TYPE UnqualifiedType;
			typedef TYPE ProxyType;

			static inline EParamFlags GetFlags()
			{
				return EParamFlags::AvailableInput | EParamFlags::AvailableOutput;
			}
		};

		template <typename TYPE> struct SParamTraits<const TYPE&>
		{
			static const bool IsSupported = true;

			typedef TYPE        UnqualifiedType;
			typedef const TYPE& ProxyType;

			static inline EParamFlags GetFlags()
			{
				return EParamFlags::AvailableInput;
			}
		};

		template <typename TYPE> struct SParamTraits<TYPE*>
		{
			static const bool IsSupported = false;
		};

		template <> struct SParamTraits<void>
		{
			static const bool IsSupported = true;

			typedef void UnqualifiedType;
			typedef void ProxyType;

			static inline EParamFlags GetFlags()
			{
				return EParamFlags::Void;
			}
		};

		struct SParamBinding
		{
			inline SParamBinding()
				: flags(EParamFlags::Void)
				, idx(s_invalidIdx)
				, id(s_invalidId)
			{}

			template <typename TYPE> inline void Bind()
			{
				typedef SParamTraits<TYPE> Traits;

				static_assert(Traits::IsSupported, "Parameter type is not supported");

				flags  = Traits::GetFlags();
				pValue = MakeAnyShared<typename Traits::UnqualifiedType>();
			}

			EParamFlags flags;
			uint32      idx;
			uint32      id;
			string      name;
			string      description;
			IAnyPtr     pValue;
		};

		struct SBinding
		{
			inline SBinding()
				: flags(EEnvFunctionFlags::None)
				, paramCount(0)
				, inputCount(0)
				, outputCount(0)
			{}

			inline void BindInput(uint32 paramIdx, uint32 id, const char* szName, const char* szDescription, const IAnyPtr& pValue) // #SchematycTODO : Should this function be global?
			{
				// #SchematycTODO : Make sure id is unique and name is valid!!!

				SCHEMATYC2_SYSTEM_ASSERT_FATAL(paramIdx < paramCount);
				SCHEMATYC2_SYSTEM_ASSERT_FATAL(szName && szDescription);

				SParamBinding& param = params[paramIdx];
				SCHEMATYC2_SYSTEM_ASSERT_FATAL((param.flags & EParamFlags::AvailableInput) != 0);

				param.flags       = (param.flags & ~EParamFlags::AvailableInput) | EParamFlags::BoundToInput;
				param.idx         = inputCount;
				param.id          = id;
				param.name        = szName;
				param.description = szDescription;

				if(pValue)
				{
					param.pValue = pValue;
				}

				inputs[inputCount] = paramIdx;
				++ inputCount;
			}

			inline void BindOutput(uint32 paramIdx, uint32 id, const char* szName, const char* szDescription) // #SchematycTODO : Should this function be global?
			{
				// #SchematycTODO : Make sure id is unique and name is valid!!!

				SCHEMATYC2_SYSTEM_ASSERT_FATAL(paramIdx < paramCount);
				SCHEMATYC2_SYSTEM_ASSERT_FATAL(szName && szDescription);

				SParamBinding& param = params[paramIdx];
				SCHEMATYC2_SYSTEM_ASSERT_FATAL((param.flags & EParamFlags::AvailableOutput) != 0);

				param.flags       = (param.flags & ~EParamFlags::AvailableOutput) | EParamFlags::BoundToOutput;
				param.idx         = outputCount;
				param.id          = id;
				param.name        = szName;
				param.description = szDescription;

				outputs[outputCount] = paramIdx;
				++ outputCount;
			}

			EEnvFunctionFlags flags;
			EnvTypeId         contextTypeId;
			StubPtr           pStub;
			uint8             pFunction[sizeof(void*) * 4];
			uint32            paramCount;
			SParamBinding     params[s_maxEnvFunctionParams];
			uint32            inputCount;
			uint32            inputs[s_maxEnvFunctionParams];
			uint32            outputCount;
			uint32            outputs[s_maxEnvFunctionParams];
		};

		template <typename TYPE> inline const typename SParamTraits<TYPE>::UnqualifiedType& InputParam(const SBinding& binding, uint32 paramIdx, const EnvFunctionInputs& inputs)
		{
			const SParamBinding& param = binding.params[paramIdx];
			if((param.flags & EParamFlags::BoundToInput) != 0)
			{
				const IAny* pInput = inputs[param.idx];
				if(pInput)
				{
					return *reinterpret_cast<const typename SParamTraits<TYPE>::UnqualifiedType*>(pInput->ToVoidPtr());
				}
			}
			return DefaultInit<typename SParamTraits<TYPE>::UnqualifiedType>();
		}

		template <typename TYPE> inline void OutputParam(const SBinding& binding, uint32 paramIdx, const EnvFunctionOutputs& outputs, const TYPE& value)
		{
			const SParamBinding& param = binding.params[paramIdx];
			if((param.flags & EParamFlags::BoundToOutput) != 0)
			{
				IAny* pOutput = outputs[param.idx];
				if(pOutput)
				{
					*reinterpret_cast<typename SParamTraits<TYPE>::UnqualifiedType*>(pOutput->ToVoidPtr()) = value;
				}
			}
		}

		template <typename FUNCTION_PTR_TYPE> struct SBinder
		{
			static inline void Bind(SBinding&, FUNCTION_PTR_TYPE);

			static const bool IsSupported = false;
		};

		template <>
		struct SBinder<SEnvFunctionResult (*)(const SEnvFunctionContext& context)>
		{
			typedef SEnvFunctionResult (*FunctionPtr)(const SEnvFunctionContext&);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (*pFunction)(context);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context)>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<OBJECT*>(pObject)->*pFunction)(context);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context) const>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&) const;

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member | EEnvFunctionFlags::Const;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<const OBJECT*>(pObject)->*pFunction)(context);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename PARAM0>
		struct SBinder<SEnvFunctionResult (*)(const SEnvFunctionContext& context, PARAM0)>
		{
			typedef SEnvFunctionResult (*FunctionPtr)(const SEnvFunctionContext&, PARAM0);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 1;

				binding.params[0].Bind<PARAM0>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (*pFunction)(context, param0);

				OutputParam<PARAM0>(binding, 0, outputs, param0);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0)>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 1;

				binding.params[0].Bind<PARAM0>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<OBJECT*>(pObject)->*pFunction)(context, param0);

				OutputParam<PARAM0>(binding, 0, outputs, param0);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0) const>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0) const;

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member | EEnvFunctionFlags::Const;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 1;

				binding.params[0].Bind<PARAM0>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<const OBJECT*>(pObject)->*pFunction)(context, param0);

				OutputParam<PARAM0>(binding, 0, outputs, param0);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename PARAM0, typename PARAM1>
		struct SBinder<SEnvFunctionResult (*)(const SEnvFunctionContext& context, PARAM0, PARAM1)>
		{
			typedef SEnvFunctionResult (*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 2;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (*pFunction)(context, param0, param1);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1)>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");
			
				binding.flags                                      = EEnvFunctionFlags::Member;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 2;
			
				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<OBJECT*>(pObject)->*pFunction)(context, param0, param1);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1) const>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1) const;

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member | EEnvFunctionFlags::Const;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 2;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<const OBJECT*>(pObject)->*pFunction)(context, param0, param1);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename PARAM0, typename PARAM1, typename PARAM2>
		struct SBinder<SEnvFunctionResult (*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2)>
		{
			typedef SEnvFunctionResult (*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 3;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (*pFunction)(context, param0, param1, param2);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2)>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 3;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<OBJECT*>(pObject)->*pFunction)(context, param0, param1, param2);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2) const>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2) const;

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member | EEnvFunctionFlags::Const;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 3;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<const OBJECT*>(pObject)->*pFunction)(context, param0, param1, param2);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3>
		struct SBinder<SEnvFunctionResult (*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3)>
		{
			typedef SEnvFunctionResult (*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 4;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (*pFunction)(context, param0, param1, param2, param3);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3)>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 4;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<OBJECT*>(pObject)->*pFunction)(context, param0, param1, param2, param3);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3) const>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3) const;

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member | EEnvFunctionFlags::Const;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 4;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<const OBJECT*>(pObject)->*pFunction)(context, param0, param1, param2, param3);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4>
		struct SBinder<SEnvFunctionResult (*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4)>
		{
			typedef SEnvFunctionResult (*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 5;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (*pFunction)(context, param0, param1, param2, param3, param4);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4)>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 5;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<OBJECT*>(pObject)->*pFunction)(context, param0, param1, param2, param3, param4);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4) const>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4) const;

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member | EEnvFunctionFlags::Const;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 5;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<const OBJECT*>(pObject)->*pFunction)(context, param0, param1, param2, param3, param4);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5>
		struct SBinder<SEnvFunctionResult (*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)>
		{
			typedef SEnvFunctionResult (*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 6;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
				binding.params[5].Bind<PARAM5>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);
				typename SParamTraits<PARAM5>::ProxyType param5 = InputParam<PARAM5>(binding, 5, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (*pFunction)(context, param0, param1, param2, param3, param4, param5);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);
				OutputParam<PARAM5>(binding, 5, outputs, param5);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 6;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
				binding.params[5].Bind<PARAM5>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);
				typename SParamTraits<PARAM5>::ProxyType param5 = InputParam<PARAM5>(binding, 5, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<OBJECT*>(pObject)->*pFunction)(context, param0, param1, param2, param3, param4, param5);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);
				OutputParam<PARAM5>(binding, 5, outputs, param5);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5) const>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5) const;

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member | EEnvFunctionFlags::Const;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 6;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
				binding.params[5].Bind<PARAM5>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);
				typename SParamTraits<PARAM5>::ProxyType param5 = InputParam<PARAM5>(binding, 5, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<const OBJECT*>(pObject)->*pFunction)(context, param0, param1, param2, param3, param4, param5);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);
				OutputParam<PARAM5>(binding, 5, outputs, param5);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6>
		struct SBinder<SEnvFunctionResult (*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)>
		{
			typedef SEnvFunctionResult (*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 7;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
				binding.params[5].Bind<PARAM5>();
				binding.params[6].Bind<PARAM6>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);
				typename SParamTraits<PARAM5>::ProxyType param5 = InputParam<PARAM5>(binding, 5, inputs);
				typename SParamTraits<PARAM6>::ProxyType param6 = InputParam<PARAM6>(binding, 6, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (*pFunction)(context, param0, param1, param2, param3, param4, param5, param6);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);
				OutputParam<PARAM5>(binding, 5, outputs, param5);
				OutputParam<PARAM6>(binding, 6, outputs, param6);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 7;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
				binding.params[5].Bind<PARAM5>();
				binding.params[6].Bind<PARAM6>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);
				typename SParamTraits<PARAM5>::ProxyType param5 = InputParam<PARAM5>(binding, 5, inputs);
				typename SParamTraits<PARAM6>::ProxyType param6 = InputParam<PARAM6>(binding, 6, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<OBJECT*>(pObject)->*pFunction)(context, param0, param1, param2, param3, param4, param5, param6);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);
				OutputParam<PARAM5>(binding, 5, outputs, param5);
				OutputParam<PARAM6>(binding, 6, outputs, param6);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6) const>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6) const;

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member | EEnvFunctionFlags::Const;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 7;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
				binding.params[5].Bind<PARAM5>();
				binding.params[6].Bind<PARAM6>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);
				typename SParamTraits<PARAM5>::ProxyType param5 = InputParam<PARAM5>(binding, 5, inputs);
				typename SParamTraits<PARAM6>::ProxyType param6 = InputParam<PARAM6>(binding, 6, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<const OBJECT*>(pObject)->*pFunction)(context, param0, param1, param2, param3, param4, param5, param6);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);
				OutputParam<PARAM5>(binding, 5, outputs, param5);
				OutputParam<PARAM6>(binding, 6, outputs, param6);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7>
		struct SBinder<SEnvFunctionResult (*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)>
		{
			typedef SEnvFunctionResult (*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 8;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
				binding.params[5].Bind<PARAM5>();
				binding.params[6].Bind<PARAM6>();
				binding.params[7].Bind<PARAM7>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);
				typename SParamTraits<PARAM5>::ProxyType param5 = InputParam<PARAM5>(binding, 5, inputs);
				typename SParamTraits<PARAM6>::ProxyType param6 = InputParam<PARAM6>(binding, 6, inputs);
				typename SParamTraits<PARAM7>::ProxyType param7 = InputParam<PARAM7>(binding, 7, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (*pFunction)(context, param0, param1, param2, param3, param4, param5, param6, param7);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);
				OutputParam<PARAM5>(binding, 5, outputs, param5);
				OutputParam<PARAM6>(binding, 6, outputs, param6);
				OutputParam<PARAM7>(binding, 7, outputs, param7);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 8;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
				binding.params[5].Bind<PARAM5>();
				binding.params[6].Bind<PARAM6>();
				binding.params[7].Bind<PARAM7>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);
				typename SParamTraits<PARAM5>::ProxyType param5 = InputParam<PARAM5>(binding, 5, inputs);
				typename SParamTraits<PARAM6>::ProxyType param6 = InputParam<PARAM6>(binding, 6, inputs);
				typename SParamTraits<PARAM7>::ProxyType param7 = InputParam<PARAM7>(binding, 7, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<OBJECT*>(pObject)->*pFunction)(context, param0, param1, param2, param3, param4, param5, param6, param7);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);
				OutputParam<PARAM5>(binding, 5, outputs, param5);
				OutputParam<PARAM6>(binding, 6, outputs, param6);
				OutputParam<PARAM7>(binding, 7, outputs, param7);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7) const>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7) const;

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member | EEnvFunctionFlags::Const;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 8;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
				binding.params[5].Bind<PARAM5>();
				binding.params[6].Bind<PARAM6>();
				binding.params[7].Bind<PARAM7>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);
				typename SParamTraits<PARAM5>::ProxyType param5 = InputParam<PARAM5>(binding, 5, inputs);
				typename SParamTraits<PARAM6>::ProxyType param6 = InputParam<PARAM6>(binding, 6, inputs);
				typename SParamTraits<PARAM7>::ProxyType param7 = InputParam<PARAM7>(binding, 7, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<const OBJECT*>(pObject)->*pFunction)(context, param0, param1, param2, param3, param4, param5, param6, param7);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);
				OutputParam<PARAM5>(binding, 5, outputs, param5);
				OutputParam<PARAM6>(binding, 6, outputs, param6);
				OutputParam<PARAM7>(binding, 7, outputs, param7);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8>
		struct SBinder<SEnvFunctionResult (*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)>
		{
			typedef SEnvFunctionResult (*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 9;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
				binding.params[5].Bind<PARAM5>();
				binding.params[6].Bind<PARAM6>();
				binding.params[7].Bind<PARAM7>();
				binding.params[8].Bind<PARAM8>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);
				typename SParamTraits<PARAM5>::ProxyType param5 = InputParam<PARAM5>(binding, 5, inputs);
				typename SParamTraits<PARAM6>::ProxyType param6 = InputParam<PARAM6>(binding, 6, inputs);
				typename SParamTraits<PARAM7>::ProxyType param7 = InputParam<PARAM7>(binding, 7, inputs);
				typename SParamTraits<PARAM8>::ProxyType param8 = InputParam<PARAM8>(binding, 8, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (*pFunction)(context, param0, param1, param2, param3, param4, param5, param6, param7, param8);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);
				OutputParam<PARAM5>(binding, 5, outputs, param5);
				OutputParam<PARAM6>(binding, 6, outputs, param6);
				OutputParam<PARAM7>(binding, 7, outputs, param7);
				OutputParam<PARAM8>(binding, 8, outputs, param8);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 9;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
				binding.params[5].Bind<PARAM5>();
				binding.params[6].Bind<PARAM6>();
				binding.params[7].Bind<PARAM7>();
				binding.params[8].Bind<PARAM8>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);
				typename SParamTraits<PARAM5>::ProxyType param5 = InputParam<PARAM5>(binding, 5, inputs);
				typename SParamTraits<PARAM6>::ProxyType param6 = InputParam<PARAM6>(binding, 6, inputs);
				typename SParamTraits<PARAM7>::ProxyType param7 = InputParam<PARAM7>(binding, 7, inputs);
				typename SParamTraits<PARAM8>::ProxyType param8 = InputParam<PARAM8>(binding, 8, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<OBJECT*>(pObject)->*pFunction)(context, param0, param1, param2, param3, param4, param5, param6, param7, param8);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);
				OutputParam<PARAM5>(binding, 5, outputs, param5);
				OutputParam<PARAM6>(binding, 6, outputs, param6);
				OutputParam<PARAM7>(binding, 7, outputs, param7);
				OutputParam<PARAM8>(binding, 8, outputs, param8);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8) const>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8) const;

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member | EEnvFunctionFlags::Const;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 9;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
				binding.params[5].Bind<PARAM5>();
				binding.params[6].Bind<PARAM6>();
				binding.params[7].Bind<PARAM7>();
				binding.params[8].Bind<PARAM8>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);
				typename SParamTraits<PARAM5>::ProxyType param5 = InputParam<PARAM5>(binding, 5, inputs);
				typename SParamTraits<PARAM6>::ProxyType param6 = InputParam<PARAM6>(binding, 6, inputs);
				typename SParamTraits<PARAM7>::ProxyType param7 = InputParam<PARAM7>(binding, 7, inputs);
				typename SParamTraits<PARAM8>::ProxyType param8 = InputParam<PARAM8>(binding, 8, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<const OBJECT*>(pObject)->*pFunction)(context, param0, param1, param2, param3, param4, param5, param6, param7, param8);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);
				OutputParam<PARAM5>(binding, 5, outputs, param5);
				OutputParam<PARAM6>(binding, 6, outputs, param6);
				OutputParam<PARAM7>(binding, 7, outputs, param7);
				OutputParam<PARAM8>(binding, 8, outputs, param8);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9>
		struct SBinder<SEnvFunctionResult (*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)>
		{
			typedef SEnvFunctionResult (*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 10;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
				binding.params[5].Bind<PARAM5>();
				binding.params[6].Bind<PARAM6>();
				binding.params[7].Bind<PARAM7>();
				binding.params[8].Bind<PARAM8>();
				binding.params[9].Bind<PARAM9>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);
				typename SParamTraits<PARAM5>::ProxyType param5 = InputParam<PARAM5>(binding, 5, inputs);
				typename SParamTraits<PARAM6>::ProxyType param6 = InputParam<PARAM6>(binding, 6, inputs);
				typename SParamTraits<PARAM7>::ProxyType param7 = InputParam<PARAM7>(binding, 7, inputs);
				typename SParamTraits<PARAM8>::ProxyType param8 = InputParam<PARAM8>(binding, 8, inputs);
				typename SParamTraits<PARAM9>::ProxyType param9 = InputParam<PARAM9>(binding, 9, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (*pFunction)(context, param0, param1, param2, param3, param4, param5, param6, param7, param8, param9);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);
				OutputParam<PARAM5>(binding, 5, outputs, param5);
				OutputParam<PARAM6>(binding, 6, outputs, param6);
				OutputParam<PARAM7>(binding, 7, outputs, param7);
				OutputParam<PARAM8>(binding, 8, outputs, param8);
				OutputParam<PARAM9>(binding, 9, outputs, param9);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9);

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 10;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
				binding.params[5].Bind<PARAM5>();
				binding.params[6].Bind<PARAM6>();
				binding.params[7].Bind<PARAM7>();
				binding.params[8].Bind<PARAM8>();
				binding.params[9].Bind<PARAM9>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);
				typename SParamTraits<PARAM5>::ProxyType param5 = InputParam<PARAM5>(binding, 5, inputs);
				typename SParamTraits<PARAM6>::ProxyType param6 = InputParam<PARAM6>(binding, 6, inputs);
				typename SParamTraits<PARAM7>::ProxyType param7 = InputParam<PARAM7>(binding, 7, inputs);
				typename SParamTraits<PARAM8>::ProxyType param8 = InputParam<PARAM8>(binding, 8, inputs);
				typename SParamTraits<PARAM9>::ProxyType param9 = InputParam<PARAM9>(binding, 9, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<OBJECT*>(pObject)->*pFunction)(context, param0, param1, param2, param3, param4, param5, param6, param7, param8, param9);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);
				OutputParam<PARAM5>(binding, 5, outputs, param5);
				OutputParam<PARAM6>(binding, 6, outputs, param6);
				OutputParam<PARAM7>(binding, 7, outputs, param7);
				OutputParam<PARAM8>(binding, 8, outputs, param8);
				OutputParam<PARAM9>(binding, 9, outputs, param9);

				return result;
			}

			static const bool IsSupported = true;
		};

		template <typename OBJECT, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9>
		struct SBinder<SEnvFunctionResult (OBJECT::*)(const SEnvFunctionContext& context, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9) const>
		{
			typedef SEnvFunctionResult (OBJECT::*FunctionPtr)(const SEnvFunctionContext&, PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9) const;

			static inline void Bind(SBinding& binding, FunctionPtr pFunction)
			{
				static_assert(sizeof(FunctionPtr) <= sizeof(binding.pFunction), "Function pointer exceeds maximum size!");

				binding.flags                                      = EEnvFunctionFlags::Member | EEnvFunctionFlags::Const;
				binding.contextTypeId                              = GetEnvTypeId<OBJECT>();
				binding.pStub                                      = &SBinder::Stub;
				*reinterpret_cast<FunctionPtr*>(binding.pFunction) = pFunction;
				binding.paramCount                                 = 10;

				binding.params[0].Bind<PARAM0>();
				binding.params[1].Bind<PARAM1>();
				binding.params[2].Bind<PARAM2>();
				binding.params[3].Bind<PARAM3>();
				binding.params[4].Bind<PARAM4>();
				binding.params[5].Bind<PARAM5>();
				binding.params[6].Bind<PARAM6>();
				binding.params[7].Bind<PARAM7>();
				binding.params[8].Bind<PARAM8>();
				binding.params[9].Bind<PARAM9>();
			}

			static SEnvFunctionResult Stub(const SBinding& binding, const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs)
			{
				typename SParamTraits<PARAM0>::ProxyType param0 = InputParam<PARAM0>(binding, 0, inputs);
				typename SParamTraits<PARAM1>::ProxyType param1 = InputParam<PARAM1>(binding, 1, inputs);
				typename SParamTraits<PARAM2>::ProxyType param2 = InputParam<PARAM2>(binding, 2, inputs);
				typename SParamTraits<PARAM3>::ProxyType param3 = InputParam<PARAM3>(binding, 3, inputs);
				typename SParamTraits<PARAM4>::ProxyType param4 = InputParam<PARAM4>(binding, 4, inputs);
				typename SParamTraits<PARAM5>::ProxyType param5 = InputParam<PARAM5>(binding, 5, inputs);
				typename SParamTraits<PARAM6>::ProxyType param6 = InputParam<PARAM6>(binding, 6, inputs);
				typename SParamTraits<PARAM7>::ProxyType param7 = InputParam<PARAM7>(binding, 7, inputs);
				typename SParamTraits<PARAM8>::ProxyType param8 = InputParam<PARAM8>(binding, 8, inputs);
				typename SParamTraits<PARAM9>::ProxyType param9 = InputParam<PARAM9>(binding, 9, inputs);

				FunctionPtr        pFunction = *reinterpret_cast<const FunctionPtr*>(binding.pFunction);
				SEnvFunctionResult result = (static_cast<const OBJECT*>(pObject)->*pFunction)(context, param0, param1, param2, param3, param4, param5, param6, param7, param8, param9);

				OutputParam<PARAM0>(binding, 0, outputs, param0);
				OutputParam<PARAM1>(binding, 1, outputs, param1);
				OutputParam<PARAM2>(binding, 2, outputs, param2);
				OutputParam<PARAM3>(binding, 3, outputs, param3);
				OutputParam<PARAM4>(binding, 4, outputs, param4);
				OutputParam<PARAM5>(binding, 5, outputs, param5);
				OutputParam<PARAM6>(binding, 6, outputs, param6);
				OutputParam<PARAM7>(binding, 7, outputs, param7);
				OutputParam<PARAM8>(binding, 8, outputs, param8);
				OutputParam<PARAM9>(binding, 9, outputs, param9);

				return result;
			}

			static const bool IsSupported = true;
		};
	}

	class CEnvFunctionDescriptor : public IEnvFunctionDescriptor
	{
	public:

		template <typename FUNCTION_PTR_TYPE> inline CEnvFunctionDescriptor(FUNCTION_PTR_TYPE pFunction)
		{
			typedef EnvFunctionDescriptorUtils::SBinder<FUNCTION_PTR_TYPE> FunctionBinder;

			static_assert(FunctionBinder::IsSupported, "Unsupported function signature!");

			FunctionBinder::Bind(m_binding, pFunction);
		}

		// IEnvFunctionDescriptor

		virtual SGUID GetGUID() const override
		{
			return m_guid;
		}
		
		virtual const char* GetName() const override
		{
			return m_name.c_str();
		}

		virtual const char* GetNamespace() const override
		{
			return m_namespace.c_str();
		}

		virtual const char* GetFileName() const override
		{
			return m_fileName.c_str();
		}

		virtual const char* GetAuthor() const override
		{
			return m_author.c_str();
		}

		virtual const char* GetDescription() const override
		{
			return m_description.c_str();
		}

		virtual EEnvFunctionFlags GetFlags() const override
		{
			return m_binding.flags;
		}

		virtual EnvTypeId GetContextTypeId() const override
		{
			return m_binding.contextTypeId;
		}

		virtual uint32 GetInputCount() const override
		{
			return m_binding.inputCount;
		}

		virtual uint32 GetInputId(uint32 inputIdx) const override
		{
			return inputIdx < m_binding.inputCount ? m_binding.params[m_binding.inputs[inputIdx]].id : s_invalidId;
		}

		virtual const char* GetInputName(uint32 inputIdx) const override
		{
			return inputIdx < m_binding.inputCount ? m_binding.params[m_binding.inputs[inputIdx]].name.c_str() : "";
		}

		virtual const char* GetInputDescription(uint32 inputIdx) const override
		{
			return inputIdx < m_binding.inputCount ? m_binding.params[m_binding.inputs[inputIdx]].description.c_str() : "";
		}

		virtual IAnyConstPtr GetInputValue(uint32 inputIdx) const override
		{
			return inputIdx < m_binding.inputCount ? m_binding.params[m_binding.inputs[inputIdx]].pValue : IAnyConstPtr();
		}

		virtual uint32 GetOutputCount() const override
		{
			return m_binding.outputCount;
		}

		virtual uint32 GetOutputId(uint32 outputIdx) const override
		{
			return outputIdx < m_binding.outputCount ? m_binding.params[m_binding.outputs[outputIdx]].id : s_invalidId;
		}

		virtual const char* GetOutputName(uint32 outputIdx) const override
		{
			return outputIdx < m_binding.outputCount ? m_binding.params[m_binding.outputs[outputIdx]].name.c_str() : "";
		}

		virtual const char* GetOutputDescription(uint32 outputIdx) const override
		{
			return outputIdx < m_binding.outputCount ? m_binding.params[m_binding.outputs[outputIdx]].description.c_str() : "";
		}

		virtual IAnyConstPtr GetOutputValue(uint32 outputIdx) const override
		{
			return outputIdx < m_binding.outputCount ? m_binding.params[m_binding.outputs[outputIdx]].pValue : IAnyConstPtr();
		}

		virtual SEnvFunctionResult Execute(const SEnvFunctionContext& context, void* pObject, const EnvFunctionInputs& inputs, const EnvFunctionOutputs& outputs) const override
		{
			return m_binding.pStub ? (*m_binding.pStub)(m_binding, context, pObject, inputs, outputs) : SEnvFunctionResult();
		}

		// ~IEnvFunctionDescriptor

		inline void SetGUID(const SGUID& guid)
		{
			m_guid = guid;
		}

		inline void SetName(const char* szName)
		{
			m_name = szName;
		}

		inline void SetNamespace(const char* szNamespace)
		{
			m_namespace = szNamespace;
		}

		virtual void SetFileName(const char* szFileName, const char* szProjectDir)
		{
			StringUtils::MakeProjectRelativeFileName(szFileName, szProjectDir, m_fileName);
		}

		inline void SetAuthor(const char* szAuthor)
		{
			m_author = szAuthor;
		}

		inline void SetDescription(const char* szDescription)
		{
			m_description = szDescription;
		}

		inline uint32 GetParamCount() const
		{
			return m_binding.paramCount;
		}

		inline void BindInput(uint32 paramIdx, uint32 id, const char* szName, const char* szDescription)
		{
			m_binding.BindInput(paramIdx, id, szName, szDescription, IAnyPtr());
		}

		template <typename TYPE> inline void BindInput(uint32 paramIdx, uint32 id, const char* szName, const char* szDescription, const TYPE& value)
		{
			m_binding.BindInput(paramIdx, id, szName, szDescription, MakeAnyShared(value));
		}

		inline void BindOutput(uint32 paramIdx, uint32 id, const char* szName, const char* szDescription)
		{
			m_binding.BindOutput(paramIdx, id, szName, szDescription);
		}

	private:

		SGUID                                m_guid;
		string                               m_name;
		string                               m_namespace;
		string                               m_fileName;
		string                               m_author;
		string                               m_description;
		EnvFunctionDescriptorUtils::SBinding m_binding;
	};

	DECLARE_SHARED_POINTERS(CEnvFunctionDescriptor)

	template <typename FUNCTION_PTR_TYPE> inline CEnvFunctionDescriptorPtr MakeEnvFunctionDescriptorShared(FUNCTION_PTR_TYPE pFunction)
	{
		return std::make_shared<CEnvFunctionDescriptor>(pFunction);
	}
}
