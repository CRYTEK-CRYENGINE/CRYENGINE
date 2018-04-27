// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __INCLUDING_FROM_TYPELIST_H__
	#error should only include this file from typelist.h
#endif

#include <CryCore/Assert/CryAssert.h>
#include <type_traits>

namespace NTypelist
{
namespace impl {
template<class TList> struct length;
}
namespace impl {
template<class TList> struct max_alignment;
}
namespace impl {
template<class TList> struct max_sizeof;
}
namespace impl {
template<class TList, class T> struct find;
}
namespace impl {
template<class TList, size_t Index> struct get;
}
namespace impl {
template<class TList1, class TList2> struct join;
}
namespace impl {
template<class TList, template<class> class TTrait> struct runtime_trait_evaluator;
}
namespace impl {
template<class TList, class TInit, template<class, class> class TFunc> struct foldl;
}
namespace impl {
template<class TList, class TInit, template<class, class> class TFunc> struct foldr;
}

//! Retrieves the index of the first occurrence of the specified type within a Typelist.
//! Fires a static_assert in case the Typelist does not contain the requested type.
template<class TList, class T>
struct IndexOf
{
	static_assert(impl::find<TList, T>::value < impl::length<TList>::value, "TList does not contain the specified type T!");
	static const size_t value = impl::find<TList, T>::value;
};

//! Retrieves a single type stored within a Typelist by its index.
//! Fires a static_assert in case the index is out of range.
template<class TList, size_t Index>
struct TypeAt
{
	static_assert(Index < impl::length<TList>::value, "Index is out of range!");
	typedef typename impl::get<TList, Index>::type type;
};

//! Computes the length of a Typelist.
template<class TList>
struct Length
{
	static const size_t value = impl::length<TList>::value;
};

//! Computes the maximum sizeof() of all types stored within a Typelist.
//! \return 0 for an empty Typelist.
template<class TList>
struct MaximumSize
{
	static const size_t value = impl::max_sizeof<TList>::value;
};

//! Computes the maximum alignment requirement of all types stored within a Typelist.
//! \return 1 for an empty Typelist.
template<class TList>
struct MaximumAlignment
{
	static const size_t value = impl::max_alignment<TList>::value;
};

//! Evaluates to true if Typelist contains the specified type T.
template<class TList, class T>
struct IncludesType
{
	static const bool value = (impl::find<TList, T>::value < impl::length<TList>::value);
};

//! Concatenates two Typelists together.
//! The resulting type is a Typelist containing all elements of TList1 followed by all
//! elements of TList2. The relative order of elements within both Typelists are preserved.
template<class TList1, class TList2>
struct ListJoin
{
	typedef typename impl::join<TList1, TList2>::type type;
};

//! Wraps an integral value with a type.
template<int I>
struct Int2Type
{
	static const int value = I;
};

//! Given a Typelist and a trait template, instantiates a functor type that performs runtime evaluation of the trait for a given index of the Typelist element.
//! TODO: Additional support for trait's static member functions might be handy.
//! \param TList The Typelist to iterate.
//! \param TTrait The trait template to evaluate. Shall provide a 'value' static member which type is consistent for all of the Typelist's elements.
template<class TList, template<class> class TTrait>
struct RuntimeTraitEvaluator
{
	static_assert(impl::length<TList>::value > 0, "A runtime_trait_evaluator cannot be instantiated for an empty list!");
	typedef typename impl::runtime_trait_evaluator<TList, TTrait>::value_type value_type;

	//! Evaluates the trait for a single Typelist element.
	//! \param index Index of the Typelist element.
	//! \return The value of the trait template.
	value_type operator()(size_t index) const
	{
		assert(index < impl::length<TList>::value);
		return impl::runtime_trait_evaluator<TList, TTrait>()(index);
	}
};

//! A meta-function that performs a left-fold operation on a Typelist.
//! \param TList The Typelist to fold.
//! \param TInit Initial value of the fold.
//! \param TFunc A binary folding operation. Shall take the current fold value (accumulator) as the fist template argument and a Typelist element as the second. Shall evaluate to the new fold value.
template<class TList, class TInit, template<class, class> class TFunc>
struct foldl
	: impl::foldl<TList, TInit, TFunc>
{};

//! A meta-function that performs a right-fold operation on a Typelist.
//! \param TList The Typelist to fold.
//! \param TInit Initial value of the fold.
//! \param TFunc A binary folding operation. Shall take the current fold value (accumulator) as the fist template argument and a Typelist element as the second. Shall evaluate to the new fold value.
template<class TList, class TInit, template<class, class> class TFunc>
struct foldr
	: impl::foldr<TList, TInit, TFunc>
{};

}

namespace NTypelist
{
namespace impl
{

template<class TList>
struct length
{
	struct init
	{
		static const size_t value = 0;
	};

	template<class TAcc, class Tx>
	struct evaluator
	{
		static const size_t value = TAcc::value + 1;
	};

	static const size_t value = foldl<TList, init, evaluator>::value;
};

template<class TList>
struct max_alignment
{
	struct init
	{
		static const size_t value = 1;
	};

	template<class TAcc, class Tx>
	struct evaluator
	{
		static const size_t value = std::alignment_of<Tx>::value > TAcc::value
		                            ? std::alignment_of<Tx>::value
		                            : TAcc::value;
	};

	static const size_t value = foldl<TList, init, evaluator>::value;
};

template<class TList>
struct max_sizeof
{
	struct init
	{
		static const size_t value = 0;
	};

	template<class TAcc, class Tx>
	struct evaluator
	{
		static const size_t value = sizeof(Tx) > TAcc::value
		                            ? sizeof(Tx)
		                            : TAcc::value;
	};

	static const size_t value = foldl<TList, init, evaluator>::value;
};

template<class TList, typename T>
struct find
{
	struct init
	{
		static const size_t value = 0;
	};

	template<class TAcc, class Tx>
	struct evaluator
	{
		static const size_t value = std::is_same<T, Tx>::value
		                            ? 0
		                            : TAcc::value + 1;
	};

	static const size_t value = foldr<TList, init, evaluator>::value;
};

template<class TList, size_t Index>
struct get
{
	struct init
	{
		static const size_t item_index = Index + 1;
		typedef void type;      //!< A dummy type for the std::conditional.
	};

	template<class TAcc, class Tx>
	struct evaluator
	{
		static const size_t item_index = TAcc::item_index - 1;
		typedef typename std::conditional<item_index == 0, Tx, typename TAcc::type>::type type;
	};

	typedef typename foldl<TList, init, evaluator>::type type;
};

template<class TList1, class TList2>
struct join
{
	struct init
	{
		typedef TList2 type;
	};

	template<class TAcc, class Tx>
	struct evaluator
	{
		typedef CNode<Tx, typename TAcc::type> type;
	};

	typedef typename foldr<TList1, init, evaluator>::type type;
};

template<class TList, template<class> class TTrait>
struct runtime_trait_evaluator
{
	typedef decltype (TTrait<typename get<TList, 1>::type>::value) value_type;

	struct init
	{
		static const size_t iteration = length<TList>::value;

		static value_type   evaluate(size_t index)
		{
			assert(false);
			return size_t();
		}
	};

	template<class TAcc, class Tx>
	struct evaluator
	{
		static const size_t iteration = TAcc::iteration - 1;

		static value_type   evaluate(size_t index)
		{
			if (index == iteration)
			{
				return TTrait<Tx>::value;
			}
			else
			{
				return TAcc::evaluate(index);
			}
		}
	};

	value_type operator()(size_t index) const
	{
		return foldr<TList, init, evaluator>::evaluate(index);
	}
};

template<class THead, class TTail, class TInit, template<class, class> class TFunc>
struct foldl<CNode<THead, TTail>, TInit, TFunc>
	: foldl<TTail, TFunc<TInit, THead>, TFunc>
{};

template<class TInit, template<class, class> class TFunc>
struct foldl<CEnd, TInit, TFunc>
	: TInit
{};

template<class THead, class TTail, class TInit, template<class, class> class TFunc>
struct foldr<CNode<THead, TTail>, TInit, TFunc>
	: TFunc<foldr<TTail, TInit, TFunc>, THead>
{};

template<class TInit, template<class, class> class TFunc>
struct foldr<CEnd, TInit, TFunc>
	: TInit
{};

}
}
