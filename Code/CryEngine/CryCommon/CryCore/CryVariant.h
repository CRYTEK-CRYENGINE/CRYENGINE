// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <type_traits>
#include <CryCore/StlUtils.h>

// Based on C++17's std::variant interface: http://en.cppreference.com/w/cpp/utility/variant
// Everything that would be in the 'std' namespace according to the standard was implemented in the 'stl' namespace.
template<class...>
class CryVariant;

namespace cry_variant
{
template<class, class...>
struct get_index;

template<class T, class... Types>
struct get_index<T, CryVariant<T, Types...>> : std::integral_constant<size_t, 0> {};

template<class T, class U, class... Types>
struct get_index<T, CryVariant<U, Types...>> : std::integral_constant<size_t, get_index<T, CryVariant<Types...>>::value + 1> {};

static_assert(get_index<bool, CryVariant<bool, char, short, int>>::value == 0, "Wrong variant index found.");
static_assert(get_index<char, CryVariant<bool, char, short, int>>::value == 1, "Wrong variant index found.");
static_assert(get_index<short, CryVariant<bool, char, short, int>>::value == 2, "Wrong variant index found.");
static_assert(get_index<int, CryVariant<bool, char, short, int>>::value == 3, "Wrong variant index found.");
}

namespace stl
{
template<class T>
struct variant_size;

template<class... Types>
struct variant_size<CryVariant<Types...>> : std::integral_constant<size_t, sizeof...(Types)> {};

static_assert(variant_size<CryVariant<bool, char, short, int>>::value == 4, "Wrong variant size.");

template<size_t, class>
struct variant_alternative;

template<size_t I, class T, class... Types>
struct variant_alternative<I, CryVariant<T, Types...>> : variant_alternative<I - 1, CryVariant<Types...>> {};

template<class T, class... Types>
struct variant_alternative<0, CryVariant<T, Types...>>
{
	typedef T type;
};

static_assert(std::is_same<variant_alternative<0, CryVariant<bool, char, short, int>>::type, bool>::value, "Wrong variant alternative found.");
static_assert(std::is_same<variant_alternative<1, CryVariant<bool, char, short, int>>::type, char>::value, "Wrong variant alternative found.");
static_assert(std::is_same<variant_alternative<2, CryVariant<bool, char, short, int>>::type, short>::value, "Wrong variant alternative found.");
static_assert(std::is_same<variant_alternative<3, CryVariant<bool, char, short, int>>::type, int>::value, "Wrong variant alternative found.");

constexpr size_t variant_npos = -1;

template<class T, class... Types>
bool holds_alternative(const CryVariant<Types...>& variant)
{
	return variant.index() == cry_variant::get_index<typename std::remove_cv<T>::type, CryVariant<Types...>>::value;
}

template<class T, class... Types>
T& get(CryVariant<Types...>& variant)
{
	CRY_ASSERT(holds_alternative<T>(variant));
	return reinterpret_cast<T&>(variant.m_data);
}

template<class T, class... Types>
const T& get(const CryVariant<Types...>& variant)
{
	// Use non-const version
	return get<typename std::remove_cv<T>::type>(const_cast<CryVariant<Types...>&>(variant));
}

template<size_t I, class... Types>
typename stl::variant_alternative<I, CryVariant<Types...>>::type& get(CryVariant<Types...>& variant)
{
	static_assert(I < variant_size<CryVariant<Types...>>::value, "I is too big to be part of Types...");
	return get<typename stl::variant_alternative<I, CryVariant<Types...>>::type>(variant);
}

template<size_t I, class... Types>
const typename stl::variant_alternative<I, CryVariant<Types...>>::type& get(const CryVariant<Types...>& variant)
{
	return get<typename stl::variant_alternative<I, CryVariant<Types...>>::type>(variant);
}

template<class T, class... Types>
typename std::add_pointer<T>::type get_if(CryVariant<Types...>* pVariant)
{
	if (pVariant != nullptr && holds_alternative<T>(*pVariant))
	{
		return &get<T>(*pVariant);
	}
	return nullptr;
}

template<class T, class... Types>
typename std::add_pointer<const T>::type get_if(const CryVariant<Types...>* pVariant)
{
	// Use non-const version
	return get_if<T>(const_cast<CryVariant<Types...>*>(pVariant));
}

template<size_t I, class... Types>
typename std::add_pointer<typename stl::variant_alternative<I, CryVariant<Types...>>::type>::type get_if(CryVariant<Types...>* pVariant)
{
	return get_if<typename stl::variant_alternative<I, CryVariant<Types...>>::type>(pVariant);
}

template<size_t I, class... Types>
typename std::add_pointer<const typename stl::variant_alternative<I, CryVariant<Types...>>::type>::type get_if(const CryVariant<Types...>* pVariant)
{
	return get_if<typename stl::variant_alternative<I, CryVariant<Types...>>::type>(pVariant);
}

template<class Visitor, class... Variants>
auto visit(Visitor&& visitor, Variants&&... variants) -> typename std::result_of<Visitor(Variants&&...)>::type
{
	return visitor(variants...);
}
}

template<class... TArgs>
class CryVariant
{
	template<class T, class... Types>
	friend T& stl::get(CryVariant<Types...>&);

	template<class...>
	struct variant_helper;
	template<class T, class... Types>
	struct variant_helper<T, Types...>;

	typedef typename stl::aligned_union<0, TArgs...>::type data_type;
	typedef variant_helper<TArgs...> helper_type;

	static_assert(stl::variant_size<CryVariant>::value > 0, "CryVariant needs to hold at least one possible type.");

public:
	CryVariant()
		: m_index(0)
	{
		typedef typename stl::variant_alternative<0, CryVariant<TArgs...>>::type type;
		new (&m_data) type();
	}

	CryVariant(const CryVariant& other)
		: m_index(stl::variant_npos)
	{
		memset(&m_data, 0, sizeof(m_data));
		helper_type::copy_value(other, *this);
	}

	CryVariant(CryVariant&& other)
		: m_index(stl::variant_npos)
	{
		memset(&m_data, 0, sizeof(m_data));
		helper_type::move_value(std::forward<CryVariant>(other), *this);
	}

	template<class T>
	CryVariant(const T& other)
		: m_index(stl::variant_npos)
	{
		memset(&m_data, 0, sizeof(m_data));
		emplace<T>(other);
	}

	~CryVariant()
	{
		helper_type::destroy_value(*this);
	}

	size_t index() const
	{
		return m_index;
	}

	bool valueless_by_exception() const
	{
		return index() == stl::variant_npos;
	}

	template<class T, class... Args>
	void emplace(Args&&... args)
	{
		typedef typename std::remove_cv<typename std::remove_reference<T>::type>::type type;

		helper_type::destroy_value(*this);

		new (&m_data) type(std::forward<Args>(args)...);
		m_index = cry_variant::get_index<type, CryVariant>::value;
	}

	template<size_t I, class... Args>
	void emplace(Args&&... args)
	{
		emplace<typename stl::variant_alternative<I, CryVariant>::type>(std::forward<Args...>(args...));
	}

	void swap(CryVariant& other)
	{
		if (*this != other)
		{
			if (index() == other.index())
			{
				helper_type::swap_value(*this, other);
			}
			else
			{
				auto temp = std::move(other);
				other = std::move(*this);
				*this = std::move(temp);
			}
		}
	}

	CryVariant& operator=(const CryVariant& rhs)
	{
		if (*this != rhs)
		{
			helper_type::copy_value(rhs, *this);
		}
		return *this;
	}

	CryVariant& operator=(CryVariant&& rhs)
	{
		if (*this != rhs)
		{
			helper_type::move_value(std::forward<CryVariant>(rhs), *this);
		}
		return *this;
	}

	template<class T>
	CryVariant& operator=(const T& rhs)
	{
		emplace<T>(rhs);
		return *this;
	}

	bool operator==(const CryVariant& rhs) const
	{
		return helper_type::compare_equals(*this, rhs);
	}

	bool operator!=(const CryVariant& rhs) const
	{
		return !(*this == rhs);
	}

	bool operator<(const CryVariant& rhs) const
	{
		return helper_type::compare_less(*this, rhs);
	}

private:
	data_type m_data;
	size_t m_index;
};

template<class... TArgs>
template<class T, class... Types>
struct CryVariant<TArgs...>::variant_helper<T, Types...>
{
	static bool compare_equals(const CryVariant<TArgs...>& lhs, const CryVariant<TArgs...>& rhs)
	{
		if (stl::holds_alternative<T>(lhs))
			return lhs.index() == rhs.index() && stl::get<T>(lhs) == stl::get<T>(rhs);
		else
			return variant_helper<Types...>::compare_equals(lhs, rhs);
	}

	static bool compare_less(const CryVariant<TArgs...>& lhs, const CryVariant<TArgs...>& rhs)
	{
		if (stl::holds_alternative<T>(lhs))
			return lhs.index() == rhs.index() ? stl::get<T>(lhs) < stl::get<T>(rhs) : lhs.index() < rhs.index();
		else
			return variant_helper<Types...>::compare_less(lhs, rhs);
	}

	static void copy_value(const CryVariant<TArgs...>& from, CryVariant<TArgs...>& to)
	{
		if (!from.valueless_by_exception())
		{
			if (stl::holds_alternative<T>(from))
			{
				to.emplace<T>(stl::get<T>(from));
				to.m_index = from.index();
			}
			else
			{
				variant_helper<Types...>::copy_value(from, to);
			}
		}
		else
		{
			// "Copy" of an empty variant
			destroy_value(to);
		}
	}

	static void destroy_value(CryVariant<TArgs...>& variant)
	{
		if (!variant.valueless_by_exception())
		{
			if (stl::holds_alternative<T>(variant))
			{
				stl::get<T>(variant).~T();
				variant.m_index = stl::variant_npos;
			}
			else
			{
				variant_helper<Types...>::destroy_value(variant);
			}
		}
	}

	static void move_value(CryVariant<TArgs...>&& from, CryVariant<TArgs...>& to)
	{
		if (!from.valueless_by_exception())
		{
			if (stl::holds_alternative<T>(from))
			{
				to.emplace<T>(std::move(stl::get<T>(from)));
				to.m_index = from.index();
				destroy_value(from);
			}
			else
			{
				variant_helper<Types...>::move_value(std::forward<CryVariant<TArgs...>>(from), to);
			}
		}
		else
		{
			// "Move" an empty variant
			destroy_value(to);
		}
	}

	static void swap_value(CryVariant<TArgs...>& lhs, CryVariant<TArgs...>& rhs)
	{
		if (!lhs.valueless_by_exception() && !rhs.valueless_by_exception())
		{
			if (stl::holds_alternative<T>(lhs))
			{
				CRY_ASSERT_MESSAGE(stl::holds_alternative<T>(rhs), "Both variants need to contain the same type!");
				std::swap(stl::get<T>(lhs), stl::get<T>(rhs));
			}
			else
			{
				variant_helper<Types...>::swap_value(lhs, rhs);
			}
		}
	}
};

template<class... TArgs>
template<class...>
struct CryVariant<TArgs...>::variant_helper
{
	static bool compare_equals(const CryVariant<TArgs...>&, const CryVariant<TArgs...>&)       { return false; }
	static bool compare_less(const CryVariant<TArgs...>& lhs, const CryVariant<TArgs...>& rhs) { return lhs.index() < rhs.index(); }
	static void copy_value(const CryVariant<TArgs...>& from, CryVariant<TArgs...>& to)         { destroy_value(to); }
	static void destroy_value(CryVariant<TArgs...>&)                                           {}
	static void move_value(CryVariant<TArgs...>&&, CryVariant<TArgs...>&)                      {}
	static void swap_value(CryVariant<TArgs...>&, CryVariant<TArgs...>&)                       {}
};
