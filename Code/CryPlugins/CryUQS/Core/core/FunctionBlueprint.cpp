// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FunctionBlueprint.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		CFunctionBlueprint::CFunctionBlueprint(Client::IFunctionFactory& functionFactory, const CLeafFunctionReturnValue& leafFunctionReturnValue, bool bAddReturnValueToDebugRenderWorldUponExecution)
			: m_functionFactory(functionFactory)
			, m_returnValueInCaseOfLeafFunction(leafFunctionReturnValue)
			, m_bAddReturnValueToDebugRenderWorldUponExecution(bAddReturnValueToDebugRenderWorldUponExecution)
		{
		}

		Client::FunctionUniquePtr CFunctionBlueprint::InstantiateCallHierarchy(const SQueryBlackboard& blackboard, Shared::CUqsString& error) const
		{
			const CLeafFunctionReturnValue* pLeafFunctionReturnValue = m_returnValueInCaseOfLeafFunction.IsActuallyALeafFunction() ? &m_returnValueInCaseOfLeafFunction : nullptr;
			const Client::IFunction::SCtorContext ctorContext(pLeafFunctionReturnValue, blackboard, m_functionFactory.GetInputParameterRegistry(), m_bAddReturnValueToDebugRenderWorldUponExecution);
			Client::FunctionUniquePtr pFunc = m_functionFactory.CreateFunction(ctorContext);
			assert(pFunc);       // function factories are never supposed to return nullptr

			//
			// recursively build our input parameters (which are live functions that return the parameter value when getting called)
			//

			for (const CFunctionBlueprint* pFuncBP : m_resolvedInputs)
			{
				Client::FunctionUniquePtr pChildFunc = pFuncBP->InstantiateCallHierarchy(blackboard, error);
				if (pChildFunc == nullptr)
				{
					// an error occurred and already got written to given output error string
					return nullptr;
				}
				pFunc->AddChildAndTransferItsOwnership(pChildFunc);
			}

			//
			// validate the live function
			//

			Client::IFunction::SValidationContext validationContext(m_functionFactory.GetName(), error);

#ifdef UQS_CHECK_RETURN_TYPE_CONSISTENCY_IN_CALL_HIERARCHY
			// ensure all child functions are present and that the types of their return values match our input types
			pFunc->DebugAssertChildReturnTypes();
#endif

			// ensure all runtime-parameters are present and match their types
			if (!pFunc->ValidateDynamic(validationContext))
			{
				return nullptr;
			}

			return pFunc;
		}

		Client::IFunctionFactory& CFunctionBlueprint::GetFactory() const
		{
			return m_functionFactory;
		}

	}
}
