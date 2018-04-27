// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{
		namespace Internal
		{
			class CDeferredEvaluatorDeleter;    // below
		}

		//===================================================================================
		//
		// DeferredEvaluatorUniquePtr
		//
		//===================================================================================

		typedef std::unique_ptr<IDeferredEvaluator, Internal::CDeferredEvaluatorDeleter>  DeferredEvaluatorUniquePtr;

		//===================================================================================
		//
		// IDeferredEvaluatorFactory
		//
		//===================================================================================

		struct IDeferredEvaluatorFactory
		{
			virtual                                    ~IDeferredEvaluatorFactory() {}
			virtual const char*                        GetName() const = 0;
			virtual const CryGUID&                     GetGUID() const = 0;
			virtual const char*                        GetDescription() const = 0;
			virtual const IInputParameterRegistry&     GetInputParameterRegistry() const = 0;
			virtual DeferredEvaluatorUniquePtr         CreateDeferredEvaluator(const void* pParams) = 0;
			virtual void                               DestroyDeferredEvaluator(IDeferredEvaluator* pDeferredEvaluatorToDestroy) = 0;
			virtual IParamsHolderFactory&              GetParamsHolderFactory() const = 0;
		};

		namespace Internal
		{

			//===================================================================================
			//
			// CDeferredEvaluatorDeleter
			//
			//===================================================================================

			class CDeferredEvaluatorDeleter
			{
			public:
				explicit                                CDeferredEvaluatorDeleter();         // default ctor is required for when smart pointer using this deleter gets implicitly constructed via nullptr (i. e. with only 1 argument for the smart pointer's ctor)
				explicit                                CDeferredEvaluatorDeleter(IDeferredEvaluatorFactory& deferredEvaluatorFactory);
				void                                    operator()(IDeferredEvaluator* pDeferredEvaluatorToDelete);

			private:
				IDeferredEvaluatorFactory*              m_pDeferredEvaluatorFactory;          // this one created the deferred-evaluator before
			};

			inline CDeferredEvaluatorDeleter::CDeferredEvaluatorDeleter()
				: m_pDeferredEvaluatorFactory(nullptr)
			{}

			inline CDeferredEvaluatorDeleter::CDeferredEvaluatorDeleter(IDeferredEvaluatorFactory& deferredEvaluatorFactory)
				: m_pDeferredEvaluatorFactory(&deferredEvaluatorFactory)
			{}

			inline void CDeferredEvaluatorDeleter::operator()(IDeferredEvaluator* pDeferredEvaluatorToDelete)
			{
				assert(m_pDeferredEvaluatorFactory);
				m_pDeferredEvaluatorFactory->DestroyDeferredEvaluator(pDeferredEvaluatorToDelete);
			}

		} // namespace Internal

	}
}
