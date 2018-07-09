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
			// CFunc_GlobalParam<>
			//
			//===================================================================================

			template <class TGlobalParam>
			class CFunc_GlobalParam final : public CFunctionBase<CFunc_GlobalParam<TGlobalParam>, TGlobalParam, IFunctionFactory::ELeafFunctionKind::GlobalParam>
			{
			private:
				// these 4 typedefs exist only to make gcc happy
				typedef CFunctionBase<CFunc_GlobalParam<TGlobalParam>, TGlobalParam, IFunctionFactory::ELeafFunctionKind::GlobalParam> BaseClass;
				typedef typename BaseClass::SCtorContext SCtorContext;
				typedef typename BaseClass::SExecuteContext SExecuteContext;
				typedef typename BaseClass::SValidationContext SValidationContext;

			public:
				explicit                      CFunc_GlobalParam(const SCtorContext& ctorContext);
				TGlobalParam                  DoExecute(const SExecuteContext& executeContext) const;

			private:
				virtual bool                  ValidateDynamic(const SValidationContext& validationContext) const override;

			private:
				string                        m_nameOfGlobalParam;               // for validation error message
				bool                          m_bGlobalParamExists;
				const Shared::CTypeInfo*      m_pTypeOfGlobalParam;
				const void*                   m_pValueOfGlobalParam;             // ultimately points into CQueryBase::m_globalParams (if the global param exists)
			};

			template <class TGlobalParam>
			CFunc_GlobalParam<TGlobalParam>::CFunc_GlobalParam(const SCtorContext& ctorContext)
				: BaseClass(ctorContext)
				, m_bGlobalParamExists(false)
				, m_pTypeOfGlobalParam(nullptr)
				, m_pValueOfGlobalParam(nullptr)
			{
				assert(ctorContext.pOptionalReturnValueInCaseOfLeafFunction);

				const Core::ILeafFunctionReturnValue::SGlobalParamInfo globalParamInfo = ctorContext.pOptionalReturnValueInCaseOfLeafFunction->GetGlobalParam(ctorContext.blackboard);

				m_nameOfGlobalParam = globalParamInfo.szNameOfGlobalParam;

				if (globalParamInfo.bGlobalParamExists)
				{
					m_pTypeOfGlobalParam = globalParamInfo.pTypeOfGlobalParam;
					m_pValueOfGlobalParam = globalParamInfo.pValueOfGlobalParam;
					m_bGlobalParamExists = true;
				}
			}

			template <class TGlobalParam>
			bool CFunc_GlobalParam<TGlobalParam>::ValidateDynamic(const SValidationContext& validationContext) const
			{
				if (m_bGlobalParamExists)
				{
					const Shared::CTypeInfo& expectedType = Shared::SDataTypeHelper<TGlobalParam>::GetTypeInfo();

					if (expectedType == *m_pTypeOfGlobalParam)
					{
						return true;
					}
					else
					{
						validationContext.error.Format("%s: global param '%s' exists, but mismatches the type (expected a '%s', but actually stored is a '%s')",
							validationContext.szNameOfFunctionBeingValidated, m_nameOfGlobalParam.c_str(), expectedType.name(), m_pTypeOfGlobalParam->name());
						return false;
					}
				}
				else
				{
					validationContext.error.Format("%s: global param '%s' does not exist", validationContext.szNameOfFunctionBeingValidated, m_nameOfGlobalParam.c_str());
					return false;
				}
			}

			template <class TGlobalParam>
			TGlobalParam CFunc_GlobalParam<TGlobalParam>::DoExecute(const SExecuteContext& executeContext) const
			{
				assert(m_bGlobalParamExists);
				assert(*m_pTypeOfGlobalParam == Shared::SDataTypeHelper<TGlobalParam>::GetTypeInfo());	// cannot fail if the validation succeeded (presuming the caller did not cheat)
				return *static_cast<const TGlobalParam*>(m_pValueOfGlobalParam);
			}

		}
	}
}
