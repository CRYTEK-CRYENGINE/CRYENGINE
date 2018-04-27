// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CTextualGlobalConstantParamsBlueprint
		//
		//===================================================================================

		class CTextualGlobalConstantParamsBlueprint : public ITextualGlobalConstantParamsBlueprint
		{
		public:
			explicit                                      CTextualGlobalConstantParamsBlueprint();

			// ITextualGlobalConstantParamsBlueprint
			virtual void                                  AddParameter(const char* szName, const char* szTypeName, const CryGUID& typeGUID, const char* szValue, bool bAddToDebugRenderWorld, DataSource::SyntaxErrorCollectorUniquePtr pSyntaxErrorCollector) override;
			virtual size_t                                GetParameterCount() const override;
			virtual SParameterInfo                        GetParameter(size_t index) const override;
			// ~ITextualGlobalConstantParamsBlueprint

		private:
			struct SStoredParameterInfo
			{
				explicit                                  SStoredParameterInfo(const char* _szName, const char* _szTypeName, const CryGUID& _typeGUID, const char* _szValue, bool _bAddToDebugRenderWorld, DataSource::SyntaxErrorCollectorUniquePtr _pSyntaxErrorCollector);
				explicit                                  SStoredParameterInfo(SStoredParameterInfo&& other);
				string                                    name;
				string                                    typeName;
				CryGUID                                   typeGUID;
				string                                    value;
				bool                                      bAddToDebugRenderWorld;
				DataSource::SyntaxErrorCollectorUniquePtr pSyntaxErrorCollector;
			};

		private:
			                                              UQS_NON_COPYABLE(CTextualGlobalConstantParamsBlueprint);

		private:
			std::vector<SStoredParameterInfo>             m_parameters;
		};

		inline CTextualGlobalConstantParamsBlueprint::SStoredParameterInfo::SStoredParameterInfo(const char* _szName, const char* _szTypeName, const CryGUID& _typeGUID, const char* _szValue, bool _bAddToDebugRenderWorld, DataSource::SyntaxErrorCollectorUniquePtr _pSyntaxErrorCollector)
			: name(_szName)
			, typeName(_szTypeName)
			, typeGUID(_typeGUID)
			, value(_szValue)
			, bAddToDebugRenderWorld(_bAddToDebugRenderWorld)
			, pSyntaxErrorCollector(std::move(_pSyntaxErrorCollector))
		{}

		inline CTextualGlobalConstantParamsBlueprint::SStoredParameterInfo::SStoredParameterInfo(SStoredParameterInfo&& other)
			: name(std::move(other.name))
			, typeName(std::move(other.typeName))
			, typeGUID(std::move(other.typeGUID))
			, value(std::move(other.value))
			, bAddToDebugRenderWorld(std::move(other.bAddToDebugRenderWorld))
			, pSyntaxErrorCollector(std::move(other.pSyntaxErrorCollector))
		{}

		//===================================================================================
		//
		// CGlobalConstantParamsBlueprint
		//
		//===================================================================================

		class CGlobalConstantParamsBlueprint
		{
		public:
			struct SParamInfo
			{
				explicit                        SParamInfo(Client::IItemFactory* _pItemFactory, void* _pItem, bool _bAddToDebugRenderWorld);
				Client::IItemFactory*           pItemFactory;
				void*                           pItem;
				bool                            bAddToDebugRenderWorld;
			};

		public:
			explicit                            CGlobalConstantParamsBlueprint();
			                                    ~CGlobalConstantParamsBlueprint();

			bool                                Resolve(const ITextualGlobalConstantParamsBlueprint& source);
			const std::map<string, SParamInfo>& GetParams() const;
			void                                AddSelfToDictAndReplace(Shared::CVariantDict& out) const;
			void                                PrintToConsole(CLogger& logger) const;

		private:
			                                    UQS_NON_COPYABLE(CGlobalConstantParamsBlueprint);

		private:
			std::map<string, SParamInfo>        m_constantParams;
		};

		inline CGlobalConstantParamsBlueprint::SParamInfo::SParamInfo(Client::IItemFactory* _pItemFactory, void* _pItem, bool _bAddToDebugRenderWorld)
			: pItemFactory(_pItemFactory)
			, pItem(_pItem)
			, bAddToDebugRenderWorld(_bAddToDebugRenderWorld)
		{}

	}
}
