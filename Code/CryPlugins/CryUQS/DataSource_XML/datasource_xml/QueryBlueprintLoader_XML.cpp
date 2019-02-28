// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QueryBlueprintLoader_XML.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace DataSource_XML
	{

		static void DeleteErrorProvider(DataSource::ISyntaxErrorCollector* pSyntaxErrorCollectorToDelete)
		{
			delete pSyntaxErrorCollectorToDelete;
		}

		static DataSource::SyntaxErrorCollectorUniquePtr MakeNewSyntaxErrorCollectorUniquePtr(int lineNumber, const std::shared_ptr<CXMLDataErrorCollector>& pDataErrorCollector)
		{
			return DataSource::SyntaxErrorCollectorUniquePtr(new CSyntaxErrorCollector_XML(lineNumber, pDataErrorCollector), DataSource::CSyntaxErrorCollectorDeleter(&DeleteErrorProvider));
		}

		CQueryBlueprintLoader_XML::CQueryBlueprintLoader_XML(const char* szQueryName, const char* szXmlFilePath, const std::shared_ptr<CXMLDataErrorCollector>& pDataErrorCollector)
			: m_queryName(szQueryName)
			, m_xmlFilePath(szXmlFilePath)
			, m_queryElement()
			, m_pDataErrorCollector(pDataErrorCollector)
			, m_pQuery(nullptr)
		{
			// nothing
		}

		bool CQueryBlueprintLoader_XML::LoadTextualQueryBlueprint(Core::ITextualQueryBlueprint& out, Shared::IUqsString& error)
		{
			// load the XML file
			m_queryElement = gEnv->pSystem->LoadXmlFromFile(m_xmlFilePath.c_str());
			if (!m_queryElement)
			{
				// file not found or invalid xml
				error.Format("XML file '%s' not found or it contains invalid xml", m_xmlFilePath.c_str());
				return false;
			}

			// expect <Query> root node
			if (!m_queryElement->isTag("Query"))
			{
				error.Format("XML file '%s': root node must be <Query>, but is <%s>", m_xmlFilePath.c_str(), m_queryElement->getTag());
				return false;
			}

			bool bSuccess;

			m_pQuery = &out;
			m_pQuery->SetName(m_queryName.c_str());
			bSuccess = ParseQueryElement(m_queryElement, error);
			m_pQuery = nullptr;

			return bSuccess;
		}

		bool CQueryBlueprintLoader_XML::ParseQueryElement(const XmlNodeRef& queryElement, Shared::IUqsString& error)
		{
			assert(queryElement->isTag("Query"));

			m_pQuery->SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(queryElement->getLine(), m_pDataErrorCollector));

			// notice: the name of the query blueprint came in from somewhere outside (e. g. file name) and has already been set by LoadTextualQueryBlueprint()

			// "factory" attribute
			const char* szFactory = queryElement->getAttr("factory");
			m_pQuery->SetQueryFactoryName(szFactory);

			// "queryFactoryGUID" attribute (optional because of backwards compatibility, but must represent a valid GUID if present)
			CryGUID queryFactoryGUID = CryGUID::Null();
			const char* szQueryFactoryGUID;
			if (queryElement->getAttr("queryFactoryGUID", &szQueryFactoryGUID))
			{
				if (!Shared::Internal::CGUIDHelper::TryParseFromString(queryFactoryGUID, szQueryFactoryGUID))
				{
					error.Format("line #%i: <Query>'s 'queryFactoryGUID' attribute is not in a valid GUID format: '%s'", queryElement->getLine(), szQueryFactoryGUID);
					return false;
				}
			}
			m_pQuery->SetQueryFactoryGUID(queryFactoryGUID);

			// "maxItemsToKeepInResultSet" attribute
			uint64 maxItemsToKeepInResultSet = 0;  // notice: we use an uint64 because that will match the overloaded getAttr() that takes an additional parameter to specify hex format (which we will explicitly disallow!)
			const bool bUseHexFormat = false;
			queryElement->getAttr("maxItemsToKeepInResultSet", maxItemsToKeepInResultSet, bUseHexFormat);
			m_pQuery->SetMaxItemsToKeepInResultSet((size_t)maxItemsToKeepInResultSet);

			for (int i = 0; i < queryElement->getChildCount(); ++i)
			{
				XmlNodeRef child = queryElement->getChild(i);

				if (child->isTag("GlobalParams"))
				{
					if (!ParseGlobalParamsElement(child, error))
						return false;
				}
				else if (child->isTag("Generator"))
				{
					if (!ParseGeneratorElement(child, error))
						return false;
				}
				else if (child->isTag("InstantEvaluator"))
				{
					if (!ParseInstantEvaluatorElement(child, error))
						return false;
				}
				else if (child->isTag("DeferredEvaluator"))
				{
					if (!ParseDeferredEvaluatorElement(child, error))
						return false;
				}
				else if (child->isTag("Query"))
				{
					// child queries

					Core::ITextualQueryBlueprint* pParentQuery = m_pQuery;
					m_pQuery = &m_pQuery->AddChild();
					const bool bSucceeded = ParseQueryElement(child, error);
					m_pQuery = pParentQuery;

					if (!bSucceeded)
						return false;
				}
				// TODO: <SecondaryGenerator> element(s) ?
				else
				{
					error.Format("line #%i: unknown tag: <%s>", child->getLine(), child->getTag());
					return false;
				}
			}

			return true;
		}

		bool CQueryBlueprintLoader_XML::ParseGlobalParamsElement(const XmlNodeRef& globalParamsElement, Shared::IUqsString& error)
		{
			assert(globalParamsElement->isTag("GlobalParams"));

			// <ConstantParam> and <RuntimeParam> elements
			for (int i = 0; i < globalParamsElement->getChildCount(); ++i)
			{
				XmlNodeRef child = globalParamsElement->getChild(i);

				if (child->isTag("ConstantParam"))
				{
					// "name" attribute
					const char* szName;
					if (!child->getAttr("name", &szName))
					{
						error.Format("line #%i: <ConstantParam> is missing the attribute 'name'", child->getLine());
						return false;
					}

					// "type" attribute (should actually be called "typeName", but is already in use by now)
					const char* szTypeName;
					if (!child->getAttr("type", &szTypeName))
					{
						error.Format("line #%i: <ConstantParam> is missing the attribute 'type'", child->getLine());
						return false;
					}

					// "typeGUID" (optional because of backwards compatibility, but must represent a valid GUID if present)
					CryGUID typeGUID = CryGUID::Null();
					{
						const char* szTypeGUID;
						if (child->getAttr("typeGUID", &szTypeGUID))
						{
							if (!Shared::Internal::CGUIDHelper::TryParseFromString(typeGUID, szTypeGUID))
							{
								error.Format("line #%i: <ConstantParam>'s attribute 'typeGUID' is not in a valid GUID format: '%s'", child->getLine(), szTypeGUID);
								return false;
							}
						}
					}

					// "value" attribute
					const char* szValue;
					if (!child->getAttr("value", &szValue))
					{
						error.Format("line #%i: <ConstantParam> is missing the attribute 'value'", child->getLine());
						return false;
					}

					// "addToDebugRenderWorld" attribute (optional because it got introduced after some query blueprints were already in use; default to 'false')
					bool bAddToDebugRenderWorld = false;
					child->getAttr("addToDebugRenderWorld", bAddToDebugRenderWorld);

					m_pQuery->GetGlobalConstantParams().AddParameter(szName, szTypeName, typeGUID, szValue, bAddToDebugRenderWorld, MakeNewSyntaxErrorCollectorUniquePtr(child->getLine(), m_pDataErrorCollector));
				}
				else if (child->isTag("RuntimeParam"))
				{
					// "name" attribute
					const char* szName;
					if (!child->getAttr("name", &szName))
					{
						error.Format("line #%i: <RuntimeParam> is missing the attribute 'name'", child->getLine());
						return false;
					}

					// "type" attribute (should actually be called "typeName", but is already in use by now)
					const char* szTypeName;
					if (!child->getAttr("type", &szTypeName))
					{
						error.Format("line #%i: <RuntimeParam> is missing the attribute 'type'", child->getLine());
						return false;
					}

					// "typeGUID" (optional because of backwards compatibility, but must represent a valid GUID if present)
					CryGUID typeGUID = CryGUID::Null();
					{
						const char* szTypeGUID;
						if (child->getAttr("typeGUID", &szTypeGUID))
						{
							if (!Shared::Internal::CGUIDHelper::TryParseFromString(typeGUID, szTypeGUID))
							{
								error.Format("line #%i: <RuntimeParam>'s attribute 'typeGUID' is not in a valid GUID format: '%s'", child->getLine(), szTypeGUID);
								return false;
							}
						}
					}

					// "addToDebugRenderWorld" attribute (optional because it got introduced after some query blueprints were already in use; default to 'false')
					bool bAddToDebugRenderWorld = false;
					child->getAttr("addToDebugRenderWorld", bAddToDebugRenderWorld);

					m_pQuery->GetGlobalRuntimeParams().AddParameter(szName, szTypeName, typeGUID, bAddToDebugRenderWorld, MakeNewSyntaxErrorCollectorUniquePtr(child->getLine(), m_pDataErrorCollector));
				}
				else
				{
					error.Format("line #%i: unknown tag: <%s>", child->getLine(), child->getTag());
					return false;
				}
			}

			return true;
		}

		bool CQueryBlueprintLoader_XML::ParseGeneratorElement(const XmlNodeRef& generatorElement, Shared::IUqsString& error)
		{
			assert(generatorElement->isTag("Generator"));

			Core::ITextualGeneratorBlueprint& textualGeneratorBP = m_pQuery->SetGenerator();
			textualGeneratorBP.SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(generatorElement->getLine(), m_pDataErrorCollector));

			// "name" attribute
			const char* szName;
			if (!generatorElement->getAttr("name", &szName))
			{
				error.Format("line #%i: <Generator> is missing the attribute 'name'", generatorElement->getLine());
				return false;
			}
			textualGeneratorBP.SetGeneratorName(szName);

			// "generatorFactoryGUID" attribute (optional because of backwards compatibility, but must represent a valid GUID if present)
			CryGUID generatorFactoryGUID = CryGUID::Null();
			const char* szGeneratorFactoryGUID;
			if (generatorElement->getAttr("generatorFactoryGUID", &szGeneratorFactoryGUID))
			{
				if (!Shared::Internal::CGUIDHelper::TryParseFromString(generatorFactoryGUID, szGeneratorFactoryGUID))
				{
					error.Format("line #%i: <Generator>'s 'generatorFactoryGUID' attribute is not in a valid GUID format: '%s'", generatorElement->getLine(), szGeneratorFactoryGUID);
					return false;
				}
			}
			textualGeneratorBP.SetGeneratorGUID(generatorFactoryGUID);

			// parse <Input> elements (expect only these elements)
			Core::ITextualInputBlueprint& textualInputRoot = textualGeneratorBP.GetInputRoot();
			textualInputRoot.SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(generatorElement->getLine(), m_pDataErrorCollector));	// same as its parent (the textual-generator-blueprint)
			for (int i = 0; i < generatorElement->getChildCount(); ++i)
			{
				XmlNodeRef child = generatorElement->getChild(i);

				if (child->isTag("Input"))
				{
					if (!ParseInputElement(child, textualInputRoot, error))
						return false;
				}
				else
				{
					error.Format("line #%i: unknown tag: <%s>", child->getLine(), child->getTag());
					return false;
				}
			}

			return true;
		}

		bool CQueryBlueprintLoader_XML::ParseInstantEvaluatorElement(const XmlNodeRef& instantEvaluatorElement, Shared::IUqsString& error)
		{
			assert(instantEvaluatorElement->isTag("InstantEvaluator"));

			Core::ITextualEvaluatorBlueprint& textualInstantEvaluatorBP = m_pQuery->AddInstantEvaluator();

			return CommonParseEvaluatorElement(instantEvaluatorElement, "instantEvaluatorFactoryGUID", textualInstantEvaluatorBP, error);
		}

		bool CQueryBlueprintLoader_XML::ParseDeferredEvaluatorElement(const XmlNodeRef& deferredEvaluatorElement, Shared::IUqsString& error)
		{
			assert(deferredEvaluatorElement->isTag("DeferredEvaluator"));

			Core::ITextualEvaluatorBlueprint& textualDeferredEvaluatorBP = m_pQuery->AddDeferredEvaluator();

			return CommonParseEvaluatorElement(deferredEvaluatorElement, "deferredEvaluatorFactoryGUID", textualDeferredEvaluatorBP, error);
		}

		bool CQueryBlueprintLoader_XML::ParseFunctionElement(const XmlNodeRef& functionElement, Core::ITextualInputBlueprint& parentInput, Shared::IUqsString& error)
		{
			assert(functionElement->isTag("Function"));

			// whether the "returnValue" attribute is present specifies whether it's a leaf- or non-leaf function

			if (functionElement->haveAttr("returnValue"))
			{
				// leaf function
				// TODO: do something with that "returnValue" attribute
			}
			else
			{
				// non-leaf function => recurse further down (expect <Input> elements)
				for (int i = 0; i < functionElement->getChildCount(); ++i)
				{
					XmlNodeRef child = functionElement->getChild(i);

					if (child->isTag("Input"))
					{
						if (!ParseInputElement(child, parentInput, error))
							return false;
					}
					else
					{
						error.Format("line #%i: unknown tag: <%s>", child->getLine(), child->getTag());
						return false;
					}
				}
			}

			return true;
		}

		bool CQueryBlueprintLoader_XML::ParseInputElement(const XmlNodeRef& inputElement, Core::ITextualInputBlueprint& parentInput, Shared::IUqsString& error)
		{
			assert(inputElement->isTag("Input"));

			// "name" attribute (name of the parameter)
			const char* szParamName;
			if (!inputElement->getAttr("name", &szParamName))
			{
				error.Format("line #%i: <Input> is missing the attribute 'name'", inputElement->getLine());
				return false;
			}

			// "paramID" attribute (unique ID of the paramter; optional for backwards compatibility)
			Client::CInputParameterID paramID = Client::CInputParameterID::CreateEmpty();
			const char* szParamID;
			if (inputElement->getAttr("paramID", &szParamID))
			{
				if (strlen(szParamID) != 4)
				{
					error.Format("line #%i: <Input>'s 'paramID' is supposed to be exactly 4 characters long (but is: %i) - did the format change? -> please investigate", inputElement->getLine(), (int)strlen(szParamID));
					return false;
				}

				char idAsFourCharacterString[5];
				cry_strcpy(idAsFourCharacterString, szParamID);
				paramID = Client::CInputParameterID::CreateFromString(idAsFourCharacterString);
			}

			// <Function> element
			XmlNodeRef functionElement = inputElement->findChild("Function");
			if (!functionElement)
			{
				error.Format("line #%i: <Input> is missing the <Function> element", inputElement->getLine());
				return false;
			}

			// <Function>'s "name" attribute (name of the function)
			const char* szFunctionName;
			if (!functionElement->getAttr("name", &szFunctionName))
			{
				error.Format("line #%i: <Function> is missing the attribute 'name'", functionElement->getLine());
				return false;
			}

			// <Function>'s "functionFactoryGUID" attribute (optional because of backwards compatibility, but must represent a valid GUID if present)
			CryGUID functionFactoryGUID = CryGUID::Null();
			const char* szFunctionFactoryGUID;
			if (functionElement->getAttr("functionFactoryGUID", &szFunctionFactoryGUID))
			{
				if (!Shared::Internal::CGUIDHelper::TryParseFromString(functionFactoryGUID, szFunctionFactoryGUID))
				{
					error.Format("line #%i: <Function>'s 'functionFactoryGUID' attribute is not in a valid GUID format: '%s'", functionElement->getLine(), szFunctionFactoryGUID);
					return false;
				}
			}

			//  "returnValue"
			const char* szFunctionReturnValue = functionElement->getAttr("returnValue");   // optional attribute; defaults to ""

			// "addReturnValueToDebugRenderWorldUponExecution"
			bool bAddReturnValueToDebugRenderWorldUponExecution = false;
			functionElement->getAttr("addReturnValueToDebugRenderWorldUponExecution", bAddReturnValueToDebugRenderWorldUponExecution);

			Core::ITextualInputBlueprint& newChild = parentInput.AddChild(szParamName, paramID, szFunctionName, functionFactoryGUID, szFunctionReturnValue, bAddReturnValueToDebugRenderWorldUponExecution);
			newChild.SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(functionElement->getLine(), m_pDataErrorCollector));

			return ParseFunctionElement(functionElement, newChild, error);
		}

		bool CQueryBlueprintLoader_XML::CommonParseEvaluatorElement(const XmlNodeRef& evaluatorElement, const char* szAttributeForEvaluatorFactoryGUID, Core::ITextualEvaluatorBlueprint& outTextualEvaluatorBP, Shared::IUqsString& error)
		{
			const char* szEvaluatorElementName = evaluatorElement->getTag(); // for error messages

			outTextualEvaluatorBP.SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(evaluatorElement->getLine(), m_pDataErrorCollector));

			// "name" attribute
			{
				const char* szEvaluatorName;
				if (!evaluatorElement->getAttr("name", &szEvaluatorName))
				{
					error.Format("line #%i: <%s> is missing the attribute 'name'", evaluatorElement->getLine(), szEvaluatorElementName);
					return false;
				}
				outTextualEvaluatorBP.SetEvaluatorName(szEvaluatorName);
			}

			// attribute containing the GUID of the evaluator-factory (optional because of backwards compatibility, but must represent a valid GUID if present)
			{
				CryGUID evaluatorFactoryGUID = CryGUID::Null();
				const char* szEvaluatorFactoryGUID;
				if (evaluatorElement->getAttr(szAttributeForEvaluatorFactoryGUID, &szEvaluatorFactoryGUID))
				{
					if (!Shared::Internal::CGUIDHelper::TryParseFromString(evaluatorFactoryGUID, szEvaluatorFactoryGUID))
					{
						error.Format("line #%i: <%s>'s '%s' attribute is not in a valid GUID format: '%s'", evaluatorElement->getLine(), szEvaluatorElementName, szAttributeForEvaluatorFactoryGUID, szEvaluatorFactoryGUID);
						return false;
					}
				}
				outTextualEvaluatorBP.SetEvaluatorGUID(evaluatorFactoryGUID);
			}

			// "weight" attribute
			{
				float weight = 1.0f;
				if (!evaluatorElement->getAttr("weight", weight))
				{
					error.Format("line #%i: <%s> is missing the attribute 'weight'", evaluatorElement->getLine(), szEvaluatorElementName);
					return false;
				}
				outTextualEvaluatorBP.SetWeight(weight);
			}

			// "scoreTransform" attribute (treat it as optional for older queries, so that the default score-transform will get picked in such a case)
			{
				const char* szScoreTransform = "";
				if (evaluatorElement->getAttr("scoreTransform", &szScoreTransform))
				{
					outTextualEvaluatorBP.SetScoreTransformName(szScoreTransform);
				}
			}

			// "scoreTransformFactoryGUID" attribute (optional because of backwards compatibility, but must represent a valid GUID if present)
			{
				CryGUID scoreTransformFactoryGUID = CryGUID::Null();
				const char* szScoreTransformGUID;
				if (evaluatorElement->getAttr("scoreTransformFactoryGUID", &szScoreTransformGUID))
				{
					if (!Shared::Internal::CGUIDHelper::TryParseFromString(scoreTransformFactoryGUID, szScoreTransformGUID))
					{
						error.Format("line #%i: <%s>'s 'scoreTransformFactoryGUID' attribute is not in a valid GUID format: '%s'", evaluatorElement->getLine(), szEvaluatorElementName, szScoreTransformGUID);
						return false;
					}
				}
				outTextualEvaluatorBP.SetScoreTransformGUID(scoreTransformFactoryGUID);
			}

			// "negateDiscard" attribute
			{
				bool bNegateDiscard = false;
				evaluatorElement->getAttr("negateDiscard", bNegateDiscard);
				outTextualEvaluatorBP.SetNegateDiscard(bNegateDiscard);
			}

			// parse <Input> elements (expect only these elements)
			{
				Core::ITextualInputBlueprint& textualInputRoot = outTextualEvaluatorBP.GetInputRoot();
				textualInputRoot.SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(evaluatorElement->getLine(), m_pDataErrorCollector));	// same as its parent (the evaluator-blueprint)
				for (int i = 0; i < evaluatorElement->getChildCount(); ++i)
				{
					XmlNodeRef child = evaluatorElement->getChild(i);

					if (child->isTag("Input"))
					{
						if (!ParseInputElement(child, textualInputRoot, error))
							return false;
					}
					else
					{
						error.Format("line #%i: unknown tag: <%s>", child->getLine(), child->getTag());
						return false;
					}
				}
			}

			return true;
		}

	}
}
