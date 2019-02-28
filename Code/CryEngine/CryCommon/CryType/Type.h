// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "TypeUtils.h"
#include "TypeOperators.h"
#include "TypeTraits.h"

namespace Cry {
namespace Type {

class CTypeId
{
public:
	typedef uint32 ValueType;
	static const ValueType Invalid = 0;

	template<typename TYPE>
	constexpr static CTypeId Create()
	{
		return CTypeId(Utils::Explicit<typename PureType<TYPE>::Type>());
	}

	constexpr CTypeId()
		: m_typeNameCrc(0)
	{}

	CTypeId(const CTypeId& typeId)
		: m_typeNameCrc(typeId.m_typeNameCrc)
	{}

	bool IsValid() const
	{
		return m_typeNameCrc == Invalid;
	}

	constexpr ValueType GetValue() const
	{
		return m_typeNameCrc;
	}

	CTypeId operator=(CTypeId rhs)
	{
		m_typeNameCrc = rhs.m_typeNameCrc;
		return *this;
	}

	constexpr bool operator==(CTypeId rhs) const
	{
		return m_typeNameCrc == rhs.m_typeNameCrc;
	}

	constexpr bool operator!=(CTypeId rhs) const
	{
		return m_typeNameCrc == rhs.m_typeNameCrc;
	}

	constexpr operator ValueType() const
	{
		return m_typeNameCrc;
	}

protected:
	friend class CTypeDesc;

	template<typename TYPE>
	constexpr CTypeId(Utils::Explicit<TYPE> )
		: m_typeNameCrc(Utils::SCompileTime_TypeInfo<TYPE>::GetCrc32())
	{}

private:
	ValueType m_typeNameCrc;
};

class CTypeDesc
{
public:
	template<typename TYPE>
	static CTypeDesc Create()
	{
		return CTypeDesc(Utils::Explicit<typename PureType<TYPE>::Type>(), 0);
	}

	template<typename TYPE, size_t ARRAY_LENGTH>
	static CTypeDesc Create()
	{
		return CTypeDesc(Utils::Explicit<typename PureType<TYPE>::Type>(), ARRAY_LENGTH);
	}

	CTypeDesc()
		: m_typeId()
		, m_arrayLength(0)
		, m_size(0)
		, m_alignment(0)
		, m_isAbstract(false)
		, m_isArray(false)
		, m_isClass(false)
		, m_isEnum(false)
		, m_isFundamental(false)
		, m_isUnion(false)
	{}

	CTypeDesc(const CTypeDesc& desc)
		: m_rawName(desc.m_rawName)
		, m_typeId(desc.m_typeId)
		, m_arrayLength(desc.m_arrayLength)
		, m_size(desc.m_size)
		, m_alignment(desc.m_alignment)
		, m_isAbstract(desc.m_isAbstract)
		, m_isArray(desc.m_isArray)
		, m_isClass(desc.m_isClass)
		, m_isEnum(desc.m_isEnum)
		, m_isFundamental(desc.m_isFundamental)
		, m_isUnion(desc.m_isUnion)
		, m_constructorDefault(desc.m_constructorDefault)
		, m_constructorCopy(desc.m_constructorCopy)
		, m_constructorMove(desc.m_constructorMove)
		, m_destructor(desc.m_destructor)
		, m_operatorCopyAssign(desc.m_operatorCopyAssign)
		, m_operatorEqual(desc.m_operatorEqual)
	{}

	~CTypeDesc()
	{

	}

	CTypeId             GetTypeId() const             { return m_typeId; }
	const char*         GetRawName() const            { return m_rawName.c_str(); }
	uint16              GetSize() const               { return m_size; }
	uint16              GetAlignment() const          { return m_alignment; }

	bool                IsAbstract() const            { return m_isAbstract; }
	bool                IsArray() const               { return m_isArray; }
	bool                IsClass() const               { return m_isClass; }
	bool                IsEnum() const                { return m_isEnum; }
	bool                IsFundamental() const         { return m_isFundamental; }
	bool                IsUnion() const               { return m_isUnion; }

	CDefaultConstructor GetDefaultConstructor() const { return m_constructorDefault; }
	CCopyConstructor    GetCopyConstructor() const    { return m_constructorCopy; }
	CMoveConstructor    GetMoveConstructor() const    { return m_constructorMove; }

	CDestructor         GetDestructor() const         { return m_destructor; }

	CCopyAssignOperator GetCopyAssignOperator() const { return m_operatorCopyAssign; }
	CEqualOperator      GetEqualOperator() const      { return m_operatorEqual; }

private:
	template<typename TYPE>
	CTypeDesc(Utils::Explicit<TYPE>, size_t arrayLength)
		: m_rawName(Utils::SCompileTime_TypeInfo<TYPE>::GetName().c_str(), Utils::SCompileTime_TypeInfo<TYPE>::GetName().length())
		, m_typeId(CTypeId::Create<TYPE>())
		, m_arrayLength(arrayLength)
		, m_size(sizeof(TYPE))
		, m_alignment(AlignOf<TYPE>::Value)
		, m_isAbstract(std::is_abstract<TYPE>::value)
		, m_isArray(arrayLength != 0)
		, m_isClass(std::is_class<TYPE>::value)
		, m_isEnum(std::is_enum<TYPE>::value)
		, m_isFundamental(std::is_fundamental<TYPE>::value)
		, m_isUnion(std::is_union<TYPE>::value)
		, m_constructorDefault(CAdapater_DefaultConstructor<TYPE>())
		, m_constructorCopy(CAdapater_CopyConstructor<TYPE>())
		, m_constructorMove(CAdapter_MoveConstructor<TYPE>())
		, m_destructor(CAdapter_Destructor<TYPE>())
		, m_operatorCopyAssign(CAdapter_CopyAssign<TYPE>())
		, m_operatorEqual(CAdapter_Equal<TYPE>())
	{
		static_assert(sizeof(TYPE) <= UINT16_MAX, "Size of type exceeds limit of uint16.");
		static_assert(AlignOf<TYPE>::Value <= UINT16_MAX, "Alignment requirement of type exceeds limit of uint16.");
	}

private:
	string              m_rawName;
	CTypeId             m_typeId;
	uint32              m_arrayLength;
	uint16              m_size;
	uint16              m_alignment;

	bool                m_isAbstract;
	bool                m_isArray;
	bool                m_isClass;
	bool                m_isEnum;
	bool                m_isFundamental;
	bool                m_isUnion;

	CDefaultConstructor m_constructorDefault;
	CCopyConstructor    m_constructorCopy;
	CMoveConstructor    m_constructorMove;

	CDestructor         m_destructor;

	CCopyAssignOperator m_operatorCopyAssign;
	CEqualOperator      m_operatorEqual;
};

inline bool operator==(const CTypeDesc& lh, CTypeId rh)
{
	return lh.GetTypeId() == rh;
}

inline bool operator!=(const CTypeDesc& lh, CTypeId rh)
{
	return lh.GetTypeId() != rh;
}

inline bool operator==(CTypeId lh, const CTypeDesc& rh)
{
	return lh == rh.GetTypeId();
}

inline bool operator!=(CTypeId lh, const CTypeDesc& rh)
{
	return lh != rh.GetTypeId();
}

} // ~Reflection namespace
} // ~Cry namespace

namespace Cry {

template<typename TYPE>
constexpr inline Cry::Type::CTypeId TypeIdOf()
{
	return Cry::Type::CTypeId::Create<TYPE>();
}

template<typename TYPE>
constexpr inline Cry::Type::CTypeId::ValueType TypeIdValue()
{
	typedef typename Cry::Type::PureType<TYPE>::Type T;
	//return Cry::Type::Utils::SCompileTime_TypeInfo<T>::GetCrc32();
	return CCrc32::Compute_CompileTime(Cry::Type::Utils::SCompileTime_TypeInfo<T>::GetName().c_str(), Cry::Type::Utils::SCompileTime_TypeInfo<T>::GetName().length());
}

template<typename TYPE>
static const Cry::Type::CTypeDesc& TypeDescOf()
{
	static Cry::Type::CTypeDesc typeDesc = Cry::Type::CTypeDesc::Create<TYPE>();
	return typeDesc;
}

}

using CryTypeId = Cry::Type::CTypeId;
using CryTypeDesc = Cry::Type::CTypeDesc;
