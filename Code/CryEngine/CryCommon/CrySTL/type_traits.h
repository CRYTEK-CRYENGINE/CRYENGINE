// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.
#include <type_traits> // std::false_type, std::true_type

namespace stl
{
	//! Until CWG 1558 (a C++14 defect), unused parameters in alias templates were not guaranteed to ensure SFINAE and could be ignored, so earlier compilers require a more complex definition of void_t
	//! See std::void_t
	template <typename... Ts>
	struct make_void
	{
		using type = void;
	};

	//! Equivalent of std::void_t (C++17)
	template <typename... Ts>
	using void_t = typename make_void<Ts...>::type;

	namespace detail
	{
		struct nonesuch
		{
			nonesuch() = delete;
			~nonesuch() = delete;
			nonesuch(nonesuch const&) = delete;
			void operator=(nonesuch const&) = delete;
		};

		template <class Default, class AlwaysVoid, template <class...> class Op, class... Args>
		struct detector
		{
			using value_t = std::false_type;
			using type = Default;
		};

		template <class Default, template <class...> class Op, class... Args>
		struct detector<Default, void_t<Op<Args...>>, Op, Args...>
		{
			using value_t = std::true_type;
			using type = Op<Args...>;
		};

	} // namespace detail

	//! Equivalent of std::experimental::is_detected (potentially C++20)
	//! see library fundamentals TS v2
	//! 
	//! Validates whether an expression can be compiled by trying to instantiate the template 
	//! Op<Args...>. If the template instantiation succeeds, is_detected is std::true_type,
	//! otherwise it is equal to std::false_type.
	//!
	//! Example Usage:
	//! template<class T> using SerializationFunc = decltype( std::declval<T>().Serialize( std::declval<Serialization::IArchive&>() ) );
	//! 
	//! static_assert(stl::is_detected<SerializationFunc, MyClass>::value, "Must provide member function void Serialize(Serialization::IArchive& ar) !!");
	template <template <class...> class Op, class... Args>
	using is_detected = typename detail::detector<detail::nonesuch, void, Op, Args...>::value_t;
}