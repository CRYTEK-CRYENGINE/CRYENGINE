/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

namespace yasli{

class Archive;
namespace Helpers{

template<bool C, class T1, class T2>
struct Selector{};

template<class T1, class T2>
struct Selector<false, T1, T2>{
    typedef T2 type;
};

template<class T1, class T2>
struct Selector<true, T1, T2>{
    typedef T1 type;
};

template<class C, class T1, class T2>
struct Select{
    typedef typename Selector<C::value, T1,T2>::type selected_type;
    typedef typename selected_type::type type;
};

template<class T>
struct Identity{
    typedef T type;
};

template<class T, int Size>
struct ArraySize{
    enum{ value = true };
};

template<int Size, class S8, class S16, class S32, class S64>
struct SelectIntSize {};
template<class S8, class S16, class S32, class S64> struct SelectIntSize<1, S8, S16, S32, S64> { typedef S8 type; };
template<class S8, class S16, class S32, class S64> struct SelectIntSize<2, S8, S16, S32, S64> { typedef S16 type; };
template<class S8, class S16, class S32, class S64> struct SelectIntSize<4, S8, S16, S32, S64> { typedef S32 type; };
template<class S8, class S16, class S32, class S64> struct SelectIntSize<8, S8, S16, S32, S64> { typedef S64 type; };

template<class T> struct IsSigned { enum { value = false }; };
template<> struct IsSigned<signed char> { enum { value = true }; };
template<> struct IsSigned<signed short> { enum { value = true }; };
template<> struct IsSigned<signed int> { enum { value = true }; };
template<> struct IsSigned<signed long> { enum { value = true }; };
template<> struct IsSigned<signed long long> { enum { value = true }; };

}
}

