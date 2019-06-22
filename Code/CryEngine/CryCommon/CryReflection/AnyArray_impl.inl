// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <CryReflection/Any.h>
#include <CryReflection/IModule.h>
#include <CryReflection/ITypeDesc.h>

#include <CryString/StringUtils.h>

namespace Cry {
namespace Reflection {

inline CAnyArray::CAnyArray()
	: m_isPointer(false)
	, m_isConst(false)
{

}

inline CAnyArray::~CAnyArray()
{
	DestructAll();
}

inline CAnyArray::CAnyArray(const CAny& any)
{
	m_typeIndex = any.GetTypeIndex();
	m_isPointer = any.IsPointer();
	m_isConst = any.IsConst();
}

inline CAnyArray::CAnyArray(TypeIndex typeIndex, bool isPointer)
	: CAnyArray()
{
	SetType(typeIndex, isPointer);
}

inline CAnyArray::CAnyArray(const ITypeDesc& typeDesc, bool isPointer)
	: CAnyArray()
{
	SetType(typeDesc, isPointer);
}

inline void CAnyArray::SetType(TypeIndex typeIndex, bool isPointer)
{
	if (m_pData == nullptr)
	{
		m_typeIndex = typeIndex;
		m_isPointer = isPointer;
		return;
	}
	CRY_ASSERT_MESSAGE(m_pData == nullptr, "Array already uses type index %i.", m_typeIndex.ToValueType());
}

inline void CAnyArray::SetType(const ITypeDesc& typeDesc, bool isPointer)
{
	if (m_pData == nullptr)
	{
		m_typeIndex = typeDesc.GetIndex();
		m_isPointer = isPointer;
		return;
	}
	CRY_ASSERT_MESSAGE(m_pData == nullptr, "Array already uses type index %i.", m_typeIndex.ToValueType());
}

inline const ITypeDesc* CAnyArray::GetTypeDesc() const
{
	if (m_typeIndex.IsValid())
	{
		const ITypeDesc* pTypeDesc = CoreRegistry::GetTypeByIndex(m_typeIndex);
		if (pTypeDesc)
		{
			return pTypeDesc;
		}
		CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
	}
	CRY_ASSERT_MESSAGE(m_typeIndex.IsValid(), "Type index is not valid.");

	return nullptr;
}

inline void CAnyArray::Resize(uint32 size)
{
	const size_t totalSize = GetAlignedSize() * size;
	if (m_pData == nullptr)
	{
		m_size = size;
		m_pData = reinterpret_cast<unsigned char*>(CryModuleMalloc(totalSize));
		if (m_isPointer == false)
		{
			CallDefaultConstructors(0, size);
		}
	}
	else // m_pData
	{
		if (size <= m_size)
		{
			if (m_isPointer == false)
			{
				CallDestructors(size, m_size);
			}
		}
		else // size > m_size
		{
			if (size > m_capacity)
			{
				m_pData = reinterpret_cast<unsigned char*>(CryModuleRealloc(static_cast<void*>(m_pData), totalSize));
				m_capacity = size;
			}

			if (m_isPointer == false)
			{
				CallDefaultConstructors(m_size, size);
			}
		}
		m_size = size;
	}
}

inline void CAnyArray::Reserve(uint32 capacity)
{
	if (capacity > m_capacity)
	{
		const size_t totalSize = GetAlignedSize() * capacity;
		if (m_pData)
		{
			m_pData = reinterpret_cast<unsigned char*>(CryModuleRealloc(static_cast<void*>(m_pData), totalSize));
		}
		else
		{
			m_pData = reinterpret_cast<unsigned char*>(CryModuleMalloc(totalSize));
		}
		m_capacity = capacity;
	}
}

template<typename VALUE_TYPE>
inline void CAnyArray::AddValue(const VALUE_TYPE& source)
{
	constexpr bool isPointer = std::is_pointer<decltype(source)>::value;
	static_assert(isPointer == false, "Trying to add pointer type 'source' to value array.");

	if (m_isPointer == false)
	{
		constexpr CTypeId typeId = Type::IdOf<VALUE_TYPE>();
		const ITypeDesc* pTypeDesc = CoreRegistry::GetTypeById(typeId);
		if (pTypeDesc)
		{
			AddValueInternal(&source, *pTypeDesc);
			return;
		}
		CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
	}
	CRY_ASSERT_MESSAGE(m_isPointer == false, "Can't add value type 'source' to pointer array.");
}

inline void CAnyArray::AddValue(const CAny& source)
{
	if ((m_isPointer == false) && (source.IsEmpty() == false))
	{
		if (source.IsPointer() == false)
		{
			AddValueInternal(source.GetConstPointer(), *source.GetTypeDesc());
			return;
		}
		CRY_ASSERT_MESSAGE(source.IsPointer() == false, "Trying to add pointer type 'source' to value array.");
	}
	CRY_ASSERT_MESSAGE(m_isPointer == false, "Can't add value type 'source' to pointer array.");
	CRY_ASSERT_MESSAGE(source.IsEmpty(), "Parameter 'source' is empty.");
}

inline void CAnyArray::AddValue(const void* pSource, TypeIndex typeIndex)
{
	if (pSource && m_isPointer == false)
	{
		const ITypeDesc* pTypeDesc = CoreRegistry::GetTypeByIndex(typeIndex);
		if (pTypeDesc)
		{
			AddValueInternal(pSource, *pTypeDesc);
			return;
		}
		CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
	}
	CRY_ASSERT_MESSAGE(pSource, "Input 'pSource' must be non-null.");
	CRY_ASSERT_MESSAGE(m_isPointer == false, "Can't add value type 'source' to pointer array.");
}

inline void CAnyArray::AddValue(const void* pSource, const ITypeDesc& typeDesc)
{
	if (pSource && m_isPointer == false)
	{
		AddValueInternal(pSource, typeDesc);
		return;
	}
	CRY_ASSERT_MESSAGE(pSource, "Input 'pSource' must be non-null.");
	CRY_ASSERT_MESSAGE(m_isPointer == false, "Can't add value type 'source' to pointer array.");
}

template<typename VALUE_TYPE>
inline void CAnyArray::AddPointer(VALUE_TYPE* pPointer)
{
	constexpr bool isPointer = std::is_pointer<decltype(pPointer)>::value;
	static_assert(isPointer, "Trying to add value type 'pPointer' to pointer array.");

	if (m_isPointer)
	{
		constexpr CTypeId typeId = Type::IdOf<VALUE_TYPE>();
		const ITypeDesc* pTypeDesc = CoreRegistry::GetTypeById(typeId);
		if (pTypeDesc)
		{
			AddPointerInternal(static_cast<void*>(pPointer), *pTypeDesc);
			return;
		}
		CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
	}
	CRY_ASSERT_MESSAGE(m_isPointer, "Can't add pointer 'pPointer' to value array.");
}

inline void CAnyArray::AddPointer(const CAny& source)
{
	if ((m_isPointer) && (source.IsEmpty() == false))
	{
		if (source.IsPointer())
		{
			AddPointerInternal(source.GetConstPointer(), *source.GetTypeDesc());
			return;
		}
		CRY_ASSERT_MESSAGE(source.IsPointer(), "Trying to add value type 'source' to pointer array.");
	}
	CRY_ASSERT_MESSAGE(m_isPointer, "Can't add pointer type 'source' to value array.");
	CRY_ASSERT_MESSAGE(source.IsEmpty(), "Parameter 'source' is empty.");
}

inline void CAnyArray::AddPointer(const void* pPointer, TypeIndex typeIndex)
{
	if (m_isPointer)
	{
		const ITypeDesc* pTypeDesc = CoreRegistry::GetTypeByIndex(typeIndex);
		if (pTypeDesc)
		{
			AddPointerInternal(pPointer, *pTypeDesc);
			return;
		}
		CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
	}
	CRY_ASSERT_MESSAGE(m_isPointer, "Can't assign pointer 'pPointer' to value array.");
}

inline void CAnyArray::AddPointer(const void* pPointer, const ITypeDesc& typeDesc)
{
	if (m_isPointer)
	{
		AddPointerInternal(pPointer, typeDesc);
		return;
	}
	CRY_ASSERT_MESSAGE(m_isPointer, "Can't assign pointer 'pPointer' to value array.");
}

template<typename VALUE_TYPE>
inline void CAnyArray::InsertValueAt(uint32 index, const VALUE_TYPE& source)
{
	constexpr bool isPointer = std::is_pointer<decltype(source)>::value;
	static_assert(isPointer == false, "Trying to add pointer type 'source' to value array.");

	if (m_isPointer == false)
	{
		constexpr CTypeId typeId = Type::IdOf<VALUE_TYPE>();
		const ITypeDesc* pTypeDesc = CoreRegistry::GetTypeById(typeId);
		if (pTypeDesc)
		{
			ShiftRightInternal(index);
			AssignCopyValueAt(index, &source, *pTypeDesc);
			return;
		}
		CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
	}
	CRY_ASSERT_MESSAGE(m_isPointer == false, "Can't add value type 'source' to pointer array.");
}

inline void CAnyArray::InsertValueAt(uint32 index, CAny& source)
{
	if ((m_isPointer == false) && (source.IsEmpty() == false))
	{
		if (source.IsPointer() == false)
		{
			ShiftRightInternal(index);
			AssignCopyValueAt(index, source.GetConstPointer(), *source.GetTypeDesc());
			return;
		}
		CRY_ASSERT_MESSAGE(source.IsPointer() == false, "Trying to add pointer type 'source' to value array.");
	}
	CRY_ASSERT_MESSAGE(m_isPointer == false, "Can't add value type 'source' to pointer array.");
	CRY_ASSERT_MESSAGE(source.IsEmpty(), "Parameter 'source' is empty.");
}

inline void CAnyArray::InsertValueAt(uint32 index, const void* pSource, TypeIndex typeIndex)
{
	if (pSource && m_isPointer == false)
	{
		const ITypeDesc* pTypeDesc = CoreRegistry::GetTypeByIndex(typeIndex);
		if (pTypeDesc)
		{
			ShiftRightInternal(index);
			AssignCopyValueAt(index, pSource, *pTypeDesc);
			return;
		}
		CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
	}
	CRY_ASSERT_MESSAGE(pSource, "Input 'pSource' must be non-null.");
	CRY_ASSERT_MESSAGE(m_isPointer == false, "Can't add value type 'source' to pointer array.");
}

inline void CAnyArray::InsertValueAt(uint32 index, const void* pSource, const ITypeDesc& typeDesc)
{
	if (pSource && m_isPointer == false)
	{
		ShiftRightInternal(index);
		AssignCopyValueAt(index, pSource, typeDesc);
		return;
	}
	CRY_ASSERT_MESSAGE(pSource, "Input 'pSource' must be non-null.");
	CRY_ASSERT_MESSAGE(m_isPointer == false, "Can't add value type 'source' to pointer array.");
}

template<typename VALUE_TYPE>
inline void CAnyArray::InsertPointerAt(uint32 index, const VALUE_TYPE* pPointer)
{
	constexpr bool isPointer = std::is_pointer<decltype(pPointer)>::value;
	static_assert(isPointer, "Trying to add value type 'pPointer' to pointer array.");

	if (m_isPointer)
	{
		constexpr CTypeId typeId = Type::IdOf<VALUE_TYPE>();
		const ITypeDesc* pTypeDesc = CoreRegistry::GetTypeById(typeId);
		if (pTypeDesc)
		{
			ShiftRightInternal(index);
			AssignPointerAt(index, pPointer, *pTypeDesc);
			return;
		}
		CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
	}
	CRY_ASSERT_MESSAGE(m_isPointer, "Can't add pointer 'pPointer' to value array.");
}

inline void CAnyArray::InsertPointerAt(uint32 index, const CAny& source)
{
	if ((m_isPointer) && (source.IsEmpty() == false))
	{
		if (source.IsPointer())
		{
			ShiftRightInternal(index);
			AssignPointerAt(index, source.GetConstPointer(), *source.GetTypeDesc());
			return;
		}
		CRY_ASSERT_MESSAGE(source.IsPointer(), "Trying to add value type 'source' to pointer array.");
	}
	CRY_ASSERT_MESSAGE(m_isPointer, "Can't add pointer type 'source' to value array.");
	CRY_ASSERT_MESSAGE(source.IsEmpty() == false, "Parameter 'source' is empty.");
}

inline void CAnyArray::InsertPointerAt(uint32 index, const void* pPointer, TypeIndex typeIndex)
{
	if (m_isPointer)
	{
		const ITypeDesc* pTypeDesc = CoreRegistry::GetTypeByIndex(typeIndex);
		if (pTypeDesc)
		{
			ShiftRightInternal(index);
			AssignPointerAt(index, pPointer, *pTypeDesc);
			return;
		}
		CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
	}
	CRY_ASSERT_MESSAGE(m_isPointer, "Can't assign pointer 'pPointer' to value array.");
}

inline void CAnyArray::InsertPointerAt(uint32 index, const void* pPointer, const ITypeDesc& typeDesc)
{
	if (m_isPointer)
	{
		ShiftRightInternal(index);
		AssignPointerAt(index, pPointer, typeDesc);
		return;
	}
	CRY_ASSERT_MESSAGE(m_isPointer, "Can't assign pointer 'pPointer' to value array.");
}

template<typename VALUE_TYPE>
inline void CAnyArray::AssignCopyValueAt(uint32 index, const VALUE_TYPE& source)
{
	constexpr bool isPointer = std::is_pointer<decltype(source)>::value;
	static_assert(isPointer == false, "Trying to assign pointer type 'source' to value array.");

	if ((m_isConst == false) && (m_isPointer == false) && (index < m_size))
	{
		constexpr CTypeId typeId = Type::IdOf<VALUE_TYPE>();
		const Reflection::ITypeDesc* pTypeDesc = CoreRegistry::GetTypeById(typeId);
		if (pTypeDesc && (m_typeIndex == pTypeDesc->GetIndex()))
		{
			Type::CCopyAssignOperator copyAssignOperator = pTypeDesc->GetCopyAssignOperator();
			if (copyAssignOperator)
			{
				copyAssignOperator(GetPointerInternal(index), &source);
				return;
			}
			CRY_ASSERT_MESSAGE(copyAssignOperator, "'%s' has no copy assign operator.", pTypeDesc->GetFullQualifiedName());
		}
		CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
		CRY_ASSERT_MESSAGE(m_typeIndex == pTypeDesc->GetIndex(), "'source' doesn't match the array type.");
	}
	CRY_ASSERT_MESSAGE(m_isConst == false, "Array elements are const and can't be changed.");
	CRY_ASSERT_MESSAGE(m_isPointer == false, "Can't assign value 'source' to pointer array.");
	CRY_ASSERT_MESSAGE(index < m_size, "'index' (%i) is out of bounds (%i)", index, m_size);
}

inline void CAnyArray::AssignCopyValueAt(uint32 index, const CAny& source)
{
	if ((m_isConst == false) && (m_isPointer == false) && (index < m_size))
	{
		if ((source.IsEmpty() == false) && (source.IsPointer() == false))
		{
			const Reflection::ITypeDesc* pTypeDesc = source.GetTypeDesc();
			if (pTypeDesc && (m_typeIndex == pTypeDesc->GetIndex()))
			{
				Type::CCopyAssignOperator copyAssignOperator = pTypeDesc->GetCopyAssignOperator();
				if (copyAssignOperator)
				{
					copyAssignOperator(GetPointerInternal(index), source.GetConstPointer());
					return;
				}
				CRY_ASSERT_MESSAGE(copyAssignOperator, "'%s' has no copy assign operator.", pTypeDesc->GetFullQualifiedName());
			}
			CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
			CRY_ASSERT_MESSAGE(m_typeIndex == pTypeDesc->GetIndex(), "'source' doesn't match the array type.");
		}
		CRY_ASSERT_MESSAGE(source.IsPointer() == false, "'source' is pointer type.");
		CRY_ASSERT_MESSAGE(source.IsEmpty(), "'source' is empty.");
	}
	CRY_ASSERT_MESSAGE(m_isConst == false, "Array elements are const and can't be changed.");
	CRY_ASSERT_MESSAGE(m_isPointer == false, "Can't assign value 'source' to pointer array.");
	CRY_ASSERT_MESSAGE(index < m_size, "'index' (%i) is out of bounds (%i).", index, m_size);
}

inline void CAnyArray::AssignCopyValueAt(uint32 index, const void* pSource, TypeIndex typeIndex)
{
	if (pSource && (m_isConst == false) && (m_isPointer == false) && (index < m_size))
	{
		const Reflection::ITypeDesc* pTypeDesc = CoreRegistry::GetTypeByIndex(typeIndex);
		if (pTypeDesc && (m_typeIndex == pTypeDesc->GetIndex()))
		{
			Type::CCopyAssignOperator copyAssignOperator = pTypeDesc->GetCopyAssignOperator();
			if (copyAssignOperator)
			{
				copyAssignOperator(GetPointerInternal(index), pSource);
				return;
			}
			CRY_ASSERT_MESSAGE(copyAssignOperator, "Type '%s' has no copy assign operator.", pTypeDesc->GetFullQualifiedName());
		}
		CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
		CRY_ASSERT_MESSAGE(m_typeIndex == pTypeDesc->GetIndex(), "'typeIndex' doesn't match the array type.");
	}
	CRY_ASSERT_MESSAGE(pSource, "Input 'pSource' must be non-null.");
	CRY_ASSERT_MESSAGE(m_isConst == false, "Array elements are const and can't be changed.");
	CRY_ASSERT_MESSAGE(m_isPointer == false, "Can't assign value 'pSource' to pointer array.");
	CRY_ASSERT_MESSAGE(index < m_size, "'index' (%i) is out of bounds (%i).", index, m_size);
}

inline void CAnyArray::AssignCopyValueAt(uint32 index, const void* pSource, const ITypeDesc& typeDesc)
{
	if (pSource && (m_isConst == false) && (m_isPointer == false) && (index < m_size))
	{
		if (m_typeIndex == typeDesc.GetIndex())
		{
			Type::CCopyAssignOperator copyAssignOperator = typeDesc.GetCopyAssignOperator();
			if (copyAssignOperator)
			{
				copyAssignOperator(GetPointerInternal(index), pSource);
				return;
			}
			CRY_ASSERT_MESSAGE(copyAssignOperator, "Type '%s' has no copy assign operator.", typeDesc.GetFullQualifiedName());
		}
		CRY_ASSERT_MESSAGE(m_typeIndex == typeDesc.GetIndex(), "'typeDesc' doesn't match the array type.");
	}
	CRY_ASSERT_MESSAGE(pSource, "Input 'pSource' must be non-null.");
	CRY_ASSERT_MESSAGE(m_isConst == false, "Array elements are const and can't be changed.");
	CRY_ASSERT_MESSAGE(m_isPointer == false, "Can't assign value 'pSource' to pointer array.");
	CRY_ASSERT_MESSAGE(index < m_size, "'index' (%i) is out of bounds (%i).", index, m_size);
}

template<typename VALUE_TYPE>
inline void CAnyArray::AssignMoveValueAt(uint32 index, VALUE_TYPE& source)
{
	constexpr bool isPointer = std::is_pointer<decltype(source)>::value;
	static_assert(isPointer == false, "Trying to assign pointer type 'source' to value array.");

	if ((m_isConst == false) && (m_isPointer == false) && (index < m_size))
	{
		const Cry::Reflection::ITypeDesc* pTypeDesc = CoreRegistry::GetTypeById(Type::IdOf<VALUE_TYPE>());
		if (pTypeDesc && (m_typeIndex == pTypeDesc->GetIndex()))
		{
			Type::CMoveAssignOperator moveAssignOperator = pTypeDesc->GetMoveAssignOperator();
			if (moveAssignOperator)
			{
				moveAssignOperator(GetPointerInternal(index), &source);
				return;
			}
			CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
			CRY_ASSERT_MESSAGE(moveAssignOperator, "Type '%s' has no move assign operator.", pTypeDesc->GetFullQualifiedName());
		}
		CRY_ASSERT_MESSAGE(m_typeIndex == pTypeDesc->GetIndex(), "Type doesn't match the array type.");
	}
	CRY_ASSERT_MESSAGE(m_isConst == false, "Array elements are const and can't be changed.");
	CRY_ASSERT_MESSAGE(m_isPointer == false, "Can't assign value 'pSource' to pointer array.");
	CRY_ASSERT_MESSAGE(index < m_size, "'index' (%i) is out of bounds (%i).", index, m_size);
}

inline void CAnyArray::AssignMoveValueAt(uint32 index, CAny& source)
{
	if ((m_isConst == false) && (m_isPointer == false) && (index < m_size))
	{
		if ((source.IsEmpty() == false) && (source.IsPointer() == false) && (source.IsConst() == false))
		{
			const Reflection::ITypeDesc* pTypeDesc = source.GetTypeDesc();
			if (pTypeDesc && (m_typeIndex == pTypeDesc->GetIndex()))
			{
				Type::CMoveAssignOperator moveAssignOperator = pTypeDesc->GetMoveAssignOperator();
				if (moveAssignOperator)
				{
					moveAssignOperator(GetPointerInternal(index), source.GetPointer());
					return;
				}
				CRY_ASSERT_MESSAGE(moveAssignOperator, "'%s' has no move assign operator.", pTypeDesc->GetFullQualifiedName());
			}
			CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
			CRY_ASSERT_MESSAGE(m_typeIndex == pTypeDesc->GetIndex(), "'source' doesn't match the array type.");
		}
		CRY_ASSERT_MESSAGE(source.IsPointer() == false, "'source' is pointer type.");
		CRY_ASSERT_MESSAGE(source.IsEmpty(), "'source' is empty.");
		CRY_ASSERT_MESSAGE(source.IsConst() == false, "'source' is const and can't be moved.");
	}
	CRY_ASSERT_MESSAGE(m_isConst == false, "Array elements are const and can't be changed.");
	CRY_ASSERT_MESSAGE(m_isPointer == false, "Can't assign value 'source' to pointer array.");
	CRY_ASSERT_MESSAGE(index < m_size, "'index' (%i) is out of bounds (%i).", index, m_size);
}

inline void CAnyArray::AssignMoveValueAt(uint32 index, void* pSource, TypeIndex typeIndex)
{
	if (pSource && (m_isConst == false) && (m_isPointer == false) && (index < m_size))
	{
		const Cry::Reflection::ITypeDesc* pTypeDesc = CoreRegistry::GetTypeByIndex(typeIndex);
		if (pTypeDesc && (m_typeIndex == pTypeDesc->GetIndex()))
		{
			Type::CMoveAssignOperator moveAssignOperator = pTypeDesc->GetMoveAssignOperator();
			if (moveAssignOperator)
			{
				moveAssignOperator(GetPointerInternal(index), pSource);
				return;
			}
			CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
			CRY_ASSERT_MESSAGE(moveAssignOperator, "Type '%s' has no move assign operator.", pTypeDesc->GetFullQualifiedName());
		}
		CRY_ASSERT_MESSAGE(m_typeIndex == pTypeDesc->GetIndex(), "'typeIndex' doesn't match the array type.");
	}
	CRY_ASSERT_MESSAGE(pSource, "Input 'pSource' must be non-null.");
	CRY_ASSERT_MESSAGE(m_isConst == false, "Array elements are const and can't be changed.");
	CRY_ASSERT_MESSAGE(m_isPointer == false, "Can't assign value 'pSource' to pointer array.");
	CRY_ASSERT_MESSAGE(index < m_size, "'index' (%i) is out of bounds (%i).", index, m_size);
}

inline void CAnyArray::AssignMoveValueAt(uint32 index, void* pSource, const ITypeDesc& typeDesc)
{
	if (pSource && (m_isConst == false) && (m_isPointer == false) && (index < m_size))
	{
		if (m_typeIndex == typeDesc.GetIndex())
		{
			Type::CMoveAssignOperator moveAssignOperator = typeDesc.GetMoveAssignOperator();
			if (moveAssignOperator)
			{
				moveAssignOperator(GetPointerInternal(index), pSource);
				return;
			}
			CRY_ASSERT_MESSAGE(moveAssignOperator, "Type '%s' has no move assign operator.", typeDesc.GetFullQualifiedName());
		}
		CRY_ASSERT_MESSAGE(m_typeIndex == typeDesc.GetIndex(), "'typeDesc' doesn't match the array type.");
	}
	CRY_ASSERT_MESSAGE(pSource, "Input 'pSource' must be non-null.");
	CRY_ASSERT_MESSAGE(m_isConst == false, "Array elements are const and can't be changed.");
	CRY_ASSERT_MESSAGE(m_isPointer == false, "Can't assign value 'pSource' to pointer array.");
	CRY_ASSERT_MESSAGE(index < m_size, "'index' (%i) is out of bounds (%i).", index, m_size);
}

template<typename VALUE_TYPE>
inline void CAnyArray::AssignPointerAt(uint32 index, const VALUE_TYPE* pPointer)
{
	constexpr bool isPointer = std::is_pointer<decltype(pPointer)>::value;
	static_assert(isPointer, "Trying to assign value type 'pPointer' to pointer array.");

	if ((m_isConst == false) && m_isPointer)
	{
		constexpr CTypeId typeId = Type::IdOf<VALUE_TYPE>();
		const ITypeDesc* pTypeDesc = CoreRegistry::GetTypeById(typeId);
		if (pTypeDesc)
		{
			AssignPointerInternal(index, static_cast<const void*>(pPointer), *pTypeDesc);
			return;
		}
		CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
	}
	CRY_ASSERT_MESSAGE(m_isPointer, "Can't assign pointer 'pPointer' to value array.");
	CRY_ASSERT_MESSAGE(m_isConst == false, "Array elements are const and can't be changed.");
}

inline void CAnyArray::AssignPointerAt(uint32 index, const CAny& source)
{
	if ((m_isConst == false) && m_isPointer && source.IsPointer())
	{
		const ITypeDesc* pTypeDesc = source.GetTypeDesc();
		if (pTypeDesc)
		{
			AssignPointerInternal(index, source.GetConstPointer(), *pTypeDesc);
			return;
		}
		CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
	}
	CRY_ASSERT_MESSAGE(m_isPointer, "Can't assign pointer 'source' to value array.");
	CRY_ASSERT_MESSAGE(source.IsPointer(), "Trying to add value type 'source' to pointer array.");
	CRY_ASSERT_MESSAGE(m_isConst == false, "Array elements are const and can't be changed.");
}

inline void CAnyArray::AssignPointerAt(uint32 index, const void* pPointer, TypeIndex typeIndex)
{
	if ((m_isConst == false) && m_isPointer)
	{
		const ITypeDesc* pTypeDesc = CoreRegistry::GetTypeByIndex(typeIndex);
		if (pTypeDesc)
		{
			AssignPointerInternal(index, pPointer, *pTypeDesc);
			return;
		}
		CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
	}
	CRY_ASSERT_MESSAGE(m_isPointer, "Can't assign pointer 'pPointer' to value array.");
	CRY_ASSERT_MESSAGE(m_isConst == false, "Array elements are const and can't be changed.");
}

inline void CAnyArray::AssignPointerAt(uint32 index, const void* pPointer, const ITypeDesc& typeDesc)
{
	if ((m_isConst == false) && m_isPointer)
	{
		AssignPointerInternal(index, pPointer, typeDesc);
		return;
	}
	CRY_ASSERT_MESSAGE(m_isPointer, "Can't assign pointer 'pPointer' to value array.");
	CRY_ASSERT_MESSAGE(m_isConst == false, "Array elements are const and can't be changed.");
}

template<typename VALUE_TYPE>
inline VALUE_TYPE* CAnyArray::GetPointer(uint32 index)
{
	if (m_isConst == false)
	{
		constexpr CTypeId typeId = Type::IdOf<VALUE_TYPE>();
		const ITypeDesc* pTypeDesc = CoreRegistry::GetTypeById(typeId);
		if (pTypeDesc)
		{
			if (m_typeIndex == pTypeDesc->GetIndex())
			{
				return reinterpret_cast<VALUE_TYPE*>(GetPointer(index));
			}
			CRY_ASSERT_MESSAGE(m_typeIndex == pTypeDesc->GetIndex(), "Type doesn't match the array type.");
		}
		CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");
	}
	CRY_ASSERT_MESSAGE(m_isConst == false, "Const element can't be accessed as non-const pointer.");
	return nullptr;
}

inline void* CAnyArray::GetPointer(uint32 index)
{
	if (m_isConst == false)
	{
		if (index < m_size)
		{
			return GetPointerInternal(index);
		}
		CRY_ASSERT_MESSAGE(index < m_size, "'index' (%i) is out of bounds (%i).", index, m_size);
	}
	CRY_ASSERT_MESSAGE(m_isConst == false, "Const element can't be accessed as non-const pointer.");

	return nullptr;
}

template<typename VALUE_TYPE>
inline const VALUE_TYPE* CAnyArray::GetConstPointer(uint32 index) const
{
	constexpr CTypeId typeId = Type::IdOf<VALUE_TYPE>();
	const ITypeDesc* pTypeDesc = CoreRegistry::GetTypeById(typeId);
	if (pTypeDesc)
	{
		if (m_typeIndex == pTypeDesc->GetIndex())
		{
			return reinterpret_cast<const VALUE_TYPE*>(GetConstPointer(index));
		}
		CRY_ASSERT_MESSAGE(m_typeIndex == pTypeDesc->GetIndex(), "Type 'VALUE_TYPE' mismatch.");
	}
	CRY_ASSERT_MESSAGE(pTypeDesc, "Type not found in reflection registry.");

	return nullptr;
}

inline const void* CAnyArray::GetConstPointer(uint32 index) const
{
	if (index < m_size)
	{
		return GetPointerInternal(index);
	}
	CRY_ASSERT_MESSAGE(index < m_size, "'index' (%i) is out of bounds (%i).", index, m_size);

	return nullptr;
}

inline void CAnyArray::Remove(uint32 index)
{
	if (m_isPointer == false)
	{
		CallDestructors(index, index + 1);
	}

	ShiftLeftInternal(index);
}

inline void CAnyArray::ClearAll()
{
	if (m_isPointer == false)
	{
		CallDestructors(0, m_size);
	}

	m_size = 0;
}

inline void CAnyArray::DestructAll()
{
	if (m_typeIndex.IsValid())
	{
		CallDestructors(0, m_size);
	}

	CryModuleFree(m_pData);
	m_pData = nullptr;
	m_size = 0;
	m_capacity = 0;
	m_typeIndex = TypeIndex::Invalid;
	m_isPointer = false;
	m_isConst = false;
}

inline bool CAnyArray::Serialize(Serialization::IArchive& archive)
{
	const bool isLoad = archive.isInput();

	bool isPointer = m_isPointer;
	bool IsConst = m_isConst;

	archive(isPointer, "isPointer");
	archive(IsConst, "isConst");

	uint32 size = m_size;
	uint32 capacity = m_capacity;

	archive(size, "size");
	archive(capacity, "capacity");

	if (isLoad)
	{
		CGuid loadedGuid;
		archive(loadedGuid, "guid");

		const Reflection::ITypeDesc* pTypeDesc = CoreRegistry::GetTypeByGuid(loadedGuid);
		if (pTypeDesc)
		{
			m_typeIndex = pTypeDesc->GetIndex();
		}
		// TODO: This is a data error. Replace assert with a log message when available.
		CRY_ASSERT_MESSAGE(pTypeDesc, "Type '%s' doesn't exist in the type registry anymore.", pTypeDesc->GetLabel());
		// ~TODO

		m_isPointer = isPointer;
		m_isConst = IsConst;

		Reserve(capacity);
		Resize(size);
	}

	const Reflection::ITypeDesc* pTypeDesc = GetTypeDesc();
	if (pTypeDesc)
	{
		if (!isLoad)
		{
			archive(pTypeDesc->GetGuid(), "guid");
		}

		if (m_isPointer)
		{
			for (uint32 i = 0; i < m_size; ++i)
			{
				if (!archive(reinterpret_cast<uintptr_t>(GetPointerInternal(i)), CryStringUtils::toString(i).c_str()))
				{
					return false;
				}
			}
			return true;
		}
		else
		{
			Type::CSerializeFunction serializeFunction = pTypeDesc->GetSerializeFunction();
			if (serializeFunction)
			{
				for (uint32 i = 0; i < m_size; ++i)
				{
					if (!serializeFunction(archive, GetPointerInternal(i), CryStringUtils::toString(i).c_str(), nullptr))
					{
						return false;
					}
				}
				return true;
			}
			CRY_ASSERT_MESSAGE(serializeFunction, "'%s' doesn't have a serialize function.", pTypeDesc->GetFullQualifiedName());
		}
	}

	return false;
}

// TODO: We may want a user friendly string here instead in the future
inline string CAnyArray::ToString() const
{
	bool isPointer = m_isPointer;
	bool isConst = m_isConst;

	string anyArrayString;
	anyArrayString.Format("{ \"m_pData\": %p, \"m_size\": %lu, \"m_capacity\": %lu, \"m_typeIndex\": %u, \"m_isPointer\": %u, \"m_isConst\": %u }",
	                      static_cast<void*>(m_pData), m_size, m_capacity, static_cast<TypeIndex::ValueType>(m_typeIndex), isPointer, isConst);
	return anyArrayString;
}
// ~TODO

inline void* CAnyArray::GetBuffer() const
{
	return m_pData;
}

// Internal Methods
inline size_t CAnyArray::GetAlignedSize() const
{
	if (m_isPointer)
	{
		return sizeof(void*);
	}
	else
	{
		const ITypeDesc* pTypeDesc = GetTypeDesc();
		if (pTypeDesc)
		{
			const size_t size = pTypeDesc->GetSize();
			const size_t padding = size % pTypeDesc->GetAlignment();
			return size + padding;
		}
	}
	return 0;
}

inline void* CAnyArray::GetPointerInternal(uint32 index) const
{
	void* pPointer = static_cast<void*>(m_pData + (index * GetAlignedSize()));
	if (m_isPointer)
	{
		pPointer = static_cast<void*>(*reinterpret_cast<uintptr_t**>(pPointer));
	}
	return pPointer;
}

inline void CAnyArray::CallDestructors(uint32 begin, uint32 end)
{
	const ITypeDesc* pTypeDesc = GetTypeDesc();
	if (pTypeDesc)
	{
		end = end < m_size ? end : m_size;

		Type::CDestructor destructor = pTypeDesc->GetDestructor();
		if (destructor)
		{
			for (uint32 i = begin; i < end; ++i)
			{
				destructor(GetPointerInternal(i));
			}
			return;
		}
		CRY_ASSERT_MESSAGE(destructor, "'%s' has no destructor.", pTypeDesc->GetFullQualifiedName());
	}
}

inline void CAnyArray::CallDefaultConstructors(uint32 begin, uint32 end)
{
	const ITypeDesc* pTypeDesc = GetTypeDesc();
	if (pTypeDesc)
	{
		end = end < m_size ? end : m_size;

		Type::CDefaultConstructor defaultConstructor = pTypeDesc->GetDefaultConstructor();
		if (defaultConstructor)
		{
			const size_t alignedSize = GetAlignedSize();
			for (uint32 i = begin; i < end; ++i)
			{
				defaultConstructor(static_cast<void*>(m_pData + (i * alignedSize)));
			}
			return;
		}
		CRY_ASSERT_MESSAGE(defaultConstructor, "'%s' has no default constructor.", pTypeDesc->GetFullQualifiedName());
	}
}

inline void CAnyArray::AddValueInternal(const void* pSource, const ITypeDesc& typeDesc)
{
	if (m_typeIndex == typeDesc.GetIndex())
	{
		// TODO: This only increases the size by 1, if that becomes a problem change it
		// Resize if necessary
		const uint32 insertIndex = m_size;
		if (m_size + 1 > m_capacity)
		{
			Reserve(m_size + 1);
		}
		// ~TODO

		Type::CCopyConstructor copyConstructor = typeDesc.GetCopyConstructor();
		if (copyConstructor)
		{
			copyConstructor(GetPointerInternal(insertIndex), pSource);
			++m_size;
			return;
		}
		CRY_ASSERT_MESSAGE(copyConstructor, "'%s' has no copy constructor.", typeDesc.GetFullQualifiedName());
	}
	CRY_ASSERT_MESSAGE(m_typeIndex == typeDesc.GetIndex(), "'typeDesc' doesn't match the array type.");
}

inline void CAnyArray::AddPointerInternal(const void* pPointer, const ITypeDesc& typeDesc)
{
	if (m_typeIndex == typeDesc.GetIndex())
	{
		// TODO: This only increases the size by 1, if that becomes a problem change it
		// Resize if necessary
		const uint32 insertIndex = m_size;
		if (m_size + 1 > m_capacity)
		{
			Reserve(m_size + 1);
		}
		// ~TODO

		*reinterpret_cast<uintptr_t*>(m_pData + (insertIndex * GetAlignedSize())) = reinterpret_cast<uintptr_t>(pPointer);
		++m_size;
		return;
	}
	CRY_ASSERT_MESSAGE(m_typeIndex == typeDesc.GetIndex(), "'typeDesc' doesn't match the array type.");
}

inline void CAnyArray::AssignPointerInternal(uint32 index, const void* pPointer, const ITypeDesc& typeDesc)
{
	if ((index < m_size) && (m_typeIndex == typeDesc.GetIndex()))
	{
		*reinterpret_cast<uintptr_t*>(m_pData + (index * GetAlignedSize())) = reinterpret_cast<uintptr_t>(pPointer);
		return;
	}
	CRY_ASSERT_MESSAGE(index < m_size, "'index' (%i) is out of bounds (%i).", index, m_size);
	CRY_ASSERT_MESSAGE(m_typeIndex == typeDesc.GetIndex(), "'typeDesc' doesn't match the array type.");
}

inline void CAnyArray::ShiftLeftInternal(uint32 index)
{
	if (m_size > 0)
	{
		const ITypeDesc* pTypeDesc = GetTypeDesc();
		if (pTypeDesc)
		{
			for (uint32 i = index; i < m_size - 1; ++i)
			{
				void* pSource = GetPointerInternal(i + 1);
				if (m_isPointer)
				{
					AssignPointerAt(i, pSource, *pTypeDesc);
				}
				else
				{
					AssignMoveValueAt(i, pSource, *pTypeDesc);
				}
			}

			--m_size;
			return;
		}
	}
	CRY_ASSERT_MESSAGE(m_size > 0, "Can't shift with 0 elements.");
}

inline void CAnyArray::ShiftRightInternal(uint32 index)
{
	if ((m_size > 0) && (index < m_size))
	{
		const ITypeDesc* pTypeDesc = GetTypeDesc();
		if (pTypeDesc)
		{
			void* pSource = GetPointerInternal(m_size - 1);
			if (m_isPointer)
			{
				AddPointerInternal(pSource, *pTypeDesc);
				for (uint32 i = m_size - 1; i > index; --i)
				{
					pSource = GetPointerInternal(i - 1);
					AssignPointerAt(i, pSource, *pTypeDesc);
				}
			}
			else
			{
				AddValueInternal(pSource, *pTypeDesc);
				for (uint32 i = m_size - 1; i > index; --i)
				{
					pSource = GetPointerInternal(i - 1);
					AssignMoveValueAt(i, pSource, *pTypeDesc);
				}
			}
			return;
		}
	}
	CRY_ASSERT_MESSAGE(m_size > 0, "Can't shift with 0 elements.");
	CRY_ASSERT_MESSAGE(index < m_size, "Element index (%i) is out of bounds (%i)", index, m_size);
}

} // ~Reflection namespace
} // ~Cry namespace
