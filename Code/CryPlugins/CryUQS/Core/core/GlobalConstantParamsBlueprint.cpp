// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GlobalConstantParamsBlueprint.h"

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

		CTextualGlobalConstantParamsBlueprint::CTextualGlobalConstantParamsBlueprint()
		{
			// nothing
		}

		void CTextualGlobalConstantParamsBlueprint::AddParameter(const char* szName, const char* szTypeName, const CryGUID& typeGUID, const char* szValue, bool bAddToDebugRenderWorld, DataSource::SyntaxErrorCollectorUniquePtr pSyntaxErrorCollector)
		{
			m_parameters.emplace_back(szName, szTypeName, typeGUID, szValue, bAddToDebugRenderWorld, std::move(pSyntaxErrorCollector));
		}

		size_t CTextualGlobalConstantParamsBlueprint::GetParameterCount() const
		{
			return m_parameters.size();
		}

		ITextualGlobalConstantParamsBlueprint::SParameterInfo CTextualGlobalConstantParamsBlueprint::GetParameter(size_t index) const
		{
			assert(index < m_parameters.size());
			const SStoredParameterInfo& pi = m_parameters[index];
			return SParameterInfo(pi.name.c_str(), pi.typeName.c_str(), pi.typeGUID, pi.value.c_str(), pi.bAddToDebugRenderWorld, pi.pSyntaxErrorCollector.get());
		}

		//===================================================================================
		//
		// CGlobalConstantParamsBlueprint
		//
		//===================================================================================

		CGlobalConstantParamsBlueprint::CGlobalConstantParamsBlueprint()
		{
			// nothing
		}

		CGlobalConstantParamsBlueprint::~CGlobalConstantParamsBlueprint()
		{
			// destroy all items (they have been created by Resolve())
			for (auto& pair : m_constantParams)
			{
				pair.second.pItemFactory->DestroyItems(pair.second.pItem);
			}
		}

		bool CGlobalConstantParamsBlueprint::Resolve(const ITextualGlobalConstantParamsBlueprint& source)
		{
			const size_t numParams = source.GetParameterCount();

			for (size_t i = 0; i < numParams; ++i)
			{
				const ITextualGlobalConstantParamsBlueprint::SParameterInfo p = source.GetParameter(i);

				// ensure each parameter exists only once
				if (m_constantParams.find(p.szName) != m_constantParams.cend())
				{
					if (DataSource::ISyntaxErrorCollector* pSE = p.pSyntaxErrorCollector)
					{
						pSE->AddErrorMessage("Duplicate parameter: '%s'", p.szName);
					}
					return false;
				}

				// find the item factory: first by GUID, then by name
				Client::IItemFactory* pItemFactory;
				if (!(pItemFactory = g_pHub->GetItemFactoryDatabase().FindFactoryByGUID(p.typeGUID)))
				{
					if (!(pItemFactory = g_pHub->GetItemFactoryDatabase().FindFactoryByName(p.szTypeName)))
					{
						if (DataSource::ISyntaxErrorCollector* pSE = p.pSyntaxErrorCollector)
						{
							Shared::CUqsString typeGuidAsString;
							Shared::Internal::CGUIDHelper::ToString(p.typeGUID, typeGuidAsString);
							pSE->AddErrorMessage("Unknown item type: GUID = %s, name = '%s'", typeGuidAsString.c_str(), p.szTypeName);
						}
						return false;
					}
				}

				if (!pItemFactory->CanBePersistantlySerialized())
				{
					if (DataSource::ISyntaxErrorCollector* pSE = p.pSyntaxErrorCollector)
					{
						pSE->AddErrorMessage("Items of type '%s' cannot be represented in textual form", p.szTypeName);
					}
					return false;
				}

				IItemSerializationSupport& itemSerializationSupport = g_pHub->GetItemSerializationSupport();
				Shared::CUqsString errorMessage;
				Shared::IUqsString* pErrorMessage = (p.pSyntaxErrorCollector) ? &errorMessage : nullptr;
				void* pItem = pItemFactory->CreateItems(1, Client::IItemFactory::EItemInitMode::UseDefaultConstructor);

				if (!itemSerializationSupport.DeserializeItemFromCStringLiteral(pItem, *pItemFactory, p.szValue, pErrorMessage))
				{
					if (DataSource::ISyntaxErrorCollector* pSE = p.pSyntaxErrorCollector)
					{
						pSE->AddErrorMessage("Parameter '%s' could not be parsed from its serialized representation ('%s'). Reason: %s", p.szName, p.szValue, errorMessage.c_str());
					}
					pItemFactory->DestroyItems(pItem);
					return false;
				}

				m_constantParams.insert(std::map<string, SParamInfo>::value_type(p.szName, SParamInfo(pItemFactory, pItem, p.bAddToDebugRenderWorld)));  // notice: the item will get destroyed by CGlobalConstantParamsBlueprint's dtor
			}

			return true;
		}

		const std::map<string, CGlobalConstantParamsBlueprint::SParamInfo>& CGlobalConstantParamsBlueprint::GetParams() const
		{
			return m_constantParams;
		}

		void CGlobalConstantParamsBlueprint::AddSelfToDictAndReplace(Shared::CVariantDict& out) const
		{
			for (const auto& pair : m_constantParams)
			{
				out.AddOrReplace(pair.first.c_str(), *pair.second.pItemFactory, pair.second.pItem);
			}
		}

		void CGlobalConstantParamsBlueprint::PrintToConsole(CLogger& logger) const
		{
			if (m_constantParams.empty())
			{
				logger.Printf("Global constant params: (none)");
			}
			else
			{
				logger.Printf("Global constant params:");
				CLoggerIndentation _indent;
				CItemSerializationSupport& itemSerializationSupport = g_pHub->GetItemSerializationSupport();
				for (const auto& e : m_constantParams)
				{
					const char* szParamName = e.first.c_str();
					const Client::IItemFactory* pItemFactory = e.second.pItemFactory;
					Shared::CUqsString itemAsString;
					assert(pItemFactory->CanBePersistantlySerialized()); // constant params can *always* be represented in textual form (how else should the query author provide them via e. g. an XML file?)
					itemSerializationSupport.SerializeItemToStringLiteral(e.second.pItem, *pItemFactory, itemAsString);
					logger.Printf("\"%s\" = %s [%s]", szParamName, itemAsString.c_str(), pItemFactory->GetName());
				}
			}
		}

	}
}
