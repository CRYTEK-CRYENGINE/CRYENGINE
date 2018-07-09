// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		CLeafFunctionReturnValue::CLeafFunctionReturnValue()
			: m_leafFunctionKind(Client::IFunctionFactory::ELeafFunctionKind::None)
		{
			memset(&m_returnValue, 0, sizeof m_returnValue);  // just a debugging aid
		}

		CLeafFunctionReturnValue::CLeafFunctionReturnValue(const CLeafFunctionReturnValue& rhs)
			: m_leafFunctionKind(Client::IFunctionFactory::ELeafFunctionKind::None)
		{
			memset(&m_returnValue, 0, sizeof m_returnValue);  // just a debugging aid
			CopyOtherToSelf(rhs);
		}

		CLeafFunctionReturnValue::~CLeafFunctionReturnValue()
		{
			Clear();
		}

		CLeafFunctionReturnValue& CLeafFunctionReturnValue::operator=(const CLeafFunctionReturnValue& rhs)
		{
			if (&rhs != this)
			{
				Clear();
				CopyOtherToSelf(rhs);
			}
			return *this;
		}

		void CLeafFunctionReturnValue::SetLiteral(Client::IItemFactory& itemFactory, const void* pItemToClone)
		{
			assert(m_leafFunctionKind == Client::IFunctionFactory::ELeafFunctionKind::None);

			m_returnValue.literal.pItemFactory = &itemFactory;
			m_returnValue.literal.pValue = itemFactory.CloneItem(pItemToClone);

			m_leafFunctionKind = Client::IFunctionFactory::ELeafFunctionKind::Literal;
		}

		void CLeafFunctionReturnValue::SetGlobalParam(const char* szNameOfGlobalParam)
		{
			assert(m_leafFunctionKind == Client::IFunctionFactory::ELeafFunctionKind::None);

			m_returnValue.globalParam.pNameOfGlobalParam = new Shared::CUqsString(szNameOfGlobalParam);

			m_leafFunctionKind = Client::IFunctionFactory::ELeafFunctionKind::GlobalParam;
		}

		void CLeafFunctionReturnValue::SetItemIteration()
		{
			assert(m_leafFunctionKind == Client::IFunctionFactory::ELeafFunctionKind::None);
			m_leafFunctionKind = Client::IFunctionFactory::ELeafFunctionKind::IteratedItem;
		}

		void CLeafFunctionReturnValue::SetShuttledItems()
		{
			assert(m_leafFunctionKind == Client::IFunctionFactory::ELeafFunctionKind::None);
			m_leafFunctionKind = Client::IFunctionFactory::ELeafFunctionKind::ShuttledItems;
		}

		bool CLeafFunctionReturnValue::IsActuallyALeafFunction() const
		{
			return (m_leafFunctionKind != Client::IFunctionFactory::ELeafFunctionKind::None);
		}

		CLeafFunctionReturnValue::SLiteralInfo CLeafFunctionReturnValue::GetLiteral(const SQueryBlackboard& blackboard) const
		{
			assert(m_leafFunctionKind == Client::IFunctionFactory::ELeafFunctionKind::Literal);

			const Shared::CTypeInfo& type = m_returnValue.literal.pItemFactory->GetItemType();
			const void* pValue = m_returnValue.literal.pValue;

			return SLiteralInfo(type, pValue);
		}

		CLeafFunctionReturnValue::SGlobalParamInfo CLeafFunctionReturnValue::GetGlobalParam(const SQueryBlackboard& blackboard) const
		{
			assert(m_leafFunctionKind == Client::IFunctionFactory::ELeafFunctionKind::GlobalParam);

			Client::IItemFactory* pItemFactory = nullptr;
			/*const*/ void* pValue = nullptr;
			const char* szNameOfGlobalParam = m_returnValue.globalParam.pNameOfGlobalParam->c_str();
			const bool bGlobalParamExists = blackboard.globalParams.FindItemFactoryAndObject(szNameOfGlobalParam, pItemFactory, pValue);
			const Shared::CTypeInfo* pType = bGlobalParamExists ? &pItemFactory->GetItemType() : nullptr;

			return SGlobalParamInfo(bGlobalParamExists, pType, pValue, szNameOfGlobalParam);
		}

		CLeafFunctionReturnValue::SItemIterationInfo CLeafFunctionReturnValue::GetItemIteration(const SQueryBlackboard& blackboard) const
		{
			assert(m_leafFunctionKind == Client::IFunctionFactory::ELeafFunctionKind::IteratedItem);

			const IItemList* pGeneratedItems = blackboard.pItemIterationContext ? &blackboard.pItemIterationContext->generatedItems : nullptr;

			return SItemIterationInfo(pGeneratedItems);
		}

		CLeafFunctionReturnValue::SShuttledItemsInfo CLeafFunctionReturnValue::GetShuttledItems(const SQueryBlackboard& blackboard) const
		{
			assert(m_leafFunctionKind == Client::IFunctionFactory::ELeafFunctionKind::ShuttledItems);

			return SShuttledItemsInfo(blackboard.pShuttledItems);  // might be a nullptr, which is OK here (it means that there was no previous query in the chain that put its result-set on the blackboard)
		}

		void CLeafFunctionReturnValue::Clear()
		{
			switch (m_leafFunctionKind)
			{
			case Client::IFunctionFactory::ELeafFunctionKind::None:
				// nothing
				break;

			case Client::IFunctionFactory::ELeafFunctionKind::Literal:
				m_returnValue.literal.pItemFactory->DestroyItems(m_returnValue.literal.pValue);
				break;

			case Client::IFunctionFactory::ELeafFunctionKind::GlobalParam:
				delete m_returnValue.globalParam.pNameOfGlobalParam;
				break;

			case Client::IFunctionFactory::ELeafFunctionKind::IteratedItem:
				// nothing
				break;

			case Client::IFunctionFactory::ELeafFunctionKind::ShuttledItems:
				// nothing
				break;

			default:
				assert(0);
			}

			memset(&m_returnValue, 0, sizeof m_returnValue);  // not necessary, but may help pinpoint bugs easier

			m_leafFunctionKind = Client::IFunctionFactory::ELeafFunctionKind::None;
		}

		void CLeafFunctionReturnValue::CopyOtherToSelf(const CLeafFunctionReturnValue& other)
		{
			assert(&other != this);
			assert(m_leafFunctionKind == Client::IFunctionFactory::ELeafFunctionKind::None); // should always Clear() beforehand

			switch (other.m_leafFunctionKind)
			{
			case Client::IFunctionFactory::ELeafFunctionKind::None:
				// nothing
				break;

			case Client::IFunctionFactory::ELeafFunctionKind::Literal:
				m_returnValue.literal.pItemFactory = other.m_returnValue.literal.pItemFactory;
				m_returnValue.literal.pValue = other.m_returnValue.literal.pItemFactory->CloneItem(other.m_returnValue.literal.pValue);
				break;

			case Client::IFunctionFactory::ELeafFunctionKind::GlobalParam:
				m_returnValue.globalParam.pNameOfGlobalParam = new Shared::CUqsString(other.m_returnValue.globalParam.pNameOfGlobalParam->c_str());
				break;

			case Client::IFunctionFactory::ELeafFunctionKind::IteratedItem:
				// nothing
				break;

			case Client::IFunctionFactory::ELeafFunctionKind::ShuttledItems:
				// nothing
				break;

			default:
				assert(0);
			}

			m_leafFunctionKind = other.m_leafFunctionKind;
		}

	}
}
