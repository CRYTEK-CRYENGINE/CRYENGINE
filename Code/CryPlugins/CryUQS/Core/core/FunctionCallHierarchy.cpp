// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FunctionCallHierarchy.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		bool CFunctionCallHierarchy::AddAndInstantiateFunctionBlueprint(const CFunctionBlueprint& functionBlueprintToInstantiate, const SQueryBlackboard& blackboard, Shared::CUqsString& error)
		{
			Client::FunctionUniquePtr pFunction = functionBlueprintToInstantiate.InstantiateCallHierarchy(blackboard, error);

			if (pFunction)
			{
				m_functionsToCall.push_back(std::move(pFunction));
				return true;
			}
			else
			{
				return false;
			}
		}

		// TODO: use cached memory offsets of all input parameters rather than making virtual method calls on registryToLookupParamsOffsets
		//       -> probably requires caching of these offsets by CBlueprintWithInputs::ResolveInputs() already and then propagate them from CBlueprintWithInputs::InstantiateFunctionCallHierarchy() to here
		void CFunctionCallHierarchy::ExecuteAll(const Client::IFunction::SExecuteContext& executeContext, void* pParamsToWriteTheReturnValuesTo, const Client::IInputParameterRegistry& registryToLookupParamsOffsets) const
		{
			assert(m_functionsToCall.size() == registryToLookupParamsOffsets.GetParameterCount());

			for (size_t i = 0, n = m_functionsToCall.size(); i < n; ++i)
			{
				const size_t offsetInParams = registryToLookupParamsOffsets.GetParameter(i).offset;
				void *pReturnValueDestination = static_cast<char*>(pParamsToWriteTheReturnValuesTo) + offsetInParams;
				const Client::IFunction* pFunction = m_functionsToCall[i].get();
				pFunction->Execute(executeContext, pReturnValueDestination);

				// bail out prematurely on exception (ignore the remaining function calls)
				if (executeContext.bExceptionOccurred)
				{
					return;
				}
			}
		}

	}
}
