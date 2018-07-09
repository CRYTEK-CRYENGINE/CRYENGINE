// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryExtension/CryGUID.h>

#include "IReflection.h"
#include "IDescExtension.h"
#include "IFunctionDesc.h"
#include "IVariableDesc.h"

namespace Cry {
namespace Reflection {

#define CRY_REFLECTION_REGISTER_BASE(typeDesc, baseType) \
  typeDesc.ReflectBaseType(TypeId::Of<baseType>, __SOURCE_INFO__)

#define CRY_REFLECTION_REGISTER_MEMBER_FUNCTION(typeDesc, funcPtr, label, guid) \
  typeDesc.ReflectFunction(, label, guid, __SOURCE_INFO__)

#define CRY_REFLECTION_REGISTER_MEMBER_VARIABLE(typeDesc, variable, label, guid) \
  typeDesc.ReflectVariable(&variable, label, guid, __SOURCE_INFO__)

//#define CRY_REFLECTION_REGISTER_CONVERSION(typeDesc, typeId, conversionOerator) \
//  typeDesc.AddConversionOperator(typeId, conversionOperator, __SOURCE_INFO__)

enum class ETypeCategory
{
	Unset = 0,

	Abstract,
	Class,
	Enum,
	Fundamental,
	Union
};

struct IEnumValueDesc
{
	virtual ~IEnumValueDesc() {}

	virtual const char* GetLabel() const = 0;
	virtual const char* GetDescription() const = 0;
	virtual size_t      GetValue() const = 0;
};

struct ITypeDesc : public IExtensibleDesc
{
	virtual ~ITypeDesc() {}

	virtual CryTypeId              GetTypeId() const = 0;
	virtual const CryTypeDesc&     GetTypeDesc() const = 0;

	virtual bool                   IsAbstract() const = 0;
	virtual bool                   IsArray() const = 0;
	virtual bool                   IsClass() const = 0;
	virtual bool                   IsEnum() const = 0;
	virtual bool                   IsFundamental() const = 0;
	virtual bool                   IsUnion() const = 0;

	virtual const CryGUID&         GetGuid() const = 0;
	virtual ETypeCategory          GetCategory() const = 0;
	virtual TypeIndex              GetIndex() const = 0;

	virtual const char*            GetRawName() const = 0;
	virtual void                   SetLabel(const char* szLabel) = 0;
	virtual const char*            GetLabel() const = 0;

	virtual void                   SetDescription(const char* szDescription) = 0;
	virtual const char*            GetDescription() const = 0;

	virtual uint16                 GetSize() const = 0;
	virtual uint16                 GetAlignment() const = 0;

	virtual const SSourceFileInfo& GetSourceInfo() const = 0;

	// Class Members
	virtual bool                 AddBaseType(CryTypeId typeId, const SSourceFileInfo& srcInfo) = 0;
	virtual size_t               GetBaseTypeCount() const = 0;
	virtual const ITypeDesc*     GetBaseTypeByIndex(size_t index) const = 0;

	virtual IFunctionDesc*       AddFunction(const CMemberFunction& function, const char* szLabel, const CryGUID& guid, const SSourceFileInfo& srcInfo) = 0;
	virtual size_t               GetFunctionCount() const = 0;
	virtual const IFunctionDesc* GetFunctionByIndex(size_t index) const = 0;

	virtual IVariableDesc*       AddVariable(CryTypeId typeId, ptrdiff_t offset, const char* szLabel, const CryGUID& guid, const SSourceFileInfo& srcInfo) = 0;
	virtual size_t               GetVariableCount() const = 0;
	virtual const IVariableDesc* GetVariableByIndex(size_t index) const = 0;

	// Enum Members
	virtual IEnumValueDesc*       AddEnumValue(const char* szName, size_t value, const char* szDescription) = 0;
	virtual size_t                GetEnumValueCount() const = 0;
	virtual const IEnumValueDesc* GetEnumValueByIndex(size_t index) const = 0;

	// Generic Members
	virtual Type::CDefaultConstructor GetDefaultConstructor() const = 0;
	virtual Type::CCopyConstructor    GetCopyConstructor() const = 0;
	virtual Type::CMoveConstructor    GetMoveConstructor() const = 0;

	virtual Type::CDestructor         GetDestructor() const = 0;

	virtual Type::CCopyAssignOperator GetCopyAssignOperator() const = 0;
	virtual Type::CEqualOperator      GetEqualOperator() const = 0;

	virtual bool                      AddConversionOperator(CryTypeId typeId, Type::CConversionOperator conversionOperator) = 0;
	virtual Type::CConversionOperator GetConversionOperator(CryTypeId typeId) = 0;

	// Reflection Helpers
	bool ReflectBaseType(CryTypeId baseTypeId, const SSourceFileInfo& srcInfo)
	{
		return AddBaseType(baseTypeId, srcInfo);
	}

	template<typename FUNCTION_TYPE>
	IFunctionDesc* ReflectFunction(FUNCTION_TYPE funcPtr, const char* szLabel, const CryGUID& guid, const SSourceFileInfo& srcInfo)
	{
		return AddFunction(CMemberFunctionCreator<CFunctionDesc<decltype(funcPtr)>>(funcPtr), szLabel, guid, srcInfo);
	}

	template<typename VARIABLE_TYPE, typename OBJECT_TYPE>
	IVariableDesc* ReflectVariable(VARIABLE_TYPE OBJECT_TYPE::* pVariable, const char* szLabel, const CryGUID& guid, const SSourceFileInfo& srcInfo)
	{
		const CryTypeDesc desc = TypeDescOf<VARIABLE_TYPE>();
		return AddVariable(desc.GetTypeId(), CMemberVariableOffset<VARIABLE_TYPE, OBJECT_TYPE>(pVariable), szLabel, guid, srcInfo);
	}

	IEnumValueDesc* ReflectEnumValue(const char* szLabel, size_t value, char* szDescription)
	{
		return AddEnumValue(szLabel, value, szDescription);
	}

	bool AddConversion(CryTypeId typeId, Type::CConversionOperator& conversionOperator)
	{
		return AddConversionOperator(typeId, conversionOperator);
	}

	// Operators
	bool operator==(const ITypeDesc& rhs) const
	{
		return GetGuid() == rhs.GetGuid();
	}

	bool operator!=(const ITypeDesc& rhs) const
	{
		return GetGuid() != rhs.GetGuid();
	}
};

} // ~Reflection namespace
} // ~Cry namespace
