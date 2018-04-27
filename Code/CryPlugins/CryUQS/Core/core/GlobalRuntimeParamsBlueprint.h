// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CTextualGlobalRuntimeParamsBlueprint
		//
		//===================================================================================

		class CTextualGlobalRuntimeParamsBlueprint : public ITextualGlobalRuntimeParamsBlueprint
		{
		public:
			explicit                                        CTextualGlobalRuntimeParamsBlueprint();

			// ITextualGlobalRuntimeParamsBlueprint
			virtual void                                    AddParameter(const char* szName, const char* szTypeName, const CryGUID& typeGUID, bool bAddToDebugRenderWorld, DataSource::SyntaxErrorCollectorUniquePtr pSyntaxErrorCollector) override;
			virtual size_t                                  GetParameterCount() const override;
			virtual SParameterInfo                          GetParameter(size_t index) const override;
			// ~ITextualGlobalRuntimeParamsBlueprint

		private:
			struct SStoredParameterInfo
			{
				explicit                                    SStoredParameterInfo(const char* _szName, const char* _szTypeName, const CryGUID& _typeGUID, bool _bAddToDebugRenderWorld, DataSource::SyntaxErrorCollectorUniquePtr _pSyntaxErrorCollector);
				explicit                                    SStoredParameterInfo(SStoredParameterInfo&& other);
				string                                      name;
				string                                      typeName;
				CryGUID                                     typeGUID;
				bool                                        bAddToDebugRenderWorld;
				DataSource::SyntaxErrorCollectorUniquePtr   pSyntaxErrorCollector;
			};

		private:
			                                                UQS_NON_COPYABLE(CTextualGlobalRuntimeParamsBlueprint);

		private:
			std::vector<SStoredParameterInfo>               m_parameters;
		};

		inline CTextualGlobalRuntimeParamsBlueprint::SStoredParameterInfo::SStoredParameterInfo(const char* _szName, const char* _szTypeName, const CryGUID& _typeGUID, bool _bAddToDebugRenderWorld, DataSource::SyntaxErrorCollectorUniquePtr _pSyntaxErrorCollector)
			: name(_szName)
			, typeName(_szTypeName)
			, typeGUID(_typeGUID)
			, bAddToDebugRenderWorld(_bAddToDebugRenderWorld)
			, pSyntaxErrorCollector(std::move(_pSyntaxErrorCollector))
		{}

		inline CTextualGlobalRuntimeParamsBlueprint::SStoredParameterInfo::SStoredParameterInfo(SStoredParameterInfo&& other)
			: name(std::move(other.name))
			, typeName(std::move(other.typeName))
			, typeGUID(std::move(other.typeGUID))
			, bAddToDebugRenderWorld(std::move(other.bAddToDebugRenderWorld))
			, pSyntaxErrorCollector(std::move(other.pSyntaxErrorCollector))
		{}

		//===================================================================================
		//
		// CGlobalRuntimeParamsBlueprint
		//
		//===================================================================================

		class CGlobalRuntimeParamsBlueprint
		{
		public:
			struct SParamInfo
			{
				explicit                                         SParamInfo(Client::IItemFactory* _pItemFactory, bool _bAddToDebugRenderWorld);
				Client::IItemFactory*                            pItemFactory;
				bool                                             bAddToDebugRenderWorld;
			};

		public:
			explicit                                             CGlobalRuntimeParamsBlueprint();

			bool                                                 Resolve(const ITextualGlobalRuntimeParamsBlueprint& source, const CQueryBlueprint* pParentQueryBlueprint);
			const std::map<string, SParamInfo>&                  GetParams() const;
			void                                                 PrintToConsole(CLogger& logger) const;

		private:
			                                                     UQS_NON_COPYABLE(CGlobalRuntimeParamsBlueprint);
			const Client::IItemFactory*                          FindItemFactoryByParamNameInParentRecursively(const char* szParamNameToSearchFor, const CQueryBlueprint& parentalQueryBlueprint) const;

		private:
			std::map<string, SParamInfo>                         m_runtimeParameters;
		};

		inline CGlobalRuntimeParamsBlueprint::SParamInfo::SParamInfo(Client::IItemFactory* _pItemFactory, bool _bAddToDebugRenderWorld)
			: pItemFactory(_pItemFactory)
			, bAddToDebugRenderWorld(_bAddToDebugRenderWorld)
		{}

	}
}
