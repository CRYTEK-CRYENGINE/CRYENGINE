// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CDeferredEvaluatorBlueprint
		//
		//===================================================================================

		class CDeferredEvaluatorBlueprint final : public CEvaluatorBlueprintBase
		{
		public:

			explicit                                        CDeferredEvaluatorBlueprint();
			Client::IDeferredEvaluatorFactory&              GetFactory() const;
			void                                            PrintToConsole(CLogger& logger, const char* szMessagePrefix) const;

		private:

			// CEvaluatorBlueprintBase
			virtual bool                                    ResolveFactory(const ITextualEvaluatorBlueprint& source) override;
			virtual const Client::IInputParameterRegistry&  GetInputParameterRegistry() const override;
			// ~CEvaluatorBlueprintBase

		private:

			Client::IDeferredEvaluatorFactory*              m_pDeferredEvaluatorFactory;
		};

	}
}
