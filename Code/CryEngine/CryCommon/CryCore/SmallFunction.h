// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <type_traits>        // std::aligned_storage
#include <utility>            // std::forward
#include <CryCore/StlUtils.h> // stl::conjunction

//! Flat callable wrapper
//! SmallFunction can store, copy and invoke functions, callable objects and lambdas similar to std::function.
//! It uses a fixed buffer of specified size for storage and does not allocate on the heap.
//! The class doesn't use virtual methods, instead it stores all type-erased calls directly in the structure. 
//! SmallFunction is default constructible, copyable and movable.
template<typename Signature, std::size_t BufferSize = 32U> struct SmallFunction;

template<typename ReturnType, typename ... ArgType, std::size_t BufferSize>
struct SmallFunction<ReturnType(ArgType ...), BufferSize> final
{
	SmallFunction() noexcept
		: m_Call(nullptr)
		, m_Dtor([](void* storage) { /* nothing to do */ })
		, m_Clone([](const void* srcStorage, void* dstStorage) { /* nothing to do */ })
	{
	}

	SmallFunction(std::nullptr_t) noexcept : SmallFunction() {}

	SmallFunction& operator= (std::nullptr_t) noexcept
	{
		Destroy();
		return *this;
	}

	template<
		typename F,
		typename = typename std::enable_if<!std::is_function<F>::value && !std::is_same<typename std::decay<F>::type, SmallFunction>::value>::type
	>
	SmallFunction& operator= (F&& callable) noexcept
	{
		m_Dtor(&m_Storage);
		Init(std::forward<F>(callable));
		return *this;
	}

	template<typename F, typename = typename std::enable_if<std::is_function<F>::value>::type>
	SmallFunction& operator= (F& function) noexcept
	{
		m_Dtor(&m_Storage);
		Init(&function);
		return *this;
	}

	template<
		typename F,
		typename = typename std::enable_if<!std::is_function<F>::value && !std::is_same<typename std::decay<F>::type, SmallFunction>::value>::type
		>
	SmallFunction(F&& callable) noexcept
	{
		Init(std::forward<F>(callable));
	}

	template<typename F, typename = typename std::enable_if<std::is_function<F>::value>::type>
	SmallFunction(F& function)
	{
		Init(&function);
	}

	~SmallFunction()
	{
		m_Dtor(&m_Storage);
	}

	SmallFunction(const SmallFunction& other) noexcept
		: m_Call(other.m_Call)
		, m_Dtor(other.m_Dtor)
		, m_Clone(other.m_Clone)
	{
		m_Clone(&other.m_Storage, &this->m_Storage);
	}

	SmallFunction& operator=(const SmallFunction& other) noexcept
	{
		m_Dtor(&m_Storage);
		m_Call = other.m_Call;
		m_Dtor = other.m_Dtor;
		m_Clone = other.m_Clone;
		m_Clone(&other.m_Storage, &this->m_Storage);
		return *this;
	}

	SmallFunction(SmallFunction&& other) noexcept
		: m_Call(other.m_Call)
		, m_Dtor(other.m_Dtor)
		, m_Clone(other.m_Clone)
	{
		m_Clone(&other.m_Storage, &this->m_Storage);
		other = nullptr;
	}

	SmallFunction& operator=(SmallFunction&& other) noexcept
	{
		m_Dtor(&m_Storage);
		m_Call = other.m_Call;
		m_Dtor = other.m_Dtor;
		m_Clone = other.m_Clone;
		m_Clone(&other.m_Storage, &this->m_Storage);
		other = nullptr;
		return *this;
	}

	template<typename ... ActualArgType>
	ReturnType operator()(ActualArgType&& ... args)
	{
		static_assert(sizeof ... (ActualArgType) == sizeof ... (ArgType) && 
			stl::conjunction<std::is_convertible<ActualArgType, ArgType> ...>::value,
			"Incompatible parameters called on the function"
		);
		return m_Call(&m_Storage, std::forward<ActualArgType>(args) ...);
	}

	explicit operator bool() const noexcept
	{
		return m_Call != nullptr;
	}

private:

	template<typename F>
	void Init(F&& callable) noexcept
	{
		using CallableType = typename std::remove_reference<F>::type;
		static_assert(sizeof(CallableType) <= BufferSize, "Callable object is too large to fit in the buffer, try increasing the function buffer size.");
		new(&m_Storage) CallableType(std::forward<F>(callable));

		m_Call = [](void* storage, const ArgType& ... args) -> ReturnType
		{
			CallableType* pCallable = reinterpret_cast<CallableType*>(storage);
			return (*pCallable)(args ...);
		};
		m_Dtor = [](void* storage)
		{
			reinterpret_cast<CallableType*>(storage)->~CallableType();
		};
		m_Clone = [](const void* srcStorage, void* dstStorage)
		{
			const CallableType* pCallable = reinterpret_cast<const CallableType*>(srcStorage);
			new(dstStorage) CallableType(*pCallable);
		};
	}

	void Destroy() noexcept
	{
		m_Dtor(&m_Storage);
		m_Call = nullptr;
		m_Dtor = [](void* storage) {};
		m_Clone = [](const void* srcStorage, void* dstStorage) { };
	}

	ReturnType (* m_Call)(void* storage, const ArgType& ... args);
	void (* m_Dtor)(void* storage);
	void (* m_Clone)(const void* srcStorage, void* dstStorage);

	typename std::aligned_storage<BufferSize>::type m_Storage;
};

template<typename T> bool operator== (const SmallFunction<T>& lhs, std::nullptr_t) { return !lhs; }
template<typename T> bool operator== (std::nullptr_t, const SmallFunction<T>& rhs) { return !rhs; }
template<typename T> bool operator!= (const SmallFunction<T>& lhs, std::nullptr_t) { return !!lhs; }
template<typename T> bool operator!= (std::nullptr_t, const SmallFunction<T>& rhs) { return !!rhs; }
