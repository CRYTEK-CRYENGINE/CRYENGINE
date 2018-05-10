// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
namespace detail
{
	//////////////////////////////////////////////////////////////////////////
	template<char ... Chars>
	struct Accumulator
	{
		template<char Char>
		struct Add
		{
			typedef Accumulator<Chars ..., Char> type;
		};

		// Avoid recursive creation of arrays on the stack
		template<typename Dummy>
		struct Value
		{
			static constexpr char value	[sizeof...(Chars) + 1] = {Chars ..., '\0' };
		};
	};

	//////////////////////////////////////////////////////////////////////////
	template<char ... Chars>
	template<typename Dummy>
	constexpr char Accumulator<Chars ...>::Value<Dummy>::value[];

	//////////////////////////////////////////////////////////////////////////
	template<typename Accumulated, char ... Chars>
	struct Recurse;

	//////////////////////////////////////////////////////////////////////////
	// Normal recursion
	template<typename Accumulated, char Front, char ... Tail>
	struct Recurse<Accumulated, Front, Tail ...>
	{
		typedef typename Accumulated::template Add<Front>::type Step;
		typedef typename Recurse<Step, Tail ...>::type          type;
	};

	//////////////////////////////////////////////////////////////////////////
	// NULL Terminator, end recursion
	template<typename Accumulated, char ... Tail>
	struct Recurse<Accumulated, '\0', Tail ...>
	{
		typedef Accumulated type;
	};

	//////////////////////////////////////////////////////////////////////////
	// End recursion
	template<typename Accumulated>
	struct Recurse<Accumulated>
	{
		typedef Accumulated type;
	};
}

//////////////////////////////////////////////////////////////////////////
#define STR_1(S, I)  (I < sizeof(S) ? S[I] : '\0')
#define STR_2(S, I)  STR_1(S, I), STR_1(S, I + 1)
#define STR_4(S, I)  STR_2(S, I), STR_2(S, I + 2)
#define STR_8(S, I)  STR_4(S, I), STR_4(S, I + 4)
#define STR_16(S, I) STR_8(S, I), STR_8(S, I + 8)
#define STR_32(S, I) STR_16(S, I), STR_16(S, I + 16)
#define STR_64(S, I) STR_32(S, I), STR_32(S, I + 32)
#define STR_128(S, I) STR_64(S, I), STR_64(S, I + 64)

//////////////////////////////////////////////////////////////////////////
template<char ... Chars>
struct CompileTimeString
{
	typedef typename detail::Recurse<detail::Accumulator<>, Chars ...>::type::template Value<void> TFinalString;
};

// Compile time string can be used in cases where the compiler requires a fixed memory location for const char* at compile time.
// e.g. when passing const char* as a template parameter which then uses it in constexpr: E.g. class Foo : public Bar<"MyString"> { .... }; template <const char* szStr> class Bar { constexpr GetStr() { return szStr; }
// Note: Maximum 128 characters
#define COMPILE_TIME_STRING(S) CompileTimeString<STR_128(S, 0)>::TFinalString::value