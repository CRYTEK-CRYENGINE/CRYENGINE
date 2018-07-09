// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{
		namespace Internal
		{

			//===================================================================================
			//
			// CFunc_IteratedItem<>
			//
			//===================================================================================

			template <class TItem>
			class CFunc_IteratedItem final : public CFunctionBase<CFunc_IteratedItem<TItem>, TItem, IFunctionFactory::ELeafFunctionKind::IteratedItem>
			{
			private:
				// these 4 typedefs exist only to make gcc happy
				typedef CFunctionBase<CFunc_IteratedItem<TItem>, TItem, IFunctionFactory::ELeafFunctionKind::IteratedItem> BaseClass;
				typedef typename BaseClass::SCtorContext SCtorContext;
				typedef typename BaseClass::SExecuteContext SExecuteContext;
				typedef typename BaseClass::SValidationContext SValidationContext;

			public:
				explicit                                    CFunc_IteratedItem(const SCtorContext& ctorContext);
				TItem                                       DoExecute(const SExecuteContext& executeContext) const;

			private:
				virtual bool                                ValidateDynamic(const SValidationContext& validationContext) const override;

			private:
				const Core::IItemList*                      m_pGeneratedItems;   // FIXME: weak pointer?
			};

			template <class TItem>
			CFunc_IteratedItem<TItem>::CFunc_IteratedItem(const SCtorContext& ctorContext)
				: BaseClass(ctorContext)
				, m_pGeneratedItems(nullptr)
			{
				assert(ctorContext.pOptionalReturnValueInCaseOfLeafFunction);

				const Core::ILeafFunctionReturnValue::SItemIterationInfo itemIterationInfo = ctorContext.pOptionalReturnValueInCaseOfLeafFunction->GetItemIteration(ctorContext.blackboard);

				m_pGeneratedItems = itemIterationInfo.pGeneratedItems;
			}

			template <class TItem>
			bool CFunc_IteratedItem<TItem>::ValidateDynamic(const SValidationContext& validationContext) const
			{
				if (m_pGeneratedItems)
				{
					if (m_pGeneratedItems->GetItemFactory().GetItemType() == Shared::SDataTypeHelper<TItem>::GetTypeInfo())
					{
						return true;
					}
					else
					{
						validationContext.error.Format("%s: item type mismatch: this function expects items of type '%s', but the underlying item list contains items of type '%s'",
							validationContext.szNameOfFunctionBeingValidated, Shared::SDataTypeHelper<TItem>::GetTypeInfo().name(), m_pGeneratedItems->GetItemFactory().GetItemType().name());
						return false;
					}
				}
				else
				{
					validationContext.error.Format("%s: we're currently not iterating on items (we're most likely still in the Generator phase and this function should only be used in the Evaluator phase)",
						validationContext.szNameOfFunctionBeingValidated);
					return false;
				}
			}

			template <class TItem>
			TItem CFunc_IteratedItem<TItem>::DoExecute(const SExecuteContext& executeContext) const
			{
				assert(m_pGeneratedItems);
				assert(m_pGeneratedItems->GetItemFactory().GetItemType() == Shared::SDataTypeHelper<TItem>::GetTypeInfo());
				const Client::CItemListProxy_Readable<TItem> actualItems(*m_pGeneratedItems);
				return actualItems.GetItemAtIndex(executeContext.currentItemIndex);
			}

		}
	}
}
