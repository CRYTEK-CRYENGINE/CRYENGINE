// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CFunctionBlueprint
		//
		//===================================================================================

		class CFunctionBlueprint : public CBlueprintWithInputs
		{
		public:
			explicit                         CFunctionBlueprint(Client::IFunctionFactory& functionFactory, const CLeafFunctionReturnValue& leafFunctionReturnValue, bool bAddReturnValueToDebugRenderWorldUponExecution);

			Client::FunctionUniquePtr        InstantiateCallHierarchy(const SQueryBlackboard& blackboard, Shared::CUqsString& error) const;        // returns nullptr if runtime-param type validation failed or insufficient params were provided
			Client::IFunctionFactory&        GetFactory() const;

		private:
			                                 UQS_NON_COPYABLE(CFunctionBlueprint);

		private:
			Client::IFunctionFactory&        m_functionFactory;                 // creates the "live" function at runtime
			CLeafFunctionReturnValue         m_returnValueInCaseOfLeafFunction; // this is the return value in case of a leaf-function (they don't have input)
			bool                             m_bAddReturnValueToDebugRenderWorldUponExecution;
		};

	}
}
