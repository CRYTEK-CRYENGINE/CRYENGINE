// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		//===================================================================================
		//
		// IParamsHolder
		//
		// - holds the custom input parameters for generators, evaluators and functions and gives write-access to them through a void pointer
		// - the actual memory offset of each parameter is specified by the IInputParameterRegistry
		//
		//===================================================================================

		struct IParamsHolder
		{
			virtual         ~IParamsHolder() {}
			virtual void*   GetParams() = 0;
		};

		class CParamsHolderDeleter;   // below

		//===================================================================================
		//
		// ParamsHolderUniquePtr
		//
		//===================================================================================

		typedef std::unique_ptr<IParamsHolder, CParamsHolderDeleter> ParamsHolderUniquePtr;

		//===================================================================================
		//
		// IParamsHolderFactory
		//
		//===================================================================================

		struct IParamsHolderFactory
		{
			virtual                             ~IParamsHolderFactory() {}
			virtual ParamsHolderUniquePtr       CreateParamsHolder() = 0;
			virtual void                        DestroyParamsHolder(IParamsHolder* pParamsHolderToDestroy) = 0;
		};

		//===================================================================================
		//
		// CParamsHolderDeleter
		//
		//===================================================================================

		class CParamsHolderDeleter
		{
		public:
			explicit                CParamsHolderDeleter();  // default ctor is required for when a smart pointer using this deleter gets implicitly constructed via nullptr (i. e. with only 1 argument for the smart pointer's ctor)
			explicit                CParamsHolderDeleter(IParamsHolderFactory& paramsHolderFactory);
			void                    operator()(IParamsHolder* pParamsHolderToDelete);

		private:
			IParamsHolderFactory*   m_pParamsHolderFactory;              // this one created the params-holder before
		};

		inline CParamsHolderDeleter::CParamsHolderDeleter()
			: m_pParamsHolderFactory(nullptr)
		{}

		inline CParamsHolderDeleter::CParamsHolderDeleter(IParamsHolderFactory& paramsHolderFactory)
			: m_pParamsHolderFactory(&paramsHolderFactory)
		{}

		inline void CParamsHolderDeleter::operator()(IParamsHolder* pParamsHolderToDelete)
		{
			assert(m_pParamsHolderFactory);
			m_pParamsHolderFactory->DestroyParamsHolder(pParamsHolderToDelete);
		}

	}
}
