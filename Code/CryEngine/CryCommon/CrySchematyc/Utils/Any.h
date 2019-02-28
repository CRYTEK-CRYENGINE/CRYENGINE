// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Move functions to .inl file?
// #SchematycTODO : Create cast functions for CAnyValue&, CAnyValue* and CAnyValuePtr?
// #SchematycTODO : Create typed version of CAnyValue for faster construction (avoid type info lookup)?

#pragma once

#include <CrySerialization/Forward.h>

#include "CrySchematyc/Reflection/ReflectionUtils.h"
#include "CrySchematyc/Reflection/TypeDesc.h"
#include "CrySchematyc/Utils/Assert.h"

namespace Schematyc
{

// Forward declare classes.
class CAnyValue;
class CAnyRef;
class CAnyConstRef;
class CAnyPtr;
class CAnyConstPtr;
// Forward declare shared pointers.
DECLARE_SHARED_POINTERS(CAnyValue)

// Specialized allocator for CAnyValue, allocates additional memory for value storage immediately after object.
template<typename T> struct SAnyValueAllocator
{
	typedef T        value_type;
	typedef T*       pointer;
	typedef const T& const_pointer;
	typedef T&       reference;
	typedef const T& const_reference;
	typedef size_t   size_type;

	template<class U> struct rebind
	{
		typedef SAnyValueAllocator<U> other;
	};

	inline SAnyValueAllocator(const CCommonTypeDesc& _typeDesc)
		: typeDesc(_typeDesc)
	{}

	template<typename OTHER_TYPE> inline SAnyValueAllocator(const SAnyValueAllocator<OTHER_TYPE>& rhs)
		: typeDesc(rhs.typeDesc)
	{}

	pointer address(reference x)
	{
		return &x;
	}

	const_pointer address(const_reference x) const
	{
		return &x;
	}

	inline pointer allocate(size_type n)
	{
		return static_cast<pointer>(CryModuleMalloc((sizeof(value_type) + typeDesc.GetSize()) * n));
	}

	inline void deallocate(pointer p, size_type n)
	{
		CryModuleFree(p);
	}

	size_type max_size() const
	{
		return INT_MAX;
	}

	template<class U>
	void destroy(U* p) noexcept
	{
		p->~U();
	}

	const CCommonTypeDesc& typeDesc;
};

// Owning container for value of any reflected type. It is assumed that value follows immediately after object in memory therefore instances must be allocated using Make functions provided.
class CAnyValue
{
	CAnyValue() = delete;
	CAnyValue(const CAnyValue& rhs) = delete;
	CAnyValue& operator=(const CAnyValue& rhs) = delete;

public:

	~CAnyValue();

	const CCommonTypeDesc&                      GetTypeDesc() const;
	void*                                       GetValue();
	const void*                                 GetValue() const;

	template<typename TYPE> static CAnyValuePtr MakeShared(const TYPE& value);
	static CAnyValuePtr                         MakeShared(const CCommonTypeDesc& typeDesc, const void* pValue);
	static CAnyValuePtr                         MakeSharedDefault(const CCommonTypeDesc& typeDesc);
	static CAnyValuePtr                         CloneShared(const CAnyConstRef& value);

protected:

	CAnyValue(const CCommonTypeDesc& typeDesc);
	CAnyValue(const CCommonTypeDesc& typeDesc, const void* pValue);

private:

	const CCommonTypeDesc& m_typeDesc;
};

// Reference to value of any reflected type.
class CAnyRef
{
	friend class CAnyPtr;

	CAnyRef& operator=(const CAnyRef& rhs) = delete;

public:

	CAnyRef(const CCommonTypeDesc& typeDesc, void* pValue);
	template<typename TYPE> explicit CAnyRef(TYPE& value);
	CAnyRef(CAnyValue& rhs);
	CAnyRef(const CAnyRef& rhs);

	bool                   IsValid() const;
	const CCommonTypeDesc& GetTypeDesc() const;
	void*                  GetValue() const;

protected:

	CAnyRef();

	void Reset(const CAnyRef& rhs);

private:

	const CCommonTypeDesc* m_pTypeDesc;
	void*                  m_pValue;
};

// Reference to constant value of any reflected type.
class CAnyConstRef
{
	friend class CAnyConstPtr;

	CAnyConstRef& operator=(const CAnyConstRef& rhs) = delete;

public:

	CAnyConstRef(const CCommonTypeDesc& typeDesc, const void* pValue);
	template<typename TYPE> CAnyConstRef(const TYPE& value);
	CAnyConstRef(const CAnyValue& rhs);
	CAnyConstRef(const CAnyRef& rhs);
	CAnyConstRef(const CAnyConstRef& rhs);

	bool IsValid() const;
	const CCommonTypeDesc& GetTypeDesc() const;
	const void*            GetValue() const;

protected:

	CAnyConstRef();

	void Reset(const CAnyConstRef& rhs);

private:

	const CCommonTypeDesc* m_pTypeDesc;
	const void*            m_pValue;
};

// Pointer to value of any reflected type.
class CAnyPtr
{
public:

	CAnyPtr();
	CAnyPtr(const CCommonTypeDesc& typeDesc, void* pValue);
	template<typename TYPE> explicit CAnyPtr(TYPE* pValue);
	CAnyPtr(CAnyValue* pValue);
	CAnyPtr(const CAnyValuePtr& pValue);
	CAnyPtr(const CAnyRef* rhs);
	CAnyPtr(const CAnyPtr& rhs);

	explicit       operator bool() const;
	const CAnyRef* operator->() const;
	const CAnyRef& operator*() const;
	CAnyPtr&       operator=(const CAnyPtr& rhs);

private:

	CAnyRef m_ref;
};

// Pointer to constant value of any reflected type.
class CAnyConstPtr
{
public:

	CAnyConstPtr();
	CAnyConstPtr(const CCommonTypeDesc& typeDesc, const void* pValue);
	template<typename TYPE> CAnyConstPtr(const TYPE* pValue);
	CAnyConstPtr(const CAnyValue* pValue);
	CAnyConstPtr(const CAnyValuePtr& pValue);
	CAnyConstPtr(const CAnyValueConstPtr& pValue);
	CAnyConstPtr(const CAnyConstRef* rhs);
	CAnyConstPtr(const CAnyPtr& rhs);
	CAnyConstPtr(const CAnyConstPtr& rhs);

	explicit            operator bool() const;
	const CAnyConstRef* operator->() const;
	const CAnyConstRef& operator*() const;
	CAnyConstPtr&       operator=(const CAnyConstPtr& rhs);

private:

	CAnyConstRef m_ref;
};

// CAnyValue functions.

inline CAnyValue::~CAnyValue()
{
	m_typeDesc.GetOperators().destruct(GetValue());
}

inline const CCommonTypeDesc& CAnyValue::GetTypeDesc() const
{
	return m_typeDesc;
}

inline void* CAnyValue::GetValue()
{
	return this + 1;
}

inline const void* CAnyValue::GetValue() const
{
	return this + 1;
}

template<typename TYPE> inline CAnyValuePtr CAnyValue::MakeShared(const TYPE& value)
{
	SCHEMATYC_VERIFY_TYPE_IS_REFLECTED(TYPE);
	static_assert(std::is_copy_constructible<TYPE>::value, "Type must provide copy constructor!");
	return MakeShared(Schematyc::GetTypeDesc<TYPE>(), &value);
}

inline CAnyValuePtr CAnyValue::MakeShared(const CCommonTypeDesc& typeDesc, const void* pValue)
{
	class CAnyValueImpl : public CAnyValue
	{
	public:

		inline CAnyValueImpl(const CCommonTypeDesc& typeDesc, const void* pValue)
			: CAnyValue(typeDesc, pValue)
		{}
	};

	SCHEMATYC_CORE_ASSERT_MESSAGE(typeDesc.GetOperators().copyConstruct, "Type must provide copy constructor!");
	if (typeDesc.GetOperators().copyConstruct)
	{
		SCHEMATYC_CORE_ASSERT(pValue);
		if (pValue)
		{
			SAnyValueAllocator<CAnyValueImpl> allocator(typeDesc);
			return std::static_pointer_cast<CAnyValue>(std::allocate_shared<CAnyValueImpl, SAnyValueAllocator<CAnyValueImpl>>(allocator, typeDesc, pValue));
		}
	}
	return CAnyValuePtr();
}

inline CAnyValuePtr CAnyValue::MakeSharedDefault(const CCommonTypeDesc& typeDesc)
{
	class CAnyValueImpl : public CAnyValue
	{
	public:

		inline CAnyValueImpl(const CCommonTypeDesc& typeDesc)
			: CAnyValue(typeDesc)
		{}
	};

	SCHEMATYC_CORE_ASSERT_MESSAGE(typeDesc.GetOperators().defaultConstruct, "Type must provide default constructor!");
	if (typeDesc.GetOperators().defaultConstruct)
	{
		SAnyValueAllocator<CAnyValueImpl> allocator(typeDesc);
		return std::static_pointer_cast<CAnyValue>(std::allocate_shared<CAnyValueImpl, SAnyValueAllocator<CAnyValueImpl>>(allocator, typeDesc));
	}
	return CAnyValuePtr();
}

inline CAnyValuePtr CAnyValue::CloneShared(const CAnyConstRef& value)
{
	return MakeShared(value.GetTypeDesc(), value.GetValue());
}

inline CAnyValue::CAnyValue(const CCommonTypeDesc& typeDesc)
	: m_typeDesc(typeDesc)
{
	m_typeDesc.GetOperators().defaultConstruct(GetValue());
}

inline CAnyValue::CAnyValue(const CCommonTypeDesc& typeDesc, const void* pValue)
	: m_typeDesc(typeDesc)
{
	m_typeDesc.GetOperators().copyConstruct(GetValue(), pValue);
}

// CAnyRef functions.

inline CAnyRef::CAnyRef(const CCommonTypeDesc& typeDesc, void* pValue)
	: m_pTypeDesc(&typeDesc)
	, m_pValue(pValue)
{
	SCHEMATYC_CORE_ASSERT(IsValid());
}

template<typename TYPE> inline CAnyRef::CAnyRef(TYPE& value)
	: m_pTypeDesc(&Schematyc::GetTypeDesc<TYPE>())
	, m_pValue(&value)
{}

inline CAnyRef::CAnyRef(CAnyValue& rhs)
	: m_pTypeDesc(&rhs.GetTypeDesc())
	, m_pValue(rhs.GetValue())
{}

inline CAnyRef::CAnyRef(const CAnyRef& rhs)
	: m_pTypeDesc(rhs.m_pTypeDesc)
	, m_pValue(rhs.m_pValue)
{}

inline bool CAnyRef::IsValid() const
{
	return m_pValue != nullptr;
}

inline const CCommonTypeDesc& CAnyRef::GetTypeDesc() const
{
	SCHEMATYC_CORE_ASSERT(IsValid());
	return *m_pTypeDesc;
}

inline void* CAnyRef::GetValue() const
{
	SCHEMATYC_CORE_ASSERT(IsValid());
	return m_pValue;
}

inline CAnyRef::CAnyRef()
	: m_pTypeDesc(nullptr)
	, m_pValue(nullptr)
{}

inline void CAnyRef::Reset(const CAnyRef& rhs)
{
	m_pTypeDesc = rhs.m_pTypeDesc;
	m_pValue = rhs.m_pValue;
}

// CAnyConstRef functions.

inline CAnyConstRef::CAnyConstRef(const CCommonTypeDesc& typeDesc, const void* pValue)
	: m_pTypeDesc(&typeDesc)
	, m_pValue(pValue)
{
	SCHEMATYC_CORE_ASSERT(IsValid());
}

template<typename TYPE> inline CAnyConstRef::CAnyConstRef(const TYPE& value)
	: m_pTypeDesc(&Schematyc::GetTypeDesc<TYPE>())
	, m_pValue(&value)
{}

inline CAnyConstRef::CAnyConstRef(const CAnyValue& rhs)
	: m_pTypeDesc(&rhs.GetTypeDesc())
	, m_pValue(rhs.GetValue())
{}

inline CAnyConstRef::CAnyConstRef(const CAnyRef& rhs)
	: m_pTypeDesc(&rhs.GetTypeDesc())
	, m_pValue(rhs.GetValue())
{}

inline CAnyConstRef::CAnyConstRef(const CAnyConstRef& rhs)
	: m_pTypeDesc(rhs.m_pTypeDesc)
	, m_pValue(rhs.m_pValue)
{}

inline bool CAnyConstRef::IsValid() const
{
	return m_pValue != nullptr;
}

inline const CCommonTypeDesc& CAnyConstRef::GetTypeDesc() const
{
	SCHEMATYC_CORE_ASSERT(IsValid());
	return *m_pTypeDesc;
}

inline const void* CAnyConstRef::GetValue() const
{
	return m_pValue;
}

inline CAnyConstRef::CAnyConstRef()
	: m_pTypeDesc(nullptr)
	, m_pValue(nullptr)
{}

inline void CAnyConstRef::Reset(const CAnyConstRef& rhs)
{
	m_pTypeDesc = rhs.m_pTypeDesc;
	m_pValue = rhs.m_pValue;
}

// CAnyPtr functions.

inline CAnyPtr::CAnyPtr() {}

inline CAnyPtr::CAnyPtr(const CCommonTypeDesc& typeDesc, void* pValue)
	: m_ref(typeDesc, pValue)
{}

template<typename TYPE> inline CAnyPtr::CAnyPtr(TYPE* pValue)
	: m_ref(Schematyc::GetTypeDesc<TYPE>(), pValue)
{}

inline CAnyPtr::CAnyPtr(CAnyValue* pValue)
	: m_ref(pValue ? CAnyRef(*pValue) : CAnyRef())
{}

inline CAnyPtr::CAnyPtr(const CAnyValuePtr& pValue)
	: m_ref(pValue ? CAnyRef(*pValue) : CAnyRef())
{}

inline CAnyPtr::CAnyPtr(const CAnyRef* rhs)
	: m_ref(rhs ? *rhs : CAnyRef())
{}

inline CAnyPtr::CAnyPtr(const CAnyPtr& rhs)
	: m_ref(rhs.m_ref)
{}

inline CAnyPtr::operator bool() const
{
	return m_ref.IsValid();
}

inline const CAnyRef* CAnyPtr::operator->() const
{
	SCHEMATYC_CORE_ASSERT(m_ref.IsValid());
	return &m_ref;
}

inline const CAnyRef& CAnyPtr::operator*() const
{
	SCHEMATYC_CORE_ASSERT(m_ref.IsValid());
	return m_ref;
}

inline CAnyPtr& CAnyPtr::operator=(const CAnyPtr& rhs)
{
	m_ref.Reset(rhs.m_ref);
	return *this;
}

// CAnyConstPtr functions.

inline CAnyConstPtr::CAnyConstPtr() {}

inline CAnyConstPtr::CAnyConstPtr(const CCommonTypeDesc& typeDesc, const void* pValue)
	: m_ref(typeDesc, pValue)
{}

template<typename TYPE> inline CAnyConstPtr::CAnyConstPtr(const TYPE* pValue)
	: m_ref(Schematyc::GetTypeDesc<TYPE>(), pValue)
{}

inline CAnyConstPtr::CAnyConstPtr(const CAnyValue* rhs)
	: m_ref(rhs ? CAnyConstRef(*rhs) : CAnyConstRef())
{}

inline CAnyConstPtr::CAnyConstPtr(const CAnyConstRef* pValue)
	: m_ref(pValue ? *pValue : CAnyConstRef())
{}

inline CAnyConstPtr::CAnyConstPtr(const CAnyValuePtr& pValue)
	: m_ref(pValue ? *pValue : CAnyConstRef())
{}

inline CAnyConstPtr::CAnyConstPtr(const CAnyValueConstPtr& pValue)
	: m_ref(pValue ? *pValue : CAnyConstRef())
{}

inline CAnyConstPtr::CAnyConstPtr(const CAnyPtr& rhs)
	: m_ref(*rhs)
{}

inline CAnyConstPtr::CAnyConstPtr(const CAnyConstPtr& rhs)
	: m_ref(rhs.m_ref)
{}

inline CAnyConstPtr::operator bool() const
{
	return m_ref.IsValid();
}

inline const CAnyConstRef* CAnyConstPtr::operator->() const
{
	SCHEMATYC_CORE_ASSERT(m_ref.IsValid());
	return &m_ref;
}

inline const CAnyConstRef& CAnyConstPtr::operator*() const
{
	SCHEMATYC_CORE_ASSERT(m_ref.IsValid());
	return m_ref;
}

inline CAnyConstPtr& CAnyConstPtr::operator=(const CAnyConstPtr& rhs)
{
	m_ref.Reset(rhs.m_ref);
	return *this;
}

// Utility functions.

namespace Any
{

inline bool CopyAssign(const CAnyRef& lhs, const CAnyConstRef& rhs)
{
	const CCommonTypeDesc& typeDesc = lhs.GetTypeDesc();
	SCHEMATYC_CORE_ASSERT_MESSAGE(typeDesc.GetOperators().copyAssign, "Type must provide copy assignment operator!");
	if (typeDesc.GetOperators().copyAssign)
	{
		const void* pRHSValue = Helpers::DynamicCast(rhs.GetTypeDesc(), rhs.GetValue(), typeDesc);
		SCHEMATYC_CORE_ASSERT_MESSAGE(pRHSValue, "Type mismatch!");
		if (pRHSValue)
		{
			(*typeDesc.GetOperators().copyAssign)(lhs.GetValue(), pRHSValue);
			return true;
		}
	}
	return false;
}

inline bool Equals(const CAnyConstRef& lhs, const CAnyConstRef& rhs)
{
	const CCommonTypeDesc& typeDesc = lhs.GetTypeDesc();
	SCHEMATYC_CORE_ASSERT_MESSAGE(typeDesc.GetOperators().equals, "Type must provide equals operator!");
	if (typeDesc.GetOperators().equals)
	{
		const void* pRHSValue = Helpers::DynamicCast(rhs.GetTypeDesc(), rhs.GetValue(), typeDesc);
		SCHEMATYC_CORE_ASSERT_MESSAGE(pRHSValue, "Type mismatch!");
		if (pRHSValue)
		{
			return (*typeDesc.GetOperators().equals)(lhs.GetValue(), pRHSValue);
		}
	}
	return false;
}

inline bool ToString(IString& output, const CAnyConstRef& input)
{
	const CCommonTypeDesc& typeDesc = input.GetTypeDesc();
	SCHEMATYC_CORE_ASSERT_MESSAGE(typeDesc.GetOperators().toString, "Type must provide 'ToString' method!");
	if (typeDesc.GetOperators().toString)
	{
		(*typeDesc.GetOperators().toString)(output, input.GetValue());
		return true;
	}
	return false;
}

} // Any

// Serialization functions.

inline bool Serialize(Serialization::IArchive& archive, const CAnyRef& value, const char* szName, const char* szLabel)
{
	const CCommonTypeDesc& typeDesc = value.GetTypeDesc();
	SCHEMATYC_CORE_ASSERT_MESSAGE(typeDesc.GetOperators().toString, "Type must provide 'Serialize' method!");
	if (typeDesc.GetOperators().serialize)
	{
		return (*typeDesc.GetOperators().serialize)(archive, value.GetValue(), szName, szLabel);
	}
	return false;
}

inline bool Serialize(Serialization::IArchive& archive, CAnyValue& value, const char* szName, const char* szLabel)
{
	const CCommonTypeDesc& typeDesc = value.GetTypeDesc();
	SCHEMATYC_CORE_ASSERT_MESSAGE(typeDesc.GetOperators().serialize, "Type must provide 'Serialize' method!");
	if (typeDesc.GetOperators().serialize)
	{
		return (*typeDesc.GetOperators().serialize)(archive, value.GetValue(), szName, szLabel);
	}
	return false;
}

// Dynamic cast functions.

template<typename TO_TYPE> TO_TYPE& DynamicCast(const CAnyRef& from)
{
	TO_TYPE* pResult = Helpers::DynamicCast<TO_TYPE>(from.GetTypeDesc(), from.GetValue());
	SCHEMATYC_CORE_ASSERT(pResult);
	return *pResult;
}

template<typename TO_TYPE> const TO_TYPE& DynamicCast(const CAnyConstRef& from)
{
	const TO_TYPE* pResult = Helpers::DynamicCast<TO_TYPE>(from.GetTypeDesc(), from.GetValue());
	SCHEMATYC_CORE_ASSERT(pResult);
	return *pResult;
}

template<typename TO_TYPE> TO_TYPE* DynamicCast(const CAnyPtr& pFrom)
{
	return pFrom ? Helpers::DynamicCast<TO_TYPE>(pFrom->GetTypeDesc(), pFrom->GetValue()) : nullptr;
}

template<typename TO_TYPE> const TO_TYPE* DynamicCast(const CAnyConstPtr& pFrom)
{
	return pFrom ? Helpers::DynamicCast<TO_TYPE>(pFrom->GetTypeDesc(), pFrom->GetValue()) : nullptr;
}

} // Schematyc
