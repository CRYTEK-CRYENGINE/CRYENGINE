// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Schematyc
{

template<typename TYPE> inline auto ReflectType(CTypeDesc<TYPE> &desc)->decltype(TYPE::ReflectType(std::declval<CTypeDesc<TYPE>&>()), void())
{
	TYPE::ReflectType(desc);
}

namespace Utils
{

template<typename TYPE> inline void EnumToString(IString& output, const void* pInput)
{
	const Serialization::EnumDescription& enumDescription = Serialization::getEnumDescription<TYPE>();
	output.assign(enumDescription.labelByIndex(enumDescription.indexByValue(static_cast<int>(*static_cast<const TYPE*>(pInput)))));
}

inline void ArrayToString(IString& output, const CArrayTypeDesc& typeDesc, const void* pInput)
{
	output.assign("[ ");

	const SArrayOperators& arrayOperators = typeDesc.GetArrayOperators();
	const CCommonTypeDesc& elementTypeDesc = typeDesc.GetElementTypeDesc();
	const STypeOperators& elementOperators = elementTypeDesc.GetOperators();
	if (arrayOperators.size && arrayOperators.atConst && elementOperators.toString)
	{
		const uint32 size = (*arrayOperators.size)(pInput);
		for (uint32 pos = 0; pos < size; ++pos)
		{
			CStackString temp;
			const void* pElement = (*arrayOperators.atConst)(pInput, pos);
			(*elementOperators.toString)(temp, pElement);

			output.append(temp.c_str());
			output.append(", ");
		}
	}

	output.TrimRight(", ");
	output.append(" ]");
}

template<typename TYPE> inline void ArrayToString(IString& output, const void* pInput)
{
	ArrayToString(output, GetTypeDesc<TYPE>(), pInput);
}

class CClassSerializer
{
public:

	inline CClassSerializer(const CClassDesc& typeDesc, void* pValue)
		: m_typeDesc(typeDesc)
		, m_pValue(pValue)
	{}

	inline void Serialize(Serialization::IArchive& archive)
	{
		for (const CClassBaseDesc& base : m_typeDesc.GetBases())
		{
			const CCommonTypeDesc& baseTypeDesc = base.GetTypeDesc();
			const STypeOperators typeOperators = baseTypeDesc.GetOperators();
			if (typeOperators.serialize)
			{
				(*typeOperators.serialize)(archive, static_cast<uint8*>(m_pValue) + base.GetOffset(), baseTypeDesc.GetName().c_str(), baseTypeDesc.GetLabel());
			}
		}

		for (const CClassMemberDesc& memberDesc : m_typeDesc.GetMembers())
		{
			const CCommonTypeDesc& memberTypeDesc = memberDesc.GetTypeDesc();
			const STypeOperators typeOperators = memberTypeDesc.GetOperators();
			if (typeOperators.serialize)
			{
				(*typeOperators.serialize)(archive, static_cast<uint8*>(m_pValue) + memberDesc.GetOffset(), memberDesc.GetName(), memberDesc.GetLabel());
			}
		}
	}

private:

	const CClassDesc& m_typeDesc;
	void*             m_pValue = nullptr;
};

inline bool SerializeClass(Serialization::IArchive& archive, const CClassDesc& typeDesc, void* pValue, const char* szName, const char* szLabel)
{
	return archive(CClassSerializer(typeDesc, pValue), szName, szLabel);
}

template<typename TYPE> inline bool SerializeClass(Serialization::IArchive& archive, void* pValue, const char* szName, const char* szLabel)
{
	return SerializeClass(archive, GetTypeDesc<TYPE>(), pValue, szName, szLabel);
}

inline void ClassToString(IString& output, const CClassDesc& typeDesc, const void* pInput)
{
	output.assign("{ ");

	for (const CClassBaseDesc& base : typeDesc.GetBases())
	{
		const CCommonTypeDesc& baseTypeDesc = base.GetTypeDesc();
		const STypeOperators typeOperators = baseTypeDesc.GetOperators();
		if (typeOperators.toString)
		{
			CStackString temp;
			(*typeOperators.toString)(temp, static_cast<const uint8*>(pInput) + base.GetOffset());

			output.append(temp.c_str());
			output.append(", ");
		}
	}

	for (const CClassMemberDesc& memberDesc : typeDesc.GetMembers())
	{
		const CCommonTypeDesc& memberTypeDesc = memberDesc.GetTypeDesc();
		const STypeOperators typeOperators = memberTypeDesc.GetOperators();
		if (typeOperators.toString)
		{
			CStackString temp;
			(*typeOperators.toString)(temp, static_cast<const uint8*>(pInput) + memberDesc.GetOffset());

			if (memberTypeDesc.GetCategory() == ETypeCategory::String)
			{
				output.append("'");
				output.append(temp.c_str());
				output.append("', ");
			}
			else
			{
				output.append(temp.c_str());
				output.append(", ");
			}
		}
	}

	output.TrimRight(", ");
	output.append(" }");
}

template<typename TYPE> inline void ClassToString(IString& output, const void* pInput)
{
	ClassToString(output, GetTypeDesc<TYPE>(), pInput);
}

} // Utils

inline CCommonTypeDesc::CCommonTypeDesc() {}

inline CCommonTypeDesc::CCommonTypeDesc(ETypeCategory category)
	: m_category(category)
{}

inline ETypeCategory CCommonTypeDesc::GetCategory() const
{
	return m_category;
}

inline void CCommonTypeDesc::SetName(const CTypeName& name)
{
	m_name = name;
}

inline const CTypeName& CCommonTypeDesc::GetName() const
{
	return m_name;
}

inline void CCommonTypeDesc::SetSize(size_t size)
{
	m_size = size;
}

inline size_t CCommonTypeDesc::GetSize() const
{
	return m_size;
}

inline void CCommonTypeDesc::SetGUID(const CryGUID& guid)
{
	m_guid = guid;
}

inline const CryGUID& CCommonTypeDesc::GetGUID() const
{
	return m_guid;
}

inline void CCommonTypeDesc::SetLabel(const char* szLabel)
{
	m_szLabel = szLabel;
}

inline const char* CCommonTypeDesc::GetLabel() const
{
	return m_szLabel ? m_szLabel : m_name.c_str();
}

inline void CCommonTypeDesc::SetDescription(const char* szDescription)
{
	m_szDescription = szDescription;
}

inline const char* CCommonTypeDesc::GetDescription() const
{
	return m_szDescription;
}

inline void CCommonTypeDesc::SetFlags(const TypeFlags& flags)
{
	m_flags = flags;
}

inline const TypeFlags& CCommonTypeDesc::GetFlags() const
{
	return m_flags;
}

inline const STypeOperators& CCommonTypeDesc::GetOperators() const
{
	return m_operators;
}

inline const void* CCommonTypeDesc::GetDefaultValue() const
{
	return m_pDefaultValue ? m_pDefaultValue->Get() : nullptr;
}

inline bool CCommonTypeDesc::operator==(const CCommonTypeDesc& rhs) const
{
	return m_guid == rhs.m_guid;
}

inline bool CCommonTypeDesc::operator!=(const CCommonTypeDesc& rhs) const
{
	return m_guid != rhs.m_guid;
}

inline CIntegralTypeDesc::CIntegralTypeDesc()
	: CCommonTypeDesc(ETypeCategory::Integral)
{}

inline CFloatingPointTypeDesc::CFloatingPointTypeDesc()
	: CCommonTypeDesc(ETypeCategory::FloatingPoint)
{}

inline CEnumTypeDesc::CEnumTypeDesc()
	: CCommonTypeDesc(ETypeCategory::Enum)
{}

inline CStringTypeDesc::CStringTypeDesc()
	: CCommonTypeDesc(ETypeCategory::String)
{}

inline const SStringOperators& CStringTypeDesc::GetStringOperators() const
{
	return m_stringOperators;
}

inline CArrayTypeDesc::CArrayTypeDesc()
	: CCommonTypeDesc(ETypeCategory::Array)
{}

inline const CCommonTypeDesc& CArrayTypeDesc::GetElementTypeDesc() const
{
	CRY_ASSERT(m_pElementTypeDesc);
	return *m_pElementTypeDesc;
}

inline const SArrayOperators& CArrayTypeDesc::GetArrayOperators() const
{
	return m_arrayOperators;
}

inline CClassBaseDesc::CClassBaseDesc(const CCommonTypeDesc& typeDesc, ptrdiff_t offset)
	: m_typeDesc(typeDesc)
	, m_offset(offset)
{}

inline const CCommonTypeDesc& CClassBaseDesc::GetTypeDesc() const
{
	return m_typeDesc;
}

inline ptrdiff_t CClassBaseDesc::GetOffset() const
{
	return m_offset;
}

inline CClassMemberDesc::CClassMemberDesc(const CCommonTypeDesc& typeDesc, ptrdiff_t offset, uint32 id, const char* szName, const char* szLabel, const char* szDescription, Utils::IDefaultValuePtr&& pDefaultValue)
	: m_typeDesc(typeDesc)
	, m_offset(offset)
	, m_id(id)
	, m_szName(szName)
	, m_szLabel(szLabel)
	, m_szDescription(szDescription)
	, m_pDefaultValue(std::forward<Utils::IDefaultValuePtr>(pDefaultValue))
{}

inline const CCommonTypeDesc& CClassMemberDesc::GetTypeDesc() const
{
	return m_typeDesc;
}

inline ptrdiff_t CClassMemberDesc::GetOffset() const
{
	return m_offset;
}

inline uint32 CClassMemberDesc::GetId() const
{
	return m_id;
}

inline const char* CClassMemberDesc::GetName() const
{
	return m_szName;
}

inline const char* CClassMemberDesc::GetLabel() const
{
	return m_szLabel ? m_szLabel : m_szName;
}

inline const char* CClassMemberDesc::GetDescription() const
{
	return m_szDescription;
}

inline const void* CClassMemberDesc::GetDefaultValue() const
{
	return m_pDefaultValue ? m_pDefaultValue->Get() : m_typeDesc.GetDefaultValue();
}

inline CClassDesc::CClassDesc()
	: CCommonTypeDesc(ETypeCategory::Class)
{}

inline const CClassBaseDescArray& CClassDesc::GetBases() const
{
	return m_bases;
}

inline const CClassBaseDesc* CClassDesc::FindBaseByTypeDesc(const CCommonTypeDesc& typeDesc) const
{
	for (const CClassBaseDesc& base : m_bases)
	{
		if (base.GetTypeDesc() == typeDesc)
		{
			return &base;
		}
	}
	return nullptr;
}

inline const CClassBaseDesc* CClassDesc::FindBaseByTypeID(const CryGUID& typeId) const
{
	for (const CClassBaseDesc& base : m_bases)
	{
		if (base.GetTypeDesc().GetGUID() == typeId)
		{
			return &base;
		}
	}
	return nullptr;
}

inline const CClassMemberDescArray& CClassDesc::GetMembers() const
{
	return m_members;
}

inline const CClassMemberDesc* CClassDesc::FindMemberByOffset(ptrdiff_t offset) const
{
	for (const CClassMemberDesc& member : m_members)
	{
		if (member.GetOffset() == offset)
		{
			return &member;
		}
	}
	return nullptr;
}

inline const CClassMemberDesc* CClassDesc::FindMemberById(uint32 id) const
{
	for (const CClassMemberDesc& member : m_members)
	{
		if (member.GetId() == id)
		{
			return &member;
		}
	}
	return nullptr;
}

inline const CClassMemberDesc* CClassDesc::FindMemberByName(const char* szName) const
{
	for (const CClassMemberDesc& member : m_members)
	{
		if (strcmp(member.GetName(), szName) == 0)
		{
			return &member;
		}
	}
	return nullptr;
}

// Convert member id 
inline uint32 CClassDesc::FindMemberIndexById(uint32 id) const
{
	for (size_t i = 0, num = m_members.size(); i < num; ++i)
	{
		if (m_members[i].GetId() == id)
			return i;
	}
	return ~0u;
}


inline CClassDesc::CClassDesc(ETypeCategory category)
	: CCommonTypeDesc(category)
{}

inline bool CClassDesc::AddBase(const CCommonTypeDesc& typeDesc, ptrdiff_t offset)
{
	const bool bDuplicateTypeDesc = FindBaseByTypeDesc(typeDesc) != nullptr;
	CRY_ASSERT(!bDuplicateTypeDesc);
	if (bDuplicateTypeDesc)
	{
		return false;
	}

	m_bases.emplace_back(typeDesc, offset);
	return true;
}

inline CClassMemberDesc& CClassDesc::AddMember(const CCommonTypeDesc& typeDesc, ptrdiff_t offset, uint32 id, const char* szName, const char* szLabel, const char* szDescription, Utils::IDefaultValuePtr&& pDefaultValue)
{
	CClassMemberDesc* pNullMember = nullptr;

	const bool bDuplicateOffset = FindMemberByOffset(offset) != nullptr;
	CRY_ASSERT(!bDuplicateOffset);
	if (bDuplicateOffset)
	{
		// Returns NULL reference on purpose
		return *pNullMember;
	}

	const bool bDuplicateId = FindMemberById(id) != nullptr;
	CRY_ASSERT(!bDuplicateId);
	if (bDuplicateId)
	{
		// Returns NULL reference on purpose
		return *pNullMember;
	}

	const bool bDuplicateName = FindMemberByName(szName) != nullptr;
	CRY_ASSERT(!bDuplicateName);
	if (bDuplicateName)
	{
		// Returns NULL reference on purpose
		return *pNullMember;
	}

	m_members.emplace_back(typeDesc, offset, id, szName, szLabel, szDescription, std::forward<Utils::IDefaultValuePtr>(pDefaultValue));
	return m_members.back();
}

inline CCustomClassDesc::CCustomClassDesc()
	: CClassDesc(ETypeCategory::CustomClass)
{}

namespace Helpers
{

template<typename TYPE> constexpr auto IsReflectedType(int)->decltype(ReflectType(std::declval<CTypeDesc<TYPE>&>()), bool())
{
	return true;
}

template<typename TYPE> constexpr bool IsReflectedType(...)
{
	return false;
}

template<typename TYPE> class CAppliedTypeDesc : public CTypeDesc<TYPE>
{
public:

	typedef CTypeDesc<TYPE> TypeDesc;

public:

	inline CAppliedTypeDesc()
	{
		TypeDesc::Apply();
		ReflectType(static_cast<TypeDesc&>(*this));
	}
};

template<typename TYPE> class CSimpleTypeDesc : public CTypeDesc<TYPE>
{
public:
	inline CSimpleTypeDesc()
	{
		ReflectType(static_cast<CTypeDesc<TYPE>&>(*this));
	}
};

} // Helpers

template<typename TYPE> constexpr bool IsReflectedType()
{
	return Helpers::IsReflectedType<TYPE>(0);
}

template<typename TYPE> inline const CTypeDesc<TYPE>& GetTypeDesc()
{
	SCHEMATYC_VERIFY_TYPE_IS_REFLECTED(TYPE);

	static const Helpers::CAppliedTypeDesc<TYPE> s_desc;
	return s_desc;
}

template<typename TYPE> inline CryGUID GetTypeGUID()
{
	SCHEMATYC_VERIFY_TYPE_IS_REFLECTED(TYPE);

	static const Helpers::CSimpleTypeDesc<TYPE> s_desc;
	return s_desc.GetGUID();
}

} // Schematyc
