// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{
		namespace Internal
		{

			//===================================================================================
			//
			// CFunctionFactoryBase
			//
			//===================================================================================

			class CFunctionFactoryBase : public IFunctionFactory, public Shared::CFactoryBase<CFunctionFactoryBase>
			{
			public:
				// IFunctionFactory
				virtual const char*                       GetName() const override final;
				virtual const CryGUID&                    GetGUID() const override final;
				virtual const char*                       GetDescription() const override final;
				virtual const IInputParameterRegistry&    GetInputParameterRegistry() const override final;
				// ~IFunctionFactory

				// IFunctionFactory: forward to derived class
				virtual const Shared::CTypeInfo&          GetReturnType() const override = 0;
				virtual const Shared::CTypeInfo*          GetContainedType() const override = 0;
				virtual ELeafFunctionKind                 GetLeafFunctionKind() const override = 0;
				virtual FunctionUniquePtr                 CreateFunction(const IFunction::SCtorContext& ctorContext) override = 0;
				virtual void                              DestroyFunction(IFunction* pFunctionToDestroy) override = 0;
				// ~IFunctionFactory

			protected:
				explicit                                  CFunctionFactoryBase(const char* szFunctionName, const CryGUID& guid, const char* szDescription);

			protected:
				string                                    m_description;
				CInputParameterRegistry                   m_inputParameterRegistry;
			};

			inline CFunctionFactoryBase::CFunctionFactoryBase(const char* szFunctionName, const CryGUID& guid, const char* szDescription)
				: CFactoryBase(szFunctionName, guid)
				, m_description(szDescription)
			{}

			inline const char* CFunctionFactoryBase::GetName() const
			{
				return CFactoryBase::GetName();
			}

			inline const CryGUID& CFunctionFactoryBase::GetGUID() const
			{
				return CFactoryBase::GetGUID();
			}

			inline const char* CFunctionFactoryBase::GetDescription() const
			{
				return m_description.c_str();
			}

			inline const IInputParameterRegistry& CFunctionFactoryBase::GetInputParameterRegistry() const
			{
				return m_inputParameterRegistry;
			}

			//===================================================================================
			//
			// SFunctionParamsExpositionHelper<>
			//
			// - helper struct that is used by CFunctionFactory<>'s ctor to register the parameters of the function that factory creates
			// - depending on whether it's a leaf-function or non-leaf function, parameters may or may not get registered (leaf-function don't have input parameters)
			// - this goes hand in hand with how CFunctionBase<>::Execute() is implemented: leaf-functions never get passed in any parameters when being called, while non-leaf functions do
			// - actually, this template should better reside in the private section of CFunctionFactory<>, but template specializations are not allowed inside a class
			//
			//===================================================================================

			template <class TFunction, bool isLeafFunction>
			struct SFunctionParamsExpositionHelper
			{
			};

			template <class TFunction>
			struct SFunctionParamsExpositionHelper<TFunction, true>
			{
				static void Expose(CInputParameterRegistry& registry)
				{
					// nothing (leaf-function have no parameters to expose)
				}
			};

			template <class TFunction>
			struct SFunctionParamsExpositionHelper<TFunction, false>
			{
				static void Expose(CInputParameterRegistry& registry)
				{
					typedef typename TFunction::SParams Params;
					Params::Expose(registry);
				}
			};

		} // namespace Internal

		//===================================================================================
		//
		// CFunctionFactory<>
		//
		//===================================================================================

		template <class TFunction>
		class CFunctionFactory : public Internal::CFunctionFactoryBase
		{
		public:

			struct SCtorParams
			{
				const char*                     szName = "";
				CryGUID                         guid = CryGUID::Null();
				const char*                     szDescription = "";
			};

		public:

			explicit                            CFunctionFactory(const SCtorParams& ctorParams);

			// IFunctionFactory
			virtual const Shared::CTypeInfo&    GetReturnType() const override final;
			virtual const Shared::CTypeInfo*    GetContainedType() const override final;
			virtual ELeafFunctionKind           GetLeafFunctionKind() const override final;
			virtual FunctionUniquePtr           CreateFunction(const IFunction::SCtorContext& ctorContext) override final;
			virtual void                        DestroyFunction(IFunction* pFunctionToDestroy) override final;
			// ~IFunctionFactory
		};

		template <class TFunction>
		inline CFunctionFactory<TFunction>::CFunctionFactory(const SCtorParams& ctorParams)
			: CFunctionFactoryBase(ctorParams.szName, ctorParams.guid, ctorParams.szDescription)
		{
			const bool bIsLeafFunction = TFunction::kLeafFunctionKind != ELeafFunctionKind::None;
			Internal::SFunctionParamsExpositionHelper<TFunction, bIsLeafFunction>::Expose(m_inputParameterRegistry);
		}

		template <class TFunction>
		const Shared::CTypeInfo& CFunctionFactory<TFunction>::GetReturnType() const
		{
			return Shared::SDataTypeHelper<typename TFunction::ReturnType>::GetTypeInfo();
		}

		template <class TFunction>
		const Shared::CTypeInfo* CFunctionFactory<TFunction>::GetContainedType() const
		{
			return Internal::SContainedTypeRetriever<typename TFunction::ReturnType>::GetTypeInfo();
		}

		template <class TFunction>
		IFunctionFactory::ELeafFunctionKind CFunctionFactory<TFunction>::GetLeafFunctionKind() const
		{
			return TFunction::kLeafFunctionKind;
		}

		template <class TFunction>
		FunctionUniquePtr CFunctionFactory<TFunction>::CreateFunction(const IFunction::SCtorContext& ctorContext)
		{
#if 0
			TFunction* pFunction = new TFunction(ctorContext);
#else
			// notice: we assign the instantiated function to its base class pointer to ensure that the function type itself (and not accidentally another function type) was injected at its class definition
			CFunctionBase<TFunction, typename TFunction::ReturnType, TFunction::kLeafFunctionKind>* pFunction = new TFunction(ctorContext);
#endif
			Internal::CFunctionDeleter deleter(*this);
			return FunctionUniquePtr(pFunction, deleter);
		}

		template <class TFunction>
		void CFunctionFactory<TFunction>::DestroyFunction(IFunction* pFunctionToDestroy)
		{
			delete pFunctionToDestroy;
		}

	}
}
