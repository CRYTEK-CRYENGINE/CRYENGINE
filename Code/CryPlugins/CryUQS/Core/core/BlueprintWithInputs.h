// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		struct SItemIterationContext;
		class CFunctionCallHierarchy;
		class CFunctionBlueprint;

		//===================================================================================
		//
		// CBlueprintWithInputs
		//
		// - base class for blueprints that get their input parameters via function calls
		//
		//===================================================================================

		class CBlueprintWithInputs
		{
		public:
			bool                               InstantiateFunctionCallHierarchy(CFunctionCallHierarchy& out, const SQueryBlackboard& blackboard, Shared::CUqsString& error) const;

		protected:
			explicit                           CBlueprintWithInputs() {}
			                                   ~CBlueprintWithInputs();

			void                               ResolveInputs(const CInputBlueprint& rootOfInputs);

		private:
			                                   UQS_NON_COPYABLE(CBlueprintWithInputs);

		protected:
			std::vector<CFunctionBlueprint*>   m_resolvedInputs;				// input parameters in order of how they appear at runtime
		};

	}
}
