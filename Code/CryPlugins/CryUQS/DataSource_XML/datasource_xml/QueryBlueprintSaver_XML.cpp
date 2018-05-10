// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QueryBlueprintSaver_XML.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace DataSource_XML
	{

		CQueryBlueprintSaver_XML::CQueryBlueprintSaver_XML(const char* szXmlFileNameToSaveTo)
			: m_xmlFileNameToSaveTo(szXmlFileNameToSaveTo)
			, m_queryElementToSaveTo()
			, m_pQuery(nullptr)
		{
			// nothing
		}

		bool CQueryBlueprintSaver_XML::SaveTextualQueryBlueprint(const Core::ITextualQueryBlueprint& queryBlueprintToSave, Shared::IUqsString& error)
		{
			m_queryElementToSaveTo = gEnv->pSystem->CreateXmlNode("Query");

			m_pQuery = &queryBlueprintToSave;
			SaveQueryElement(m_queryElementToSaveTo);
			m_pQuery = nullptr;

			if (m_queryElementToSaveTo->saveToFile(m_xmlFileNameToSaveTo.c_str()))
			{
				return true;
			}
			else
			{
				error.Format("Could not save the query-blueprint to file '%s' (file might be read-only)", m_xmlFileNameToSaveTo.c_str());
				return false;
			}
		}

		void CQueryBlueprintSaver_XML::SaveQueryElement(const XmlNodeRef& queryElementToSaveTo)
		{
			assert(queryElementToSaveTo->isTag("Query"));

			// notice: we don't save the name of the query blueprint here, as the caller has a better understanding of how to deal with that
			//         (e. g. the name might be that of the file in which the query blueprint is stored)

			// "factory" attribute
			queryElementToSaveTo->setAttr("factory", m_pQuery->GetQueryFactoryName());

			// "queryFactoryGUID" attribute
			Shared::CUqsString queryFactoryGuidAsString;
			Shared::Internal::CGUIDHelper::ToString(m_pQuery->GetQueryFactoryGUID(), queryFactoryGuidAsString);
			queryElementToSaveTo->setAttr("queryFactoryGUID", queryFactoryGuidAsString.c_str());

			// "maxItemsToKeepInResultSet" attribute
			uint64 maxItemsToKeepInResultSet = m_pQuery->GetMaxItemsToKeepInResultSet();  // notice: we use an uint64 because that will match the overloaded setAttr() that takes an additional parameter to specify hex format (which we will explicitly disallow!)
			const bool bUseHexFormat = false;
			queryElementToSaveTo->setAttr("maxItemsToKeepInResultSet", maxItemsToKeepInResultSet, bUseHexFormat);

			// <GlobalParams>
			SaveGlobalParamsElement(queryElementToSaveTo->newChild("GlobalParams"));

			// <Generator>
			if (m_pQuery->GetGenerator() != nullptr)
			{
				SaveGeneratorElement(queryElementToSaveTo->newChild("Generator"));
			}

			// all <InstantEvaluator>s
			for (size_t i = 0; i < m_pQuery->GetInstantEvaluatorCount(); ++i)
			{
				const Core::ITextualEvaluatorBlueprint& textualInstantEvaluatorBP = m_pQuery->GetInstantEvaluator(i);
				XmlNodeRef instantEvaluatorElement = queryElementToSaveTo->newChild("InstantEvaluator");
				SaveInstantEvaluatorElement(instantEvaluatorElement, textualInstantEvaluatorBP);
			}

			// all <DeferredEvaluator>s
			for (size_t i = 0; i < m_pQuery->GetDeferredEvaluatorCount(); ++i)
			{
				const Core::ITextualEvaluatorBlueprint& textualDeferredEvaluatorBP = m_pQuery->GetDeferredEvaluator(i);
				XmlNodeRef deferredEvaluatorElement = queryElementToSaveTo->newChild("DeferredEvaluator");
				SaveDeferredEvaluatorElement(deferredEvaluatorElement, textualDeferredEvaluatorBP);
			}

			// all child <Query>s
			const Core::ITextualQueryBlueprint* pParent = m_pQuery;
			for (size_t i = 0; i < pParent->GetChildCount(); ++i)
			{
				const Core::ITextualQueryBlueprint& childQueryBP = pParent->GetChild(i);
				m_pQuery = &childQueryBP;
				XmlNodeRef childQueryElement = queryElementToSaveTo->newChild("Query");
				SaveQueryElement(childQueryElement);
			}
			m_pQuery = pParent;
		}

		void CQueryBlueprintSaver_XML::SaveGlobalParamsElement(const XmlNodeRef& globalParamsElementToSaveTo)
		{
			assert(globalParamsElementToSaveTo->isTag("GlobalParams"));

			// <ConstantParam>s
			{
				const Core::ITextualGlobalConstantParamsBlueprint& constantParamsBP = m_pQuery->GetGlobalConstantParams();
				for (size_t i = 0; i < constantParamsBP.GetParameterCount(); ++i)
				{
					const Core::ITextualGlobalConstantParamsBlueprint::SParameterInfo pi = constantParamsBP.GetParameter(i);
					XmlNodeRef constantParamElement = globalParamsElementToSaveTo->newChild("ConstantParam");
					Shared::CUqsString typeGuidAsString;
					Shared::Internal::CGUIDHelper::ToString(pi.typeGUID, typeGuidAsString);
					constantParamElement->setAttr("name", pi.szName);
					constantParamElement->setAttr("type", pi.szTypeName);
					constantParamElement->setAttr("typeGUID", typeGuidAsString.c_str());  // should be called "typeName", but is already in use now
					constantParamElement->setAttr("value", pi.szValue);
					constantParamElement->setAttr("addToDebugRenderWorld", pi.bAddToDebugRenderWorld);
				}
			}

			// <RuntimeParam>s
			{
				const Core::ITextualGlobalRuntimeParamsBlueprint& runtimeParamsBP = m_pQuery->GetGlobalRuntimeParams();
				for (size_t i = 0; i < runtimeParamsBP.GetParameterCount(); ++i)
				{
					const Core::ITextualGlobalRuntimeParamsBlueprint::SParameterInfo pi = runtimeParamsBP.GetParameter(i);
					XmlNodeRef runtimeParamElement = globalParamsElementToSaveTo->newChild("RuntimeParam");
					Shared::CUqsString typeGuidAsString;
					Shared::Internal::CGUIDHelper::ToString(pi.typeGUID, typeGuidAsString);
					runtimeParamElement->setAttr("name", pi.szName);
					runtimeParamElement->setAttr("type", pi.szTypeName);  // should be called "typeName", but is already in use now
					runtimeParamElement->setAttr("typeGUID", typeGuidAsString.c_str());
					runtimeParamElement->setAttr("addToDebugRenderWorld", pi.bAddToDebugRenderWorld);
				}
			}
		}

		void CQueryBlueprintSaver_XML::SaveGeneratorElement(const XmlNodeRef& generatorElementToSaveTo)
		{
			assert(generatorElementToSaveTo->isTag("Generator"));
			assert(m_pQuery->GetGenerator() != nullptr);

			const Core::ITextualGeneratorBlueprint* pGeneratorBP = m_pQuery->GetGenerator();

			// "name"
			generatorElementToSaveTo->setAttr("name", pGeneratorBP->GetGeneratorName());

			// "generatorFactoryGUID"
			Shared::CUqsString generatorFactoryGuidAsString;
			Shared::Internal::CGUIDHelper::ToString(pGeneratorBP->GetGeneratorGUID(), generatorFactoryGuidAsString);
			generatorElementToSaveTo->setAttr("generatorFactoryGUID", generatorFactoryGuidAsString.c_str());

			// all <Input>s
			{
				const Core::ITextualInputBlueprint& inputRootBP = pGeneratorBP->GetInputRoot();
				for (size_t i = 0; i < inputRootBP.GetChildCount(); ++i)
				{
					const Core::ITextualInputBlueprint& inputBP = inputRootBP.GetChild(i);
					XmlNodeRef inputElement = generatorElementToSaveTo->newChild("Input");
					SaveInputElement(inputElement, inputBP);
				}
			}
		}

		void CQueryBlueprintSaver_XML::SaveInstantEvaluatorElement(const XmlNodeRef& instantEvaluatorElementToSaveTo, const Core::ITextualEvaluatorBlueprint& instantEvaluatorBP)
		{
			assert(instantEvaluatorElementToSaveTo->isTag("InstantEvaluator"));

			CommonSaveEvaluatorElement(instantEvaluatorElementToSaveTo, "instantEvaluatorFactoryGUID", instantEvaluatorBP);
		}

		void CQueryBlueprintSaver_XML::SaveDeferredEvaluatorElement(const XmlNodeRef& deferredEvaluatorElementToSaveTo, const Core::ITextualEvaluatorBlueprint& deferredEvaluatorBP)
		{
			assert(deferredEvaluatorElementToSaveTo->isTag("DeferredEvaluator"));

			CommonSaveEvaluatorElement(deferredEvaluatorElementToSaveTo, "deferredEvaluatorFactoryGUID", deferredEvaluatorBP);
		}

		void CQueryBlueprintSaver_XML::SaveFunctionElement(const XmlNodeRef& functionElementToSaveTo, const Core::ITextualInputBlueprint& parentInput)
		{
			assert(functionElementToSaveTo->isTag("Function"));

			// <Function>'s "name" attribute (name of the function)
			functionElementToSaveTo->setAttr("name", parentInput.GetFuncName());

			// <Function>'s "functionFactoryGUID" attribute
			Shared::CUqsString functionFactoryGuidAsString;
			Shared::Internal::CGUIDHelper::ToString(parentInput.GetFuncGUID(), functionFactoryGuidAsString);
			functionElementToSaveTo->setAttr("functionFactoryGUID", functionFactoryGuidAsString.c_str());

			// <Function>'s "addReturnValueToDebugRenderWorldUponExecution" attribute
			functionElementToSaveTo->setAttr("addReturnValueToDebugRenderWorldUponExecution", parentInput.GetAddReturnValueToDebugRenderWorldUponExecution());  // notice: setAttr() will convert the passed in bool to an int

			// <Function>'s "returnValue" attribute (only if we are a leaf-function!)
			if (parentInput.GetChildCount() == 0)
			{
				functionElementToSaveTo->setAttr("returnValue", parentInput.GetFuncReturnValueLiteral());
			}
			else
			{
				// all <Input>s (we're a non-leaf function!)
				for (size_t i = 0; i < parentInput.GetChildCount(); ++i)
				{
					const Core::ITextualInputBlueprint& inputBP = parentInput.GetChild(i);
					XmlNodeRef inputElement = functionElementToSaveTo->newChild("Input");
					SaveInputElement(inputElement, inputBP);
				}
			}
		}

		void CQueryBlueprintSaver_XML::SaveInputElement(const XmlNodeRef& inputElementToSaveTo, const Core::ITextualInputBlueprint& inputBP)
		{
			assert(inputElementToSaveTo->isTag("Input"));

			// "name" attribute (name of the parameter)
			inputElementToSaveTo->setAttr("name", inputBP.GetParamName());

			// "paramID" (unique ID of the parameter)
			char paramIdAsFourCharacterString[5];
			inputBP.GetParamID().ToString(paramIdAsFourCharacterString);
			inputElementToSaveTo->setAttr("paramID", paramIdAsFourCharacterString);

			// <Function> (value of the parameter)
			XmlNodeRef functionElement = inputElementToSaveTo->newChild("Function");

			SaveFunctionElement(functionElement, inputBP);
		}

		void CQueryBlueprintSaver_XML::CommonSaveEvaluatorElement(const XmlNodeRef& evaluatorElementToSaveTo, const char* szAttributeForEvaluatorFactoryGUID, const Core::ITextualEvaluatorBlueprint& evaluatorBP)
		{
			// "name" attribute
			{
				evaluatorElementToSaveTo->setAttr("name", evaluatorBP.GetEvaluatorName());
			}

			// attribute for the GUID of the evaluator-factory
			{
				Shared::CUqsString evaluatorFactoryGUIDAsString;
				Shared::Internal::CGUIDHelper::ToString(evaluatorBP.GetEvaluatorGUID(), evaluatorFactoryGUIDAsString);
				evaluatorElementToSaveTo->setAttr(szAttributeForEvaluatorFactoryGUID, evaluatorFactoryGUIDAsString.c_str());
			}

			// "weight" attribute
			{
				evaluatorElementToSaveTo->setAttr("weight", evaluatorBP.GetWeight());
			}

			// "scoreTransform" attribute (should actually be called "scoreTransformName", but this is now in use already)
			{
				evaluatorElementToSaveTo->setAttr("scoreTransform", evaluatorBP.GetScoreTransformName());
			}

			// "scoreTransformFactoryGUID" attribute (optional because of backwards compatibility, but must represent a valid GUID if present)
			{
				Shared::CUqsString guidAsString;
				Shared::Internal::CGUIDHelper::ToString(evaluatorBP.GetScoreTransformGUID(), guidAsString);
				evaluatorElementToSaveTo->setAttr("scoreTransformFactoryGUID", guidAsString.c_str());
			}

			// "negateDiscard" attribute
			{
				evaluatorElementToSaveTo->setAttr("negateDiscard", evaluatorBP.GetNegateDiscard());
			}

			// all <Input>s
			{
				const Core::ITextualInputBlueprint& inputRootBP = evaluatorBP.GetInputRoot();
				for (size_t i = 0; i < inputRootBP.GetChildCount(); ++i)
				{
					const Core::ITextualInputBlueprint& inputBP = inputRootBP.GetChild(i);
					XmlNodeRef inputElement = evaluatorElementToSaveTo->newChild("Input");
					SaveInputElement(inputElement, inputBP);
				}
			}
		}

	}
}
