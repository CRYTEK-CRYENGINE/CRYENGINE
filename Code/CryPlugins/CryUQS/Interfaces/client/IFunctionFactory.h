// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		//===================================================================================
		//
		// IFunctionFactory
		//
		//===================================================================================

		struct IFunctionFactory
		{
			// - this is basically a helper for the editor to allow the user to pick only very specific "return values" for leaf-functions from a UI control
			// - it's also used by some validation code to perform special validation depending on what kind of function we're dealing with
			enum class ELeafFunctionKind
			{
				None,             // we're not a leaf-function (since we have at least one input parameter)
				GlobalParam,      // the function returns one of the global parameters
				IteratedItem,     // the function returns the item that is currently being iterated on in the "main query loop"
				Literal,          // the function returns a literal value that was parsed from its textual representation; the same functionality could already be achieved via a global parameter, but literals are easier to set up for ad-hoc tests
				ShuttledItems,    // the function returns the item list that was the output of the previous query
			};

			virtual                                 ~IFunctionFactory() {}
			virtual const char*                     GetName() const = 0;
			virtual const CryGUID&                  GetGUID() const = 0;
			virtual const char*                     GetDescription() const = 0;
			virtual const IInputParameterRegistry&  GetInputParameterRegistry() const = 0;
			virtual const Shared::CTypeInfo&        GetReturnType() const = 0;
			virtual const Shared::CTypeInfo*        GetContainedType() const = 0;   // this is for ELeafFunctionKind::ShuttledItems functions: these functions actually return a ["pointer-to" ("list-of" "item")] and we need to get the type of that "item"
			virtual ELeafFunctionKind               GetLeafFunctionKind() const = 0;
			virtual FunctionUniquePtr               CreateFunction(const IFunction::SCtorContext& ctorContext) = 0;
			virtual void                            DestroyFunction(IFunction* pFunctionToDestroy) = 0;
		};

		namespace Internal
		{
			//===================================================================================
			//
			// CFunctionDeleter - deleter functor for std::unique_ptr that uses the original factory to destroy the function
			//
			//===================================================================================

			class CFunctionDeleter
			{
			public:
				explicit                            CFunctionDeleter();       // default ctor is required for when smart pointer using this deleter gets implicitly constructed via nullptr (i. e. with only 1 argument for the smart pointer's ctor)
				explicit                            CFunctionDeleter(IFunctionFactory& functionFactory);
				void                                operator()(IFunction* pFunctionToDestroy);

			private:
				IFunctionFactory*                   m_pFunctionFactory;
			};

			inline CFunctionDeleter::CFunctionDeleter()
				: m_pFunctionFactory(nullptr)
			{}

			inline CFunctionDeleter::CFunctionDeleter(IFunctionFactory& functionFactory)
				: m_pFunctionFactory(&functionFactory)
			{}

			inline void CFunctionDeleter::operator()(IFunction* pFunctionToDestroy)
			{
				assert(m_pFunctionFactory);
				m_pFunctionFactory->DestroyFunction(pFunctionToDestroy);
			}
		} // namespace Internal
	}
}
