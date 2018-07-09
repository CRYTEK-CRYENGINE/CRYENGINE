// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{
		namespace Internal
		{
			class CInstantEvaluatorDeleter;     // below
		}

		//===================================================================================
		//
		// InstantEvaluatorUniquePtr
		//
		//===================================================================================

		typedef std::unique_ptr<IInstantEvaluator, Internal::CInstantEvaluatorDeleter>  InstantEvaluatorUniquePtr;

		//===================================================================================
		//
		// IInstantEvaluatorFactory
		//
		//===================================================================================

		struct IInstantEvaluatorFactory
		{
			enum class ECostCategory
			{
				Cheap,
				Expensive
			};

			enum class EEvaluationModality
			{
				Testing,
				Scoring
				// TODO: combine both as well?
			};

			virtual                                 ~IInstantEvaluatorFactory() {}
			virtual const char*                     GetName() const = 0;
			virtual const CryGUID&                  GetGUID() const = 0;
			virtual const char*                     GetDescription() const = 0;
			virtual const IInputParameterRegistry&  GetInputParameterRegistry() const = 0;
			virtual InstantEvaluatorUniquePtr       CreateInstantEvaluator() = 0;
			virtual void                            DestroyInstantEvaluator(IInstantEvaluator* pInstantEvaluatorToDestroy) = 0;
			virtual ECostCategory                   GetCostCategory() const = 0;
			virtual EEvaluationModality             GetEvaluationModality() const = 0;
			virtual IParamsHolderFactory&           GetParamsHolderFactory() const = 0;
		};

		namespace Internal
		{

			//===================================================================================
			//
			// CInstantEvaluatorDeleter
			//
			//===================================================================================

			class CInstantEvaluatorDeleter
			{
			public:
				explicit                            CInstantEvaluatorDeleter();         // default ctor is required for when a smart pointer using this deleter gets implicitly constructed via nullptr (i. e. with only 1 argument for the smart pointer's ctor)
				explicit                            CInstantEvaluatorDeleter(IInstantEvaluatorFactory& instantEvaluatorFactory);
				void                                operator()(IInstantEvaluator* pInstantEvaluatorToDelete);

			private:
				IInstantEvaluatorFactory*           m_pInstantEvaluatorFactory;         // this one created the instant-evaluator before
			};

			inline CInstantEvaluatorDeleter::CInstantEvaluatorDeleter()
				: m_pInstantEvaluatorFactory(nullptr)
			{}

			inline CInstantEvaluatorDeleter::CInstantEvaluatorDeleter(IInstantEvaluatorFactory& instantEvaluatorFactory)
				: m_pInstantEvaluatorFactory(&instantEvaluatorFactory)
			{}

			inline void CInstantEvaluatorDeleter::operator()(IInstantEvaluator* pInstantEvaluatorToDelete)
			{
				assert(m_pInstantEvaluatorFactory);
				m_pInstantEvaluatorFactory->DestroyInstantEvaluator(pInstantEvaluatorToDelete);
			}

		} // namespace Internal

	}
}
