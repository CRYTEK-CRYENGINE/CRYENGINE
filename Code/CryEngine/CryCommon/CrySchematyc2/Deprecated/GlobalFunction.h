// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Investigate - Is there a dependency between the order of params and the order in which they are bound to inputs/outputs? If so can we just sort inputs and outputs?
// #SchematycTODO : Investigate - Can we bind a reference/pointer to an input and an output?
// #SchematycTODO : Compile time error messages for unsupported types such as pointers?

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CrySerialization/MathImpl.h>
#include <CrySerialization/STL.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_PreprocessorUtils.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_TypeUtils.h>

#include "CrySchematyc2/Any.h"
#include "CrySchematyc2/Deprecated/IGlobalFunction.h"
#include "CrySchematyc2/Deprecated/Variant.h"
#include "CrySchematyc2/Services/ILog.h"
#include "CrySchematyc2/Utils/StringUtils.h"

#define SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(function, guid) Schematyc2::SMakeGlobalFunctionShared<TYPE_OF(&function)>()(&function, guid, TemplateUtils::GetFunctionName<TYPE_OF(&function), &function>(), __FILE__, "Code")

namespace Schematyc2
{
	typedef std::vector<size_t> TSizeTVector;

	namespace GlobalFunction
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
				static const TYPE	defaultValue = TYPE();
				return defaultValue;
			}
		};

		template <typename TYPE, size_t SIZE> struct GetDefaultValue<TYPE[SIZE]>
		{
			typedef TYPE Array[SIZE];

			inline const Array& operator () () const
			{
				static const Array	defaultValue = { TYPE() };
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

		namespace ParamFlags
		{
			enum EValue
			{
				NONE						= 0x0000,
				IS_RETURN				= 0x0001,
				IS_REFERENCE		= 0x0002,
				IS_POINTER			= 0x0004,
				IS_CONST_TARGET	= 0x0008,
				IS_CONST				= 0x0010,
				IS_INPUT				= 0x0020,
				IS_OUTPUT				= 0x0040
			};
		};

		DECLARE_ENUM_FLAGS(ParamFlags::EValue)

		struct SParam
		{
			inline SParam(const IAnyPtr& _pValue, ParamFlags::EValue _flags)
				: pValue(_pValue)
				, flags(_flags)
			{}

			IAnyPtr							pValue;
			string							name;
			string							description;
			ParamFlags::EValue	flags;
		};

		template <typename TYPE> inline ParamFlags::EValue GetParamFlags()
		{
			ParamFlags::EValue	flags = ParamFlags::NONE;
			if(TemplateUtils::ReferenceTraits<TYPE>::IsReference)
			{
				flags |= ParamFlags::IS_REFERENCE;
				if(TemplateUtils::ConstTraits<typename TemplateUtils::ReferenceTraits<TYPE>::RemoveReference>::IsConst)
				{
					flags |= ParamFlags::IS_CONST_TARGET;
				}
			}
			if(TemplateUtils::PointerTraits<TYPE>::IsPointer)
			{
				flags |= ParamFlags::IS_POINTER;
				if(TemplateUtils::ConstTraits<typename TemplateUtils::PointerTraits<TYPE>::RemovePointer>::IsConst)
				{
					flags |= ParamFlags::IS_CONST_TARGET;
				}
			}
			if(TemplateUtils::ConstTraits<TYPE>::IsConst)
			{
				flags |= ParamFlags::IS_CONST;
			}
			return flags;
		}

		template <typename TYPE> inline SParam MakeParam(ParamFlags::EValue flags)
		{
			typedef typename GetStorageType<TYPE>::Result StorageType;
			return SParam(MakeAnyShared<StorageType>(GetDefaultValue<StorageType>()()), flags | GetParamFlags<TYPE>());
		}

		template<> inline SParam MakeParam<const char*>(ParamFlags::EValue flags)
		{
			return SParam(MakeAnyShared(CPoolString()), flags | GetParamFlags<const char*>());
		}

		class CBase : public IGlobalFunction
		{
		public:

			inline CBase(const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
				: m_guid(guid)
				, m_inputParamsAreResources(false)
			{
				StringUtils::SeparateTypeNameAndNamespace(declaration, m_name, m_namespace);
				SetFileName(fileName, projectDir);
			}

			// IGlobalFunction

			virtual SGUID GetGUID() const override
			{
				return m_guid;
			}

			virtual void SetName(const char* name) override
			{
				m_name = name;
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

			virtual void SetFileName(const char* fileName, const char* projectDir) override
			{
				StringUtils::MakeProjectRelativeFileName(fileName, projectDir, m_fileName);
			}

			virtual const char* GetFileName() const override
			{
				return m_fileName.c_str();
			}

			virtual void SetAuthor(const char* author) override
			{
				m_author = author;
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

			virtual bool BindInput(size_t iParam, const char* name, const char* szDescription) override
			{
				SCHEMATYC2_SYSTEM_ASSERT_FATAL((name != NULL) && (szDescription != NULL));
				if(CanBindInput(iParam) == true)
				{
					SParam& param = m_params[iParam];
					param.name        = name;
					param.description = szDescription;
					param.flags |= ParamFlags::IS_INPUT;
					m_inputs.reserve(10);
					m_inputs.push_back(iParam);

					CVariantVectorOutputArchive	archive(m_variantInputs);
					archive(*param.pValue, "", "");
					return true;
				}
				return false;
			}

			virtual size_t GetInputCount() const override
			{
				return m_inputs.size();
			}

			virtual IAnyConstPtr GetInputValue(size_t iInput) const override
			{
				return iInput < m_inputs.size() ? m_params[m_inputs[iInput]].pValue : IAnyConstPtr();
			}

			virtual const char* GetInputName(size_t iInput) const override
			{
				return iInput < m_inputs.size() ? m_params[m_inputs[iInput]].name.c_str() : "";
			}

			virtual const char* GetInputDescription(size_t iInput) const override
			{
				return iInput < m_inputs.size() ? m_params[m_inputs[iInput]].description.c_str() : "";
			}

			virtual TVariantConstArray GetVariantInputs() const override
			{
				return m_variantInputs;
			}

			virtual bool BindOutput(size_t iParam, const char* name, const char* szDescription) override
			{
				SCHEMATYC2_SYSTEM_ASSERT_FATAL((name != NULL) && (szDescription != NULL));
				if(CanBindOutput(iParam) == true)
				{
					SParam&	param = m_params[iParam];
					param.name        = name;
					param.description = szDescription;
					param.flags |= ParamFlags::IS_OUTPUT;
					m_outputs.reserve(10);
					m_outputs.push_back(iParam);

					CVariantVectorOutputArchive	archive(m_variantOutputs);
					archive(*param.pValue, "", "");
					return true;
				}
				return false;
			}

			virtual size_t GetOutputCount() const override
			{
				return m_outputs.size();
			}

			virtual IAnyConstPtr GetOutputValue(size_t iOutput) const override
			{
				return iOutput < m_outputs.size() ? m_params[m_outputs[iOutput]].pValue : IAnyConstPtr();
			}

			virtual const char* GetOutputName(size_t iOutput) const override
			{
				return iOutput < m_outputs.size() ? m_params[m_outputs[iOutput]].name.c_str() : "";
			}

			virtual const char* GetOutputDescription(size_t iOutput) const override
			{
				return iOutput < m_outputs.size() ? m_params[m_outputs[iOutput]].description.c_str() : "";
			}

			virtual TVariantConstArray GetVariantOutputs() const override
			{
				return m_variantOutputs;
			}

			virtual void RegisterInputParamsForResourcePrecache() override
			{
				m_inputParamsAreResources = true;
			}

			virtual bool AreInputParamsResources() const override
			{
				return m_inputParamsAreResources;
			}

			// ~IGlobalFunction

		protected:

			// IGlobalFunction

			virtual bool BindInput_Protected(size_t iParam, const char* name, const char* szDescription, const IAny& value) override
			{
				if(CanBindInput(iParam) == true)
				{
					SParam&	param = m_params[iParam];
					SCHEMATYC2_SYSTEM_ASSERT_FATAL(value.GetTypeInfo().GetTypeId() == param.pValue->GetTypeInfo().GetTypeId());
					if(value.GetTypeInfo().GetTypeId() == param.pValue->GetTypeInfo().GetTypeId())
					{
						param.name        = name;
						param.description = szDescription;
						param.pValue      = value.Clone();
						param.flags |= ParamFlags::IS_INPUT;
						m_inputs.reserve(10);
						m_inputs.push_back(iParam);

						CVariantVectorOutputArchive	archive(m_variantInputs);
						archive(value, "", "");
						return true;
					}
				}
				return false;
			}

			// ~IGlobalFunction

			template <typename TYPE> inline void AddParam(ParamFlags::EValue flags)
			{
				m_params.reserve(10);
				m_params.push_back(MakeParam<TYPE>(flags));
			}

			template <typename TYPE> inline void LoadParam(CVariantArrayInputArchive& archive, size_t iParam, TYPE& value) const
			{
				if((m_params[iParam].flags & ParamFlags::IS_INPUT) != 0)
				{
					archive(value, "", "");
				}
			}

			inline void LoadParam(CVariantArrayInputArchive& archive, size_t iParam, const char*& value) const
			{
				if((m_params[iParam].flags & ParamFlags::IS_INPUT) != 0)
				{
					archive(value);
				}
			}

			template <typename TO, typename FROM> inline TO ConvertParam(FROM& rhs) const
			{
				return ParamConversion<TO, FROM>()(rhs);
			}

			template <typename TYPE> inline void StoreParam(CVariantArrayOutputArchive& archive, size_t iParam, TYPE& value) const
			{
				if((m_params[iParam].flags & ParamFlags::IS_OUTPUT) != 0)
				{
					archive(value, "", "");
				}
			}

			template <typename TYPE> inline void StoreParam(CVariantArrayOutputArchive& archive, size_t iParam, const TYPE& value) const
			{
				if((m_params[iParam].flags & ParamFlags::IS_OUTPUT) != 0)
				{
					archive(const_cast<TYPE&>(value), "", "");
				}
			}

			template <typename TYPE> inline void StoreParam(CVariantArrayOutputArchive& archive, size_t iParam, TYPE* value) const {}

			template <typename TYPE> inline void StoreParam(CVariantArrayOutputArchive& archive, size_t iParam, const TYPE* value) const {}

			void StoreParam(CVariantArrayOutputArchive& archive, size_t iParam, const char*& value) const
			{
				if((m_params[iParam].flags & ParamFlags::IS_OUTPUT) != 0)
				{
					archive(value);
				}
			}

		private:

			typedef std::vector<SParam> TParamVector;

			inline bool CanBindInput(size_t iParam) const
			{
				SCHEMATYC2_SYSTEM_ASSERT_FATAL(iParam < m_params.size());
				if(iParam < m_params.size())
				{
					const bool	isReturn = (m_params[iParam].flags & ParamFlags::IS_RETURN) != 0;
					const bool	isBound = (m_params[iParam].flags & (ParamFlags::IS_INPUT | ParamFlags::IS_OUTPUT)) != 0;
					SCHEMATYC2_SYSTEM_ASSERT_FATAL(!isReturn && !isBound);
					if(!isReturn && !isBound)
					{
						return true;
					}
				}
				return false;
			}

			inline bool CanBindOutput(size_t iParam) const
			{
				SCHEMATYC2_SYSTEM_ASSERT_FATAL(iParam < m_params.size());
				if(iParam < m_params.size())
				{
					const bool	isReturn = (m_params[iParam].flags & ParamFlags::IS_RETURN) != 0;
					const bool	isReferenceOrPointer = (m_params[iParam].flags & (ParamFlags::IS_REFERENCE | ParamFlags::IS_POINTER)) != 0;
					const bool	isConstTarget = (m_params[iParam].flags & ParamFlags::IS_CONST_TARGET) != 0;
					const bool	isBound = (m_params[iParam].flags & (ParamFlags::IS_INPUT | ParamFlags::IS_OUTPUT)) != 0;
					SCHEMATYC2_SYSTEM_ASSERT_FATAL((isReturn || (isReferenceOrPointer && !isConstTarget)) && !isBound);
					if((isReturn || (isReferenceOrPointer && !isConstTarget)) && !isBound)
					{
						return true;
					}
				}
				return false;
			}

			SGUID          m_guid;
			string         m_name;
			string         m_namespace;
			string         m_fileName;
			string         m_author;
			string         m_description;
			string         m_wikiLink;
			TParamVector   m_params;
			TSizeTVector   m_inputs;
			TVariantVector m_variantInputs;
			TSizeTVector   m_outputs;
			TVariantVector m_variantOutputs;
			bool           m_inputParamsAreResources;
		};
	}

	template <typename SIGNATURE> class CGlobalFunction;

	template <> class CGlobalFunction<void ()> : public GlobalFunction::CBase
	{
	public:

		typedef void (*TFunctionPtr)();

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			m_functionPtr();
		}

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0> class CGlobalFunction<PARAM0 ()> : public GlobalFunction::CBase
	{
	public:

		typedef PARAM0 (*TFunctionPtr)();

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::IS_RETURN);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			PARAM0	param0 = m_functionPtr();

			CBase::StoreParam(outputsArchive, 0, param0);
		}

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0> class CGlobalFunction<void (PARAM0)> : public GlobalFunction::CBase
	{
	public:

		typedef void (*TFunctionPtr)(PARAM0);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM0>::Result	param0;

			CBase::LoadParam(inputsArchive, 0, param0);

			m_functionPtr(param0);

			CBase::StoreParam(outputsArchive, 0, param0);
		}

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1> class CGlobalFunction<PARAM0 (PARAM1)> : public GlobalFunction::CBase
	{
	public:

		typedef PARAM0 (*TFunctionPtr)(PARAM1);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::IS_RETURN);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;

			CBase::LoadParam(inputsArchive, 1, param1);

			PARAM0	param0 = m_functionPtr(CBase::ConvertParam<PARAM1>(param1));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
		}

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1> class CGlobalFunction<void (PARAM0, PARAM1)> : public GlobalFunction::CBase
	{
	public:

		typedef void (*TFunctionPtr)(PARAM0, PARAM1);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM0>::Result	param0;
			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);

			m_functionPtr(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
		}

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2> class CGlobalFunction<PARAM0 (PARAM1, PARAM2)> : public GlobalFunction::CBase
	{
	public:

		typedef PARAM0 (*TFunctionPtr)(PARAM1, PARAM2);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::IS_RETURN);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM2>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;
			typename GlobalFunction::GetStorageType<PARAM2>::Result	param2;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);

			PARAM0	param0 = m_functionPtr(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
		}

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2> class CGlobalFunction<void (PARAM0, PARAM1, PARAM2)> : public GlobalFunction::CBase
	{
	public:

		typedef void (*TFunctionPtr)(PARAM0, PARAM1, PARAM2);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM2>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM0>::Result	param0;
			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;
			typename GlobalFunction::GetStorageType<PARAM2>::Result	param2;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);

			m_functionPtr(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2));
			
			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
		}

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3> class CGlobalFunction<PARAM0 (PARAM1, PARAM2, PARAM3)> : public GlobalFunction::CBase
	{
	public:

		typedef PARAM0 (*TFunctionPtr)(PARAM1, PARAM2, PARAM3);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::IS_RETURN);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM2>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM3>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;
			typename GlobalFunction::GetStorageType<PARAM2>::Result	param2;
			typename GlobalFunction::GetStorageType<PARAM3>::Result	param3;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);

			PARAM0	param0 = m_functionPtr(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3));
			
			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
		}

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3> class CGlobalFunction<void (PARAM0, PARAM1, PARAM2, PARAM3)> : public GlobalFunction::CBase
	{
	public:

		typedef void (*TFunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM2>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM3>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM0>::Result	param0;
			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;
			typename GlobalFunction::GetStorageType<PARAM2>::Result	param2;
			typename GlobalFunction::GetStorageType<PARAM3>::Result	param3;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);

			m_functionPtr(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3));
			
			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
		}

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4> class CGlobalFunction<PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4)> : public GlobalFunction::CBase
	{
	public:

		typedef PARAM0 (*TFunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::IS_RETURN);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM2>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM3>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM4>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;
			typename GlobalFunction::GetStorageType<PARAM2>::Result	param2;
			typename GlobalFunction::GetStorageType<PARAM3>::Result	param3;
			typename GlobalFunction::GetStorageType<PARAM4>::Result	param4;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);

			PARAM0	param0 = m_functionPtr(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4));
			
			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
		}

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4> class CGlobalFunction<void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4)> : public GlobalFunction::CBase
	{
	public:

		typedef void (*TFunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM2>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM3>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM4>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM0>::Result	param0;
			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;
			typename GlobalFunction::GetStorageType<PARAM2>::Result	param2;
			typename GlobalFunction::GetStorageType<PARAM3>::Result	param3;
			typename GlobalFunction::GetStorageType<PARAM4>::Result	param4;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);

			m_functionPtr(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
		}

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5> class CGlobalFunction<PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)> : public GlobalFunction::CBase
	{
	public:

		typedef PARAM0 (*TFunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::IS_RETURN);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM2>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM3>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM4>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM5>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;
			typename GlobalFunction::GetStorageType<PARAM2>::Result	param2;
			typename GlobalFunction::GetStorageType<PARAM3>::Result	param3;
			typename GlobalFunction::GetStorageType<PARAM4>::Result	param4;
			typename GlobalFunction::GetStorageType<PARAM5>::Result	param5;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);

			PARAM0	param0 = m_functionPtr(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
		}

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5> class CGlobalFunction<void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)> : public GlobalFunction::CBase
	{
	public:

		typedef void (*TFunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM2>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM3>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM4>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM5>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM0>::Result	param0;
			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;
			typename GlobalFunction::GetStorageType<PARAM2>::Result	param2;
			typename GlobalFunction::GetStorageType<PARAM3>::Result	param3;
			typename GlobalFunction::GetStorageType<PARAM4>::Result	param4;
			typename GlobalFunction::GetStorageType<PARAM5>::Result	param5;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);

			m_functionPtr(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
		}

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6> class CGlobalFunction<PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)> : public GlobalFunction::CBase
	{
	public:

		typedef PARAM0 (*TFunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::IS_RETURN);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM2>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM3>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM4>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM5>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM6>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;
			typename GlobalFunction::GetStorageType<PARAM2>::Result	param2;
			typename GlobalFunction::GetStorageType<PARAM3>::Result	param3;
			typename GlobalFunction::GetStorageType<PARAM4>::Result	param4;
			typename GlobalFunction::GetStorageType<PARAM5>::Result	param5;
			typename GlobalFunction::GetStorageType<PARAM6>::Result	param6;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);

			PARAM0	param0 = m_functionPtr(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
		}

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6> class CGlobalFunction<void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)> : public GlobalFunction::CBase
	{
	public:

		typedef void (*TFunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM2>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM3>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM4>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM5>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM6>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM0>::Result	param0;
			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;
			typename GlobalFunction::GetStorageType<PARAM2>::Result	param2;
			typename GlobalFunction::GetStorageType<PARAM3>::Result	param3;
			typename GlobalFunction::GetStorageType<PARAM4>::Result	param4;
			typename GlobalFunction::GetStorageType<PARAM5>::Result	param5;
			typename GlobalFunction::GetStorageType<PARAM6>::Result	param6;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);

			m_functionPtr(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
		}

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7> class CGlobalFunction<PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)> : public GlobalFunction::CBase
	{
	public:

		typedef PARAM0 (*TFunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::IS_RETURN);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM2>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM3>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM4>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM5>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM6>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM7>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;
			typename GlobalFunction::GetStorageType<PARAM2>::Result	param2;
			typename GlobalFunction::GetStorageType<PARAM3>::Result	param3;
			typename GlobalFunction::GetStorageType<PARAM4>::Result	param4;
			typename GlobalFunction::GetStorageType<PARAM5>::Result	param5;
			typename GlobalFunction::GetStorageType<PARAM6>::Result	param6;
			typename GlobalFunction::GetStorageType<PARAM7>::Result	param7;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);
			CBase::LoadParam(inputsArchive, 7, param7);

			PARAM0	param0 = m_functionPtr(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
			CBase::StoreParam(outputsArchive, 7, param7);
		}

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7> class CGlobalFunction<void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)> : public GlobalFunction::CBase
	{
	public:

		typedef void (*TFunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM2>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM3>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM4>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM5>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM6>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM7>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM0>::Result	param0;
			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;
			typename GlobalFunction::GetStorageType<PARAM2>::Result	param2;
			typename GlobalFunction::GetStorageType<PARAM3>::Result	param3;
			typename GlobalFunction::GetStorageType<PARAM4>::Result	param4;
			typename GlobalFunction::GetStorageType<PARAM5>::Result	param5;
			typename GlobalFunction::GetStorageType<PARAM6>::Result	param6;
			typename GlobalFunction::GetStorageType<PARAM7>::Result	param7;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);
			CBase::LoadParam(inputsArchive, 7, param7);

			m_functionPtr(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7));

			CBase::StoreParam(outputsArchive, 0, param0);
			CBase::StoreParam(outputsArchive, 1, param1);
			CBase::StoreParam(outputsArchive, 2, param2);
			CBase::StoreParam(outputsArchive, 3, param3);
			CBase::StoreParam(outputsArchive, 4, param4);
			CBase::StoreParam(outputsArchive, 5, param5);
			CBase::StoreParam(outputsArchive, 6, param6);
			CBase::StoreParam(outputsArchive, 7, param7);
		}

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8> class CGlobalFunction<PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)> : public GlobalFunction::CBase
	{
	public:

		typedef PARAM0 (*TFunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::IS_RETURN);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM2>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM3>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM4>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM5>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM6>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM7>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM8>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;
			typename GlobalFunction::GetStorageType<PARAM2>::Result	param2;
			typename GlobalFunction::GetStorageType<PARAM3>::Result	param3;
			typename GlobalFunction::GetStorageType<PARAM4>::Result	param4;
			typename GlobalFunction::GetStorageType<PARAM5>::Result	param5;
			typename GlobalFunction::GetStorageType<PARAM6>::Result	param6;
			typename GlobalFunction::GetStorageType<PARAM7>::Result	param7;
			typename GlobalFunction::GetStorageType<PARAM8>::Result	param8;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);
			CBase::LoadParam(inputsArchive, 7, param7);
			CBase::LoadParam(inputsArchive, 8, param8);

			PARAM0	param0 = m_functionPtr(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7), CBase::ConvertParam<PARAM8>(param8));

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

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8> class CGlobalFunction<void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)> : public GlobalFunction::CBase
	{
	public:

		typedef void (*TFunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM2>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM3>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM4>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM5>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM6>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM7>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM8>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM0>::Result	param0;
			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;
			typename GlobalFunction::GetStorageType<PARAM2>::Result	param2;
			typename GlobalFunction::GetStorageType<PARAM3>::Result	param3;
			typename GlobalFunction::GetStorageType<PARAM4>::Result	param4;
			typename GlobalFunction::GetStorageType<PARAM5>::Result	param5;
			typename GlobalFunction::GetStorageType<PARAM6>::Result	param6;
			typename GlobalFunction::GetStorageType<PARAM7>::Result	param7;
			typename GlobalFunction::GetStorageType<PARAM8>::Result	param8;

			CBase::LoadParam(inputsArchive, 0, param0);
			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);
			CBase::LoadParam(inputsArchive, 7, param7);
			CBase::LoadParam(inputsArchive, 8, param8);

			m_functionPtr(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7), CBase::ConvertParam<PARAM8>(param8));

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

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9> class CGlobalFunction<PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)> : public GlobalFunction::CBase
	{
	public:

		typedef PARAM0 (*TFunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::IS_RETURN);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM2>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM3>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM4>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM5>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM6>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM7>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM8>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM9>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;
			typename GlobalFunction::GetStorageType<PARAM2>::Result	param2;
			typename GlobalFunction::GetStorageType<PARAM3>::Result	param3;
			typename GlobalFunction::GetStorageType<PARAM4>::Result	param4;
			typename GlobalFunction::GetStorageType<PARAM5>::Result	param5;
			typename GlobalFunction::GetStorageType<PARAM6>::Result	param6;
			typename GlobalFunction::GetStorageType<PARAM7>::Result	param7;
			typename GlobalFunction::GetStorageType<PARAM8>::Result	param8;
			typename GlobalFunction::GetStorageType<PARAM9>::Result	param9;

			CBase::LoadParam(inputsArchive, 1, param1);
			CBase::LoadParam(inputsArchive, 2, param2);
			CBase::LoadParam(inputsArchive, 3, param3);
			CBase::LoadParam(inputsArchive, 4, param4);
			CBase::LoadParam(inputsArchive, 5, param5);
			CBase::LoadParam(inputsArchive, 6, param6);
			CBase::LoadParam(inputsArchive, 7, param7);
			CBase::LoadParam(inputsArchive, 8, param8);
			CBase::LoadParam(inputsArchive, 9, param9);

			PARAM0	param0 = m_functionPtr(CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7), CBase::ConvertParam<PARAM8>(param8), CBase::ConvertParam<PARAM9>(param9));

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

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9> class CGlobalFunction<void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)> : public GlobalFunction::CBase
	{
	public:

		typedef void (*TFunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9);

		inline CGlobalFunction(TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir)
			: CBase(guid, declaration, fileName, projectDir)
			, m_functionPtr(functionPtr)
		{
			CBase::AddParam<PARAM0>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM1>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM2>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM3>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM4>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM5>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM6>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM7>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM8>(GlobalFunction::ParamFlags::NONE);
			CBase::AddParam<PARAM9>(GlobalFunction::ParamFlags::NONE);
		}

		// IGlobalFunction

		virtual void Call(const TVariantConstArray& inputs, const TVariantArray& outputs) const override
		{
			CVariantArrayInputArchive		inputsArchive(inputs);
			CVariantArrayOutputArchive	outputsArchive(outputs);

			typename GlobalFunction::GetStorageType<PARAM0>::Result	param0;
			typename GlobalFunction::GetStorageType<PARAM1>::Result	param1;
			typename GlobalFunction::GetStorageType<PARAM2>::Result	param2;
			typename GlobalFunction::GetStorageType<PARAM3>::Result	param3;
			typename GlobalFunction::GetStorageType<PARAM4>::Result	param4;
			typename GlobalFunction::GetStorageType<PARAM5>::Result	param5;
			typename GlobalFunction::GetStorageType<PARAM6>::Result	param6;
			typename GlobalFunction::GetStorageType<PARAM7>::Result	param7;
			typename GlobalFunction::GetStorageType<PARAM8>::Result	param8;
			typename GlobalFunction::GetStorageType<PARAM9>::Result	param9;

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

			m_functionPtr(CBase::ConvertParam<PARAM0>(param0), CBase::ConvertParam<PARAM1>(param1), CBase::ConvertParam<PARAM2>(param2), CBase::ConvertParam<PARAM3>(param3), CBase::ConvertParam<PARAM4>(param4), CBase::ConvertParam<PARAM5>(param5), CBase::ConvertParam<PARAM6>(param6), CBase::ConvertParam<PARAM7>(param7), CBase::ConvertParam<PARAM8>(param8), CBase::ConvertParam<PARAM9>(param9));

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

		// ~IGlobalFunction

	private:

		TFunctionPtr	m_functionPtr;
	};

	template <typename FUNCTION_PTR_TYPE> struct SMakeGlobalFunctionShared;

	template <> struct SMakeGlobalFunctionShared<void (*)()>
	{
		typedef void (*TFunctionPtr)();

		typedef CGlobalFunction<void ()> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0> struct SMakeGlobalFunctionShared<PARAM0 (*)()>
	{
		typedef PARAM0 (*TFunctionPtr)();

		typedef CGlobalFunction<PARAM0 ()> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0> struct SMakeGlobalFunctionShared<void (*)(PARAM0)>
	{
		typedef void (*TFunctionPtr)(PARAM0);

		typedef CGlobalFunction<void (PARAM0)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1> struct SMakeGlobalFunctionShared<PARAM0 (*)(PARAM1)>
	{
		typedef PARAM0 (*TFunctionPtr)(PARAM1);

		typedef CGlobalFunction<PARAM0 (PARAM1)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1> struct SMakeGlobalFunctionShared<void (*)(PARAM0, PARAM1)>
	{
		typedef void (*TFunctionPtr)(PARAM0, PARAM1);

		typedef CGlobalFunction<void (PARAM0, PARAM1)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2> struct SMakeGlobalFunctionShared<PARAM0 (*)(PARAM1, PARAM2)>
	{
		typedef PARAM0 (*TFunctionPtr)(PARAM1, PARAM2);

		typedef CGlobalFunction<PARAM0 (PARAM1, PARAM2)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2> struct SMakeGlobalFunctionShared<void (*)(PARAM0, PARAM1, PARAM2)>
	{
		typedef void (*TFunctionPtr)(PARAM0, PARAM1, PARAM2);

		typedef CGlobalFunction<void (PARAM0, PARAM1, PARAM2)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3> struct SMakeGlobalFunctionShared<PARAM0 (*)(PARAM1, PARAM2, PARAM3)>
	{
		typedef PARAM0 (*TFunctionPtr)(PARAM1, PARAM2, PARAM3);

		typedef CGlobalFunction<PARAM0 (PARAM1, PARAM2, PARAM3)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3> struct SMakeGlobalFunctionShared<void (*)(PARAM0, PARAM1, PARAM2, PARAM3)>
	{
		typedef void (*TFunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3);

		typedef CGlobalFunction<void (PARAM0, PARAM1, PARAM2, PARAM3)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4> struct SMakeGlobalFunctionShared<PARAM0 (*)(PARAM1, PARAM2, PARAM3, PARAM4)>
	{
		typedef PARAM0 (*TFunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4);

		typedef CGlobalFunction<PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4> struct SMakeGlobalFunctionShared<void (*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4)>
	{
		typedef void (*TFunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4);

		typedef CGlobalFunction<void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5> struct SMakeGlobalFunctionShared<PARAM0 (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)>
	{
		typedef PARAM0 (*TFunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5);

		typedef CGlobalFunction<PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5> struct SMakeGlobalFunctionShared<void (*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)>
	{
		typedef void (*TFunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5);

		typedef CGlobalFunction<void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6> struct SMakeGlobalFunctionShared<PARAM0 (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)>
	{
		typedef PARAM0 (*TFunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6);

		typedef CGlobalFunction<PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6> struct SMakeGlobalFunctionShared<void (*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)>
	{
		typedef void (*TFunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6);

		typedef CGlobalFunction<void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7> struct SMakeGlobalFunctionShared<PARAM0 (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)>
	{
		typedef PARAM0 (*TFunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7);

		typedef CGlobalFunction<PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7> struct SMakeGlobalFunctionShared<void (*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)>
	{
		typedef void (*TFunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7);

		typedef CGlobalFunction<void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8> struct SMakeGlobalFunctionShared<PARAM0 (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)>
	{
		typedef PARAM0 (*TFunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8);

		typedef CGlobalFunction<PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8> struct SMakeGlobalFunctionShared<void (*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)>
	{
		typedef void (*TFunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8);

		typedef CGlobalFunction<void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9> struct SMakeGlobalFunctionShared<PARAM0 (*)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)>
	{
		typedef PARAM0 (*TFunctionPtr)(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9);

		typedef CGlobalFunction<PARAM0 (PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};

	template <typename PARAM0, typename PARAM1, typename PARAM2, typename PARAM3, typename PARAM4, typename PARAM5, typename PARAM6, typename PARAM7, typename PARAM8, typename PARAM9> struct SMakeGlobalFunctionShared<void (*)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)>
	{
		typedef void (*TFunctionPtr)(PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9);

		typedef CGlobalFunction<void (PARAM0, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5, PARAM6, PARAM7, PARAM8, PARAM9)> TGlobalFunction;

		inline IGlobalFunctionPtr operator () (TFunctionPtr functionPtr, const SGUID& guid, const char* declaration, const char* fileName, const char* projectDir) const
		{
			return IGlobalFunctionPtr(new TGlobalFunction(functionPtr, guid, declaration, fileName, projectDir));
		}
	};
}
