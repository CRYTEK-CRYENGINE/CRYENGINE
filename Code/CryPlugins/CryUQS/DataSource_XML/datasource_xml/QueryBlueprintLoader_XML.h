// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace DataSource_XML
	{

		class CXMLDataErrorCollector;

		//===================================================================================
		//
		// CQueryBlueprintLoader_XML
		//
		//===================================================================================

		class CQueryBlueprintLoader_XML : public DataSource::IQueryBlueprintLoader
		{
		public:
			explicit                                  CQueryBlueprintLoader_XML(const char* szQueryName, const char* szXmlFilePath, const std::shared_ptr<CXMLDataErrorCollector>& pDataErrorCollector);
			virtual bool                              LoadTextualQueryBlueprint(Core::ITextualQueryBlueprint& out, Shared::IUqsString& error) override;

		private:
			bool                                      ParseQueryElement(const XmlNodeRef& queryElement, Shared::IUqsString& error);
			bool                                      ParseGlobalParamsElement(const XmlNodeRef& globalParamsElement, Shared::IUqsString& error);
			bool                                      ParseGeneratorElement(const XmlNodeRef& generatorElement, Shared::IUqsString& error);
			bool                                      ParseInstantEvaluatorElement(const XmlNodeRef& instantEvaluatorElement, Shared::IUqsString& error);
			bool                                      ParseDeferredEvaluatorElement(const XmlNodeRef& deferredEvaluatorElement, Shared::IUqsString& error);
			bool                                      ParseFunctionElement(const XmlNodeRef& functionElement, Core::ITextualInputBlueprint& parentInput, Shared::IUqsString& error);
			bool                                      ParseInputElement(const XmlNodeRef& inputElement, Core::ITextualInputBlueprint& parentInput, Shared::IUqsString& error);
			bool                                      CommonParseEvaluatorElement(const XmlNodeRef& evaluatorElement, const char* szAttributeForEvaluatorFactoryGUID, Core::ITextualEvaluatorBlueprint&  outTextualEvaluatorBP, Shared::IUqsString& error);  // common code for ParseInstantEvaluatorElement() and ParseDeferredEvaluatorElement()

		private:
			string                                    m_queryName;
			string                                    m_xmlFilePath;
			XmlNodeRef                                m_queryElement;
			std::shared_ptr<CXMLDataErrorCollector>   m_pDataErrorCollector;
			Core::ITextualQueryBlueprint*             m_pQuery;	// points to what was passed in to LoadTextualQueryBlueprint() throughout the loading process
		};

	}
}
