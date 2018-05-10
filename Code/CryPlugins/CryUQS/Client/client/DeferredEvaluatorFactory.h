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
			// CDeferredEvaluatorFactoryBase
			//
			//===================================================================================

			class CDeferredEvaluatorFactoryBase : public IDeferredEvaluatorFactory, public IParamsHolderFactory, public Shared::CFactoryBase<CDeferredEvaluatorFactoryBase>
			{
			public:
				// IDeferredEvaluatorFactory
				virtual const char*                      GetName() const override final;
				virtual const CryGUID&                   GetGUID() const override final;
				virtual const char*                      GetDescription() const override final;
				virtual const IInputParameterRegistry&   GetInputParameterRegistry() const override final;
				virtual IParamsHolderFactory&            GetParamsHolderFactory() const override final;
				// ~IDeferredEvaluatorFactory

				// IDeferredEvaluatorFactory: forward to derived class
				virtual DeferredEvaluatorUniquePtr       CreateDeferredEvaluator(const void* pParams) override = 0;
				virtual void                             DestroyDeferredEvaluator(IDeferredEvaluator* pDeferredEvaluatorToDestroy) override = 0;
				// ~IDeferredEvaluatorFactory

				// IParamsHolderFactory: forward to derived class
				virtual ParamsHolderUniquePtr            CreateParamsHolder() override = 0;
				virtual void                             DestroyParamsHolder(IParamsHolder* pParamsHolderToDestroy) override = 0;
				// ~IParamsHolderFactory

			protected:
				explicit                                 CDeferredEvaluatorFactoryBase(const char* szEvaluatorName, const CryGUID& guid, const char* szDescription);

			protected:
				CInputParameterRegistry                  m_inputParameterRegistry;

			private:
				string                                   m_description;
				IParamsHolderFactory*                    m_pParamsHolderFactory;      // points to *this; it's a trick to allow GetParamsHolderFactory() return a non-const reference to *this
			};

			inline CDeferredEvaluatorFactoryBase::CDeferredEvaluatorFactoryBase(const char* szEvaluatorName, const CryGUID& guid, const char* szDescription)
				: CFactoryBase(szEvaluatorName, guid)
				, m_description(szDescription)
			{
				m_pParamsHolderFactory = this;
			}

			inline const char* CDeferredEvaluatorFactoryBase::GetName() const
			{
				return CFactoryBase::GetName();
			}

			inline const CryGUID& CDeferredEvaluatorFactoryBase::GetGUID() const
			{
				return CFactoryBase::GetGUID();
			}

			inline const char* CDeferredEvaluatorFactoryBase::GetDescription() const
			{
				return m_description.c_str();
			}

			inline const IInputParameterRegistry& CDeferredEvaluatorFactoryBase::GetInputParameterRegistry() const
			{
				return m_inputParameterRegistry;
			}

			inline IParamsHolderFactory& CDeferredEvaluatorFactoryBase::GetParamsHolderFactory() const
			{
				return *m_pParamsHolderFactory;
			}

		} // namespace Internal

		//===================================================================================
		//
		// CDeferredEvaluatorFactory<>
		//
		//===================================================================================

		template <class TDeferredEvaluator>
		class CDeferredEvaluatorFactory final : public Internal::CDeferredEvaluatorFactoryBase
		{
		public:

			struct SCtorParams
			{
				const char*                          szName = "";
				CryGUID                              guid = CryGUID::Null();
				const char*                          szDescription = "";
			};

		public:

			explicit                                 CDeferredEvaluatorFactory(const SCtorParams& ctorParams);

			// IDeferredEvaluatorFactory
			virtual DeferredEvaluatorUniquePtr       CreateDeferredEvaluator(const void* pParams) override;
			virtual void                             DestroyDeferredEvaluator(IDeferredEvaluator* pDeferredEvaluatorToDestroy) override;
			// ~IDeferredEvaluatorFactory

			// IParamsHolderFactory
			virtual ParamsHolderUniquePtr            CreateParamsHolder() override;
			virtual void                             DestroyParamsHolder(IParamsHolder* pParamsHolderToDestroy) override;
			// ~IParamsHolderFactory
		};

		template <class TDeferredEvaluator>
		CDeferredEvaluatorFactory<TDeferredEvaluator>::CDeferredEvaluatorFactory(const SCtorParams& ctorParams)
			: CDeferredEvaluatorFactoryBase(ctorParams.szName, ctorParams.guid, ctorParams.szDescription)
		{
			typedef typename TDeferredEvaluator::SParams Params;
			Params::Expose(m_inputParameterRegistry);
		}

		template <class TDeferredEvaluator>
		DeferredEvaluatorUniquePtr CDeferredEvaluatorFactory<TDeferredEvaluator>::CreateDeferredEvaluator(const void* pParams)
		{
			const typename TDeferredEvaluator::SParams* pActualParams = static_cast<const typename TDeferredEvaluator::SParams*>(pParams);
			TDeferredEvaluator* pEvaluator = new TDeferredEvaluator(*pActualParams);
			Internal::CDeferredEvaluatorDeleter deleter(*this);
			return DeferredEvaluatorUniquePtr(pEvaluator, deleter);
		}

		template <class TDeferredEvaluator>
		void CDeferredEvaluatorFactory<TDeferredEvaluator>::DestroyDeferredEvaluator(IDeferredEvaluator* pDeferredEvaluatorToDestroy)
		{
			delete pDeferredEvaluatorToDestroy;
		}

		template <class TDeferredEvaluator>
		ParamsHolderUniquePtr CDeferredEvaluatorFactory<TDeferredEvaluator>::CreateParamsHolder()
		{
			Internal::CParamsHolder<typename TDeferredEvaluator::SParams>* pParamsHolder = new Internal::CParamsHolder<typename TDeferredEvaluator::SParams>;
			CParamsHolderDeleter deleter(*this);
			return ParamsHolderUniquePtr(pParamsHolder, deleter);
		}

		template <class TDeferredEvaluator>
		void CDeferredEvaluatorFactory<TDeferredEvaluator>::DestroyParamsHolder(IParamsHolder* pParamsHolderToDestroy)
		{
			delete pParamsHolderToDestroy;
		}

	}
}
