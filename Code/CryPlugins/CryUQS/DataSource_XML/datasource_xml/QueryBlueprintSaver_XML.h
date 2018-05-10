// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace DataSource_XML
	{

		//===================================================================================
		//
		// CQueryBlueprintSaver_XML
		//
		//===================================================================================

		class CQueryBlueprintSaver_XML : public DataSource::IQueryBlueprintSaver
		{
		public:
			explicit                                    CQueryBlueprintSaver_XML(const char* szXmlFileNameToSaveTo);
			virtual bool                                SaveTextualQueryBlueprint(const Core::ITextualQueryBlueprint& queryBlueprintToSave, Shared::IUqsString& error) override;

		private:
			void                                        SaveQueryElement(const XmlNodeRef& queryElementToSaveTo);
			void                                        SaveGlobalParamsElement(const XmlNodeRef& globalParamsElementToSaveTo);
			void                                        SaveGeneratorElement(const XmlNodeRef& generatorElementToSaveTo);
			void                                        SaveInstantEvaluatorElement(const XmlNodeRef& instantEvaluatorElementToSaveTo, const Core::ITextualEvaluatorBlueprint& instantEvaluatorBP);
			void                                        SaveDeferredEvaluatorElement(const XmlNodeRef& deferredEvaluatorElementToSaveTo, const Core::ITextualEvaluatorBlueprint& deferredEvaluatorBP);
			void                                        SaveFunctionElement(const XmlNodeRef& functionElementToSaveTo, const Core::ITextualInputBlueprint& parentInput);
			void                                        SaveInputElement(const XmlNodeRef& inputElementToSaveTo, const Core::ITextualInputBlueprint& inputBP);
			void                                        CommonSaveEvaluatorElement(const XmlNodeRef& evaluatorElementToSaveTo, const char* szAttributeForEvaluatorFactoryGUID, const Core::ITextualEvaluatorBlueprint& evaluatorBP);  // common code for SaveInstantEvaluatorElement() and SaveDeferredEvaluatorElement()

		private:
			string                                      m_xmlFileNameToSaveTo;
			XmlNodeRef                                  m_queryElementToSaveTo;
			const Core::ITextualQueryBlueprint*         m_pQuery;	// points to what was passed in to SaveTextualQueryBlueprint() throughout the saving process
		};

	}
}
