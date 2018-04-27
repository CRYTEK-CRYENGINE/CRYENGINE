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
			// CFunc_Literal<>
			//
			//===================================================================================

			template <class TLiteral>
			class CFunc_Literal final : public CFunctionBase<CFunc_Literal<TLiteral>, TLiteral, IFunctionFactory::ELeafFunctionKind::Literal>
			{
			private:
				// these 4 typedefs exist only to make gcc happy
				typedef CFunctionBase<CFunc_Literal<TLiteral>, TLiteral, IFunctionFactory::ELeafFunctionKind::Literal> BaseClass;
				typedef typename BaseClass::SCtorContext SCtorContext;
				typedef typename BaseClass::SExecuteContext SExecuteContext;
				typedef typename BaseClass::SValidationContext SValidationContext;

			public:
				explicit                 CFunc_Literal(const SCtorContext& ctorContext);
				TLiteral                 DoExecute(const SExecuteContext& executeContext) const;

			private:
				virtual bool             ValidateDynamic(const SValidationContext& validationContext) const override;

			private:
				TLiteral                 m_literal;
			};

			template <class TLiteral>
			CFunc_Literal<TLiteral>::CFunc_Literal(const SCtorContext& ctorContext)
				: BaseClass(ctorContext)
			{
				assert(ctorContext.pOptionalReturnValueInCaseOfLeafFunction);

				const Core::ILeafFunctionReturnValue::SLiteralInfo literalInfo = ctorContext.pOptionalReturnValueInCaseOfLeafFunction->GetLiteral(ctorContext.blackboard);

				// if this fails then something might have gone wrong in CInputBlueprint::Resolve()
				assert(literalInfo.type == Shared::SDataTypeHelper<TLiteral>::GetTypeInfo());

				m_literal = *static_cast<const TLiteral*>(literalInfo.pValue);
			}

			template <class TLiteral>
			bool CFunc_Literal<TLiteral>::ValidateDynamic(const SValidationContext& validationContext) const
			{
				return true;
			}

			template <class TLiteral>
			TLiteral CFunc_Literal<TLiteral>::DoExecute(const SExecuteContext& executeContext) const
			{
				return m_literal;
			}

		}
	}
}
