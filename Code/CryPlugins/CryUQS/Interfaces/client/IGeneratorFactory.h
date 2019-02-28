// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		struct IGenerator;

		namespace Internal
		{
			class CGeneratorDeleter;   // below
		}

		//===================================================================================
		//
		// GeneratorUniquePtr
		//
		//===================================================================================

		typedef std::unique_ptr<IGenerator, Internal::CGeneratorDeleter>  GeneratorUniquePtr;

		//===================================================================================
		//
		// IGeneratorFactory
		//
		//===================================================================================

		struct IGeneratorFactory
		{
			virtual                                  ~IGeneratorFactory() {}
			virtual const char*                      GetName() const = 0;
			virtual const CryGUID&                   GetGUID() const = 0;
			virtual const char*                      GetDescription() const = 0;
			virtual const IInputParameterRegistry&   GetInputParameterRegistry() const = 0;
			virtual const Shared::CTypeInfo*         GetTypeOfShuttledItemsToExpect() const = 0;
			virtual const Shared::CTypeInfo&         GetTypeOfItemsToGenerate() const = 0;
			virtual GeneratorUniquePtr               CreateGenerator(const void* pParams) = 0;
			virtual void                             DestroyGenerator(IGenerator* pGeneratorToDestroy) = 0;
			virtual IParamsHolderFactory&            GetParamsHolderFactory() const = 0;
		};

		namespace Internal
		{
			//===================================================================================
			//
			// CGeneratorDeleter - deleter functor for std::unique_ptr that uses the original factory to destroy the generator
			//
			//===================================================================================

			class CGeneratorDeleter
			{
			public:
				explicit                            CGeneratorDeleter();       // default ctor is required for when smart pointer using this deleter gets implicitly constructed via nullptr (i. e. with only 1 argument for the smart pointer's ctor)
				explicit                            CGeneratorDeleter(IGeneratorFactory& factory);
				void                                operator()(IGenerator* pGeneratorToDelete);

			private:
				IGeneratorFactory*                  m_pFactory;      // this one created the generator before
			};

			inline CGeneratorDeleter::CGeneratorDeleter()
				: m_pFactory(nullptr)
			{}

			inline CGeneratorDeleter::CGeneratorDeleter(IGeneratorFactory& factory)
				: m_pFactory(&factory)
			{}

			inline void CGeneratorDeleter::operator()(IGenerator* pGeneratorToDelete)
			{
				assert(m_pFactory);
				m_pFactory->DestroyGenerator(pGeneratorToDelete);
			}

		} // namespace Internal

	}
}
