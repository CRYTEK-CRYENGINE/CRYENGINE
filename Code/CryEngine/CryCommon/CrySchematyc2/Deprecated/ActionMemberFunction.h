// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Investigate - Is there a dependency between the order of params and the order in which they are bound to inputs/outputs? If so can we just sort inputs and outputs?
// #SchematycTODO : Investigate - Can we bind a reference/pointer to an input and an output?
// #SchematycTODO : Compile time error messages for unsupported types such as pointers?
// #SchematycTODO : Use preprocessor macros or variadic templates to reduce duplication of code.
// #SchematycTODO : Replace new with make_shared. Currently CBase takes 6 parameters but make_shared only supports 5.
// #SchematycTODO : Flag const member functions?

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CrySerialization/MathImpl.h>
#include <CrySerialization/STL.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_PreprocessorUtils.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_TypeUtils.h>

#include "CrySchematyc2/Any.h"
#include "CrySchematyc2/Deprecated/Variant.h"
#include "CrySchematyc2/Env/IEnvRegistry.h"
#include "CrySchematyc2/Services/ILog.h"
#include "CrySchematyc2/Utils/StringUtils.h"

#define SCHEMATYC2_MAKE_ACTION_MEMBER_FUNCTION_SHARED(function, guid, actionGUID) Schematyc2::SMakeActionMemberFunctionShared<TYPE_OF(&function)>()(&function, guid, actionGUID, TemplateUtils::GetFunctionName<TYPE_OF(&function), &function>(), __FILE__, "Code")

namespace Schematyc2
{
	typedef std::vector<uint32> UInt32Vector;

	namespace ActionMemberFunctionUtils
	{
		template <typename TYPE> struct GetStorageType
		{
			typedef typename TemplateUtils::GetUnqualifiedType<TYPE>::Result Result;
		};

		template <> struct GetStorageType<const char*>
		{
			typedef const char* Result;
		};

		template <typename TYPE> struct GetDefaultValue
		{
			inline const TYPE& operator () () const
			{
				static const TYPE defaultValue = TYPE();
				return defaultValue;
			}
		};

		template <typename TYPE, uint32 SIZE> struct GetDefaultValue<TYPE[SIZE]>
		{
			typedef TYPE Array[SIZE];

			inline const Array& operator () () const
			{
				static const Array defaultValue = { TYPE() };
				return defaultValue;
			}
		};

		template <typename TO, typename FROM> struct ParamConversion
		{
			inline TO operator () (FROM& rhs)
			{
				return rhs;
			}
		};

		template <typename FROM> struct ParamConversion<FROM*, FROM>
		{
			inline FROM* operator () (FROM& rhs)
			{
				return &rhs;
			}
		};

		template <typename FROM> struct ParamConversion<const FROM*, FROM>
		{
			inline const FROM* operator () (FROM& rhs)
			{
				return &rhs;
			}
		};

		enum class EParamFlags
		{
			None          = 0,
			IsReturn      = BIT(0),
			IsReference   = BIT(1),
			IsPointer     = BIT(2),
			IsConstTarget = BIT(3),
			IsConst       = BIT(4),
			IsInput       = BIT(5),
			IsOutput      = BIT(6)
		};

		DECLARE_ENUM_CLASS_FLAGS(EParamFlags)

		struct SParam
		{
			inline SParam(const IAnyPtr& _pValue, EParamFlags _flags)
				: pValue(_pValue)
				, flags(_flags)
			{}

			IAnyPtr     pValue;
			string      name;
			string      description;
			EParamFlags flags;
		};

		template <typename TYPE> inline EParamFlags GetParamFlags()
		{
			EParamFlags flags = EParamFlags::None;
			if(TemplateUtils::ReferenceTraits<TYPE>::IsReference)
			{
				flags |= EParamFlags::IsReference;
				if(TemplateUtils::ConstTraits<typename TemplateUtils::ReferenceTraits<TYPE>::RemoveReference>::IsConst)
				{
					flags |= EParamFlags::IsConstTarget;
				}
			}
			if(TemplateUtils::PointerTraits<TYPE>::IsPointer)
			{
				flags |= EParamFlags::IsPointer;
				if(TemplateUtils::ConstTraits<typename TemplateUtils::PointerTraits<TYPE>::RemovePointer>::IsConst)
				{
					flags |= EParamFlags::IsConstTarget;
				}
			}
			if(TemplateUtils::ConstTraits<TYPE>::IsConst)
			{
				flags |= EParamFlags::IsConst;
			}
			return flags;
		}

		template <typename TYPE> inline SParam MakeParam(EParamFlags flags)
		{
			typedef typename GetStorageType<TYPE>::Result StorageType;
			return SParam(MakeAnyShared<StorageType>(GetDefaultValue<StorageType>()()), flags | GetParamFlags<TYPE>());
		}

		template<> inline SParam MakeParam<const char*>(EParamFlags flags)
		{
			return SParam(MakeAnyShared(CPoolString()), flags | GetParamFlags<const char*>());
		}

		class CBase : public IActionMemberFunction
		{
		public:

			inline CBase(const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
				: m_guid(guid)
				, m_actionGUID(actionGUID)
			{
				StringUtils::SeparateTypeNameAndNamespace(szDeclaration, m_name, m_namespace);
				SetFileName(szFileName, szProjectDir);
			}

			// IActionMemberFunction

			virtual SGUID GetGUID() const override
			{
				return m_guid;
			}

			virtual SGUID GetActionGUID() const override
			{
				return m_actionGUID;
			}

			virtual void SetName(const char* szName) override
			{
				m_name = szName;
			}

			virtual const char* GetName() const override
			{
				return m_name.c_str();
			}

			virtual void SetNamespace(const char* szNamespace) override
			{
				m_namespace = szNamespace;
			}

			virtual const char* GetNamespace() const override
			{
				return m_namespace.c_str();
			}

			virtual void SetFileName(const char* szFileName, const char* szProjectDir) override
			{
				StringUtils::MakeProjectRelativeFileName(szFileName, szProjectDir, m_fileName);
			}

			virtual const char* GetFileName() const override
			{
				return m_fileName.c_str();
			}

			virtual void SetAuthor(const char* szAuthor) override
			{
				m_author = szAuthor;
			}

			virtual const char* GetAuthor() const override
			{
				return m_author.c_str();
			}

			virtual void SetDescription(const char* szDescription) override
			{
				m_description = szDescription;
			}

			virtual const char* GetDescription() const override
			{
				return m_description.c_str();
			}

			virtual void SetWikiLink(const char* szWikiLink) override
			{
				m_wikiLink = szWikiLink;
			}

			virtual const char* GetWikiLink() const override
			{
				return m_wikiLink.c_str();
			}

			virtual bool BindInput(uint32 paramIdx, const char* szName, const char* szDescription) override
			{
				SCHEMATYC2_SYSTEM_ASSERT_FATAL(szName && szDescription);
				if(CanBindInput(paramIdx))
				{
					SParam& param = m_params[paramIdx];
					param.name        = szName;
					param.description = szDescription;
					param.flags |= EParamFlags::IsInput;
					m_inputs.reserve(10);
					m_inputs.push_back(paramIdx);

					CVariantVectorOutputArchive	archive(m_variantInputs);
					archive(*param.pValue, "", "");
					return true;
				}
				return false;
			}

			virtual uint32 GetInputCount() const override
			{
				return m_inputs.size();
			}

			virtual IAnyConstPtr GetInputValue(uint32 inputIdx) const override
			{
				return inputIdx < m_inputs.size() ? m_params[m_inputs[inputIdx]].pValue : IAnyConstPtr();
			}

			virtual const char* GetInputName(uint32 inputIdx) const override
			{
				return inputIdx < m_inputs.size() ? m_params[m_inputs[inputIdx]].name.c_str() : "";
			}

			virtual const char* GetInputDescription(uint32 inputIdx) const override
			{
				return inputIdx < m_inputs.size() ? m_params[m_inputs[inputIdx]].description.c_str() : "";
			}

			virtual TVariantConstArray GetVariantInputs() const override
			{
				return m_variantInputs;
			}

			virtual bool BindOutput(uint32 paramIdx, const char* szName, const char* szDescription) override
			{
				SCHEMATYC2_SYSTEM_ASSERT_FATAL(szName && szDescription);
				if(CanBindOutput(paramIdx))
				{
					SParam& param = m_params[paramIdx];
					param.name        = szName;
					param.description = szDescription;
					param.flags |= EParamFlags::IsOutput;
					m_outputs.reserve(10);
					m_outputs.push_back(paramIdx);

					CVariantVectorOutputArchive	archive(m_variantOutputs);
					archive(*param.pValue, "", "");
					return true;
				}
				return false;
			}

			virtual uint32 GetOutputCount() const override
			{
				return m_outputs.size();
			}

			virtual IAnyConstPtr GetOutputValue(uint32 outputIdx) const override
			{
				return outputIdx < m_outputs.size() ? m_params[m_outputs[outputIdx]].pValue : IAnyConstPtr();
			}

			virtual const char* GetOutputName(uint32 outputIdx) const override
			{
				return outputIdx < m_outputs.size() ? m_params[m_outputs[outputIdx]].name.c_str() : "";
			}

			virtual const char* GetOutputDescription(uint32 outputIdx) const override
			{
				return outputIdx < m_outputs.size() ? m_params[m_outputs[outputIdx]].description.c_str() : "";
			}

			virtual TVariantConstArray GetVariantOutputs() const override
			{
				return m_variantOutputs;
			}

			// ~IActionMemberFunction

		protected:

			// IActionMemberFunction

			virtual bool BindInput_Protected(uint32 paramIdx, const char* szName, const char* szDescription, const IAny& value) override
			{
				if(CanBindInput(paramIdx))
				{
					SParam& param = m_params[paramIdx];
					SCHEMATYC2_SYSTEM_ASSERT_FATAL(value.GetTypeInfo().GetTypeId() == param.pValue->GetTypeInfo().GetTypeId());
					if(value.GetTypeInfo().GetTypeId() == param.pValue->GetTypeInfo().GetTypeId())
					{
						param.name        = szName;
						param.description = szDescription;
						param.pValue      = value.Clone();
						param.flags |= EParamFlags::IsInput;
						m_inputs.reserve(10);
						m_inputs.push_back(paramIdx);

						CVariantVectorOutputArchive	archive(m_variantInputs);
						archive(value, "", "");
						return true;
					}
				}
				return false;
			}

			// ~IActionMemberFunction

			template <typename TYPE> inline void AddParam(EParamFlags flags)
			{
				m_params.reserve(10);
				m_params.push_back(MakeParam<TYPE>(flags));
			}

			template <typename TYPE> inline void LoadParam(CVariantArrayInputArchive& archive, uint32 paramIdx, TYPE& value) const
			{
				if((m_params[paramIdx].flags & EParamFlags::IsInput) != 0)
				{
					archive(value, "", "");
				}
			}

			inline void LoadParam(CVariantArrayInputArchive& archive, uint32 paramIdx, const char*& value) const
			{
				if((m_params[paramIdx].flags & EParamFlags::IsInput) != 0)
				{
					archive(value);
				}
			}

			template <typename TO, typename FROM> inline TO ConvertParam(FROM& rhs) const
			{
				return ParamConversion<TO, FROM>()(rhs);
			}

			template <typename TYPE> inline void StoreParam(CVariantArrayOutputArchive& archive, uint32 paramIdx, TYPE& value) const
			{
				if((m_params[paramIdx].flags & EParamFlags::IsOutput) != 0)
				{
					archive(value, "", "");
				}
			}

			template <typename TYPE> inline void StoreParam(CVariantArrayOutputArchive& archive, uint32 paramIdx, const TYPE& value) const
			{
				if((m_params[paramIdx].flags & EParamFlags::IsOutput) != 0)
				{
					archive(const_cast<TYPE&>(value), "", "");
				}
			}

			template <typename TYPE> inline void StoreParam(CVariantArrayOutputArchive& archive, uint32 paramIdx, TYPE* value) const {}

			template <typename TYPE> inline void StoreParam(CVariantArrayOutputArchive& archive, uint32 paramIdx, const TYPE* value) const {}

			void StoreParam(CVariantArrayOutputArchive& archive, uint32 paramIdx, const char*& value) const
			{
				if((m_params[paramIdx].flags & EParamFlags::IsOutput) != 0)
				{
					archive(value);
				}
			}

		private:

			typedef std::vector<SParam> ParamVector;

			inline bool CanBindInput(uint32 paramIdx) const
			{
				SCHEMATYC2_SYSTEM_ASSERT_FATAL(paramIdx < m_params.size());
				if(paramIdx < m_params.size())
				{
					const bool bIsReturn = (m_params[paramIdx].flags & EParamFlags::IsReturn) != 0;
					const bool bIsBound = (m_params[paramIdx].flags & (EParamFlags::IsInput | EParamFlags::IsOutput)) != 0;
					SCHEMATYC2_SYSTEM_ASSERT_FATAL(!bIsReturn && !bIsBound);
					if(!bIsReturn && !bIsBound)
					{
						return true;
					}
				}
				return false;
			}

			inline bool CanBindOutput(uint32 paramIdx) const
			{
				SCHEMATYC2_SYSTEM_ASSERT_FATAL(paramIdx < m_params.size());
				if(paramIdx < m_params.size())
				{
					const bool bIsReturn = (m_params[paramIdx].flags & EParamFlags::IsReturn) != 0;
					const bool bIsReferenceOrPointer = (m_params[paramIdx].flags & (EParamFlags::IsReference | EParamFlags::IsPointer)) != 0;
					const bool bIsConstTarget = (m_params[paramIdx].flags & EParamFlags::IsConstTarget) != 0;
					const bool bIsBound = (m_params[paramIdx].flags & (EParamFlags::IsInput | EParamFlags::IsOutput)) != 0;
					SCHEMATYC2_SYSTEM_ASSERT_FATAL((bIsReturn || (bIsReferenceOrPointer && !bIsConstTarget)) && !bIsBound);
					if((bIsReturn || (bIsReferenceOrPointer && !bIsConstTarget)) && !bIsBound)
					{
						return true;
					}
				}
				return false;
			}

			SGUID          m_guid;
			SGUID          m_actionGUID;
			string         m_name;
			string         m_namespace;
			string         m_fileName;
			string         m_author;
			string         m_description;
			string         m_wikiLink;
			ParamVector    m_params;
			UInt32Vector   m_inputs;
			TVariantVector m_variantInputs;
			UInt32Vector   m_outputs;
			TVariantVector m_variantOutputs;
		};
	}

	template <class ACTION, typename SIGNATURE> class CActionMemberFunction;

	template <class ACTION> class CActionMemberFunction<ACTION, void ()> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)();

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			(static_cast<ACTION&>(action).*m_pFunction)();
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION> class CActionMemberFunction<const ACTION, void ()> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)() const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			(static_cast<ACTION&>(action).*m_pFunction)();
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0> class CActionMemberFunction<ACTION, PARAM0 ()> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)();

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			PARAM0 param0 = (static_cast<ACTION&>(action).*m_pFunction)();

			CBase::StoreParam(outputsArchive, 0, param0);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0> class CActionMemberFunction<const ACTION, PARAM0 ()> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)() const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			PARAM0 param0 = (static_cast<ACTION&>(action).*m_pFunction)();

			CBase::StoreParam(outputsArchive, 0, param0);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0> class CActionMemberFunction<ACTION, void (PARAM0)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;

			CBase::LoadParam(inputsArchive, 0, param0);

			(static_cast<ACTION&>(action).*m_pFunction)(param0);

			CBase::StoreParam(outputsArchive, 0, param0);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0> class CActionMemberFunction<const ACTION, void (PARAM0)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;

			CBase::LoadParam(inputsArchive, 0, param0);

			(static_cast<ACTION&>(action).*m_pFunction)(param0);

			CBase::StoreParam(outputsArchive, 0, param0);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1> class CActionMemberFunction<ACTION, PARAM0 (PARAM1)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;

			CBase::LoadParam(inputsArchive, 1, param1);

			PARAM0	param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1> class CActionMemberFunction<const ACTION, PARAM0 (PARAM1)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;

			CBase::LoadParam(inputsArchive, 1, param1);

			PARAM0	param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1> class CActionMemberFunction<ACTION, void (PARAM0, PARAM1)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1));
			
			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1> class CActionMemberFunction<const ACTION, void (PARAM0, PARAM1)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2> class CActionMemberFunction<ACTION, PARAM0 (PARAM1, PARAM2)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);

			PARAM0 param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2));
			
			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2> class CActionMemberFunction<const ACTION, PARAM0 (PARAM1, PARAM2)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);

			PARAM0 param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2> class CActionMemberFunction<ACTION, void (PARAM0, PARAM1, PARAM2)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2));
			
			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2> class CActionMemberFunction<const ACTION, void (PARAM0, PARAM1, PARAM2)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3> class CActionMemberFunction<ACTION, PARAM0 (PARAM1, PARAM2, PARAM3)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);

			PARAM0 param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3));
			
			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3> class CActionMemberFunction<const ACTION, PARAM0 (PARAM1, PARAM2, PARAM3)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);

			PARAM0	param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3> class CActionMemberFunction<ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3));
			
			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3> class CActionMemberFunction<const ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4> class CActionMemberFunction<ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);

			PARAM0 param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4> class CActionMemberFunction<const ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);

			PARAM0 param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4> class CActionMemberFunction<ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4> class CActionMemberFunction<const ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5> class CActionMemberFunction<ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);

			PARAM0 param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5> class CActionMemberFunction<const ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);

			PARAM0 param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5> class CActionMemberFunction<ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5> class CActionMemberFunction<const ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6> class CActionMemberFunction<ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM6>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM6>::Result param6;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);

			PARAM0 param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6> class CActionMemberFunction<const ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM6>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM6>::Result param6;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);

			PARAM0 param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6> class CActionMemberFunction<ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM6>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM6>::Result param6;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6> class CActionMemberFunction<const ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM6>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM6>::Result param6;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7> class CActionMemberFunction<ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM6>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM7>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM6>::Result param6;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM7>::Result param7;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);
			CBase::LoadParam(inputsArchive, 7, param7);

			PARAM0 param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
			CBase::StoreParam(outputsArchive, 7, param7);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7> class CActionMemberFunction<const ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM6>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM7>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM6>::Result param6;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM7>::Result param7;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);
			CBase::LoadParam(inputsArchive, 7, param7);

			PARAM0 param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
			CBase::StoreParam(outputsArchive, 7, param7);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7> class CActionMemberFunction<ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM6>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM7>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4; 
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM6>::Result param6;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM7>::Result param7;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);
			CBase::LoadParam(inputsArchive, 7, param7);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
			CBase::StoreParam(outputsArchive, 7, param7);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7> class CActionMemberFunction<const ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM6>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM7>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM6>::Result param6;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM7>::Result param7;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);
			CBase::LoadParam(inputsArchive, 7, param7);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
			CBase::StoreParam(outputsArchive, 7, param7);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8> class CActionMemberFunction<ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM6>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM7>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM8>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM6>::Result param6;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM7>::Result param7;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM8>::Result param8;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);
			CBase::LoadParam(inputsArchive, 7, param7);
			CBase::LoadParam(inputsArchive, 8, param8);

			PARAM0 param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7), CBase::ConvertParam<PARAM8>(param8));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
			CBase::StoreParam(outputsArchive, 7, param7);
			CBase::StoreParam(outputsArchive, 8, param8);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8> class CActionMemberFunction<const ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM6>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM7>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM8>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM6>::Result param6;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM7>::Result param7;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM8>::Result param8;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);
			CBase::LoadParam(inputsArchive, 7, param7);
			CBase::LoadParam(inputsArchive, 8, param8);

			PARAM0 param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7), CBase::ConvertParam<PARAM8>(param8));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
			CBase::StoreParam(outputsArchive, 7, param7);
			CBase::StoreParam(outputsArchive, 8, param8);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8> class CActionMemberFunction<ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM6>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM7>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM8>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM6>::Result param6;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM7>::Result param7;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM8>::Result param8;
			
			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);
			CBase::LoadParam(inputsArchive, 7, param7);
			CBase::LoadParam(inputsArchive, 8, param8);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7), CBase::ConvertParam<PARAM8>(param8));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
			CBase::StoreParam(outputsArchive, 7, param7);
			CBase::StoreParam(outputsArchive, 8, param8);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8> class CActionMemberFunction<const ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM6>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM7>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM8>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM6>::Result param6;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM7>::Result param7;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM8>::Result param8;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);
			CBase::LoadParam(inputsArchive, 7, param7);
			CBase::LoadParam(inputsArchive, 8, param8);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7), CBase::ConvertParam<PARAM8>(param8));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
			CBase::StoreParam(outputsArchive, 7, param7);
			CBase::StoreParam(outputsArchive, 8, param8);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9> class CActionMemberFunction<ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM6>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM7>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM8>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM9>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM6>::Result param6;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM7>::Result param7;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM8>::Result param8;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM8>::Result param9;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);
			CBase::LoadParam(inputsArchive, 7, param7);
			CBase::LoadParam(inputsArchive, 8, param8);
			CBase::LoadParam(inputsArchive, 9, param9);

			PARAM0 param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7), CBase::ConvertParam<PARAM8>(param8), CBase::ConvertParam<PARAM9>(param9));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
			CBase::StoreParam(outputsArchive, 7, param7);
			CBase::StoreParam(outputsArchive, 8, param8);
			CBase::StoreParam(outputsArchive, 9, param9);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9> class CActionMemberFunction<const ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::IsReturn);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM6>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM7>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM8>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM9>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM6>::Result param6;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM7>::Result param7;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM8>::Result param8;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM9>::Result param9;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);
			CBase::LoadParam(inputsArchive, 7, param7);
			CBase::LoadParam(inputsArchive, 8, param8);
			CBase::LoadParam(inputsArchive, 9, param9);

			PARAM0 param0 = (static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7), CBase::ConvertParam<PARAM8>(param8), CBase::ConvertParam<PARAM9>(param9));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
			CBase::StoreParam(outputsArchive, 7, param7);
			CBase::StoreParam(outputsArchive, 8, param8);
			CBase::StoreParam(outputsArchive, 9, param9);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9> class CActionMemberFunction<ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9);

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM6>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM7>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM8>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM9>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM6>::Result param6;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM7>::Result param7;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM8>::Result param8;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM9>::Result param9;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);
			CBase::LoadParam(inputsArchive, 7, param7);
			CBase::LoadParam(inputsArchive, 8, param8);
			CBase::LoadParam(inputsArchive, 9, param9);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7), CBase::ConvertParam<PARAM8>(param8), CBase::ConvertParam<PARAM9>(param9));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
			CBase::StoreParam(outputsArchive, 7, param7);
			CBase::StoreParam(outputsArchive, 7, param7);
			CBase::StoreParam(outputsArchive, 8, param8);
			CBase::StoreParam(outputsArchive, 9, param9);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <class ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9> class CActionMemberFunction<const ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)> : public ActionMemberFunctionUtils::CBase
	{
	public:

		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9) const;

		inline CActionMemberFunction(FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: CBase(guid, actionGUID, szDeclaration, szFileName, szProjectDir)
			, m_pFunction(pFunction)
		{
			CBase::AddParam<PARAM0>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM1>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM2>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM3>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM4>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM5>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM6>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM7>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM8>(ActionMemberFunctionUtils::EParamFlags::None);
			CBase::AddParam<PARAM9>(ActionMemberFunctionUtils::EParamFlags::None);
		}

		// IActionMemberFunction

		virtual void Call(IAction& action, const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive  inputsArchive(inputs);
			CVariantArrayOutputArchive outputsArchive(outputs);

			typename ActionMemberFunctionUtils::GetStorageType<PARAM0>::Result param0;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM1>::Result param1;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM2>::Result param2;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM3>::Result param3;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM4>::Result param4;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM5>::Result param5;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM6>::Result param6;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM7>::Result param7;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM8>::Result param8;
			typename ActionMemberFunctionUtils::GetStorageType<PARAM9>::Result param9;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);
			CBase::LoadParam(inputsArchive, 7, param7);
			CBase::LoadParam(inputsArchive, 8, param8);
			CBase::LoadParam(inputsArchive, 9, param9);

			(static_cast<ACTION&>(action).*m_pFunction)(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7), CBase::ConvertParam<PARAM8>(param8), CBase::ConvertParam<PARAM9>(param9));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
			CBase::StoreParam(outputsArchive, 7, param7);
			CBase::StoreParam(outputsArchive, 8, param8);
			CBase::StoreParam(outputsArchive, 9, param9);
		}

		// ~IActionMemberFunction

	private:

		FunctionPtr m_pFunction;
	};

	template <typename FUNCTION_PTR_TYPE> struct SMakeActionMemberFunctionShared;

	template <typename ACTION> struct SMakeActionMemberFunctionShared<void (ACTION::*)()>
	{
		typedef void (ACTION::*FunctionPtr)();

		typedef CActionMemberFunction<ACTION, void ()> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION> struct SMakeActionMemberFunctionShared<void (ACTION::*)() const>
	{
		typedef void (ACTION::*FunctionPtr)() const;

		typedef CActionMemberFunction<const ACTION, void ()> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)()>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)();

		typedef CActionMemberFunction<ACTION, PARAM0 ()> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)() const>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)() const;

		typedef CActionMemberFunction<const ACTION, PARAM0 ()> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0)>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0);

		typedef CActionMemberFunction<ACTION, void (PARAM0)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0) const>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0) const;

		typedef CActionMemberFunction<const ACTION, void (PARAM0)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1)>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1);

		typedef CActionMemberFunction<ACTION, PARAM0 (PARAM1)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1) const>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1) const;

		typedef CActionMemberFunction<const ACTION, PARAM0 (PARAM1)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1)>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1);

		typedef CActionMemberFunction<ACTION, void (PARAM0, PARAM1)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1) const>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1) const;

		typedef CActionMemberFunction<const ACTION, void (PARAM0, PARAM1)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1, PARAM2)>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2);

		typedef CActionMemberFunction<ACTION, PARAM0 (PARAM1, PARAM2)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1, PARAM2) const>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2) const;

		typedef CActionMemberFunction<const ACTION, PARAM0 (PARAM1, PARAM2)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1, PARAM2)>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2);

		typedef CActionMemberFunction<ACTION, void (PARAM0, PARAM1, PARAM2)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1, PARAM2) const>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2) const;

		typedef CActionMemberFunction<const ACTION, void (PARAM0, PARAM1, PARAM2)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1, PARAM2, PARAM3)>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3);

		typedef CActionMemberFunction<ACTION, PARAM0 (PARAM1, PARAM2, PARAM3)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1, PARAM2, PARAM3) const>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3) const;

		typedef CActionMemberFunction<const ACTION, PARAM0 (PARAM1, PARAM2, PARAM3)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1, PARAM2, PARAM3)>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3);

		typedef CActionMemberFunction<ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1, PARAM2, PARAM3) const>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3) const;

		typedef CActionMemberFunction<const ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1, PARAM2, PARAM3, PARAM4)>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4);

		typedef CActionMemberFunction<ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1, PARAM2, PARAM3, PARAM4) const>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4) const;

		typedef CActionMemberFunction<const ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4)>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4);

		typedef CActionMemberFunction<ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4) const>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4) const;

		typedef CActionMemberFunction<const ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5);

		typedef CActionMemberFunction<ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5) const>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5) const;

		typedef CActionMemberFunction<const ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5);

		typedef CActionMemberFunction<ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5) const>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5) const;

		typedef CActionMemberFunction<const ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6);

		typedef CActionMemberFunction<ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6) const>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6) const;

		typedef CActionMemberFunction<const ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6);

		typedef CActionMemberFunction<ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6) const>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6) const;

		typedef CActionMemberFunction<const ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7);

		typedef CActionMemberFunction<ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7) const>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7) const;

		typedef CActionMemberFunction<const ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7);

		typedef CActionMemberFunction<ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7) const>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7) const;

		typedef CActionMemberFunction<const ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8);

		typedef CActionMemberFunction<ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8) const>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8) const;

		typedef CActionMemberFunction<const ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8);

		typedef CActionMemberFunction<ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8) const>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8) const;

		typedef CActionMemberFunction<const ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9);

		typedef CActionMemberFunction<ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9> struct SMakeActionMemberFunctionShared<PARAM0 (ACTION::*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9) const>
	{
		typedef PARAM0 (ACTION::*FunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9) const;

		typedef CActionMemberFunction<const ACTION, PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9);

		typedef CActionMemberFunction<ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};

	template <typename ACTION, typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9> struct SMakeActionMemberFunctionShared<void (ACTION::*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9) const>
	{
		typedef void (ACTION::*FunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9) const;
		typedef CActionMemberFunction<const ACTION, void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)> ActionMemberFunction;

		inline IActionMemberFunctionPtr operator () (FunctionPtr pFunction, const SGUID& guid, const SGUID& actionGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir) const
		{
			return IActionMemberFunctionPtr(new ActionMemberFunction(pFunction, guid, actionGUID, szDeclaration, szFileName, szProjectDir));
		}
	};
}
