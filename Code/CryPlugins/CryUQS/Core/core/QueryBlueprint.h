// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CTextualQueryBlueprint
		//
		//===================================================================================

		class CTextualQueryBlueprint final : public ITextualQueryBlueprint
		{
		public:
			explicit                                                   CTextualQueryBlueprint();
			                                                           ~CTextualQueryBlueprint();

			///////////////////////////////////////////////////////////////////////////////////
			//
			// called by a loader that reads from an abstract data source to build the blueprint in textual form
			//
			///////////////////////////////////////////////////////////////////////////////////

			virtual void                                               SetName(const char* szName) override;
			virtual void                                               SetQueryFactoryName(const char* szFactoryName) override;
			virtual void                                               SetQueryFactoryGUID(const CryGUID& factoryGUID) override;
			virtual void                                               SetMaxItemsToKeepInResultSet(size_t maxItems) override;
			virtual ITextualGlobalConstantParamsBlueprint&             GetGlobalConstantParams() override;
			virtual ITextualGlobalRuntimeParamsBlueprint&              GetGlobalRuntimeParams() override;
			virtual ITextualGeneratorBlueprint&                        SetGenerator() override;
			virtual ITextualEvaluatorBlueprint&                        AddInstantEvaluator() override;
			virtual ITextualEvaluatorBlueprint&                        AddDeferredEvaluator() override;
			virtual ITextualQueryBlueprint&                            AddChild() override;

			///////////////////////////////////////////////////////////////////////////////////
			//
			// called by CQueryBlueprint::Resolve()
			//
			///////////////////////////////////////////////////////////////////////////////////

			virtual const char*                                        GetName() const override;
			virtual const char*                                        GetQueryFactoryName() const override;
			virtual const CryGUID&                                     GetQueryFactoryGUID() const override;
			virtual size_t                                             GetMaxItemsToKeepInResultSet() const override;
			virtual const ITextualGlobalConstantParamsBlueprint&       GetGlobalConstantParams() const override;
			virtual const ITextualGlobalRuntimeParamsBlueprint&        GetGlobalRuntimeParams() const override;
			virtual const ITextualGeneratorBlueprint*                  GetGenerator() const override;
			virtual size_t                                             GetInstantEvaluatorCount() const override;
			virtual const ITextualEvaluatorBlueprint&                  GetInstantEvaluator(size_t index) const override;
			virtual size_t                                             GetDeferredEvaluatorCount() const override;
			virtual const ITextualEvaluatorBlueprint&                  GetDeferredEvaluator(size_t index) const override;
			virtual size_t                                             GetChildCount() const override;
			virtual const ITextualQueryBlueprint&                      GetChild(size_t index) const override;

			//
			// syntax error collector
			//

			virtual void                                               SetSyntaxErrorCollector(DataSource::SyntaxErrorCollectorUniquePtr pSyntaxErrorCollector) override;
			virtual DataSource::ISyntaxErrorCollector*                 GetSyntaxErrorCollector() const override;

		private:
			                                                           UQS_NON_COPYABLE(CTextualQueryBlueprint);

		private:
			string                                                     m_name;
			string                                                     m_queryFactoryName;
			CryGUID                                                    m_queryFactoryGUID;
			size_t                                                     m_maxItemsToKeepInResultSet;
			CTextualGlobalConstantParamsBlueprint                      m_globalConstantParams;
			CTextualGlobalRuntimeParamsBlueprint                       m_globalRuntimeParams;
			std::unique_ptr<CTextualGeneratorBlueprint>                m_pGenerator;
			std::vector<CTextualEvaluatorBlueprint*>                   m_instantEvaluators;
			std::vector<CTextualEvaluatorBlueprint*>                   m_deferredEvaluators;
			DataSource::SyntaxErrorCollectorUniquePtr                  m_pSyntaxErrorCollector;
			std::vector<CTextualQueryBlueprint*>                       m_children;
		};

		//===================================================================================
		//
		// CQueryBlueprint
		//
		//===================================================================================

		class CQueryBlueprint : public IQueryBlueprint
		{
		public:
			explicit                                           CQueryBlueprint();
			                                                   ~CQueryBlueprint();

			// IQueryBlueprint
			virtual const char*                                GetName() const override;
			virtual void                                       VisitRuntimeParams(Client::IQueryBlueprintRuntimeParamVisitor& visitor) const override;
			virtual const Shared::CTypeInfo&                   GetOutputType() const override;
			// ~IQueryBlueprint

			// - called when resolving a textual blueprint into its compiled counterpart
			// - after that, the blueprint can be instantiated as "live" Query at any time
			bool                                               Resolve(const ITextualQueryBlueprint& source);

			const CQueryBlueprint*                             GetParent() const;
			size_t                                             GetChildCount() const;
			std::shared_ptr<const CQueryBlueprint>             GetChild(size_t index) const;
			int                                                GetChildIndex(const CQueryBlueprint* pChildToSearchFor) const;  // returns -1 if given child is not among m_children

			QueryBaseUniquePtr                                 CreateQuery(const CQueryBase::SCtorContext& ctorContext) const;

			// these are called during the instantiation of a "live" Query
			int                                                GetMaxItemsToKeepInResultSet() const;
			const CGlobalConstantParamsBlueprint&              GetGlobalConstantParamsBlueprint() const;
			const CGlobalRuntimeParamsBlueprint&               GetGlobalRuntimeParamsBlueprint() const;
			const CGeneratorBlueprint*                         GetGeneratorBlueprint() const;
			const std::vector<CInstantEvaluatorBlueprint*>&    GetInstantEvaluatorBlueprints() const;
			const std::vector<CDeferredEvaluatorBlueprint*>&   GetDeferredEvaluatorBlueprints() const;
			bool                                               CheckPresenceAndTypeOfGlobalRuntimeParamsRecursively(const Shared::IVariantDict& runtimeParamsToValidate, Shared::CUqsString& error) const;

			const Shared::CTypeInfo*                           GetTypeOfShuttledItemsToExpect() const;
			const CQueryFactoryBase&                           GetQueryFactory() const;

			// debugging: dumps query-blueprint details to the console
			void                                               PrintToConsole(CLogger& logger) const;

		private:
			                                                   UQS_NON_COPYABLE(CQueryBlueprint);

			void                                               SortInstantEvaluatorBlueprintsByCostAndEvaluationModality();
			void                                               GrabRuntimeParamsRecursively(std::map<string, Client::IItemFactory*>& out) const;

		private:
			string                                             m_name;
			CQueryFactoryBase*                                 m_pQueryFactory;
			int                                                m_maxItemsToKeepInResultSet;		// item limitation gets only effective if this is > 0
			CGlobalConstantParamsBlueprint                     m_globalConstantParams;
			CGlobalRuntimeParamsBlueprint                      m_globalRuntimeParams;
			std::unique_ptr<CGeneratorBlueprint>               m_pGenerator;
			std::vector<CInstantEvaluatorBlueprint*>           m_instantEvaluators;
			std::vector<CDeferredEvaluatorBlueprint*>          m_deferredEvaluators;
			std::vector<std::shared_ptr<CQueryBlueprint>>      m_children;
			const CQueryBlueprint*                             m_pParent;
		};

	}
}
