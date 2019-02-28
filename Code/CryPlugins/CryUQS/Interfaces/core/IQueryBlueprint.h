// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{
		//===================================================================================
		//
		// ITextualQueryBlueprint
		//
		//===================================================================================

		struct ITextualQueryBlueprint
		{
			virtual                                                     ~ITextualQueryBlueprint() {}

			//
			// methods for writing data
			//
			virtual void                                                SetName(const char* szName) = 0;
			virtual void                                                SetQueryFactoryName(const char* szFactoryName) = 0;
			virtual void                                                SetQueryFactoryGUID(const CryGUID& factoryGUID) = 0;
			virtual void                                                SetMaxItemsToKeepInResultSet(size_t maxItems) = 0;
			virtual ITextualGlobalConstantParamsBlueprint&              GetGlobalConstantParams() = 0;
			virtual ITextualGlobalRuntimeParamsBlueprint&               GetGlobalRuntimeParams() = 0;
			virtual ITextualGeneratorBlueprint&                         SetGenerator() = 0;             // GetGenerator() returns a nullptr until SetGenerator() gets called
			virtual ITextualEvaluatorBlueprint&                         AddInstantEvaluator() = 0;
			virtual ITextualEvaluatorBlueprint&                         AddDeferredEvaluator() = 0;
			virtual ITextualQueryBlueprint&                             AddChild() = 0;

			//
			// methods for reading data
			//

			virtual const char*                                         GetName() const = 0;
			virtual const char*                                         GetQueryFactoryName() const = 0;
			virtual const CryGUID&                                      GetQueryFactoryGUID() const = 0;
			virtual size_t                                              GetMaxItemsToKeepInResultSet() const = 0;
			virtual const ITextualGlobalConstantParamsBlueprint&        GetGlobalConstantParams() const = 0;
			virtual const ITextualGlobalRuntimeParamsBlueprint&         GetGlobalRuntimeParams() const = 0;
			virtual const ITextualGeneratorBlueprint*                   GetGenerator() const = 0;
			virtual size_t                                              GetInstantEvaluatorCount() const = 0;
			virtual const ITextualEvaluatorBlueprint&                   GetInstantEvaluator(size_t index) const = 0;
			virtual size_t                                              GetDeferredEvaluatorCount() const = 0;
			virtual const ITextualEvaluatorBlueprint&                   GetDeferredEvaluator(size_t index) const = 0;
			virtual size_t                                              GetChildCount() const = 0;
			virtual const ITextualQueryBlueprint&                       GetChild(size_t index) const = 0;

			//
			// syntax error collector
			//

			virtual void                                                SetSyntaxErrorCollector(DataSource::SyntaxErrorCollectorUniquePtr pSyntaxErrorCollector) = 0;
			virtual DataSource::ISyntaxErrorCollector*                  GetSyntaxErrorCollector() const = 0;     // called while resolving a blueprint from its textual representation into the "in-memory" representation
		};

		//===================================================================================
		//
		// IQueryBlueprint
		//
		//===================================================================================

		struct IQueryBlueprint
		{
			virtual                                                     ~IQueryBlueprint() {}
			virtual const char*                                         GetName() const = 0;
			virtual void                                                VisitRuntimeParams(Client::IQueryBlueprintRuntimeParamVisitor& visitor) const = 0;
			virtual const Shared::CTypeInfo&                            GetOutputType() const = 0;  // the type of the resulting items
		};

	}
}
