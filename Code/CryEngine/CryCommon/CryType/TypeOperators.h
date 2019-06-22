// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryType/TypeTraits.h>

namespace Cry {
namespace Type {

// Constructor: Default
class CDefaultConstructor
{
public:
	typedef void (* FuncPtr)(void* pPlacement);
	constexpr CDefaultConstructor(FuncPtr ptr = nullptr)
		: m_ptr(ptr)
	{}

	operator bool() const { return m_ptr != nullptr; }
	void operator()(void* pPlacement) { m_ptr(pPlacement); }

private:
	FuncPtr m_ptr;
};

template<typename TYPE, bool IS_SUPPORTED = std::is_default_constructible<TYPE>::value>
class CAdapater_DefaultConstructor : public CDefaultConstructor
{

};

template<typename TYPE>
class CAdapater_DefaultConstructor<TYPE, true> : public CDefaultConstructor
{
public:
	constexpr CAdapater_DefaultConstructor()
		: CDefaultConstructor(&CAdapater_DefaultConstructor::Execute)
	{}

private:
	static void Execute(void* pPlacement)
	{
		new(pPlacement) TYPE();
	}
};

// Constructor: Copy
class CCopyConstructor
{
public:
	typedef void (* FuncPtr)(void* pPlacement, const void* pRHS);
	constexpr CCopyConstructor(FuncPtr ptr = nullptr)
		: m_ptr(ptr)
	{}

	operator bool() const { return m_ptr != nullptr; }
	void operator()(void* pPlacement, const void* pRHS) { m_ptr(pPlacement, pRHS); }

private:
	FuncPtr m_ptr;
};

template<typename TYPE, bool IS_SUPPORTED = std::is_copy_constructible<TYPE>::value>
class CAdapater_CopyConstructor : public CCopyConstructor
{

};

template<typename TYPE>
class CAdapater_CopyConstructor<TYPE, true> : public CCopyConstructor
{
public:
	constexpr CAdapater_CopyConstructor()
		: CCopyConstructor(&CAdapater_CopyConstructor::Execute)
	{}

private:
	static void Execute(void* pPlacement, const void* pRHS)
	{
		const TYPE& value = *static_cast<const TYPE*>(pRHS);
		new(pPlacement) TYPE(value);
	}
};

// Constructor: Move
class CMoveConstructor
{
public:
	typedef void (* FuncPtr)(void* pPlacement, void* pRHS);
	constexpr CMoveConstructor(FuncPtr ptr = nullptr)
		: m_ptr(ptr)
	{}

	operator bool() const { return m_ptr != nullptr; }
	void operator()(void* pPlacement, void* pRHS) { m_ptr(pPlacement, pRHS); }

private:
	FuncPtr m_ptr;
};

template<typename TYPE, bool IS_SUPPORTED = std::is_move_constructible<TYPE>::value>
class CAdapter_MoveConstructor : public CMoveConstructor
{
};

template<typename TYPE>
class CAdapter_MoveConstructor<TYPE, true> : public CMoveConstructor
{
public:
	constexpr CAdapter_MoveConstructor()
		: CMoveConstructor(&CAdapter_MoveConstructor::Execute)
	{}

private:
	static void Execute(void* pPlacement, void* pRHS)
	{
		TYPE& value = *static_cast<TYPE*>(pRHS);
		new(pPlacement) TYPE(std::move(value));
	}
};

// Destructor
class CDestructor
{
public:
	typedef void (* FuncPtr)(void* pRHS);
	constexpr CDestructor(FuncPtr ptr = nullptr)
		: m_ptr(ptr)
	{}

	operator bool() const { return m_ptr != nullptr; }
	void operator()(void* pRHS) { m_ptr(pRHS); }

private:
	FuncPtr m_ptr;
};

template<typename TYPE, bool IS_SUPPORTED = std::is_copy_constructible<TYPE>::value>
class CAdapter_Destructor : public CDestructor
{
};

template<typename TYPE>
class CAdapter_Destructor<TYPE, true> : public CDestructor
{
public:
	constexpr CAdapter_Destructor()
		: CDestructor(&CAdapter_Destructor::Execute)
	{}

private:
	static void Execute(void* pRHS)
	{
		reinterpret_cast<TYPE*>(pRHS)->~TYPE();
	}
};

// Operator: Default new
class CDefaultNewOperator
{
public:
	typedef void*(* FuncPtr)();
	constexpr CDefaultNewOperator(FuncPtr ptr = nullptr)
		: m_ptr(ptr)
	{}

	operator bool() const { return m_ptr != nullptr; }
	void* operator()() { return m_ptr(); }

private:
	FuncPtr m_ptr;
};

template<typename TYPE, bool IS_SUPPORTED = std::is_default_constructible<TYPE>::value>
class CAdapter_DefaultNew : public CDefaultNewOperator
{

};

template<typename TYPE>
class CAdapter_DefaultNew<TYPE, true> : public CDefaultNewOperator
{
public:
	constexpr CAdapter_DefaultNew()
		: CDefaultNewOperator(&CAdapter_DefaultNew::Execute)
	{}

private:
	static void* Execute()
	{
		return new TYPE();
	}
};

// Operator: Delete
class CDeleteOperator
{
public:
	typedef void (* FuncPtr)(void*);
	constexpr CDeleteOperator(FuncPtr ptr = nullptr)
		: m_ptr(ptr)
	{}

	operator bool() const { return m_ptr != nullptr; }
	void operator()(void* pAddress) { m_ptr(pAddress); }

private:
	FuncPtr m_ptr;
};

template<typename TYPE, bool IS_SUPPORTED = std::is_destructible<TYPE>::value>
class CAdapter_Delete : public CDeleteOperator
{

};

template<typename TYPE>
class CAdapter_Delete<TYPE, true> : public CDeleteOperator
{
public:
	constexpr CAdapter_Delete()
		: CDeleteOperator(&CAdapter_Delete::Execute)
	{}

private:
	static void Execute(void* pAddress)
	{
		TYPE* pObject = static_cast<TYPE*>(pAddress);
		delete pObject;
	}
};

// Operator: New Array
class CNewArrayOperator
{
public:
	typedef void*(* FuncPtr)(int count);
	constexpr CNewArrayOperator(FuncPtr ptr = nullptr)
		: m_ptr(ptr)
	{}

	operator bool() const { return m_ptr != nullptr; }
	void* operator()(int count) { return m_ptr(count); }

private:
	FuncPtr m_ptr;
};

template<typename TYPE, bool IS_SUPPORTED = std::is_default_constructible<TYPE>::value>
class CAdapter_NewArray : public CNewArrayOperator
{

};

template<typename TYPE>
class CAdapter_NewArray<TYPE, true> : public CNewArrayOperator
{
public:
	constexpr CAdapter_NewArray()
		: CNewArrayOperator(&CAdapter_NewArray::Execute)
	{}

private:
	static void* Execute(int count)
	{
		return new TYPE[count];
	}
};

// Operator: Delete Array
class CDeleteArrayOperator
{
public:
	typedef void (* FuncPtr)(void*);
	constexpr CDeleteArrayOperator(FuncPtr ptr = nullptr)
		: m_ptr(ptr)
	{}

	operator bool() const { return m_ptr != nullptr; }
	void operator()(void* pAddress) { m_ptr(pAddress); }

private:
	FuncPtr m_ptr;
};

template<typename TYPE, bool IS_SUPPORTED = std::is_destructible<TYPE>::value>
class CAdapter_DeleteArray : public CDeleteArrayOperator
{

};

template<typename TYPE>
class CAdapter_DeleteArray<TYPE, true> : public CDeleteArrayOperator
{
public:
	constexpr CAdapter_DeleteArray()
		: CDeleteArrayOperator(&CAdapter_DeleteArray::Execute)
	{}

private:
	static void Execute(void* pAddress)
	{
		TYPE* pObjectArray = static_cast<TYPE*>(pAddress);
		delete[]pObjectArray;
	}
};

// Operator: Copy assign
class CCopyAssignOperator
{
public:
	typedef void (* FuncPtr)(void* pLHS, const void* pRHS);
	constexpr CCopyAssignOperator(FuncPtr ptr = nullptr)
		: m_ptr(ptr)
	{}

	operator bool() const { return m_ptr != nullptr; }
	void operator()(void* pLHS, const void* pRHS) { m_ptr(pLHS, pRHS); }

private:
	FuncPtr m_ptr;
};

template<typename TYPE, bool IS_SUPPORTED = std::is_copy_assignable<TYPE>::value>
class CAdapter_CopyAssign : public CCopyAssignOperator
{

};

template<typename TYPE>
class CAdapter_CopyAssign<TYPE, true> : public CCopyAssignOperator
{
public:
	constexpr CAdapter_CopyAssign()
		: CCopyAssignOperator(&CAdapter_CopyAssign::Execute)
	{}

private:
	static void Execute(void* pLHS, const void* pRHS)
	{
		TYPE& lhs = *static_cast<TYPE*>(pLHS);
		const TYPE& rhs = *static_cast<const TYPE*>(pRHS);
		lhs = rhs;
	}
};

// Operator: Move assign
class CMoveAssignOperator
{
public:
	typedef void (* FuncPtr)(void* pLHS, void* pRHS);
	constexpr CMoveAssignOperator(FuncPtr ptr = nullptr)
		: m_ptr(ptr)
	{}

	operator bool() const { return m_ptr != nullptr; }
	void operator()(void* pLHS, void* pRHS) { m_ptr(pLHS, pRHS); }

private:
	FuncPtr m_ptr;
};

template<typename TYPE, bool IS_SUPPORTED = std::is_move_assignable<TYPE>::value>
class CAdapter_MoveAssign : public CMoveAssignOperator
{

};

template<typename TYPE>
class CAdapter_MoveAssign<TYPE, true> : public CMoveAssignOperator
{
public:
	constexpr CAdapter_MoveAssign()
		: CMoveAssignOperator(&CAdapter_MoveAssign::Execute)
	{}

private:
	static void Execute(void* pLHS, void* pRHS)
	{
		TYPE& lhs = *static_cast<TYPE*>(pLHS);
		TYPE& rhs = *static_cast<TYPE*>(pRHS);
		lhs = std::move(rhs);
	}
};

// Operator: Equal
class CEqualOperator
{
public:
	typedef bool (* FuncPtr)(const void* pLHS, const void* pRHS);
	constexpr CEqualOperator(FuncPtr ptr = nullptr)
		: m_ptr(ptr)
	{}

	operator bool() const { return m_ptr != nullptr; }
	bool operator()(const void* pLHS, const void* pRHS) { return m_ptr(pLHS, pRHS); }

private:
	FuncPtr m_ptr;
};

template<typename TYPE, bool IS_SUPPORTED = Traits::IsEqualComparable<TYPE>::value>
class CAdapter_Equal : public CEqualOperator
{

};

template<typename TYPE>
class CAdapter_Equal<TYPE, true> : public CEqualOperator
{
public:
	constexpr CAdapter_Equal()
		: CEqualOperator(&CAdapter_Equal::Execute)
	{}

private:
	static bool Execute(const void* pLHS, const void* pRHS)
	{
		const TYPE& lhs = *static_cast<const TYPE*>(pLHS);
		const TYPE& rhs = *static_cast<const TYPE*>(pRHS);
		return (lhs == rhs);
	}
};

// Operator: Greater or equal
class CGreaterOrEqualOperator
{
public:
	typedef bool (* FuncPtr)(const void* pLHS, const void* pRHS);
	constexpr CGreaterOrEqualOperator(FuncPtr ptr = nullptr)
		: m_ptr(ptr)
	{}

	operator bool() const { return m_ptr != nullptr; }
	bool operator()(const void* pLHS, const void* pRHS) { return m_ptr(pLHS, pRHS); }

private:
	FuncPtr m_ptr;
};

template<typename TYPE, bool IS_SUPPORTED = Traits::IsGreaterOrEqualComparable<TYPE>::value>
class CAdapter_GreaterOrEqual : public CGreaterOrEqualOperator
{

};

template<typename TYPE>
class CAdapter_GreaterOrEqual<TYPE, true> : public CGreaterOrEqualOperator
{
public:
	constexpr CAdapter_GreaterOrEqual()
		: CGreaterOrEqualOperator(&CAdapter_GreaterOrEqual::Execute)
	{}

private:
	static bool Execute(const void* pLHS, const void* pRHS)
	{
		const TYPE& lhs = *static_cast<const TYPE*>(pLHS);
		const TYPE& rhs = *static_cast<const TYPE*>(pRHS);
		return (lhs >= rhs);
	}
};

// Operator: Less or equal
class CLessOrEqualOperator
{
public:
	typedef bool (* FuncPtr)(const void* pLHS, const void* pRHS);
	constexpr CLessOrEqualOperator(FuncPtr ptr = nullptr)
		: m_ptr(ptr)
	{}

	operator bool() const { return m_ptr != nullptr; }
	bool operator()(const void* pLHS, const void* pRHS) { return m_ptr(pLHS, pRHS); }

private:
	FuncPtr m_ptr;
};

template<typename TYPE, bool IS_SUPPORTED = Traits::IsLessOrEqualComparable<TYPE>::value>
class CAdapter_LessOrEqual : public CLessOrEqualOperator
{

};

template<typename TYPE>
class CAdapter_LessOrEqual<TYPE, true> : public CLessOrEqualOperator
{
public:
	constexpr CAdapter_LessOrEqual()
		: CLessOrEqualOperator(&CAdapter_LessOrEqual::Execute)
	{}

private:
	static bool Execute(const void* pLHS, const void* pRHS)
	{
		const TYPE& lhs = *static_cast<const TYPE*>(pLHS);
		const TYPE& rhs = *static_cast<const TYPE*>(pRHS);
		return (lhs <= rhs);
	}
};

// Operator: Greater
class CGreaterOperator
{
public:
	typedef bool (* FuncPtr)(const void* pLHS, const void* pRHS);
	constexpr CGreaterOperator(FuncPtr ptr = nullptr)
		: m_ptr(ptr)
	{}

	operator bool() const { return m_ptr != nullptr; }
	bool operator()(const void* pLHS, const void* pRHS) { return m_ptr(pLHS, pRHS); }

private:
	FuncPtr m_ptr;
};

template<typename TYPE, bool IS_SUPPORTED = Traits::IsGreaterComparable<TYPE>::value>
class CAdapter_Greater : public CGreaterOperator
{

};

template<typename TYPE>
class CAdapter_Greater<TYPE, true> : public CGreaterOperator
{
public:
	constexpr CAdapter_Greater()
		: CAdapter_Greater(& CAdapter_Greater::Execute)
	{}

private:
	static bool Execute(const void* pLHS, const void* pRHS)
	{
		const TYPE& lhs = *static_cast<const TYPE*>(pLHS);
		const TYPE& rhs = *static_cast<const TYPE*>(pRHS);
		return (lhs > rhs);
	}
};

// Operator: Less
class CLessOperator
{
public:
	typedef bool (* FuncPtr)(const void* pLHS, const void* pRHS);
	constexpr CLessOperator(FuncPtr ptr = nullptr)
		: m_ptr(ptr)
	{}

	operator bool() const { return m_ptr != nullptr; }
	bool operator()(const void* pLHS, const void* pRHS) { return m_ptr(pLHS, pRHS); }

private:
	FuncPtr m_ptr;
};

template<typename TYPE, bool IS_SUPPORTED = Traits::IsLessComparable<TYPE>::value>
class CAdapter_Less : public CLessOperator
{

};

template<typename TYPE>
class CAdapter_Less<TYPE, true> : public CLessOperator
{
public:
	constexpr CAdapter_Less()
		: CLessOperator(&CAdapter_Less::Execute)
	{}

private:
	static bool Execute(const void* pLHS, const void* pRHS)
	{
		const TYPE& lhs = *static_cast<const TYPE*>(pLHS);
		const TYPE& rhs = *static_cast<const TYPE*>(pRHS);
		return (lhs <= rhs);
	}
};

// TODO: Result?
// Operator: Conversion
class CConversionOperator
{
public:
	typedef void (* FuncPtr)(void* pLHS, const void* pRHS);
	constexpr CConversionOperator(FuncPtr ptr = nullptr)
		: m_ptr(ptr)
	{}

	operator bool() const { return m_ptr != nullptr; }
	void operator()(void* pLHS, const void* pRHS) { m_ptr(pLHS, pRHS); }

private:
	FuncPtr m_ptr;
};

template<typename LHS_TYPE, typename RHS_TYPE, bool IS_SUPPORTED = Traits::IsConvertible<LHS_TYPE, RHS_TYPE>::value>
class CAdapter_Conversion : public CConversionOperator
{
};

template<typename LHS_TYPE, typename RHS_TYPE>
class CAdapter_Conversion<LHS_TYPE, RHS_TYPE, true> : public CConversionOperator
{
public:
	CAdapter_Conversion()
		: CConversionOperator(&CAdapter_Conversion::Execute)
	{}

private:
	static void Execute(void* pLHS, const void* pRHS)
	{
		LHS_TYPE& lhs = *static_cast<LHS_TYPE*>(pLHS);
		const RHS_TYPE& rhs = *static_cast<const RHS_TYPE*>(pRHS);
		lhs = rhs;
	}
};
// ~TODO

} // ~Cry namespace
} // ~Reflection namespace
