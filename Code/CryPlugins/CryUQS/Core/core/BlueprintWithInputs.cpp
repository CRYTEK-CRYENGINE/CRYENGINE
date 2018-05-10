// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BlueprintWithInputs.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		CBlueprintWithInputs::~CBlueprintWithInputs()
		{
			for(CFunctionBlueprint* pFuncBP : m_resolvedInputs)
			{
				delete pFuncBP;
			}
		}

		bool CBlueprintWithInputs::InstantiateFunctionCallHierarchy(CFunctionCallHierarchy& out, const SQueryBlackboard& blackboard, Shared::CUqsString& error) const
		{
			for (size_t i = 0; i < m_resolvedInputs.size(); ++i)
			{
				const CFunctionBlueprint* pFuncBP = m_resolvedInputs[i];
				assert(pFuncBP);
				if (!out.AddAndInstantiateFunctionBlueprint(*pFuncBP, blackboard, error))
				{
					return false;
				}
			}
			return true;
		}

		void CBlueprintWithInputs::ResolveInputs(const CInputBlueprint& rootOfInputs)
		{
			for (size_t i = 0; i < rootOfInputs.GetChildCount(); ++i)
			{
				const CInputBlueprint& input = rootOfInputs.GetChild(i);
				Client::IFunctionFactory* pFunctionFactory = input.GetFunctionFactory();
				assert(pFunctionFactory);
				const CLeafFunctionReturnValue& returnValueInCaseOfLeafFunction = input.GetLeafFunctionReturnValue();
				bool bAddReturnValueToDebugRenderWorldUponExecution = input.GetAddReturnValueToDebugRenderWorldUponExecution();
				CFunctionBlueprint* pFunctionBlueprint = new CFunctionBlueprint(*pFunctionFactory, returnValueInCaseOfLeafFunction, bAddReturnValueToDebugRenderWorldUponExecution);
				m_resolvedInputs.push_back(pFunctionBlueprint);
				pFunctionBlueprint->ResolveInputs(input);
			}
		}

	}
}
